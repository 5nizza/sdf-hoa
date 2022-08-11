#include <queue>
#include "reg/mixed_domain.hpp"
#include "reg/atm_parsing_naming.hpp"
#include "reg/tst_asgn.hpp"

#include "reg/graph_algo.hpp"
#include "reg/types.hpp"

#define BDD spotBDD
    #include "spot/tl/formula.hh"
    #include "spot/twa/twagraph.hh"
    #include "spot/twa/formula2bdd.hh"
#undef BDD


using namespace std;
using namespace sdf;
using namespace spot;
using namespace graph;

#define hmap unordered_map
#define hset unordered_set

using GA = graph::GraphAlgo;
using P = Partition;


twa_graph_ptr
MixedDomain::preprocess(const twa_graph_ptr& reg_ucw)  // NOLINT (hide 'make it static')
{
    twa_graph_ptr g = spot::make_twa_graph(reg_ucw->get_dict());
    g->copy_ap_of(reg_ucw);
    g->copy_acceptance_of(reg_ucw);

    hmap<uint, uint> new_by_old;  // (technical) relate states in the old an new automaton (they have the same states up to renaming)
    auto s = [&g, &new_by_old](const uint& reg_ucw_s)
    {
        if (new_by_old.find(reg_ucw_s) == new_by_old.end())
            new_by_old.insert({reg_ucw_s, g->new_state()});
        return new_by_old.at(reg_ucw_s);
    };

    g->set_init_state(s(reg_ucw->get_init_state_number()));

    for (const auto& e : reg_ucw->edges())
    {
        auto vars_as_conj = spot::bdd_to_formula(bdd_support(e.cond), g->get_dict());

        MASSERT(vars_as_conj.is_constant() or
                vars_as_conj.is(spot::op::And) or
                vars_as_conj.is(spot::op::ap),
                "unexpected");

        vector<formula> aps;

        if (vars_as_conj.is(spot::op::And))
            for (const auto& it : vars_as_conj)
                aps.push_back(it);
        else if (vars_as_conj.is(spot::op::ap))
            aps.push_back(vars_as_conj);
        // else aps is empty

        bdd new_cond = e.cond;
        for (const auto& ap : aps)
        {
            if (!is_tst(ap.ap_name()))
                continue;

            auto [t1,t2,cmp] = parse_tst(ap.ap_name());
            if (cmp == ">" or cmp == "<" or cmp == "=" or cmp == "≠")
                continue;

            formula ap1, ap2;
            if (cmp == "≥")
            {
                ap1 = formula::ap(t1 + ">" + t2);  // NOLINT
                ap2 = formula::ap(t1 + "=" + t2);  // NOLINT
            }
            if (cmp == "≤")
            {
                ap1 = formula::ap(t1 + "<" + t2);  // NOLINT
                ap2 = formula::ap(t1 + "=" + t2);  // NOLINT
            }

            g->register_ap(ap1);
            g->register_ap(ap2);

            bdd subst = formula_to_bdd(formula::Or({ap1, ap2}), g->get_dict(), g);

            auto bdd_id_of_ap = g->register_ap(ap);  // helps us to get BDD id of the ap (otherwise does nothing)
            new_cond = bdd_compose(new_cond, subst, bdd_id_of_ap); // NB: new_cond may be equivalent to false due to conflicting tests like "out>r & out=r"
        }

        g->new_edge(s(e.src), s(e.dst), new_cond, e.acc);
    }

    return g;
}

P
MixedDomain::build_init_partition(const string_hset& sysR,
                                  const string_hset& atmR)
{
    // init_p: all atm reg are equal, sys regs are unrelated

    SpecialGraph g;
    VtoEC v_to_ec;

    auto next_v = 0;
    g.add_vertex(next_v);
    v_to_ec[next_v] = atmR;
    ++next_v;

    for (const auto& s : sysR)
    {
        g.add_vertex(next_v);
        v_to_ec[next_v] = {s};
        ++next_v;
    }

    return {g,v_to_ec};
}


