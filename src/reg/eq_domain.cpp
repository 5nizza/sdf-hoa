#include "reg/eq_domain.hpp"
#include "reg/atm_parsing_naming.hpp"
//#include "../utils.hpp"
#include "graph_algo.hpp"


using namespace sdf;
using namespace std;
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
    // Copy
    auto v_to_ec = p.v_to_ec;
    auto g = p.g;
    auto mut_excl = p.mutexl_vertices;

    add_vertex(g, v_to_ec, IN);   // IN and OUT shall not be present in g
    add_vertex(g, v_to_ec, OUT);  //

    for (const auto& tst : atm_tst_atoms)
    {
        auto [t1, t2, op] = parse_tst(tst.ap_name());
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
            auto [v_src, v_dst] = mut_excl.count(v1) ? pair(v2,v1) : pair(v1,v2);  // to avoid modifying mutexcl_vertices
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

vector<P> EqDomain::compute_all_p_io(const P& partial_p_io)
{
    // This algorithm works for any number of input/output variables.
    // NOTE:
    // the original version could produce certain partitions twice,
    // so I had to resort to using hash sets, kinda lame.
    // Here is an example where the original version could produce a partition {i,o,r} twice,
    // because there are two ways to get it:
    // 1. put i in into {r}, giving {i,r}, then put o into {i,r}: {i,o,r}
    // 2. put i into {o}, giving {i,o}, then put {i,o} into {r}: {i,o,r}

    // TODO: not finished

    vector<EqPartition> result;
    function<void(const EqPartition&)>
    rec_call =
    [&rec_call,&result](const EqPartition& cur_partition)
    {
        auto all_v = keys(cur_partition.v_to_ec);
        auto non_fixed = a_minus_b(all_v, cur_partition.mutexl_vertices);
        if (non_fixed.empty())
        {
            result.push_back(cur_partition);  // (internally, it copies cur_partition)
            return;
        }

        auto v = non_fixed.front();  // pick arbitrary one

        auto v_neq = a_union_b(cur_partition.g.get_children(v),
                               cur_partition.g.get_parents(v));
        auto merge_candidates = a_minus_b(all_v, v_neq);
        MASSERT(!merge_candidates.empty(), "not possible as it at least contains itself");
        // Candidates to merge v with.
        // Candidates contains v itself: it means "a class separate from all others".
        for (const auto& c : merge_candidates)
        {
            if (v != c)
            {
                auto new_v_to_ec = cur_partition.v_to_ec;
                auto new_g = cur_partition.g;
                GraphAlgo::merge_v1_into_v2(new_g, v, c);
                new_v_to_ec.at(c).insert(new_v_to_ec.at(v).begin(), new_v_to_ec.at(v).end());
                new_v_to_ec.erase(v);
                rec_call(EqPartition(new_g, new_v_to_ec, cur_partition.mutexl_vertices));
            }
            else  // v itself: put it into a separate equivalence class
            {
                auto new_mut_excl = cur_partition.mutexl_vertices;
                new_mut_excl.insert(v);

                auto new_g = cur_partition.g;
                // add edges from v to V \ mutexcl (thus also minus v)
                for (const auto& v_other : a_minus_b(all_v, new_mut_excl))
                    new_g.add_edge(v, v_other);
                // remove unnecessary edges between v and mutexcl vertices
                for (const auto& m : new_mut_excl)
                {
                    if (new_g.get_children(v).count(m))  // v->m
                        new_g.remove_edge(v,m);
                    if (new_g.get_children(m).count(v))  // m->v
                        new_g.remove_edge(m,v);
                }

                rec_call(EqPartition(new_g, cur_partition.v_to_ec, new_mut_excl));
            }
        }
    };
    rec_call(partial_p_io);
    return result;
}

P EqDomain::update(const P& p, const Asgn& asgn)
{
}

P EqDomain::remove_io_from_p(const P& p)
{
}

string_hset EqDomain::pick_R(const P& p_io, const string_hset& sysR)
{
}

bool EqDomain::out_is_implementable(const P& partition)
{
}

void EqDomain::introduce_sysActionAPs(const string_hset& sysR, spot::twa_graph_ptr classical_ucw,
                                      set<spot::formula>& sysTst, set<spot::formula>& sysAsgn,
                                      set<spot::formula>& sysOutR)
{

}

spot::formula EqDomain::extract_sys_tst_from_p(const P& p, const string_hset& sysR)
{
}

size_t EqDomain::hash(const P& p) { return p.hash_; }

bool EqDomain::total_equal(const P& total_p1, const P& total_p2) { return total_p1.equal_to(total_p2); }
