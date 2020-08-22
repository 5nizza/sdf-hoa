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
    set<spot::formula> inputs, outputs;
    bool is_moore;
    tie(formula, inputs, outputs, is_moore) = parse_tlsf(tlsf_file_name);
    for (auto e: inputs)
    {
        spdlog::get("console")->info() << "input e: " << e << "\n";
    }

    spdlog::get("console")->info() << "\n"
                                   << "tlsf: " << tlsf_file_name << "\n"
                                   << "formula: " << formula << "\n"
                                   << "inputs: " << join(", ", inputs) << "\n"
                                   << "outputs: " << join(", ", outputs) << "\n"
                                   << "is_moore " << is_moore;
    spdlog::get("console")->info() << "checking " << (check_unreal ? "UN" : "") << "realizability";

    bool game_is_real;
    if (check_unreal)
    {
        // we use dualized the spec
        game_is_real = check_real_formula(spot::formula::Not(formula),
                                          outputs, inputs,
                                          k_to_iterate, !is_moore, output_file_name);
        spdlog::get("console")->info() << "checking unreal, status: " << game_is_real;
    }
    else
    {
        game_is_real = check_real_formula(formula,
                                          inputs, outputs,
                                          k_to_iterate, is_moore, output_file_name);

        spdlog::get("console")->info() << "checking real, status: " << game_is_real;
    }

    if (!game_is_real)
    {   // game is won by Adam, but it does not mean the spec is unreal (due to k-reduction)
        cout << SYNTCOMP_STR_UNKNOWN << endl;
        return SYNTCOMP_RC_UNKNOWN;
    }
    else
    {   // game is real, so we know for sure
        cout << (check_unreal ? SYNTCOMP_STR_UNREAL : SYNTCOMP_STR_REAL) << endl;
        return (check_unreal ? SYNTCOMP_RC_UNREAL : SYNTCOMP_RC_REAL);
    }
}


bool sdf::check_real_atm(spot::twa_graph_ptr ucw_aut,
                         uint k,
                         bool is_moore,
                         const std::set<spot::formula> &inputs,
                         const std::set<spot::formula> &outputs,
                         const std::string &output_file_name)
{
    auto k_aut = k_reduce(ucw_aut, k);

    GameSolver gameSolver(is_moore, inputs, outputs, k_aut, output_file_name, 3600);
    return gameSolver.check_realizability();
}


bool sdf::check_real_formula(const spot::formula& formula,
                             const set<spot::formula>& inputs,
                             const set<spot::formula>& outputs,
                             const vector<uint>& k_to_iterate,
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
