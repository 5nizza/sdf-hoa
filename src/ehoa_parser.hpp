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

const std::string CONTROLLABLE_AP_TKN = "controllable-AP";  // NOLINT(cert-err58-cpp)
const std::string SYNT_MOORE_TKN = "synt-moore";            // NOLINT(cert-err58-cpp)

/**
 * Read a file in the extended HOA format.
 * The extended HOA format allows you to add
 * controllable-AP: n1 n2 ...
 * where n1, n2 ... are the indices of the controllable APs.
 * Note: controllable-AP can also be missing alltogether.
 * @return: aut, inputs, outputs, is_moore
 */
std::tuple<spot::twa_graph_ptr, std::set<spot::formula>, std::set<spot::formula>, bool>
read_ehoa(const std::string& hoa_file_name);

} //namespace sdf
