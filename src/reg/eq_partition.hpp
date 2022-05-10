#pragma once

#include <string>
#include <unordered_set>
#include <ostream>

#include "graph.hpp"
#include "my_assert.hpp"


namespace sdf
{
class OrderDomain;

class EqPartition
{
private:
    using StringSet = std::unordered_set<std::string>;
    using VtoEC = std::unordered_map<uint, StringSet>;

public:
    /*
      Representation of a partition:
      - vertex holds an equiv class
      - an edge between v1 and v2 means ec(v1) and ec(v2) hold different values
      - mutexcl_vertices hold equiv classes holding mut-excl data values.
        As an optimisation, we do _not_ put an edge between v1-v2 for v1,v2 in mutexcl_vertices,
        and assume it is always present.
      - Clarification: For every v in V \ mutexcl_vertices:
        an edge v-v1, where v1 in V, means ec(v) and ec(v1) hold different data values.

      Note: a partition is complete iff all vertices are mutexcl_vertices. (The graph has no edges then.)
    */
    const graph::Graph g;
    const VtoEC v_to_ec;
    const std::unordered_set<uint> mutexl_vertices;  // these vertices are known to hold distinct data values
    // const graph::Graph::V inp_v;  // vertex with EC containing the input variable
    // const graph::Graph::V out_v;  // vertex with EC containing the output variable

    explicit EqPartition(const graph::Graph& g,
                         const VtoEC& v_to_ec,
                         const std::unordered_set<uint>& mutexl_vertices):
                         g(g),
                         v_to_ec(v_to_ec),
                         mutexl_vertices(mutexl_vertices),
                         hash_(calc_hash())
    {
        MASSERT(g.get_vertices().size() == v_to_ec.size(), "inconsistent");
    }

    EqPartition()=delete;  // with const members, it makes no sense
    // if needed, use the explicit ones
    bool operator==(const EqPartition& rhs) const = delete;
    bool operator!=(const EqPartition& rhs) const = delete;

    friend std::ostream& operator<<(std::ostream& out, const EqPartition& p);

private:
    const std::size_t hash_;

    std::size_t calc_hash() const;                  // NB: it only returns decent hash for complete partitions
    bool equal_to(const EqPartition& other) const;  // NB: it is correct only for complete partitions

    friend class EqDomain;  // give access for hash calculation
};


std::ostream& operator<<(std::ostream& out, const EqPartition& p);  // to avoid warnings when defining sdf::operator<<(...)

} //namespace sdf