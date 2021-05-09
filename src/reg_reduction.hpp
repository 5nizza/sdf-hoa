#pragma once

#include <set>
#include <spdlog/spdlog.h>

#define BDD spotBDD
    #include <spot/parseaut/public.hh>
    #include <spot/twaalgos/hoa.hh>
    #include <spot/twa/bddprint.hh>
    #include <spot/twaalgos/translate.hh>
#undef BDD

extern "C"
{
    #include <aiger.h>
}


namespace sdf
{

/**
 * @return code according to SYNTCOMP (real_rc if real, else unknown_rc)
 */
//int reduce(const std::string& hoa_file_name,
//           uint b,
//           const std::vector<uint>& k_to_iterate,
//           bool extract_model=false,
//           const std::string& output_file_name="");

/**
 * Take reg-UCW and create a classical UCW for synthesis.
 */
void reduce(spot::twa_graph_ptr reg_ucw, uint nof_sys_regs,
            spot::twa_graph_ptr& classical_ucw,
            std::set<spot::formula>& sysTst,
            std::set<spot::formula>& sysAsgn,
            std::set<spot::formula>& sysR);

} //namespace sdf
