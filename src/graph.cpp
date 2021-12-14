#include "graph.hpp"
#include "utils.hpp"


using namespace graph;
using namespace sdf;
using namespace std;


size_t Graph::calc_hash() const
{
    return xor_hash(get_vertices(),
                    [&](const V& v) {return hash<V>()(v) ^ xor_hash(get_children(v), hash<V>());});
    // hash_of_the_vertex ^ hash_of_the_children_container
}


namespace graph
{
std::ostream& operator<<(ostream& out, const Graph& g)
{
    auto convert_v_to_str = [&g](Graph::V v)
    {
        stringstream ss;
        ss << v;
        if (g.get_children(v).empty())
            return ss.str();
        ss << " -> ";
        if (g.get_children(v).size()>1)
            ss << "{";
        ss << join(", ", g.get_children(v));
        if (g.get_children(v).size()>1)
            ss << "}";
        return ss.str();
    };

    out << "{";
    out << join(", ", g.get_vertices(), convert_v_to_str);
    out << "}";

    return out;
}
}
