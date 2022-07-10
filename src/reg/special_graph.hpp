#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>  // for cerr in assertions
#include <functional>


namespace graph
{

class SpecialGraph
{
public:
    typedef uint V;
    SpecialGraph() = default;

    SpecialGraph(const std::initializer_list<std::pair<V,V>>& dir_edges,
                 const std::initializer_list<std::pair<V,V>>& neq_edges)
    {
        for (const auto& [src, dst] : dir_edges)
            add_dir_edge(src, dst);

        for (const auto& [v1, v2] : neq_edges)
            add_neq_edge(v1, v2);
    }

    SpecialGraph(const std::initializer_list<V>& vertices)
    {
        for (const auto& v : vertices)
            add_vertex(v);
    }

    explicit
    SpecialGraph(const std::unordered_set<V>& vertices)
    {
        for (const auto& v : vertices)
            add_vertex(v);
    }

    /** Automatically adds mentioned vertices */
    void add_dir_edge(const V& src, const V& dst)
    {
        children[src].insert(dst);
        parents[dst].insert(src);

        // create if doesn't exist
        children[dst];
        parents[src];
        distinct[src];
        distinct[dst];
    }

    /** Automatically adds mentioned vertices */
    void add_neq_edge(const V& a, const V& b)
    {
        distinct[a].insert(b);
        distinct[b].insert(a);

        // create if doesn't exist
        children[a];
        children[b];
        parents[a];
        parents[b];
    }

    /** Fails if either:
        - src or dst does not exist, or
        - src -> dst  does not exist. */
    void remove_dir_edge(const V& src, const V& dst)
    {
        auto result = children.at(src).erase(dst) == 1 and
                      parents.at(dst).erase(src) == 1;

        if (not result)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the edge does not exist: " << src << " -> " << dst << std::endl;
            abort();
        }
    }

    /** Fails if either:
        - src or dst does not exist, or
        - src -> dst  does not exist. */
    void remove_neq_edge(const V& a, const V& b)
    {
        auto result = distinct.at(a).erase(b) == 1 and
                      distinct.at(b).erase(a) == 1;

        if (not result)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the edge does not exist: " << a << " ≠ " << b << std::endl;
            abort();
        }
    }

    /** Add a vertex (fails, if the vertex is already present). */
    void add_vertex(const V& vertex)
    {
        auto [it1, result1] = children.insert({vertex, {}});
        auto [it2, result2] = parents.insert({vertex, {}});
        auto [it3, result3] = distinct.insert({vertex, {}});

        if (not result1 or not result2 or not result3)
        {
            std::cerr << __FILE__ << " (" << __LINE__ << ") : " << "the vertex already exists: " << vertex << std::endl;
            abort();
        }
    }

    /** Remove the vertex and all its edges (fails if the vertex doesn't exist). */
    void remove_vertex(const V& v)
    {
        // unregister v from the parents of v_children and from the children of parents_v

        for (auto p_v : parents.at(v))
        {
            if (p_v == v) continue;  // no need to modify the parents/children of the v itself
            children.at(p_v).erase(v);
        }

        for (auto v_c : children.at(v))
        {
            if (v_c == v) continue;
            parents.at(v_c).erase(v);
        }

        for (auto v_d : distinct.at(v))
        {
            if (v_d == v) continue;
            distinct.at(v_d).erase(v);
        }

        parents.erase(v);
        children.erase(v);
        distinct.erase(v);
    }

    const std::unordered_set<V>& get_children(const V& vertex) const
    {
        return children.at(vertex);
    }

    const std::unordered_set<V>& get_parents(const V& vertex) const
    {
        return parents.at(vertex);
    }

    const std::unordered_set<V>& get_distinct(const V& vertex) const
    {
        return distinct.at(vertex);
    }

    std::unordered_set<V> get_vertices() const
    {
        std::unordered_set<V> vertices;
        for (const auto& e : children)  // could as well iterate `parents` or `distinct` instead
            vertices.insert(e.first);
        return vertices;
    }

    /** Verbatim (syntactic) comparison. This is useful in testing. */
    bool operator==(const SpecialGraph& rhs) const { return children == rhs.children && parents == rhs.parents && distinct == rhs.distinct; }
    bool operator!=(const SpecialGraph& rhs) const { return !(rhs == *this); }

    std::string to_str(const std::function<std::string(const V&)>& v_to_str = [](const V& v) {return std::to_string(v);}) const;

    friend std::ostream& operator<<(std::ostream& out, const SpecialGraph& g) { return (out << g.to_str()); }

    friend struct GraphAlgo;


private:
    std::unordered_map<V, std::unordered_set<V>> parents;
    std::unordered_map<V, std::unordered_set<V>> children;
    std::unordered_map<V, std::unordered_set<V>> distinct;
};

}
