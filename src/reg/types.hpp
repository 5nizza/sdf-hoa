#pragma once

#include <unordered_set>
#include <unordered_map>
#include <string>

#include "reg/special_graph.hpp"


/* This file contains abbreviations for often-used types */

namespace sdf
{

using string_hset = std::unordered_set<std::string>;
using EC = string_hset;
using V = graph::SpecialGraph::V;
using VtoEC = std::unordered_map<V,EC>;

} // namespace sdf