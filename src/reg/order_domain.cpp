#include "order_domain.hpp"
#include "atm_parsing_naming.hpp"

#include "graph.hpp"
#include "graph_algo.hpp"

#define BDD spotBDD
    #include "spot/tl/formula.hh"
    #include "spot/twa/twagraph.hh"
    #include "spot/twa/formula2bdd.hh"
#undef BDD


using namespace std;
using namespace sdf;
using namespace spot;

#define hmap unordered_map
#define hset unordered_set

using GA = graph::GraphAlgo;


// convert "≥" into two edges "= or >", convert "≠" into two edges "< or >"
// note: the test "True" is handled later
twa_graph_ptr
OrderDomain::preprocess(const twa_graph_ptr& reg_ucw)
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

OrdPartition
OrderDomain::build_init_partition(const hset<string>& sysR,
                                  const hset<string>& atmR)
{
    Graph g;
    g.add_vertex(0);
    hmap<V, EC> v_to_ec;
    v_to_ec.insert({0, a_union_b(sysR, atmR)});

    return OrdPartition(g, v_to_ec);
}


optional<OrdPartition>
OrderDomain::compute_partial_p_io(const OrdPartition& p,
                                  const hset<formula>& atm_tst_atoms)
{
    // copy
    Graph g = p.g;
    VtoEC v_to_ec = p.v_to_ec;

    add_vertex(g, v_to_ec, IN);
    add_vertex(g, v_to_ec, OUT);

    for (const auto& tst_atom : atm_tst_atoms)
    {
        auto [t1, t2, cmp] = parse_tst(tst_atom.ap_name());
        MASSERT(cmp != "≥" && cmp != "≤" && cmp != "≠", "should be handled before");
        MASSERT(! (is_reg_name(t1) && is_reg_name(t2)), "not supported");

        auto v1 = get_vertex_of(t1, g, v_to_ec),
             v2 = get_vertex_of(t2, g, v_to_ec);

        if (cmp == "=")
        {
            if (v1 == v2)
                continue;

            GA::merge_v1_into_v2(g, v1, v2);
            auto v1_ec = v_to_ec.at(v1);
            v_to_ec.at(v2).insert(v1_ec.begin(), v1_ec.end());
            v_to_ec.erase(v1);
        }
        else if (cmp == ">")  // t1 > t2
            g.add_edge(v1, v2);
        else if (cmp == "<")
            g.add_edge(v2, v1);
        else
            MASSERT(0, "unreachable: " << cmp);
    }

    if (GA::has_cycles(g))
        return {};

    return OrdPartition(g, v_to_ec);
}


vector<OrdPartition>
OrderDomain::compute_all_p_io(const OrdPartition& partial_p_io)
{
    vector<OrdPartition> result;
    for (auto&& [new_g,mapper] : GA::all_topo_sorts2(partial_p_io.g))
    {
//        auto is_implementable = true;
        hmap<V,EC> new_v_to_ec;
        for (auto&& [new_v,set_of_old_v] : mapper)
        {
            for (auto&& old_v: set_of_old_v)
                for (auto&& r: partial_p_io.v_to_ec.at(old_v))
                    new_v_to_ec[new_v].insert(r);

//            for (const auto& e : new_v_to_ec[new_v])
//                if (is_output_name(e))
//                {
//                    is_implementable = any_of(new_v_to_ec[new_v].begin(), new_v_to_ec[new_v].end(),
//                                              [](auto&& other)
//                                              {
//                                                  return is_input_name(other) || is_sys_reg_name(other);
//                                              });
//                    if (!is_implementable)
//                        break;
//                }
//
//            if (!is_implementable)
//                break;
        }

//        if (!is_implementable)
//            continue;

        result.emplace_back(new_g, new_v_to_ec);
    }
    return result;
}


OrdPartition
OrderDomain::update(const OrdPartition& p, const Asgn& asgn)
{
    auto new_g = p.g;  // copy
    auto new_v_to_ec = p.v_to_ec;

    for (const auto& [io, regs] : asgn.asgn)
    {
        auto v_io = get_vertex_of(io, new_g, new_v_to_ec);
        for (const auto& r : regs)
        {
            auto v_r = get_vertex_of(r, new_g, new_v_to_ec);
            if (v_r == v_io)
                continue;  // store the same value; has no effect on partition

            new_v_to_ec.at(v_io).insert(r);

            new_v_to_ec.at(v_r).erase(r);
            if (new_v_to_ec.at(v_r).empty())
            {
                GA::close_vertex(new_g, v_r);
                new_v_to_ec.erase(v_r);
            }
        }
    }

    return OrdPartition(new_g, new_v_to_ec);
}


