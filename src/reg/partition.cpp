#include "reg/partition.hpp"
#include "reg/graph_algo.hpp"

#include "../utils.hpp"


using namespace std;
using namespace sdf;
using namespace graph;
using GA = GraphAlgo;


#define hset unordered_set
#define hmap unordered_map


namespace sdf
{
ostream& operator<<(ostream& out, const Partition& p)
{
    unordered_map<V, string> v_to_str;
    for (const auto& [v,ec] : p.v_to_ec)
        v_to_str[v] = "{" + join(",", ec) + "}";
    return (out << p.graph.to_str([&v_to_str](const V& v){return v_to_str.at(v);}));
}
}  // namespace sdf


Partition
PartitionCanonical::get_canonical(const Partition& p)
{
    /**
     - Part 1: for every two vertices i,j:
       remove i!=j  if  i (->y)* -> j,
       remove i->j  if  i -> x (->y)* -> j.

     - Part 2:
       Sort the vertices according to the ECs they represent:
       sort the ECs, then every EC is represented by vertex `order(EC)`.
    */

    /// Part 1
    auto g = p.graph;  // copy

    auto reach_by_v = GA::get_reach(g);

    for (const auto& i : g.get_vertices())
    {
        auto i_children = hset<V>(g.get_children(i));  // copy (can be optimized)
        auto i_neq = hset<V>(g.get_distinct(i));       // copy (can be optimized)

        for (const auto& j : i_children)
            if (reach_by_v[i].count(j))
                g.remove_dir_edge(i, j);

        for (const auto& j : i_neq)
            if (reach_by_v[i].count(j) || i_children.count(j))
                g.remove_neq_edge(i, j);
    }

    /// Part 2

    // define the renaming new_by_old

    vector<V> old_by_new;  // using vector as a map
    // extract vector<V> from VtoEC
    transform(p.v_to_ec.begin(), p.v_to_ec.end(), back_inserter(old_by_new),
              [](const pair<V,EC>& v_ec){return v_ec.first;});
    // sort ECs according to their minimal element: e.g., {a,b} < {c,d} because a<c
    // note that the same element cannot appear in different ECs => this is a well-defined order
    sort(old_by_new.begin(), old_by_new.end(),
         [&p](const V& v1, const V& v2)
         { return *min_element(p.v_to_ec.at(v1).begin(), p.v_to_ec.at(v1).end()) < *min_element(p.v_to_ec.at(v2).begin(), p.v_to_ec.at(v2).end()); });

    hmap<V,V> new_by_old;
    for (auto v_new = 0u; v_new < old_by_new.size(); ++v_new)
        new_by_old.insert({old_by_new[v_new], v_new});

    // create a new graph by renaming the vertices
    SpecialGraph new_g;
    for (const auto& v : g.get_vertices())
        new_g.add_vertex(new_by_old.at(v));

    for (const auto& v : g.get_vertices())
    {
        for (const auto& c : g.get_children(v))
            new_g.add_dir_edge(new_by_old.at(v),new_by_old.at(c));
        for (const auto& d : g.get_distinct(v))
            new_g.add_neq_edge(new_by_old.at(v),new_by_old.at(d));
    }

    // create a new v_to_ec (with renamed vertices)
    VtoEC new_v_to_ec;
    for (const auto& [v,ec] : p.v_to_ec)
        new_v_to_ec.insert({new_by_old.at(v), ec});

    return {new_g, new_v_to_ec};
}

size_t PartitionCanonical::calc_hash(const Partition& p)
{
    auto uint_hasher = std::hash<size_t>();
    auto str_hasher = std::hash<string>();
    size_t hash_value = 17;
    for (auto v = 0u; v < p.graph.get_vertices().size(); ++v)
        hash_value = 31*hash_value
                     + xor_hash(p.graph.get_children(v), uint_hasher)
                     + 31*xor_hash(p.graph.get_distinct(v), uint_hasher)
                     + xor_hash(p.v_to_ec.at(v), str_hasher);

    return hash_value;
}


































