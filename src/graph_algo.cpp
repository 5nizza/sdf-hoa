#include <functional>
#include <queue>
#include "graph.hpp"
#include "graph_algo.hpp"


using namespace graph;
using namespace std;

using V = Graph::V;

#define hset unordered_set
#define hmap unordered_map


void
graph::GraphAlgo::merge_v1_into_v2(Graph& g, const V& v1, const V& v2)
{
    // (also works for v1 self-loop)
    if (v1 == v2)
        return;

    auto replace_v1_by_v2 = [&](hset<V>& container){container.erase(v1); container.insert(v2);};

    for (auto p_v1 : g.parents_by_v.at(v1))
    {
        replace_v1_by_v2(g.children_by_v.at(p_v1));
        if (p_v1 != v1)
            g.parents_by_v.at(v2).insert(p_v1);
    }

    for (auto v1_c : g.children_by_v.at(v1))
    {
        replace_v1_by_v2(g.parents_by_v.at(v1_c));
        if (v1_c != v1)
            g.children_by_v.at(v2).insert(v1_c);
    }

    g.children_by_v.erase(v1);
    g.parents_by_v.erase(v1);
}

bool graph::GraphAlgo::has_cycles(const Graph& g)
{
    // this simple will do: (O(V^3) and maybe even O(V^2)):
    for (auto v: g.get_vertices())
    {
        hset<V> v_children;
        get_descendants(g,v, [&v_children](const V& v){v_children.insert(v);});
        if (v_children.find(v) != v_children.end())
            return true;
    }
    return false;
}


void get_trans_closure(const Graph& g, const V& vertex,
                       const function<void(const V&)>& insert,
                       const function<hset<V>(const V&)>& get_successors)
{
    // O(V^2) worse case (for cliques)
    hset<V> visited;
    queue<V> queue;
    for (auto succ : get_successors(vertex))
        queue.push(succ);

    while (!queue.empty())
    {
        auto elem = queue.front();
        queue.pop();

        if (visited.find(elem) != visited.end())
            continue;

        visited.insert(elem);
        insert(elem);
        for (const auto& succ : get_successors(elem))
            queue.push(succ);
    }
}

void GraphAlgo::get_descendants(const Graph& g, const V& vertex, const function<void(const V&)>& insert)
{
    get_trans_closure(g, vertex, insert, [&](const V& v) { return g.children_by_v.at(v); });
}

void GraphAlgo::get_ancestors(const Graph& g, const V& vertex, const function<void(const V&)>& insert)
{
    get_trans_closure(g, vertex, insert, [&](const V& v) { return g.parents_by_v.at(v); });
}

Graph get_graph(const vector<V>& cur_path)
{
    Graph g;
    for (auto v_it = cur_path.begin() + 1; v_it != cur_path.end(); ++v_it)
        g.add_edge(*(v_it - 1), *v_it);
    return g;
}

template<typename C>
bool contains_all(const hset<V>& c_hashed, const C& c)
{
    return all_of(c.begin(), c.end(), [&c_hashed](const auto& e){ return c_hashed.find(e) != c_hashed.end(); });
}

vector<Graph>
GraphAlgo::all_topo_sorts(const Graph& g)
{
    // For a faster algorithm, see Knuth Vol.4A p.343

    vector<Graph> result;
    vector<V> cur_path;
    hset<V> cur_path_h;  // optimization
    function<void()> rec = [&]()
    {
        for (const auto& v : g.get_vertices())
        {
            if (!cur_path_h.count(v) and contains_all(cur_path_h, g.get_parents(v)))
            {
                cur_path.push_back(v); cur_path_h.insert(v);

                if (cur_path.size() == g.get_vertices().size())
                    result.push_back(get_graph(cur_path));
                else
                    rec();

                cur_path.pop_back(); cur_path_h.erase(v);
            }
        }
    };

    rec();

    return result;
}

pair<Graph, hmap<V,hset<V>>> get_graph2(const vector<hset<V>>& cur_path)
{
    Graph g;
    hmap<V,hset<V>> v_to_ec;
    for (auto v_it = cur_path.begin(); v_it+1 != cur_path.end(); ++v_it)
    {
        auto src = *min_element(v_it->begin(), v_it->end()); // in fact, they are all the same
        auto dst = *min_element((v_it+1)->begin(), (v_it+1)->end());
        g.add_edge(src, dst);
        v_to_ec.insert({src, *v_it});
    }
    auto last = cur_path.end()-1;
    auto chosen_elem = *min_element(last->begin(), last->end());
    v_to_ec.insert({chosen_elem, *last});
    if (cur_path.size() == 1)
        g.add_vertex(chosen_elem);  // and no edges
    return {g, v_to_ec};
}

vector<pair<Graph, hmap<V,hset<V>>>> GraphAlgo::all_topo_sorts2(const Graph& g)
{
    vector<pair<Graph, hmap<V,hset<V>>>> result;
    vector<hset<V>> cur_path;
    hset<V> cur_path_h;  // optimization
    auto vertices = g.get_vertices();
    function<void()> rec = [&]()
    {
        auto possible_successors = filter<V>(vertices,
                                             [&](const auto& v) {return (!cur_path_h.count(v) and contains_all(cur_path_h, g.get_parents(v)));});
        for (const auto& succ : all_subsets<V>(possible_successors, true))
        {
            cur_path.emplace_back(succ.begin(), succ.end()); cur_path_h.insert(succ.begin(), succ.end());

            if (cur_path_h.size() == vertices.size())
                result.push_back(get_graph2(cur_path));
            else
                rec();

            cur_path.pop_back(); for (const auto& e : succ) cur_path_h.erase(e);
        }
    };

    rec();

    return result;
}

void GraphAlgo::close_vertex(Graph& g, const V& v)
{
    for (const auto& p : g.get_parents(v))
        for (const auto& c : g.get_children(v))
            g.add_edge(p, c);
    g.remove_vertex(v);
}


















































