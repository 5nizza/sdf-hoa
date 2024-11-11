#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <spdlog/spdlog.h>


#define BDD spotBDD
    #include <spot/tl/formula.hh>
    #include <spot/twa/twagraph.hh>
#undef BDD

extern "C"
{
    #include <aiger.h>
}


namespace sdf
{

const uint R_OPTIM_BOUND = 120;  // reachability-analysis optimization is disabled when the number of states in the safety automaton exceeds this number

struct SpecDescr
{
    const bool check_unreal;
    const std::string& file_name;
    const bool extract_model;
    const bool do_reach_optim;
    const bool do_var_group_optim;
    const std::string& output_file_name;

    SpecDescr(bool checkUnreal,
              const std::string& fileName,
              bool extractModel = false,
              bool do_reach_optim = false,
              bool do_var_group_optim = false,
              const std::string& outputFileName = "") :
            check_unreal(checkUnreal),
            file_name(fileName),
            extract_model(extractModel),
            do_reach_optim(do_reach_optim),
            do_var_group_optim(do_var_group_optim),
            output_file_name(outputFileName) {}
};

/**
 * Backwards-exploration synthesis algorithm.
 * @return code according to SYNTCOMP (unreal_rc if unreal, real_rc if real, else unknown_rc)
 */
int run_tlsf(const SpecDescr& spec_descr,
             const std::vector<uint>& k_to_iterate);

/**
 * Backwards-exploration synthesis algorithm.
 * @return code according to SYNTCOMP (real_rc if real, else unknown_rc)
 */
int run_hoa(const SpecDescr& spec_descr,
            const std::vector<uint>& k_to_iterate);



template<typename T>
struct SpecDescr2
{
    const T& spec;
    const std::unordered_set<spot::formula>& inputs;
    const std::unordered_set<spot::formula>& outputs;
    const bool is_moore;
    const bool extract_model;
    const bool do_reach_optim;
    const bool do_var_grouping_optim;

    SpecDescr2(const T& spec,
              const std::unordered_set<spot::formula>& inputs,
              const std::unordered_set<spot::formula>& outputs,
              bool isMoore,
              bool extractModel,
              bool do_reach_optim,
              bool do_var_grouping_optim) :
            spec(spec),
            inputs(inputs), outputs(outputs),
            is_moore(isMoore),
            extract_model(extractModel),
            do_reach_optim(do_reach_optim),
            do_var_grouping_optim(do_var_grouping_optim)
            {}
};

/**
 * Backwards-exploration synthesis algorithm.
 * @return true iff the formula is realizable
 */
bool synthesize_formula(const SpecDescr2<spot::formula>& spec_descr,
                        const std::vector<uint>& k_to_iterate,
                        std::unordered_map<std::string,std::string>& output_models);

/**
 * Backwards-exploration synthesis algorithm.
 * @return true iff the UCW automaton is realizable
 */
bool synthesize_atm(const SpecDescr2<spot::twa_graph_ptr>& spec_descr,
                    const std::vector<uint>& k_to_iterate,
                    std::unordered_map<std::string,std::string>& output_models);


} //namespace sdf
