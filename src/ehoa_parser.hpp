#pragma once

#include <string>
#include <tuple>
#include <set>

#define BDD spotBDD
#include <spot/tl/formula.hh>
#include <spot/twa/twagraph.hh>
#undef BDD


namespace sdf
{
/**
 * Convert a file in the extended HOA format.
 */
std::tuple<spot::twa_graph_ptr, std::set<spot::formula>, std::set<spot::formula>, bool>
read_ehoa(const std::string& hoa_file_name);

} //namespace sdf