optional<P>
add_io_info(const P& p, const hset<TstAtom>& atm_tst_atoms)
{
    // copy
    auto g = p.graph;
    auto v_to_ec = p.v_to_ec;

    add_vertex(g, v_to_ec, IN);
    add_vertex(g, v_to_ec, OUT);

    for (const auto& tst_atom : atm_tst_atoms)
    {
        auto v1 = get_vertex_of(tst_atom.t1, v_to_ec),
             v2 = get_vertex_of(tst_atom.t2, v_to_ec);

        if (tst_atom.relation == TstAtom::equal)
        {
            if (v1 == v2)
                continue;

            GA::merge_v1_into_v2(g, v1, v2);
            auto v1_ec = v_to_ec.at(v1);
            v_to_ec.at(v2).insert(v1_ec.begin(), v1_ec.end());
            v_to_ec.erase(v1);
        }
        else if (tst_atom.relation == TstAtom::less)
            g.add_dir_edge(v2, v1);  // t1<t2
        else if (tst_atom.relation == TstAtom::nequal)
            g.add_neq_edge(v1, v2);
        else
            UNREACHABLE();
    }

    if (GA::has_dir_cycles(g) || GA::has_neq_self_loops(g))
        return {};

    return P(g, v_to_ec);
}

/** Keep a vertex iff it has an automaton register in its EC;
    I.e., remove the vertices consisting of purely system registers. */
P
extract_atm_p(const P& p)
{
    // copy
    auto new_v_to_ec = p.v_to_ec;
    auto new_g = p.graph;

    for (const auto& [v,ec] : p.v_to_ec)  // we modify new_v_to_ec => iterate over its const copy
    {
        if (all_of(ec.begin(), ec.end(), is_sys_reg_name))
        {
            GA::close_vertex(new_g, v);
            new_v_to_ec.erase(v);
        }
    }

    return {new_g, new_v_to_ec};
}

/**
 * Restore the previously removed purely-system vertices into the graph.
 * This function does not introduce loops, assuming new_g and atm_sys_p have no loops.
 */
void enhance_with_sys(SpecialGraph& new_g, VtoEC& new_v_to_ec, const P& atm_sys_p)
{
    auto last = 0u;
    for (const auto& v : new_g.get_vertices())
        last = max(last, v);

    auto added_vertices = vector<V>();

    // first, add vertices, update new_v_to_ec
    for (const auto& [old_v,ec] : atm_sys_p.v_to_ec)
        if (all_of(ec.begin(), ec.end(), is_sys_reg_name))  // EC is completely system hence was previously removed by `extract_atm_p`
        {
            // create a new vertex in new_g, update new_v_to_ec, add edges
            auto new_v = ++last;
            new_g.add_vertex(new_v);
            new_v_to_ec.insert({new_v, ec});
            added_vertices.push_back(new_v);
        }

    // now add edges
    for (const auto& new_v : added_vertices)
    {
        auto old_v = get_vertex_of(*new_v_to_ec.at(new_v).begin(), atm_sys_p.v_to_ec);      // (any register from the EC will do)
        for (const auto& old_c : atm_sys_p.graph.get_children(old_v))
        {
            auto new_c = get_vertex_of(*atm_sys_p.v_to_ec.at(old_c).begin(), new_v_to_ec);  // (any register from the EC will do)
            new_g.add_dir_edge(new_v, new_c);
        }
        for (const auto& old_p : atm_sys_p.graph.get_parents(old_v))
        {
            auto new_p = get_vertex_of(*atm_sys_p.v_to_ec.at(old_p).begin(), new_v_to_ec);
            new_g.add_dir_edge(new_p, new_v);
        }
        for (const auto& old_d : atm_sys_p.graph.get_distinct(old_v))
        {
            auto new_d = get_vertex_of(*atm_sys_p.v_to_ec.at(old_d).begin(), new_v_to_ec);
            new_g.add_neq_edge(new_d, new_v);
        }
    }
}

bool is_total(const P& p)
{
    return GA::all_topo_sorts(p.graph).size() == 1;
}

