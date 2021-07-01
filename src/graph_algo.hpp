#pragma once

#include "graph.hpp"

namespace graph
{
    /** Note: vertex v1 is removed. */
    template<typename V>
    void merge_v1_into_v2(Graph<V>& g, const V& v1, const V& v2);

    template<typename V>
    bool has_cycles(const Graph<V>& g);
}
