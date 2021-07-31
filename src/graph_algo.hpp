#pragma once

#include <stack>
#include <unordered_map>
#include <unordered_set>
#include "graph.hpp"

namespace graph
{

struct GraphAlgo
{
    using V = Graph::V;

    static std::unordered_set<V> get_descendants(const Graph& g, const V& vertex);

    static std::unordered_set<V> get_ancestors(const Graph& g, const V& vertex);

    /** Join graph with another graph. */
    static void union_(Graph& g, const Graph& that);

    /** Note: vertex v1 is removed. */
    static void merge_v1_into_v2(Graph& g, const V& v1, const V& v2);

    static bool has_cycles(const Graph& g);

};

}
