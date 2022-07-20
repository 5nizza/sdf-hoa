#pragma once
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-pass-by-value"


#include <string>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <ostream>
#include <list>

#include "my_assert.hpp"
#include "graph.hpp"
#include "reg_utils.hpp"


namespace sdf
{
using EC = std::unordered_set<std::string>;
using Graph = graph::Graph;
using V = Graph::V;


struct TotalOrdPartitionHasher;

/*
 * Partition is a graph, where v1 -> v2 means "v1>v2".
 * Each vertex describes an equivalence class, using v_to_ec mapping.
 */
class OrdPartition
{
public:
    const Graph g;
    const std::unordered_map<V, EC> v_to_ec;
    OrdPartition()=delete;  // with const above, it makes no sense

    explicit OrdPartition(const Graph& g, const std::unordered_map<V, EC>& v_to_ec): g(g), v_to_ec(v_to_ec), hash_(calc_hash_())
    {
        MASSERT(v_to_ec.size() == g.get_vertices().size(), "nof_vertices must be equal nof EC classes");
    }

    // if needed, use the explicit one from FullPartitionHelper
    bool operator==(const OrdPartition& rhs) const = delete;
    bool operator!=(const OrdPartition& rhs) const = delete;

    // NB: quite an expensive call
    friend std::ostream& operator<<(std::ostream& out, const OrdPartition& p);
    friend struct TotalOrdPartitionHasher;
    friend class OrderDomain;

private:
    const size_t hash_;
    size_t calc_hash_() const;  // NB: returns 0 for partitions with cycles (which are invalid)
};


struct TotalOrdPartitionHasher
{
    static size_t hash(const OrdPartition& p) {return p.hash_;}
    // assumes the graph is a line: node1 -> node2 -> ...
    static bool equal(const OrdPartition& p1, const OrdPartition& p2);
};


std::ostream& operator<<(std::ostream& out, const OrdPartition& p);  // to avoid warnings when defining sdf::operator<<(...)

}


#pragma clang diagnostic pop
