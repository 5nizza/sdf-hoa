#include "partition_tst_asgn.hpp"

#include <algorithm>
#include <list>
#include "utils.hpp"


using namespace std;
using namespace sdf;

#define hset unordered_set
#define hmap unordered_map


namespace sdf
{

/** Fails if graph has none or more than one roots (a root is a vertex without predecessors). */
optional<V> get_root(const Graph& g)
{
    optional<V> root;
    for (auto v: g.get_vertices())
    {
        if (g.get_parents(v).empty())
        {
            if (root.has_value())
                return {};  // fail
            else
                root = v;
        }
    }
    return root;
}


/** Check that g has shape v1->v2->...->vEnd.
    NB: syntactic check, i.e., it returns false if the graph succession is a line
    yet syntactically it looks different.
*/
bool is_line(const Graph& g)
{
    auto v_ = get_root(g);
    if (!v_.has_value())
        return false;

    auto v = v_.value();
    auto nof_nodes = g.get_vertices().size();
    for (uint i = 0; i < nof_nodes - 1; ++i, v = *g.get_children(v).begin())
        if (g.get_children(v).size() != 1)
            return false;
    return true;
}


std::ostream& operator<<(ostream& out, const Partition& p)
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

        auto v = get_root(p.g).value();
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


size_t FullPartitionHelper::calc_hash(const Partition& p)
{
    // we simply ignore the graph structure
    return xor_hash(p.v_to_ec,
                    [](const pair<V, EC>& v_ec) { return xor_hash(v_ec.second, std::hash<string>()); });
}


bool FullPartitionHelper::equal(const Partition& p1, const Partition& p2)
{
    // TODO: merge with the loop below (bcz it is expensive)
    MASSERT(is_line(p1.g), "assumption 'graph is line' is violated: " << p1.g);
    MASSERT(is_line(p2.g), "assumption 'graph is line' is violated: " << p2.g);

    if (p1.g.get_vertices().size() != p2.g.get_vertices().size())
        return false;

    if (calc_hash(p1) != calc_hash(p2))
        return false;

    auto v1 = get_root(p1.g).value();
    auto v2 = get_root(p2.g).value();

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


}







































