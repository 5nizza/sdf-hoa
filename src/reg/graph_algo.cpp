#include <functional>
#include <queue>
#include "reg/graph_algo.hpp"


using namespace graph;
using namespace std;

#define hset unordered_set
#define hmap unordered_map


void
graph::GraphAlgo::merge_v1_into_v2(SpecialGraph& g, const V& v1, const V& v2)
{
    // (also works for the self-loop v1<->v1 and for neq self-loop v1≠v1)
    if (v1 == v2)
        return;

    // 1. directed edges
    auto replace_v1_by_v2 = [&](hset<V>& container){container.erase(v1); container.insert(v2);};

    for (auto p_v1 : g.parents.at(v1))
    {
        replace_v1_by_v2(g.children.at(p_v1));
        if (p_v1 != v1)
            g.parents.at(v2).insert(p_v1);
    }

    for (auto v1_c : g.children.at(v1))
    {
        replace_v1_by_v2(g.parents.at(v1_c));
        if (v1_c != v1)
            g.children.at(v2).insert(v1_c);
    }

    g.children.erase(v1);
    g.parents.erase(v1);

    // 2. undirected (neq) edges
    for (auto d : g.distinct.at(v1))
    {
        replace_v1_by_v2(g.distinct.at(d));
        if (d != v1)
            g.distinct.at(v2).insert(d);

        if (d == v1)  // the case of self-loop
            g.distinct.at(v2).insert(v2);
    }
    g.distinct.erase(v1);
}

bool graph::GraphAlgo::has_dir_cycles(const SpecialGraph& g)
{
    // this simple will do: (O(V^3) and maybe even O(V^2)):
    for (const auto& v: g.get_vertices())
    {
        hset<V> v_children;
        walk_descendants(g, v, [&v_children](const V& v) { v_children.insert(v); return false; });
        if (v_children.find(v) != v_children.end())
            return true;
    }
    return false;
}

bool graph::GraphAlgo::has_neq_self_loops(const SpecialGraph& g)
{
    for (const auto& v : g.get_vertices())
        if (g.get_distinct(v).count(v))
            return true;

    return false;
}

/** min(None, value) = value */
inline
optional<int>
min(optional<int> a, optional<int> b)
{
    if (!a.has_value())
        return b;

    if (!b.has_value())
        return a;

    return a.value() < b.value() ? a:b;
}

/** None + value = None */
inline
optional<int>
add(optional<int> a, optional<int> b)
{
    if (!a.has_value() || !b.has_value())
        return {};
    return {a.value()+b.value()};
}

/**
 O(n^3) worst case
 @param n : the size of the graph (the vertices are 0...n-1)
 @param W : (modifiable) array of weigths; W[0][1] - weight of 0->1: if no such edge, then none; the answer will be saved into W
*/
void floyd_warshall(vector<vector<optional<int>>>& W)
{
    const auto n = W.size();
    auto Wprev = vector<vector<optional<int>>>();
    for (uint k = 0; k < n; ++k)
    {
        Wprev = W;
        for (uint i = 0; i < n; ++i)
            for (uint j = 0; j < n; ++j)
                W[i][j] = min(add(Wprev[i][k], Wprev[k][j]), Wprev[i][j]);
    }
}

hmap<V,hset<V>>
GraphAlgo::get_reach(const SpecialGraph& g)
{
    const auto vertices = g.get_vertices();
    const auto n = vertices.size();
    auto W = vector<vector<optional<int>>>(n, vector<optional<int>>(n));

    // prepare W structure for Floyd-Warshall algorithm
    // W[i][j] denotes the weight of i->j; it is None if no such edge exists.
    // i,j ∈ 0..n-1
    // 1. Mappers (our graph vertex) <-> (vertex in 0..n-1)
    auto v_to_i = hmap<V,uint>();
    auto i_to_v = hmap<V,uint>();
    for (const auto& v : vertices)
    {
        auto new_i = i_to_v.size();
        v_to_i.insert({v, new_i});
        i_to_v.insert({new_i, v});
    }

    // construct W:
    // the negative weight ensures that the Floyd-Warshall algorithm
    // will compute the paths of the maximal length (instead of minimal if use positive weights)
    for (const auto& v : vertices)
        for (const auto& c : g.get_children(v))
        {
            auto i = v_to_i.at(v),
                 j = v_to_i.at(c);
            W[i][j] = -1;  // (note: non-children will have value 'None')
        }

    floyd_warshall(W);

    auto reach_by_v = hmap<V,hset<V>>();
    for (auto i = 0u; i < n; ++i)
        for (auto j = 0u; j < n; ++j)
            if (W[i][j].has_value() and W[i][j].value() < -1)  // the longest path is more than one direct edge
            {
                auto v = i_to_v[i],
                     c = i_to_v[j];
                reach_by_v[v].insert(c);
            }
    return reach_by_v;
}

