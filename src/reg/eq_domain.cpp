#include "reg/eq_domain.hpp"
#include "reg/atm_parsing_naming.hpp"
//#include "../utils.hpp"
#include "graph_algo.hpp"


using namespace sdf;
using namespace std;
using namespace spot;
using namespace graph;

#define hset unordered_set

using P = EqPartition;


P EqDomain::build_init_partition(const string_hset& sysR, const string_hset& atmR)
{
    return EqPartition(Graph({1}), {{1, a_union_b(sysR, atmR)}}, {1});
}

optional<P> EqDomain::compute_partial_p_io(const P& p,
                                           const hset<spot::formula>& atm_tst_atoms)
{
    // copy
    auto v_to_ec = p.v_to_ec;
    auto g = p.g;
    auto mut_excl = p.mutexl_vertices;

    add_vertex(g, v_to_ec, IN);   // IN and OUT shall not be present in g
    add_vertex(g, v_to_ec, OUT);  //

    for (const auto& tst : atm_tst_atoms)
    {
        auto[t1, t2, op] = parse_tst(tst.ap_name());
        MASSERT(!(is_reg_name(t1) && is_reg_name(t2)), "not supported");
        MASSERT(op == "=" || op == "≠", "");

        auto v1 = get_vertex_of(t1, g, v_to_ec);
        auto v2 = get_vertex_of(t2, g, v_to_ec);

        if (op == "=")
        {
            if (v1 == v2)
                continue;

            if (mut_excl.count(v1) && mut_excl.count(v2))  // v1 v2 are mut exclusive
                return {};

            if (g.get_children(v1).count(v2) || g.get_children(v2).count(v1))  // v1->v2 or v2->v1
                return {};

            // merge
            auto[v_src, v_dst] = mut_excl.count(v1) ? pair(v2,v1) : pair(v1,v2);  // pick v_src v_dst in the order that avoids unnecessary modification of mutexcl_vertices
            GraphAlgo::merge_v1_into_v2(g, v_src, v_dst);
            auto ec_vSrc = v_to_ec.at(v_src);
            v_to_ec.at(v_dst).insert(ec_vSrc.begin(), ec_vSrc.end());
            v_to_ec.erase(v_src);
        }
        else // "≠"
        {
            if (v1 == v2)
                return {};  // x and y belong to the same EC, but tst says x != y

            if (mut_excl.count(v1) and mut_excl.count(v2))
                continue;  // no need to add an edge between mutexcl vertices

            g.add_edge(v1,v2);
        }
    }

    MASSERT(mut_excl == p.mutexl_vertices, "(minor) invariant");

    return EqPartition(g, v_to_ec, mut_excl);
}

hset<V> get_neighbours(const Graph& g, const V& v)
{
    return a_union_b(g.get_children(v),
                     g.get_parents(v));
}

// returns: new graph and new v_to_ec
pair<Graph, VtoEC> merge_c1_into_c2(const Graph& g, const VtoEC& v_to_ec, const V& src, const V& dst)
{
    auto new_v_to_ec = v_to_ec;
    auto new_g = g;
    GraphAlgo::merge_v1_into_v2(new_g, src, dst);
    new_v_to_ec.at(dst).insert(new_v_to_ec.at(src).begin(),
                               new_v_to_ec.at(src).end());
    new_v_to_ec.erase(src);

    return {new_g, new_v_to_ec};
}

