/**
 * Inspired by https://github.com/childsish/graph by Liam Childs (liam.h.childs@gmail.com)
 * but overhauled to fit my project.
 */
#pragma once

#include <stack>
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

    /**
     * Add a connection between from src and to dst.
     * Automatically create any missing vertices.
     */
    void add_edge(const V& src, const V& dst)
    {
        // (no need to call `add_vertex` -- the operator [] adds them)
        _edges_by_v[src].children.insert(dst);
        _edges_by_v[dst].parents.insert(src);
    }

    /** Fails if either:
     *  - src or dst does not exist, or
     *  - src -> dst  does not exist.
     *  (note: doesn't remove any vertices) */
    void remove_edge(const V& src, const V& dst)
    {
        auto result = _edges_by_v.at(src).children.erase(dst) == 1 and
                      _edges_by_v.at(dst).parents.erase(src) == 1;

        if (not result)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the edge does not exist: " << src << " -> " << dst << std::endl;
            abort();
        }
    }

    /** Add a vertex (fails, if the vertex is already present). */
    void add_vertex(const V& vertex)
    {
        auto [it, result] = _edges_by_v.emplace(vertex, VData{});
        if (not result)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the vertex already exists: " << vertex << std::endl;
            abort();
        }
    }

    /** Remove the vertex and all its edges (fails if the vertex doesn't exist). */
    void remove_vertex(const V& v)
    {
        // unregister v from the parents of v_children and from the children of parents_v

        for (auto p_v : _edges_by_v.at(v).parents)
        {
            if (p_v == v) continue;  // no need to modify the parents/children of the v itself
            _edges_by_v.at(p_v).children.erase(v);
        }

        for (auto v_c : _edges_by_v.at(v).children)
        {
            if (v_c == v) continue;
            _edges_by_v.at(v_c).parents.erase(v);
        }

        _edges_by_v.erase(v);
    }

    std::unordered_set<V> get_children(const V& vertex) const
    {
        std::unordered_set<V> children;
        if (_edges_by_v.find(vertex) == _edges_by_v.end())
            return children;

        return _edges_by_v.at(vertex).children;
    }

    std::unordered_set<V> get_parents(const V& vertex) const
    {
        std::unordered_set<V> parents;
        if (_edges_by_v.find(vertex) == _edges_by_v.end())
            return parents;

        return _edges_by_v.at(vertex).parents;
    }

    std::unordered_set<V> get_vertices() const
    {
        std::unordered_set<V> vertices;
        for (const auto& e : _edges_by_v)
            vertices.insert(e.first);
        return vertices;
    }

    /** Note: these only compare verbatim (no 'graph isomorphism'). */
    bool operator==(const Graph& rhs) const { return _edges_by_v == rhs._edges_by_v; }
    bool operator!=(const Graph& rhs) const { return !(rhs == *this); }

    friend struct GraphAlgo;

private:
    struct VData
    {
        std::unordered_set<V> parents;
        std::unordered_set<V> children;

        bool operator==(const VData& rhs) const { return parents == rhs.parents && children == rhs.children; }
        bool operator!=(const VData& rhs) const { return !(rhs == *this); }
    };

    std::unordered_map<V, VData> _edges_by_v;

};

}
