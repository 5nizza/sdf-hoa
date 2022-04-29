#pragma once

#include <set>
#include "spdlog/spdlog.h"

#define BDD spotBDD
    #include "spot/parseaut/public.hh"
    #include "spot/twaalgos/hoa.hh"
    #include "spot/twa/bddprint.hh"
    #include "spot/twaalgos/translate.hh"
#undef BDD

#include "ord_partition.hpp"


extern "C"
{
    #include "aiger.h"
}


namespace sdf
{

/**
 * Take reg-UCW and create a classical UCW for synthesis.
 * (Note: bcz twa_graph_ptr is shared_ptr, we can pass the reference and assign to it.)
 */
std::tuple<spot::twa_graph_ptr,      // new_ucw
           std::set<spot::formula>,  // sysTst
           std::set<spot::formula>,  // sysAsgn
           std::set<spot::formula>>  // sysOutR
reduce(const spot::twa_graph_ptr& reg_ucw, uint nof_sys_regs);

void tmp();

} //namespace sdf
