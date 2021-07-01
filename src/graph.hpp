/**
 * Inspired by https://github.com/childsish/graph by Liam Childs (liam.h.childs@gmail.com)
 *
 * Modified to fit my project.
 */
#pragma once

#include <stack>
#include <unordered_map>
#include <unordered_set>

namespace graph
{
    /**
     * An undirected graph that supports only a single edge between vertices.
     * Can be treated as ordered by using the get_parents and get_children
     * functions. Can be treated as unordered by using the get_neighbours function.
     */
    template<typename V>
    class Graph {
    public:

        /**
         * Add a connection between from and to.
         * add_edge automatically creates any missing vertices.
         * @param from connect from
         * @param to connect to
         */
        void add_edge(const V &from, const V &to)
        {
            add_vertex(from);
            add_vertex(to);
            _edges[from].children.insert(to);
            _edges[to].parents.insert(from);
        }

        /**
         * Add a vertex.
         * add_vertex can be used if a vertex has no connections.
         * @param vertex added vertex
         */
        void add_vertex(const V &vertex)
        {
            if (_edges.find(vertex) == _edges.end())
                _edges.emplace(vertex, VData{});
        }

        /**
         * Get children of given vertex.
         * @param vertex get the children of this vertex
         * @return children of the given vertex
         */
        std::unordered_set<V> get_children(const V &vertex) const
        {
            std::unordered_set<V> children;
            if (_edges.find(vertex) == _edges.end())
                return children;

            return _edges.at(vertex).children;
        }

        /**
         * Get parents of given vertex.
         * @param vertex get the parents of this vertex
         * @return parents of the given vertex
         */
        std::unordered_set<V> get_parents(const V &vertex) const
        {
            std::unordered_set<V> parents;
            if (_edges.find(vertex) == _edges.end())
                return parents;

            return _edges.at(vertex).parents;
        }

        /**
         * Get descendants of given vertex.
         * @param vertex get the descendants of the given vertex
         * @return descendants of the given vertex
         */
        std::unordered_set<V> get_descendants(const V &vertex) const
        {
            std::unordered_set<V> descendents;
            if (_edges.find(vertex) == _edges.end())
                return descendents;

            std::unordered_set<V> visited;
            std::stack<V> stack;
            visited.insert(vertex);
            for (auto child : _edges.at(vertex).children)
                stack.push(child);

            while (stack.size() > 0)
            {
                auto top = stack.top();
                stack.pop();

                if (visited.find(top) != visited.end())
                    continue;

                visited.insert(top);
                descendents.insert(top);
                for (const auto& child : _edges.at(top).children)
                    stack.push(child);
            }
            return descendents;
        }

        /**
         * Get ancestors of given vertex.
         * @param vertex get the ancestors of the given vertex
         * @return ancestors of the given vertex
         */
        std::unordered_set<V> get_ancestors(const V &vertex) const
        {
            std::unordered_set<V> ancestors;
            if (_edges.find(vertex) == _edges.end())
                return ancestors;

            std::unordered_set<V> visited;
            std::stack<V> stack;
            visited.insert(vertex);
            for (auto parent : _edges.at(vertex).parents)
                stack.push(parent);

            while (stack.size() > 0)
            {
                auto top = stack.top();
                stack.pop();
                if (visited.find(top) != visited.end())
                    continue;

                visited.insert(top);
                ancestors.insert(top);
                for (const auto& parent : _edges.at(top).parents)
                    stack.push(parent);
            }
            return ancestors;
        }

        std::unordered_set<V> get_vertices() const
        {
            std::unordered_set<V> vertices;
            for (const auto& e : _edges)
                vertices.insert(e.first);
            return vertices;
        }

        /**
         * Join graph with another graph.
         * *union* is a keyword, so an underscore has been added.
         * @param that graph to join with
         */
        void union_(const Graph &that)
        {
            for (const auto& item : that._edges)
            {
                add_vertex(item.first);
                for (const auto& child : item.second.children)
                    add_edge(item.first, child);
            }
        }

    private:
        struct VData
        {
            std::unordered_set<V> parents;
            std::unordered_set<V> children;
        };

        std::unordered_map<V, VData> _edges;
    };
}
