#include "ord_partition.hpp"

#include <algorithm>
#include <list>

#include "reg_utils.hpp"
#include "graph_algo.hpp"


using namespace std;
using namespace sdf;

using GA = graph::GraphAlgo;

#define hset unordered_set
#define hmap unordered_map


/** Check that g has shape v1->v2->...->vEnd.
    NB: syntactic check, i.e., it returns false if the graph succession is a line
    yet syntactically it looks different.
*/
bool is_line(const Graph& g)
{
    auto v_ = GA::get_root(g);
    if (!v_.has_value())
        return false;

    auto v = v_.value();
    auto nof_nodes = g.get_vertices().size();
    for (uint i = 0; i < nof_nodes - 1; ++i, v = *g.get_children(v).begin())
        if (g.get_children(v).size() != 1)
            return false;
    return true;
}

ostream& sdf::operator<<(ostream& out, const OrdPartition& p)
{
    auto convert_ec_to_str = [](const EC& ec)
    {
        if (ec.size() > 1)
            return "{" + join(",", ec) + "}";
        else
            return join("", *ec.begin());  // hacky way to just convert to str
    };

    if (is_line(p.g))
    {
        stringstream ss;

        auto v = GA::get_root(p.g).value();
        auto nof_nodes = p.g.get_vertices().size();
        for (uint i = 0; i < nof_nodes - 1; ++i, v = *p.g.get_children(v).begin())
            ss << convert_ec_to_str(p.v_to_ec.at(v)) << " > ";
        ss << convert_ec_to_str(p.v_to_ec.at(v));
        return out << ss.str();
    }

    auto convert_v_to_str = [&p,&convert_ec_to_str](V v)
    {
        stringstream ss;
        ss << convert_ec_to_str(p.v_to_ec.at(v));
        if (p.g.get_children(v).empty())
            return ss.str();
        ss << " -> ";
        ss << "{ ";
        ss << join(", ", p.g.get_children(v),
                   [&p, &convert_ec_to_str](auto v) { return convert_ec_to_str(p.v_to_ec.at(v)); });
        ss << " }";
        return ss.str();
    };

    out << "{ ";
    out << join(", ", p.g.get_vertices(), convert_v_to_str);
    out << " }";

    return out;
}

size_t OrdPartition::calc_hash_() const
{
    // traverse the graph and computes the hash value as:
    // SUM over: hash(depth+1) * XOR([hash(e) for e in ec(v)])
    // we use depth+1 because hash(0)=0

    function<void(V,size_t,size_t&)>
            rec_visit = [&](V v, size_t depth, size_t& result)
    {
        result += std::hash<size_t>()(depth+1) * xor_hash(this->v_to_ec.at(v), std::hash<string>());
        for (const auto& c : this->g.get_children(v))
            rec_visit(c, depth+1, result);
    };

    auto root = GA::get_root(g);
    if (!root.has_value())  // such graphs correspond to invalid partitions (with cycles)
        return 0;

    size_t hash_value = 0;
    rec_visit(root.value(), 0, hash_value);
    return hash_value;
}


bool TotalOrdPartitionHasher::equal(const OrdPartition& p1, const OrdPartition& p2)
{
    // NOTE: assumes that both graphs are lines; but the check is omitted as it is expensive.

    if (p1.g.get_vertices().size() != p2.g.get_vertices().size())
        return false;

    if (p1.hash_ != p2.hash_)
        return false;

    auto v1 = GA::get_root(p1.g).value();
    auto v2 = GA::get_root(p2.g).value();

    while (true)
    {
        if (p1.v_to_ec.at(v1) != p2.v_to_ec.at(v2))
            return false;
        if (p1.g.get_children(v1).empty())    // finished all iterations
            return true;
        v1 = *p1.g.get_children(v1).begin();
        v2 = *p2.g.get_children(v2).begin();  // exists by the assumption of being a line
    }

    MASSERT(0, "unreachable");
}







































