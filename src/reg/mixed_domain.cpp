#include "reg/mixed_domain.hpp"
#include "reg/atm_parsing_naming.hpp"

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


// convert "≥" into two edges "= or >", convert "≠" into two edges "< or >"
twa_graph_ptr
MixedDomain::preprocess(const twa_graph_ptr& reg_ucw)
{
    // TODO: check that it indeed makes sense
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
            if (cmp == ">" or cmp == "<" or cmp == "=")
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
            if (cmp == "≠")
            {
                ap1 = formula::ap(t1 + "<" + t2);  // NOLINT
                ap2 = formula::ap(t1 + ">" + t2);  // NOLINT
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

bool MixedDomain::compute_partial_p_io(P& p,
                                       const unordered_set<spot::formula>& atm_tst_atoms)
{
    return false;
}

vector<std::pair<P,formula>> MixedDomain::all_possible_atm_tst(const P& partial_p_io, const std::unordered_set<spot::formula>& atm_tst_atoms)
{
    return {};
}

vector<pair<P, spot::formula>> MixedDomain::all_possible_sys_tst(const P& p_io,
                                                                 const unordered_map<string, Relation>& sys_tst_descr)
{
    return {};
}

void MixedDomain::update(P& p, const Asgn& asgn)
{

}

void MixedDomain::remove_io_from_p(P& p)
{

}

string_hset MixedDomain::pick_all_r(const P& p_io)
{
    return {};
}

bool MixedDomain::out_is_implementable(const P& partition)
{
    return true;
}

std::set<spot::formula> MixedDomain::construct_sysTstAP(const string_hset& sysR)
{
    return {};
}

