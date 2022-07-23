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
            if (!is_atm_tst(ap.ap_name()))
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

vector<P>
compute_all_p_io(const P& p_io)
{
    vector<P> result;
    for (const auto& [new_g,mapper] : GA::all_topo_sorts2(p_io.graph))  // Note: since only IN and OUT are loose, and p_io is complete for Ratm, the complexity is O(n^2)
    {
        hmap<V,EC> new_v_to_ec;
        for (auto&& [new_v,set_of_old_v] : mapper)
            for (auto&& old_v: set_of_old_v)
                for (auto&& r: p_io.v_to_ec.at(old_v))
                    new_v_to_ec[new_v].insert(r);
        result.emplace_back(new_g, new_v_to_ec);
    }
    return result;
}

/** Restore the previously removed purely-system vertices into the graph. */
P
enhance_with_sys(const P& atm_p_io, const P& atm_sys_p)
{
    auto new_g = atm_p_io.graph;
    auto new_v_to_ec = atm_p_io.v_to_ec;

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
    // note: this function does not introduce loops, assuming atm_sys_p has no loops
    return {new_g, new_v_to_ec};
}

bool is_total(const P& p)
{
    return GA::all_topo_sorts(p.graph).size() == 1;
}

vector<P>
MixedDomain::all_possible_atm_tst(const P& atm_sys_p,
                                  const hset<TstAtom>& atm_tst_atoms)
{
    cout << atm_sys_p << endl;
    auto p = extract_atm_p(atm_sys_p);
    cout << p << endl;
    MASSERT(is_total(p), "must be total");   // (remove if slows down)
    auto p_io = add_io_info(p, atm_tst_atoms);
    if (!p_io.has_value())
        return {};

    vector<P> result;
    for (const auto& p_io2 : compute_all_p_io(p_io.value()))
        result.push_back(enhance_with_sys(p_io2, atm_sys_p));

    return result;
}


/// O(n^2) worst case
optional<P> refine_if_possible(const P& p,
                               const TstAtom& inp_sys_tst_atom)
{
    auto reg = inp_sys_tst_atom.t1 != IN ? inp_sys_tst_atom.t1 : inp_sys_tst_atom.t2;
    auto v_IN = get_vertex_of(IN, p.v_to_ec);
    auto v_reg = get_vertex_of(reg, p.v_to_ec);

    auto reg_reaches_IN = GA::walk_descendants(p.graph, v_reg, [&v_IN] (const V& v) { return v == v_IN; });
    auto IN_reaches_reg = GA::walk_ancestors(p.graph, v_reg, [&v_IN] (const V& v) { return v == v_IN; });

    // tst atom is t1{<,=,≠}t2
    if (inp_sys_tst_atom.relation == TstAtom::equal)  // IN=reg
    {
        if (v_IN == v_reg)
            return p;

        if (reg_reaches_IN ||
            IN_reaches_reg ||
            p.graph.get_distinct(v_reg).count(v_IN))
            return {};

        auto new_v_to_ec = p.v_to_ec;
        auto new_g = p.graph;

        GA::merge_v1_into_v2(new_g, v_IN, v_reg);
        new_v_to_ec.at(v_reg).insert(new_v_to_ec.at(v_IN).begin(), new_v_to_ec.at(v_IN).end());
        new_v_to_ec.erase(v_IN);

        return P(new_g, new_v_to_ec);
    }
    else if (inp_sys_tst_atom.relation == TstAtom::less && inp_sys_tst_atom.t1 == IN)  // IN<reg
    {
        if (v_reg == v_IN || IN_reaches_reg)
            return {};
        auto new_g = p.graph;
        new_g.add_dir_edge(v_reg, v_IN);
        return P(new_g, p.v_to_ec);
    }
    else if (inp_sys_tst_atom.relation == TstAtom::less && inp_sys_tst_atom.t2 == IN)  // reg<IN
    {
        if (v_IN == v_reg || reg_reaches_IN)
            return {};
        auto new_g = p.graph;
        new_g.add_dir_edge(v_IN, v_reg);
        return P(new_g, p.v_to_ec);
    }
    else if (inp_sys_tst_atom.relation == TstAtom::nequal)  // IN≠reg
    {
        if (v_IN == v_reg)
            return {};
        auto new_g = p.graph;
        new_g.add_neq_edge(v_IN, v_reg);
        return P(new_g, p.v_to_ec);
    }
    else
        MASSERT(0, "");
}

vector<pair<P, hset<TstAtom>>>
MixedDomain::all_possible_sys_tst(const P& p_io,
                                  const hmap<string, DomainName>& sys_tst_descr)
{
    /// Complexity: high: O(n^2n)
    auto result = vector<pair<P,hset<TstAtom>>>();
    auto descr_list = vector(sys_tst_descr.begin(), sys_tst_descr.end());  // fix some order

    // ------------------- a recursive function helper -------------------
    function<void(uint, const P&, hset<TstAtom>&)>
    rec = [&result, &descr_list, &rec]
    (uint cur_idx, const P& p, hset<TstAtom>& cur_sys_tst)
    {
        /// Without the recursive call, this function is n^2.
        /// The recursion depth is n.
        /// Hence O(n^2n). (Can probably be reduced to O(n^n), maybe even to O(const^n)?)
        if (cur_idx == descr_list.size())
        {
            result.emplace_back(p, cur_sys_tst);
            return;
        }

        const auto& [s,rel] = descr_list.at(cur_idx);

        vector<TstAtom> all_tst_atoms;
        if (rel == DomainName::order)
            all_tst_atoms = {TstAtom(s, TstAtom::less, IN),
                             TstAtom(IN, TstAtom::less, s),
                             TstAtom(IN, TstAtom::equal, s)};
        else if (rel == DomainName::equality)
            all_tst_atoms = {TstAtom(s, TstAtom::equal, IN),
                             TstAtom(s, TstAtom::nequal, IN)};
        else UNREACHABLE();

        for (const auto& tst_atom : all_tst_atoms)
        {
            auto refined_p = refine_if_possible(p, tst_atom);  // O(n^2)
            if (!refined_p.has_value())
                continue;
            cur_sys_tst.insert(tst_atom);
            rec(cur_idx + 1, refined_p.value(), cur_sys_tst);
            cur_sys_tst.erase(tst_atom);
        }
    };
    // -------------------------------------------------------------------

    auto cur_sys_tst = hset<TstAtom>();
    rec(0, p_io, cur_sys_tst);
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

bool MixedDomain::out_is_implementable(const P& partition)
{
    MASSERT(0, "not implemented");
}

