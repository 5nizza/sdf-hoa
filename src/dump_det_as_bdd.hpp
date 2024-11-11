#pragma once

#include <set>
#include <unordered_map>

#include <cuddObj.hh>


namespace sdf
{
    void dump_determinization_problem_to_bdd_file(const Cudd& cudd,
                                                  const std::set<uint>& inputs,
                                                  const std::set<uint>& outputs,
                                                  const std::set<uint>& states,
                                                  const BDD& init,
                                                  const BDD& error,
                                                  const BDD& win_region,
                                                  const std::unordered_map<uint,BDD>& pre_by_state);
}  // namespace sdf
