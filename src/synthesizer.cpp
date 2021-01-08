//
// Created by ayrat on 29/06/18.
//

#include "synthesizer.hpp"

#include "game_solver.hpp"
#include "k_reduce.hpp"
#include "ltl_parser.hpp"
#include "utils.hpp"
#include "syntcomp_constants.hpp"
#include "timer.hpp"

#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
#undef BDD


extern "C"
{
    #include <aiger.h>
}

#include <spdlog/spdlog.h>


using namespace std;
using namespace sdf;

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


#define logger spdlog::get("console")
#define DEBUG(message) {logger->debug() << message;}
#define INF(message) {logger->info() << message;}


int sdf::run(bool check_unreal,
             const std::string& tlsf_file_name,
             const std::vector<uint>& k_to_iterate,
             bool extract_model,
             const std::string& output_file_name)
{
    spot::formula formula;
    set<spot::formula> inputs, outputs;
    bool is_moore;
    tie(formula, inputs, outputs, is_moore) = parse_tlsf(tlsf_file_name);

    INF("\n" <<
        "  tlsf: " << tlsf_file_name << "\n" <<
        "  formula: " << formula << "\n" <<
        "  inputs: " << join(", ", inputs) << "\n" <<
        "  outputs: " << join(", ", outputs) << "\n" <<
        "  is_moore: " << is_moore);
    INF("checking " << (check_unreal ? "UN" : "") << "realizability");

    aiger* model;
    bool game_is_real;
    game_is_real = check_unreal?
            synthesize_formula(spot::formula::Not(formula), outputs, inputs, !is_moore, k_to_iterate, extract_model, model):
            synthesize_formula(formula, inputs, outputs, is_moore, k_to_iterate, extract_model, model);

    if (!game_is_real)
    {   // game is won by Adam, but it does not mean the invoked spec is unrealizable (due to k-reduction)
        cout << SYNTCOMP_STR_UNKNOWN << endl;
        return SYNTCOMP_RC_UNKNOWN;
    }

    // game is won by Eve, so the (original or dualized) spec is realizable

    if (extract_model)
    {
        if (!output_file_name.empty())
        {
            INF("writing a model to " << output_file_name);
            int res = (output_file_name == "stdout") ?
                    aiger_write_to_file(model, aiger_ascii_mode, stdout):
                    aiger_open_and_write_to_file(model, output_file_name.c_str());
            MASSERT(res, "Could not write the model to file");
        }
        aiger_reset(model);
    }

    cout << (check_unreal ? SYNTCOMP_STR_UNREAL : SYNTCOMP_STR_REAL) << endl;
    return (check_unreal ? SYNTCOMP_RC_UNREAL : SYNTCOMP_RC_REAL);
}


bool sdf::synthesize_atm(spot::twa_graph_ptr ucw_aut,
                         const std::set<spot::formula>& inputs,
                         const std::set<spot::formula>& outputs,
                         bool is_moore,
                         const vector<uint>& k_to_iterate,
                         bool extract_model,
                         aiger*& model)
{
    for (auto k: k_to_iterate)
    {
        INF("trying k = " << k);
        auto k_aut = k_reduce(ucw_aut, k);
        INF("k-automaton # states: " << k_aut->num_states());

        {   // debug
            stringstream ss;
            spot::print_dot(ss, k_aut);
            DEBUG("\n" << ss.str());
        }

        GameSolver solver(is_moore, inputs, outputs, k_aut, 3600);
        if ((!extract_model && solver.check_realizability()) ||
            (extract_model && (model = solver.synthesize())!=nullptr))
            return true;
    }

    return false;
}


bool sdf::synthesize_formula(const spot::formula& formula,
                             const set<spot::formula>& inputs,
                             const set<spot::formula>& outputs,
                             bool is_moore,
                             const vector<uint>& k_to_iterate,
                             bool extract_model,
                             aiger*& model)
{
    spot::formula neg_formula = spot::formula::Not(formula);
    spot::translator translator;
    translator.set_type(spot::postprocessor::BA);
    translator.set_pref(spot::postprocessor::SBAcc);
    translator.set_level(spot::postprocessor::Medium);  // on some examples the high optimization is the bottleneck
    // TODO: double-check the above claim: I also tried lift: with optimization `High` it is much faster!

    Timer timer;
    spot::twa_graph_ptr aut = translator.run(neg_formula);
    INF("LTL->UCW translation took (sec.): " << timer.sec_restart());
    INF("UCW automaton size (states): " << aut->num_states());

    {   // debug
        stringstream ss;
        spot::print_dot(ss, aut);
        DEBUG("\n" << ss.str());
    }

    return synthesize_atm(aut, inputs, outputs, is_moore, k_to_iterate, extract_model, model);
}

