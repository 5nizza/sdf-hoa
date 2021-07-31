#include "graph.hpp"
#include "graph_algo.hpp"


using namespace graph;
using namespace std;

using V = Graph::V;

#define hset unordered_set


void
graph::GraphAlgo::merge_v1_into_v2(Graph& g, const V& v1, const V& v2)
{
    // (also works for v1 self-loop)
    if (v1 == v2)
        return;

    auto replace_v1_by_v2 = [&](hset<V>& container){container.erase(v1); container.insert(v2);};

    for (auto p_v1 : g._edges_by_v.at(v1).parents)
    {
        replace_v1_by_v2(g._edges_by_v.at(p_v1).children);
        if (p_v1 != v1)
            g._edges_by_v.at(v2).parents.insert(p_v1);
    }

    for (auto v1_c : g._edges_by_v.at(v1).children)
    {
        replace_v1_by_v2(g._edges_by_v.at(v1_c).parents);
        if (v1_c != v1)
            g._edges_by_v.at(v2).children.insert(v1_c);
    }

    g._edges_by_v.erase(v1);
}

bool
graph::GraphAlgo::has_cycles(const Graph& g)
{
    // this O(V^2) will do:
    for (auto v: g.get_vertices())
    {
        auto v_children = get_descendants(g,v);
        if (v_children.find(v) != v_children.end())
            return true;
    }
    return false;
}

hset<V> GraphAlgo::get_descendants(const Graph& g, const V& vertex)
{
    hset<V> descendents;
    hset<V> visited;
    stack<V> stack;
    for (auto child : g._edges_by_v.at(vertex).children)
        stack.push(child);

    while (!stack.empty())
    {
        auto top = stack.top();
        stack.pop();

        if (visited.find(top) != visited.end())
            continue;

        visited.insert(top);
        descendents.insert(top);
        for (const auto& child : g._edges_by_v.at(top).children)
            stack.push(child);
    }
    return descendents;
}

void GraphAlgo::union_(Graph& g, const Graph& that)
{
    for (const auto& item : that._edges_by_v)
    {
        g.add_vertex(item.first);
        for (const auto& child : item.second.children)
            g.add_edge(item.first, child);
    }
}

hset<V>
GraphAlgo::get_ancestors(const Graph& g, const V& vertex)
{
    hset<V> ancestors;
    hset<V> visited;
    stack<V> stack;
    for (auto parent : g._edges_by_v.at(vertex).parents)
        stack.push(parent);

    while (!stack.empty())
    {
        auto top = stack.top();
        stack.pop();
        if (visited.find(top) != visited.end())
            continue;

        visited.insert(top);
        ancestors.insert(top);
        for (const auto& parent : g._edges_by_v.at(top).parents)
            stack.push(parent);
    }
    return ancestors;
}

