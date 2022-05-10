/**
 * Inspired by https://github.com/childsish/graph by Liam Childs (liam.h.childs@gmail.com)
 * but overhauled to fit my project.
 */
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <iostream>  // for cerr in assertions


namespace graph
{

class Graph
{
public:
    typedef uint V;
    Graph() = default;
    Graph(const std::initializer_list<std::pair<V,V>>& edges)
    {
        for (const auto& [src, dst] : edges)
            add_edge(src, dst);
    }

    Graph(const std::initializer_list<V>& vertices)
    {
        for (const auto& v : vertices)
            add_vertex(v);
    }

    /**
     * Add a connection between from src and to dst.
     * Automatically adds mentioned vertices.
     */
    void add_edge(const V& src, const V& dst)
    {
        children_by_v[src].insert(dst);
        parents_by_v[src];  // create if doesn't exist

        parents_by_v[dst].insert(src);
        children_by_v[dst];  // create if doesn't exist
    }

    /** Fails if either:
     *  - src or dst does not exist, or
     *  - src -> dst  does not exist.
     *  (note: doesn't remove any vertices) */
    void remove_edge(const V& src, const V& dst)
    {
        auto result = children_by_v.at(src).erase(dst) == 1 and
                      parents_by_v.at(dst).erase(src) == 1;

        if (not result)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the edge does not exist: " << src << " -> " << dst << std::endl;
            abort();
        }
    }

    /** Add a vertex (fails, if the vertex is already present). */
    void add_vertex(const V& vertex)
    {
        auto [it1, result1] = children_by_v.insert({vertex, {}});
        auto [it2, result2] = parents_by_v.insert({vertex, {}});

        if (not result1 or not result2)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the vertex already exists: " << vertex << std::endl;
            abort();
        }
    }

    /** Remove the vertex and all its edges (fails if the vertex doesn't exist). */
    void remove_vertex(const V& v)
    {
        // unregister v from the parents of v_children and from the children of parents_v

        for (auto p_v : parents_by_v.at(v))
        {
            if (p_v == v) continue;  // no need to modify the parents/children of the v itself
            children_by_v.at(p_v).erase(v);
        }

        for (auto v_c : children_by_v.at(v))
        {
            if (v_c == v) continue;
            parents_by_v.at(v_c).erase(v);
        }

        parents_by_v.erase(v);
        children_by_v.erase(v);
    }

    const std::unordered_set<V>& get_children(const V& vertex) const
    {
        return children_by_v.at(vertex);
    }

    const std::unordered_set<V>& get_parents(const V& vertex) const
    {
        return parents_by_v.at(vertex);
    }

    std::unordered_set<V> get_vertices() const
    {
        std::unordered_set<V> vertices;
        for (const auto& e : children_by_v)  // could as well iterate parentes_by_v instead
            vertices.insert(e.first);
        return vertices;
    }


    /** Note: these only compare verbatim (no 'graph isomorphism'). */
    bool operator==(const Graph& rhs) const { return children_by_v == rhs.children_by_v && parents_by_v == rhs.parents_by_v; }
    bool operator!=(const Graph& rhs) const { return !(rhs == *this); }

    friend std::ostream& operator<<(std::ostream&, const Graph&);

    friend struct GraphAlgo;


private:
    std::unordered_map<V, std::unordered_set<V>> parents_by_v;
    std::unordered_map<V, std::unordered_set<V>> children_by_v;
};

}
