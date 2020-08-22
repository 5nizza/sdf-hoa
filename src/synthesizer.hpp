//
// Created by ayrat on 29/06/18.
//
#pragma once

#include <set>
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
 * @return `true` iff after k-reduction the automaton is realizable
 */
bool check_real_atm(spot::twa_graph_ptr ucw_aut,
                    uint k,
                    bool is_moore,
                    const std::set<spot::formula>& inputs,
                    const std::set<spot::formula>& outputs,
                    const std::string &output_file_name);

/**
 * @return `true` iff a given formula is realizable using `k_to_iterate`
 */
bool check_real_formula(const spot::formula& formula,
                        const std::set<spot::formula>& inputs,
                        const std::set<spot::formula>& outputs,
                        const std::vector<uint>& k_to_iterate,
                        bool is_moore,
                        const std::string& output_file_name);


} //namespace sdf
