#pragma once
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-pass-by-value"

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <ostream>

#include "reg/special_graph.hpp"

#include "my_assert.hpp"
#include "reg/types.hpp"


namespace sdf
{


struct Partition
{
    graph::SpecialGraph graph;  // a->b means v(a) > v(b), a-b means v(a) ≠ v(b)
    VtoEC v_to_ec;

    Partition(const graph::SpecialGraph& graph, const VtoEC& v_to_ec) : graph(graph), v_to_ec(v_to_ec) {}

    /** syntactic checks */
    bool operator==(const Partition& other) const
    {
        return graph == other.graph && v_to_ec == other.v_to_ec;
    }
    bool operator!=(const Partition& rhs) const { return !(rhs == *this); }

    friend std::ostream& operator<<(std::ostream&, const Partition&);
};


class PartitionCanonical
{
public:
    const Partition p;
    const size_t hash;

private:
    /** note: use only with canonical `p` */
    static size_t calc_hash(const Partition& p);

    static
    Partition get_canonical(const Partition& p);

public:
    /** The method creates a canonical partition from @param p (which is not canonical in general). */
    explicit
    PartitionCanonical(const Partition& p) : p(get_canonical(p)), hash(calc_hash(this->p)) { }

    /** syntactic checks */
    bool operator==(const PartitionCanonical& rhs) const
    {
        return hash == rhs.hash and p == rhs.p;
    }
    bool operator!=(const PartitionCanonical& rhs) const { return !(rhs == *this); }

    friend std::ostream& operator<<(std::ostream& out, const PartitionCanonical& ph) { return (out << ph.p); }
};


} // namespace sdf

#pragma clang diagnostic pop
