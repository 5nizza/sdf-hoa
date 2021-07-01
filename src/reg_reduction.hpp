#pragma once

#include <set>
#include <spdlog/spdlog.h>

#define BDD spotBDD
    #include <spot/parseaut/public.hh>
    #include <spot/twaalgos/hoa.hh>
    #include <spot/twa/bddprint.hh>
    #include <spot/twaalgos/translate.hh>
#include "partition_tst_asgn.hpp"

#undef BDD

extern "C"
{
    #include <aiger.h>
}


namespace sdf
{

/**
 * Take reg-UCW and create a classical UCW for synthesis.
 * (Note: bcz twa_graph_ptr is shared_ptr, we can pass the reference and assign to it.)
 */
void reduce(const spot::twa_graph_ptr& reg_ucw, uint nof_sys_regs,
            spot::twa_graph_ptr& classical_ucw,
            std::set<spot::formula>& sysTst,
            std::set<spot::formula>& sysAsgn,
            std::set<spot::formula>& sysOutR);

std::string extract_reg_from_action(const std::string& action_name);

void parse_tst(const std::string& action_name,
               std::string& t1,
               std::string& t2,
               std::string& cmp);

void tmp();

} //namespace sdf