vector<P> EqDomain::compute_all_p_io(const P& partial_p_io)
{
    /*
    We assume there are at most two vertices outside mut_excl.

  - If there are no vertices outside mut_excl,
    then everything is already fixed, and partial_p_io is complete.

  - If there is one or two nonfixed vertices, we consider the cases:
    v = take any from nonfixed
    first: put v into its own class (by adding v to mutexcl), and recursive call the procedure with this new partition
    second: enumerate c in mutexcl - v.neighbours:
        merge v into class c
        recursively call the procedure with this new partition

    This algorithm works for non_fixed having the size at most 2,
    but wasn't checked for |non_fixed| > 2, but looks like it should work there as well.
    */

    vector<EqPartition> result;

    std::function<void(const EqPartition&)>
    rec = [&result,&rec](const EqPartition& p)
    {
        // check the assumption
        auto all_v = keys(p.v_to_ec);
        auto non_fixed = a_minus_b(all_v, p.mutexl_vertices);
        MASSERT(non_fixed.size() <= 2, "the algorithm assumption is violated");

        if (non_fixed.empty())    // case 0: nothing to vary, p is complete
        {
            auto new_g = Graph(p.g.get_vertices());  // we create a new graph with no edges (whereas p.g might have edges since rec() doesn't clean them)
            result.emplace_back(new_g, p.v_to_ec, p.mutexl_vertices);
            return;
        }

        // Cases 1 and 2
        auto v = non_fixed.front();

        // first, put v into its own equivalence class
        rec(EqPartition(p.g,  // this graph may have edges like "v -> some m from mut_excl" which are not needed anymore, but that is OK
                        p.v_to_ec,
                        a_union_b(p.mutexl_vertices, {v})));

        // second, merge with all possible classes
        auto merge_candidates = a_minus_b(p.mutexl_vertices, get_neighbours(p.g, v));
        for (const auto& mc: merge_candidates)
        {
            auto[new_g, new_v_to_ec] = merge_c1_into_c2(p.g, p.v_to_ec, v, mc);
            // NB: new_g may have unnecessary edges, since we merge v with mutexcl, but that is handled in the recursion leaf
            rec(EqPartition(new_g, new_v_to_ec, p.mutexl_vertices));
        }
    };

    rec(partial_p_io);
    return result;
}

EqPartition EqDomain::update(const EqPartition& p, const Asgn& asgn)
{
    auto g = p.g;
    auto v_to_ec = p.v_to_ec;
    auto mutexcl = p.mutexl_vertices;

    for (const auto& [var,registers] : asgn.asgn)
    {
        auto v_dst = get_vertex_of(var, g, v_to_ec);
        for (const auto& r : registers)
        {
            auto v_src = get_vertex_of(r, g, v_to_ec);
            if (v_src == v_dst)
                continue;
            v_to_ec.at(v_dst).insert(r);
            v_to_ec.at(v_src).erase(r);
            if (v_to_ec.at(v_src).empty())
            {
                v_to_ec.erase(v_src);
                g.remove_vertex(v_src);
                mutexcl.erase(v_src);
            }
        }
    }

    return EqPartition(g, v_to_ec, mutexcl);
}

EqPartition EqDomain::remove_io_from_p(const EqPartition& p)
{
    auto g = p.g;
    auto mutexcl = p.mutexl_vertices;
    auto v_to_ec = p.v_to_ec;

    for (const auto& var : {IN, OUT})
    {
        auto v = get_vertex_of(var, g, v_to_ec);
        v_to_ec.at(v).erase(var);
        if (v_to_ec.at(v).empty())
        {
            v_to_ec.erase(v);
            g.remove_vertex(v);
            mutexcl.erase(v);
        }
    }

    return EqPartition(g, v_to_ec, mutexcl);
}

bool EqDomain::out_is_implementable(const EqPartition& p)
{
    auto out_v = get_vertex_of(OUT, p.g, p.v_to_ec);
    auto out_ec = p.v_to_ec.at(out_v);
    for (const auto& v : out_ec)
        if (is_sys_reg_name(v) || is_input_name(v))
            return true;
    return false;
}

string_hset EqDomain::pick_R(const EqPartition& p_io)
{
    hset<string> result;
    auto out_v = get_vertex_of(OUT, p_io.g, p_io.v_to_ec);
    auto out_ec = p_io.v_to_ec.at(out_v);
    for (const auto& v : out_ec)
        if (is_sys_reg_name(v))
            result.insert(v);
    return result;
}

spot::formula EqDomain::extract_sys_tst_from_p(const EqPartition& p)
{
    MASSERT(p.mutexl_vertices.size() == p.v_to_ec.size(), "this function assumes the partition is complete");

    auto result = formula::tt();
    for (auto v : p.mutexl_vertices)
    {
        auto is_IN = p.v_to_ec.at(v).count(IN)>0;
        for (const auto& r: p.v_to_ec.at(v))
            if (is_sys_reg_name(r))
                result = formula::And({result,
                                       is_IN ?
                                       ctor_sys_tst_inp_equal_r(r)
                                       :
                                       formula::Not(ctor_sys_tst_inp_equal_r(r))});
    }

    return result;
}

set<spot::formula> EqDomain::construct_sysTstAP(const hset<string>& sysR)
{
    set<formula> sysTst;
    for (const auto& r : sysR)
        sysTst.insert(ctor_sys_tst_inp_equal_r(r));
    return sysTst;
}

size_t EqDomain::hash(const P& p) { return p.hash_; }

bool EqDomain::total_equal(const P& total_p1, const P& total_p2) { return total_p1.equal_to(total_p2); }






