/// O(n^2) worst case
optional<P> refine_if_possible(const P& p,
                               const TstAtom& tst_atom)
{
    auto v1 = get_vertex_of(tst_atom.t1, p.v_to_ec);
    auto v2 = get_vertex_of(tst_atom.t2, p.v_to_ec);

    auto v2_reaches_v1 = GA::walk_descendants(p.graph, v2, [&v1] (const V& v) { return v == v1; });
    auto v1_reaches_v2 = GA::walk_descendants(p.graph, v1, [&v2] (const V& v) { return v == v2; });

    // tst atom is t1{<,=,≠}t2
    if (tst_atom.relation == TstAtom::equal)  // t1=t2
    {
        if (v1 == v2)
            return p;

        if (v2_reaches_v1 ||
            v1_reaches_v2 ||
            p.graph.get_distinct(v2).count(v1))
            return {};

        auto new_v_to_ec = p.v_to_ec;
        auto new_g = p.graph;

        GA::merge_v1_into_v2(new_g, v1, v2);
        new_v_to_ec.at(v2).insert(new_v_to_ec.at(v1).begin(), new_v_to_ec.at(v1).end());
        new_v_to_ec.erase(v1);

        return P(new_g, new_v_to_ec);
    }
    else if (tst_atom.relation == TstAtom::less)  // t1<t2
    {
        if (v2 == v1 || v1_reaches_v2)
            return {};
        auto new_g = p.graph;
        new_g.add_dir_edge(v2, v1);
        return P(new_g, p.v_to_ec);
    }
    else if (tst_atom.relation == TstAtom::nequal)  // t1≠t2
    {
        if (v1 == v2)
            return {};
        auto new_g = p.graph;
        new_g.add_neq_edge(v1, v2);
        return P(new_g, p.v_to_ec);
    }
    else
        UNREACHABLE();
}

/**
 * @param descr_list : vector of triples (var,var,domain), were var ∈ {IN,OUT} ∪ Rsys ∪ Ratm (we need indexed access => use vector)
 *                     Note that every var◇var must be a test expression (e.g., IN<r is OK, IN<OUT is OK, but r1<r2 is not OK).
 * @param cur_idx
 * @param p
 * @param cur_tst
 * @param result
 */
void rec_enumerate(const vector<tuple<string,string,DomainName>>& descr_list,  // NOLINT
                   const uint& cur_idx,
                   const P& p,
                   hset<TstAtom>& cur_tst,
                   vector<pair<P,hset<TstAtom>>>& result)
{
    /// Complexity: high: O(n^2n)
    /// Without the recursive call, this function is n^2.
    /// The recursion depth is n.
    /// Hence O(n^2n). (Can probably be reduced to O(n^n), maybe even to O(const^n)?)
    if (cur_idx == descr_list.size())
    {
        result.emplace_back(p, cur_tst);
        return;
    }

    const auto& [v1,v2,dom] = descr_list.at(cur_idx);

    vector<TstAtom> all_tst_atoms;
    if (dom == DomainName::order)
        all_tst_atoms = {TstAtom(v1, TstAtom::less,  v2),
                         TstAtom(v2, TstAtom::less,  v1),
                         TstAtom(v1, TstAtom::equal, v2)};
    else if (dom == DomainName::equality)
        all_tst_atoms = {TstAtom(v1, TstAtom::equal,  v2),
                         TstAtom(v1, TstAtom::nequal, v2)};
    else
        UNREACHABLE();

    for (const auto& tst_atom : all_tst_atoms)
    {
        auto refined_p = refine_if_possible(p, tst_atom);  // O(n^2)
        if (!refined_p.has_value())
            continue;
        cur_tst.insert(tst_atom);
        rec_enumerate(descr_list, cur_idx + 1, refined_p.value(), cur_tst, result);
        cur_tst.erase(tst_atom);
    }
}

