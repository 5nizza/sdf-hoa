#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>

#include "reg/special_graph.hpp"
#include "reg/reg_utils.hpp"


using namespace sdf;


namespace graph
{

struct GraphAlgo
{
    using V = SpecialGraph::V;

    /**
     Use parameter `insert` to save a descendant into your favorite container.
     Note: uses BFS, hence elements are inserted in the order of the distance from `vertex`.
    */
    static void get_descendants(const SpecialGraph& g, const V& vertex, const std::function<void(const V&)>& insert);

    /**
     Use parameter `insert` to save a descendant into your favorite container.
     Note: uses BFS, hence elements are inserted in the order of the distance from `vertex`.
    */
    static void get_ancestors(const SpecialGraph& g, const V& vertex, const std::function<void(const V&)>& insert);

    /**
     (For directed edges only.)
     @return: mapping v -> set_of_vertices_reachable_through_non_immediate_edges
     Thus, if a->b and there is no non-straight path a ---> b, then `a` will not have `b` in its reachable set.
     The set of *all* reachable nodes from `a` can be computed as this_set(a) ∪ children(a).
     Note: if v has no indirectly reachable vertices, then v is absent from the returned map.
     Complexity: O(V^3)
    */
    static
    std::unordered_map<V,std::unordered_set<V>>
    get_reach(const SpecialGraph& g);

    /**
     Note: vertex v1 is removed.
     Examples:
     Self-loops: merge(1,2):
       {1->2} or {1<-2} become {2->2}
       {1->1, 2} becomes {2->2}
     Main case: merge (2,4):
       1->2->3, 4->1
       becomes
       1 <-> 4 -> 3
    */
    static void merge_v1_into_v2(SpecialGraph& g, const V& v1, const V& v2);

    /** Complexity: O(V^3), maybe less (I didn't analyse) */
    static bool has_dir_cycles(const SpecialGraph& g);

    /**
     The standard topological sorting (the number of vertices equals that of g, and they respect the original constraints g).
     Note: in the result, every graph is a minimal linear ordering of the vertices (=> no reduntant edges).
     Complexity: exponential in the worst case (unavoidable).
    */
    static std::vector<SpecialGraph>
    all_topo_sorts(const SpecialGraph& g);

    /**
     This version allows for simultaneous placement, thus the number of vertices may decrease (while still respecting the constraints g).
     Note: we do not merge vertices a,b if a-b (connected via neq edge)
     Note: as before, in the result, every graph is a minimal linear ordering of the vertices (thus has no reduntant edges).
     Note: a vertex in a result graph maps to a set of vertices of the original `g` (via the returned mapping).
     Complexity: exponential in the worst case (unavoidable).
    */
    static std::vector<std::pair<SpecialGraph, std::unordered_map<V,std::unordered_set<V>>>>
    all_topo_sorts2(const SpecialGraph& g);

    /** Remove the vertex and directly connect vertices that were connected through this vertex (for directed edges only; neq edges simply disappear). */
    static void close_vertex(SpecialGraph& g, const V& v);

    /**
     Get a root wrt. directed edges.
     Fails if graph has none or more than one roots (a root is a vertex without predecessors)
    */
    static std::optional<V> get_root(const SpecialGraph& g)
    {
        std::optional<V> root;
        for (auto v: g.get_vertices())
        {
            if (g.get_parents(v).empty())
            {
                if (root.has_value())
                    return {};  // fail
                else
                    root = v;
            }
        }
        return root;
    }

};

}
