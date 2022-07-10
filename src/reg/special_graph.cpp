#include "reg/special_graph.hpp"
#include "reg/reg_utils.hpp"


using namespace graph;
using namespace sdf;
using namespace std;


string SpecialGraph::to_str(const function<string(const V&)>& v_to_str) const
{
    stringstream out;
    out << "graph [ ";
    out << "vertices: " << join(",", get_vertices(), v_to_str);

    out << ", edges: ";
    stringstream ss;
    for (auto v: get_vertices())
    {
        if (get_children(v).empty())
            continue;
        if (!ss.str().empty())
            ss << ", ";
        ss << join(", ", get_children(v), [&v, &v_to_str](auto c) { return v_to_str(v) + "->" + v_to_str(c); });
    }
    if (ss.str().empty())
        out << "none";
    else
        out << ss.str();

    out << ", neq_edges: ";
    ss = stringstream();
    auto vertices_todo = get_vertices();
    while (!vertices_todo.empty())
    {
        auto v = pop_first<V>(vertices_todo);
        if (get_distinct(v).empty())
            continue;
        if (!ss.str().empty())
            ss << ", ";
        ss << join(", ", get_distinct(v), [&v, &v_to_str](auto v2) { return v_to_str(v) + "≠" + v_to_str(v2); });
        for (auto v_processed: get_distinct(v))
            vertices_todo.erase(v_processed);
    }

    if (ss.str().empty())
        out << "none";
    else
        out << ss.str();

    out << " ]";

    return out.str();
}

