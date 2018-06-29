//
// Created by ayrat on 29/06/18.
//
#pragma once

#include <spdlog/spdlog.h>

#define BDD spotBDD
    #include <spot/parseaut/public.hh>
    #include <spot/twaalgos/hoa.hh>
    #include <spot/twa/bddprint.hh>
    #include <spot/twaalgos/translate.hh>
#undef BDD


namespace sdf
{


/**
 * @return return code is according to SYNTCOMP constants
 *         (i.e., if unreal then unreal_rc, if real then real_rc, if unknown then unknown_rc)
 */
int run(const std::string& tlsf_file_name,
        bool check_unreal,
        const std::vector<uint>& k_to_iterate,
        const std::string& output_file_name);

/**
 * @return is_realizable
 */
bool check_real_atm(spot::twa_graph_ptr ucw_aut,
                    uint k,
                    bool is_moore,
                    const std::vector<spot::formula> &inputs,
                    const std::vector<spot::formula> &outputs,
                    const std::string &output_file_name);

/**
 * @return is_realizable
 */
bool check_real_formula(const spot::formula &formula,
                        const std::vector<spot::formula> &inputs,
                        const std::vector<spot::formula> &outputs,
                        const std::vector<uint> &k_to_iterate,
                        bool is_moore,
                        const std::string &output_file_name);


} //namespace sdf