vector<pair<P, hset<TstAtom>>>
MixedDomain::all_possible_atm_tst(const P& atm_sys_p,
                                  const hset<TstAtom>& atm_tst_atoms)
{
    auto p_io = add_io_info(atm_sys_p, atm_tst_atoms);
    if (!p_io.has_value())
        return {};

    auto descr_list = vector<tuple<string,string,DomainName>>();
    descr_list.emplace_back(IN,OUT,background_domain);
    for (const auto& [v,ec] : atm_sys_p.v_to_ec)  // iterate over Ratm
        for (const auto& r : ec)
            if (is_atm_reg_name(r))
            {
                descr_list.emplace_back(IN, r, background_domain);
                descr_list.emplace_back(OUT,r, background_domain);
            }

    auto result = vector<pair<P,hset<TstAtom>>>();
    auto cur_atm_tst = hset<TstAtom>();
    rec_enumerate(descr_list, 0, p_io.value(), cur_atm_tst, result);
    return result;
}

/*
vector<P>
MixedDomain::all_possible_atm_tst(const P& atm_sys_p,
                                  const hset<TstAtom>& atm_tst_atoms)
{
    auto p = extract_atm_p(atm_sys_p);
    MASSERT(is_total(p), "must be total");  // (remove if slows down)
    auto p_io = add_io_info(p, atm_tst_atoms);
    if (!p_io.has_value())
        return {};

    vector<P> result;
    for (auto& [new_g, mapper]: GA::all_topo_sorts2(p_io.value().graph))  // Note: since only IN and OUT are loose, and p_io is complete for Ratm, the complexity 'should be' poly(n), not exp(n)
    {
        hmap<V, EC> new_v_to_ec;
        for (const auto& [newV, setOfOldV]: mapper)
            for (const auto& oldV: setOfOldV)
                for (const auto& r: p_io.value().v_to_ec.at(oldV))
                    new_v_to_ec[newV].insert(r);
        enhance_with_sys(new_g, new_v_to_ec, atm_sys_p);
        result.emplace_back(new_g, new_v_to_ec);
    }

    return result;
}
*/

vector<pair<P, hset<TstAtom>>>
MixedDomain::all_possible_sys_tst(const P& p_io,
                                  const hmap<string, DomainName>& reg_descr)
{
    // check that when background_domain is equality, reg_descr domains are also equality
    if (background_domain == DomainName::equality)
        MASSERT(all_of(reg_descr.begin(), reg_descr.end(),
                       [](const pair<string,DomainName>& reg_dom)
                       { return !is_sys_reg_name(reg_dom.first) || reg_dom.second == DomainName::equality;}),
                "sys regs can't use 'stronger' domain than background_domain");

    auto descr_list = vector<tuple<string,string,DomainName>>();
    for (const auto& [s,dom] : reg_descr)
        descr_list.emplace_back(IN, s, dom);
    auto result = vector<pair<P,hset<TstAtom>>>();
    auto cur_sys_tst = hset<TstAtom>();

    rec_enumerate(descr_list, 0, p_io, cur_sys_tst, result);

    return result;
}

void MixedDomain::update(P& p, const Asgn& asgn)
{
    for (const auto& [io, regs] : asgn.asgn)
    {
        auto v_io = get_vertex_of(io, p.v_to_ec);
        for (const auto& r : regs)
        {
            auto v_r = get_vertex_of(r, p.v_to_ec);
            if (v_r == v_io)
                continue;  // store the same value; has no effect on partition

            p.v_to_ec.at(v_io).insert(r);
            p.v_to_ec.at(v_r).erase(r);
            if (p.v_to_ec.at(v_r).empty())
            {
                GA::close_vertex(p.graph, v_r);
                p.v_to_ec.erase(v_r);
            }
        }
    }
}

void MixedDomain::remove_io_from_p(P& p)
{
    for (const auto& var : {IN, OUT})
    {
        auto v = get_vertex_of(var, p.v_to_ec);
        p.v_to_ec.at(v).erase(var);
        if (p.v_to_ec.at(v).empty())
        {
            GA::close_vertex(p.graph, v);
            p.v_to_ec.erase(v);
        }
    }
}

string_hset
MixedDomain::pick_all_r(const P& p_io)
{
    auto result = hset<string>();
    auto out_v = get_vertex_of(OUT, p_io.v_to_ec);
    auto out_ec = p_io.v_to_ec.at(out_v);
    for (const auto& v : out_ec)
        if (is_sys_reg_name(v))
            result.insert(v);
    return result;
}