OrdPartition
OrderDomain::remove_io_from_p(const OrdPartition& p)
{
    auto new_v_to_ec = p.v_to_ec;
    auto new_g = p.g;

    for (const auto& var : {IN, OUT})
    {
        auto v = get_vertex_of(var, new_g, new_v_to_ec);
        new_v_to_ec.at(v).erase(var);
        if (new_v_to_ec.at(v).empty())
        {
            GA::close_vertex(new_g, v);
            new_v_to_ec.erase(v);
        }
    }
    return OrdPartition(new_g, new_v_to_ec);
}


/*
  From a given partition (which includes positions of i and o),
  compute system registers compatible with o.
  ASSUMPTION: there is only one output `o`.
  ASSUMPTION: p_io is complete
  NB: the partition is assumed to be complete, yet there might still be several different possible r:
      when they all reside in the same EC.
*/
hset<string>
OrderDomain::pick_R(const OrdPartition& p_io)
{
    hset<string> result;
    auto out_v = get_vertex_of(OUT, p_io.g, p_io.v_to_ec);
    auto out_ec = p_io.v_to_ec.at(out_v);
    for (const auto& v : out_ec)
        if (is_sys_reg_name(v))
            result.insert(v);
    return result;
}


// Extract the full system test from a given partition.
formula
OrderDomain::extract_sys_tst_from_p(const OrdPartition& p)
{
    auto v_IN = get_vertex_of(IN, p.g, p.v_to_ec);
    auto ec_IN = p.v_to_ec.at(v_IN);

    auto result = formula::tt();

    // components for *=r
    {
        for (const auto& r: ec_IN)
            if (is_sys_reg_name(r))
                result = formula::And({result, ctor_sys_tst_inp_equal_r(r)});
    }

    // getting system registers below *
    {
        hset<V> descendants;
        GA::get_descendants(p.g, v_IN, [&descendants](const V& v) { descendants.insert(v); });
        for (const auto& v: descendants)
            for (const auto& r: p.v_to_ec.at(v))
                // r<* is encoded as !(in=r) & !(in<r)
                if (is_sys_reg_name(r))
                    result = formula::And({result,
                                           formula::Not(ctor_sys_tst_inp_equal_r(r)),
                                           formula::Not(ctor_sys_tst_inp_smaller_r(r))
                                          });
    }

    // getting system registers above *
    {
        hset<V> ancestors;
        GA::get_ancestors(p.g, v_IN, [&ancestors](const V& v) { ancestors.insert(v); });
        for (const auto& v: ancestors)
            for (const auto& r: p.v_to_ec.at(v))
                // r>* is encoded as !(in=r) & (in<r)
                if (is_sys_reg_name(r))
                    result = formula::And({result,
                                           formula::Not(ctor_sys_tst_inp_equal_r(r)),
                                           ctor_sys_tst_inp_smaller_r(r)
                                          });
    }

    return result;
}


/**
The encoding is:

- system tests:
  for each register r, we introduce two Boolean variables, in<r and in=r.
  - the atm construction must ensure there are no transitions labeled in<r & in=r,
  - all other combinations are possible:
       in<r & !(in=r) encodes "*<r",
    !(in<r) & !(in=r) encodes "*>r",
    !(in<r) & in=r    encodes "*=r".

- system assignments:
  for each register r, introduce a Boolean variable in↓r

- system outputs:
  for each register, introduce a Boolean variable ↑r, and
  ensure that having ↑r1 & ↑r2 leads to a rejecting sink.
*/
set<formula>
OrderDomain::construct_sysTstAP(const hset<string>& sysR)
{
    set<formula> sysTst;
    for (const auto& r : sysR)
    {
        sysTst.insert(ctor_sys_tst_inp_equal_r(r));
        sysTst.insert(ctor_sys_tst_inp_smaller_r(r));
    }
    return sysTst;
}


/* returns false iff o does not belong to a class of i or of some sys register */
bool
OrderDomain::out_is_implementable(const OrdPartition& p)
{
    auto out_v = get_vertex_of(OUT, p.g, p.v_to_ec);
    auto ec = p.v_to_ec.at(out_v);
    return any_of(ec.begin(), ec.end(), [](auto&& other) {return is_input_name(other) || is_sys_reg_name(other);});
}


size_t
OrderDomain::hash(const OrdPartition& p)
{
    return p.hash_;
}


bool
OrderDomain::total_equal(const OrdPartition& p1, const OrdPartition& p2)
{
    // NOTE: assumes that both graphs are lines; but the check is omitted as it is expensive.

    if (p1.g.get_vertices().size() != p2.g.get_vertices().size())
        return false;

    if (p1.hash_ != p2.hash_)
        return false;

    auto v1 = GA::get_root(p1.g).value();
    auto v2 = GA::get_root(p2.g).value();

    while (true)
    {
        if (p1.v_to_ec.at(v1) != p2.v_to_ec.at(v2))
            return false;
        if (p1.g.get_children(v1).empty())    // finished all iterations
            return true;
        v1 = *p1.g.get_children(v1).begin();
        v2 = *p2.g.get_children(v2).begin();  // exists by the assumption of being a line
    }

    MASSERT(0, "unreachable");
}
