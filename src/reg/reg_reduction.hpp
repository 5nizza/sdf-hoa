#pragma once

#include <set>
#include "spdlog/spdlog.h"
#include "reg/mixed_domain.hpp"


#define BDD spotBDD
    #include "spot/twa/twagraph.hh"
    #include "spot/tl/formula.hh"
#undef BDD



namespace sdf
{

/**
 * Take reg-UCW and create a classical UCW for synthesis.
 * (Note: bcz twa_graph_ptr is shared_ptr, we can pass the reference and assign to it.)
 * NB: once you implement a new data domain, instantiate this template function in reg_reduction.cpp
 */
std::tuple<spot::twa_graph_ptr,      // new_ucw
           std::unordered_set<spot::formula>,  // sysTst
           std::unordered_set<spot::formula>,  // sysAsgn
           std::unordered_set<spot::formula>>  // sysOutR
reduce(MixedDomain& domain,
       const spot::twa_graph_ptr& reg_ucw,
       const std::unordered_set<std::string>& sysR,
       const std::unordered_map<std::string,DomainName>& sys_pred_descr);


void tmp();  // TODO: for testing purposes: remove on release

}  //namespace sdf
