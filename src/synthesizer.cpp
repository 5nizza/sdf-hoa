//
// Created by ayrat on 29/06/18.
//

#include "synthesizer.hpp"

#include "game_solver.hpp"
#include "k_reduce.hpp"
#include "ltl_parser.hpp"
#include "utils.hpp"
#include "syntcomp_constants.hpp"

using namespace std;
using namespace sdf;
using namespace ak_utils;


// Ensures that the logger "console" exists
struct Initializer
{
    std::shared_ptr<spdlog::logger> logger;

    Initializer()
    {
        logger = spdlog::stdout_logger_mt("console", false);
        spdlog::set_pattern("%H:%M:%S %v ");
    }
};
Initializer libraryInitializer;


int sdf::run(const std::string &tlsf_file_name,
             bool check_unreal,
             const std::vector <uint> &k_to_iterate,
             const std::string &output_file_name)
{
    spot::formula formula;
    vector<spot::formula> inputs, outputs;
    bool is_moore;
    tie(formula, inputs, outputs, is_moore) = parse_tlsf(tlsf_file_name);

    spdlog::get("console")->info() << "\n"
                                   << "formula: " << formula << "\n"
                                   << "inputs: " << join(", ", inputs) << "\n"
                                   << "outputs: " << join(", ", outputs) << "\n"
                                   << "is_moore " << is_moore;
    spdlog::get("console")->info() << "checking " << (check_unreal ? "UN" : "") << "realizability";

    bool check_status;
    if (check_unreal)
        // dualize the spec
        check_status =
            check_real_formula(spot::formula::Not(formula), outputs, inputs, k_to_iterate, !is_moore, output_file_name);
    else
        check_status = check_real_formula(formula, inputs, outputs, k_to_iterate, is_moore, output_file_name);

    if (!check_status)
    {
        cout << SYNTCOMP_STR_UNKNOWN << endl;
        return SYNTCOMP_RC_UNKNOWN;
    }
    else
    {
        cout << (check_unreal ? SYNTCOMP_STR_UNREAL : SYNTCOMP_STR_REAL) << endl;
        return (check_unreal ? SYNTCOMP_RC_UNREAL : SYNTCOMP_RC_REAL);
    }
}


bool sdf::check_real_atm(spot::twa_graph_ptr ucw_aut,
                         uint k,
                         bool is_moore,
                         const std::vector<spot::formula> &inputs,
                         const std::vector<spot::formula> &outputs,
                         const std::string &output_file_name)
{
    auto k_aut = k_reduce(ucw_aut, k);

    GameSolver gameSolver(is_moore, inputs, outputs, k_aut, output_file_name, 3600);
    bool is_realizable = gameSolver.run();
    return is_realizable;
}


bool sdf::check_real_formula(const spot::formula &formula,
                             const std::vector<spot::formula> &inputs,
                             const std::vector<spot::formula> &outputs,
                             const std::vector<uint> &k_to_iterate,
                             bool is_moore,
                             const std::string &output_file_name)  // TODO: replace file name with less low-level
{
    spot::formula neg_formula = spot::formula::Not(formula);
    spot::translator translator;
    translator.set_type(spot::postprocessor::BA);
    translator.set_pref(spot::postprocessor::SBAcc);
    translator.set_level(spot::postprocessor::Medium);  // on some examples the high optimization is the bottleneck (TODO: double-check)
    spot::twa_graph_ptr aut = translator.run(neg_formula);

    for (auto k: k_to_iterate)
    {
        auto is_realizable = check_real_atm(aut, k, is_moore, inputs, outputs, output_file_name);
        if (is_realizable)
            return true;
    }

    return false;
}
