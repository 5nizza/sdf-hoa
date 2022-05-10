#include "eq_partition.hpp"
#include "../utils.hpp"
#include "my_assert.hpp"
#include "reg/types.hpp"


using namespace sdf;
using namespace std;

#define hset unordered_set

ostream& sdf::operator<<(ostream& out, const EqPartition& p)
{
    auto v_to_str = [&p](const uint& v)
    {
        auto vars = p.v_to_ec.at(v);
        auto s = join(",", vars);
        return vars.size()>1 ? "{" + s + "}" : s;
    };
    out << "P:" << endl;
    out << "  mut exclusive: " << join(", ", p.mutexl_vertices, v_to_str) << endl;
    for (const auto& v : p.g.get_vertices())
    {
        if (p.mutexl_vertices.count(v) > 0)
            continue;
        out << "  ";
        out << v_to_str(v) << " diff from ";
        auto neighbours = a_union_b(p.g.get_children(v), p.g.get_parents(v));
        if (neighbours.empty())
            out << "<no constraints>";
        else
            out << join(", ", neighbours, v_to_str);
        out << endl;  // TODO: one too many endl
    }
    return out;
}

size_t
EqPartition::calc_hash() const
{
    // since we assume the partition is complete, we only care about mutexcl_vertices:
    // return
    // SUM over [xor_hash(ec(v)) for v in mutexcl_vertices]
    size_t result = 0;
    for (const auto& v : mutexl_vertices)
        result += xor_hash(v_to_ec.at(v), std::hash<string>());
    return result;

    // the original version (that works for incomplete partitions as well)
    /*
    // 17 * SUM[xor_hash(ec(v)) for v in mutexcl_vertices]
    // +
    // SUM[spec_xor_hash(v) for v in other_vertices],
    // where spec_xor_hash(v) is:
    //   xor_hash(ec(v)) if v has no neighbours, else
    //   xor_hash(ec(v)) ^ xor_hash(ec(n1)) ^ xor_hash(ec(n2)) ^ ... (for all neighbours of v)

    size_t result = 0;

    for (const auto& v : mutexl_vertices)
        result += xor_hash(v_to_ec.at(v), std::hash<string>());

    result = result*17;

    for (const auto& [v,ec] : v_to_ec)
    {
        if (mutexl_vertices.count(v))
            continue;
        auto v_hash = xor_hash(ec, std::hash<string>());
        auto neighbours = a_union_b(g.get_parents(v), g.get_children(v));
        if (!neighbours.empty())
        {
            size_t neighbours_hash = 0;
            for (const auto& n: neighbours)
                neighbours_hash ^= xor_hash(v_to_ec.at(n), std::hash<string>());
            v_hash ^= neighbours_hash;
        }
        result += v_hash;
    }

    return result;
    */
}

bool
EqPartition::equal_to(const EqPartition& other) const
{
    // we assume that the partition is complete
    MASSERT(mutexl_vertices.size() == v_to_ec.size() and
            other.mutexl_vertices.size() == other.v_to_ec.size(),
            "invalid use case: should not be called");

    if (mutexl_vertices.size() != other.mutexl_vertices.size())
        return false;

    if (hash_ != other.hash_)
        return false;

    auto ec_hasher = [](const EC& ec) { return xor_hash(ec, std::hash<string>()); };
    auto ec_equal_to = [](const hset<string>& ec1, const hset<string>& ec2) { return ec1 == ec2; };

    using FullPartitionSet = hset<hset<string>, decltype(ec_hasher), decltype(ec_equal_to)>;

    auto full_partition1 = FullPartitionSet(0, ec_hasher, ec_equal_to);
    for (const auto& [v,ec] : v_to_ec)
        full_partition1.insert(ec);

    auto full_partition2 = FullPartitionSet(0, ec_hasher, ec_equal_to);
    for (const auto& [v,ec] : other.v_to_ec)
        full_partition2.insert(ec);

    return full_partition1 == full_partition2;
}


