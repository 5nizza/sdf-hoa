#pragma once

#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>

#include "graph.hpp"
#include "utils.hpp"

using namespace sdf;


namespace graph
{

struct GraphAlgo
{
    using V = Graph::V;

    // Use parameter `insert` to save a descendant into your favorite container.
    // Note: uses BFS, hence elements are inserted in the order of the distance from `vertex`.
    static void get_descendants(const Graph& g, const V& vertex, const std::function<void(const V&)>& insert);

    // Use parameter `insert` to save a descendant into your favorite container.
    // Note: uses BFS, hence elements are inserted in the order of the distance from `vertex`.
    static void get_ancestors(const Graph& g, const V& vertex, const std::function<void(const V&)>& insert);

    /** Note: vertex v1 is removed. */
    static void merge_v1_into_v2(Graph& g, const V& v1, const V& v2);

    static bool has_cycles(const Graph& g);

    // Note: in the result, every graph is a minimal linear ordering of the vertices (thus has no reduntant edges).
    static std::vector<Graph> all_topo_sorts(const Graph& g);

    // This version allows for simultaneous placement.
    // Note: in the result, every graph is a minimal linear ordering of the vertices (thus has no reduntant edges).
    // Note: a vertex in a result graph maps to a set of vertices of the original `g` (via the returned mapping).
    static std::vector<std::pair<Graph, std::unordered_map<V,std::unordered_set<V>>>>
    all_topo_sorts2(const Graph& g);

    // Remove the vertex and directly connect vertices that were connected through this vertex.
    static void close_vertex(Graph& g, const V& v);

};

}
