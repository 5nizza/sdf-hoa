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
 * @return code according to SYNTCOMP (unreal_rc if unreal, real_rc if real, else unknown_rc)
 */
int run(bool check_unreal,
        const std::string& tlsf_file_name,
        bool postprocess_atm,
        const std::vector<uint>& k_to_iterate,
        bool extract_model=false,
        const std::string& output_file_name="");

/**
 * @return code according to SYNTCOMP (real_rc if real, else unknown_rc)
 */
int run(const std::string& hoa_file_name,
        const std::vector<uint>& k_to_iterate,
        bool extract_model=false,
        const std::string& output_file_name="");

/**
 * @return true iff the formula is realizable
 * @param extract_model: should extract model into `model`
 */
bool synthesize_formula(const spot::formula& formula,
                        bool postprocess_atm,
                        const std::set<spot::formula>& inputs,
                        const std::set<spot::formula>& outputs,
                        bool is_moore,
                        const std::vector<uint>& k_to_iterate,
                        bool extract_model,
                        aiger*& model);

/**
 * @return true iff the UCW automaton is realizable
 * @param extract_model: should extract model into `model`
 */
bool synthesize_atm(spot::twa_graph_ptr ucw_aut,
                    const std::set<spot::formula>& inputs,
                    const std::set<spot::formula>& outputs,
                    bool is_moore,
                    const std::vector<uint>& k_to_iterate,
                    bool extract_model,
                    aiger*& model);

} //namespace sdf
