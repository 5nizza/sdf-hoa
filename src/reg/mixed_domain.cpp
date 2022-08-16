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
    // no relations ('true')
    SpecialGraph g;
    VtoEC v_to_ec;

    auto next_v = 0;
    for (const auto& r : a_union_b(sysR, atmR))
    {
        g.add_vertex(next_v);
        v_to_ec[next_v] = {r};
        ++next_v;
    }

    return {g,v_to_ec};
}

/**
 * O(n^2) worst case
 * NOTE: modifies p in case of success only; in case of failure, keeps it untouched.
 */
bool refine_if_possible(P& p, const TstAtom& tst_atom)
{
    auto v1 = get_vertex_of(tst_atom.t1, p.v_to_ec);
    auto v2 = get_vertex_of(tst_atom.t2, p.v_to_ec);

    auto v2_reaches_v1 = GA::walk_descendants(p.graph, v2, [&v1] (const V& v) { return v == v1; });
    auto v1_reaches_v2 = GA::walk_descendants(p.graph, v1, [&v2] (const V& v) { return v == v2; });

    // tst atom is t1{<,=,≠}t2
    if (tst_atom.relation == TstAtom::equal)  // t1=t2
    {
        if (v1 == v2)
            return true;

        if (v2_reaches_v1 ||
            v1_reaches_v2 ||
            p.graph.get_distinct(v2).count(v1))
            return false;

        GA::merge_v1_into_v2(p.graph, v1, v2);
        p.v_to_ec.at(v2).insert(p.v_to_ec.at(v1).begin(), p.v_to_ec.at(v1).end());
        p.v_to_ec.erase(v1);

        return true;
    }
    else if (tst_atom.relation == TstAtom::less)  // t1<t2
    {
        if (v2 == v1 || v1_reaches_v2)
            return false;
        p.graph.add_dir_edge(v2, v1);
        return true;
    }
    else if (tst_atom.relation == TstAtom::nequal)  // t1≠t2
    {
        if (v1 == v2)
            return false;
        p.graph.add_neq_edge(v1, v2);
        return true;
    }
    else
        UNREACHABLE();
}

bool
MixedDomain::refine(P& p, const hset<TstAtom>& tst_atoms)
{
    for (const auto& tst_atom : tst_atoms)
        if (!refine_if_possible(p, tst_atom))
            return false;
    return true;
}

/**
 * @param descr_list : vector of triples (var,var,domain), were var ∈ {IN,OUT} ∪ Rsys ∪ Ratm (we need ordered access => use vector)
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
        auto refined_p = p;
        if (!refine_if_possible(refined_p, tst_atom))  // O(n^2)
            continue;
        cur_tst.insert(tst_atom);
        rec_enumerate(descr_list, cur_idx + 1, refined_p, cur_tst, result);
        cur_tst.erase(tst_atom);
    }
}

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

void MixedDomain::add_io_to_p(P& p)
{
    add_vertex(p.graph, p.v_to_ec, IN);
    add_vertex(p.graph, p.v_to_ec, OUT);
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

