#pragma once

#include <algorithm>

#include "my_assert.hpp"
#include "reg/types.hpp"
#include "../utils.hpp"


namespace sdf
{

inline
V add_vertex(graph::SpecialGraph& g,
             VtoEC& v_to_ec,
             const std::string& var_name)
{
    for (const auto& v_ec: v_to_ec)
        MASSERT(!v_ec.second.count(var_name), "the vertex is already present: " << var_name);

    auto vertices = g.get_vertices();
    auto new_v = vertices.empty() ? 0 : 1 + *std::max_element(vertices.begin(), vertices.end());
    g.add_vertex(new_v);
    v_to_ec.insert({new_v, {var_name}});
    return new_v;
}

inline
V get_vertex_of(const std::string& var,
                const graph::SpecialGraph& g,
                const VtoEC& v_to_ec)
{
    for (const auto& v : g.get_vertices())
        if (v_to_ec.at(v).count(var))
            return v;

    MASSERT(0,
            "not possible: var: " << var << ", v_to_ec: " <<
                                  join(", ", v_to_ec,
                                       [](const auto& v_ec)
                                       {
                                           return std::to_string(v_ec.first) + " -> {" + join(",", v_ec.second) + "}";
                                       }));
}

}