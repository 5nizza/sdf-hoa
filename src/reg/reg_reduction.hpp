#pragma once

#include <set>
#include "spdlog/spdlog.h"

#include "data_domain_interface.hpp"


#define BDD spotBDD
    #include "spot/twa/twagraph.hh"
    #include "spot/tl/formula.hh"
#undef BDD


extern "C"
{
    #include "aiger.h"
}


namespace sdf
{

/**
 * Take reg-UCW and create a classical UCW for synthesis.
 * (Note: bcz twa_graph_ptr is shared_ptr, we can pass the reference and assign to it.)
 * NB: once you implement a new data domain, instantiate this template function in reg_reduction.cpp
 */
template<typename Partition>
std::tuple<spot::twa_graph_ptr,      // new_ucw
           std::set<spot::formula>,  // sysTst
           std::set<spot::formula>,  // sysAsgn
           std::set<spot::formula>>  // sysOutR
reduce(DataDomainInterface<Partition>& domain,
       const spot::twa_graph_ptr& reg_ucw,
       uint nof_sys_regs);


void tmp();

}  //namespace sdf