bool walk_trans_closure_dir(const SpecialGraph& g, const V& vertex,
                            const function<bool(const V&)>& stop_after_do,
                            const function<hset<V>(const V&)>& get_successors)
{
    // O(V^2) worst case (cliques) (doesn't matter if the hashtable lookup is O(1) or O(V))
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

        if (stop_after_do(elem))
            return true;

        for (const auto& succ : get_successors(elem))
            queue.push(succ);
    }

    return false;
}

bool GraphAlgo::walk_descendants(const SpecialGraph& g, const V& vertex, const function<bool(const V&)>& do_and_stop)
{
    return walk_trans_closure_dir(g, vertex, do_and_stop, [&](const V& v) { return g.children.at(v); });
}

bool GraphAlgo::walk_ancestors(const SpecialGraph& g, const V& vertex, const function<bool(const V&)>& do_and_stop)
{
    return walk_trans_closure_dir(g, vertex, do_and_stop, [&](const V& v) { return g.parents.at(v); });
}

SpecialGraph get_graph(const vector<V>& cur_path)
{
    SpecialGraph g;
    for (auto v_it = cur_path.begin() + 1; v_it != cur_path.end(); ++v_it)
        g.add_dir_edge(*(v_it - 1), *v_it);

    return g;
}

template<typename C>
bool contains_all(const hset<V>& c_hashed, const C& c)
{
    return all_of(c.begin(), c.end(), [&c_hashed](const auto& e){ return c_hashed.find(e) != c_hashed.end(); });
}

vector<SpecialGraph>
GraphAlgo::all_topo_sorts(const SpecialGraph& g)
{
    // For a faster algorithm, see Knuth Vol.4A p.343

    vector<SpecialGraph> result;
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

pair<SpecialGraph, hmap<V,hset<V>>>
get_graph2(const vector<hset<V>>& cur_path)
{
    // TODO: check
    SpecialGraph g;
    hmap<V,hset<V>> v_to_ec;
    for (auto v_it = cur_path.begin(); v_it+1 != cur_path.end(); ++v_it)
    {
        auto src = *min_element(v_it->begin(), v_it->end()); // in fact, they are all the same
        auto dst = *min_element((v_it+1)->begin(), (v_it+1)->end());
        g.add_dir_edge(src, dst);
        v_to_ec.insert({src, *v_it});
    }
    auto last = cur_path.end()-1;
    auto chosen_elem = *min_element(last->begin(), last->end());
    v_to_ec.insert({chosen_elem, *last});

    if (cur_path.size() == 1)
        g.add_vertex(chosen_elem);  // and no edges
    return {g, v_to_ec};
}

bool have_neq_neighbours(const vector<V>& vertices, const SpecialGraph& g)
{
    if (vertices.size() <= 1)
        return false;

    // O(n^2) assuming O(1) lookup in hash set
    for (uint i = 0; i < vertices.size()-1; ++i)
        for (uint j = i+1; j < vertices.size(); ++j)
            if (g.get_distinct(vertices[i]).count(vertices[j]))
                return true;

    return false;
}

vector<pair<SpecialGraph, hmap<V,hset<V>>>>
GraphAlgo::all_topo_sorts2(const SpecialGraph& g)
{
    vector<pair<SpecialGraph, hmap<V,hset<V>>>> result;
    vector<hset<V>> cur_path;
    hset<V> cur_path_h;  // optimization
    auto vertices = g.get_vertices();

    // ---------- recursive helper -------------
    function<void()> rec = [&]()
    {
        auto possible_successors = filter<V>(vertices,
                                             [&](const auto& v) {return (!cur_path_h.count(v) && contains_all(cur_path_h, g.get_parents(v)));});
        for (const auto& succ : all_subsets<V>(possible_successors, true))
        {
            if (have_neq_neighbours(succ, g))
                continue;

            cur_path.emplace_back(succ.begin(), succ.end()); cur_path_h.insert(succ.begin(), succ.end());

            if (cur_path_h.size() == vertices.size())
                result.push_back(get_graph2(cur_path));
            else
                rec();

            cur_path.pop_back(); for (const auto& e : succ) cur_path_h.erase(e);
        }
    };
    // ----------------------------------------

    rec();

    return result;
}

void GraphAlgo::close_vertex(SpecialGraph& g, const V& v)
{
    for (const auto& p : g.get_parents(v))
        for (const auto& c : g.get_children(v))
            g.add_dir_edge(p, c);
    g.remove_vertex(v);
}


















































