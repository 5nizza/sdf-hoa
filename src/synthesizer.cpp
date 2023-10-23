#include "synthesizer.hpp"

#include "game_solver.hpp"
#include "k_reduce.hpp"
#include "ltl_parser.hpp"
#include "ehoa_parser.hpp"
#include "utils.hpp"
#include "atm_helper.hpp"
#include "syntcomp_constants.hpp"
#include "timer.hpp"

#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
    #include <spot/twaalgos/translate.hh>
//    #include <spot/parseaut/public.hh>
#undef BDD


extern "C"
{
    #include <aiger.h>
}

#include <spdlog/spdlog.h>


using namespace std;
using namespace sdf;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}  // NOLINT(bugprone-macro-parentheses)
#define INF(message) {spdlog::get("console")->info() << message;}     // NOLINT(bugprone-macro-parentheses)

#define hset unordered_set


int sdf::run_hoa(const SpecDescr& spec_descr,
                 const std::vector<uint>& k_to_iterate)
{
    auto [aut, inputs, outputs, is_moore] = read_ehoa(spec_descr.file_name);

    // TODO: what happens when tlsf does have inputs/outputs but the formula doesn't mention them?
    //       (should still have it)

    INF("\n" <<
        "  hoa: " << spec_descr.file_name << "\n" <<
        "  inputs: " << join(", ", inputs) << "\n" <<
        "  outputs: " << join(", ", outputs) << "\n" <<
        "  UCW atm size: " << aut->num_states() << "\n" <<
        "  is_moore: " << is_moore);

    {   // debug
        stringstream ss;
        spot::print_dot(ss, aut);
        DEBUG("\n" << ss.str());
    }

    aiger* model;
    bool game_is_real = synthesize_atm(SpecDescr2(aut, inputs, outputs, is_moore, spec_descr.extract_model), k_to_iterate, model);

    if (!game_is_real)
    {   // game is won by Adam, but it does not mean the invoked spec is unrealizable (due to k-reduction)
        cout << SYNTCOMP_STR_UNKNOWN << endl;
        return SYNTCOMP_RC_UNKNOWN;
    }

    // game is won by Eve, so the spec is realizable

    if (spec_descr.extract_model)
    {
        if (!spec_descr.output_file_name.empty())
        {
            INF("writing a model to " << spec_descr.output_file_name);
            int res = (spec_descr.output_file_name == "stdout") ?
                      aiger_write_to_file(model, aiger_ascii_mode, stdout):
                      aiger_open_and_write_to_file(model, spec_descr.output_file_name.c_str());
            MASSERT(res, "Could not write the model to file");
        }
        aiger_reset(model);
    }

    cout << SYNTCOMP_STR_REAL << endl;
    return SYNTCOMP_RC_REAL;
}

int sdf::run_tlsf(const SpecDescr& spec_descr,
                  const std::vector<uint>& k_to_iterate)
{
    spot::formula formula;
    hset<spot::formula> inputs, outputs;
    bool is_moore;
    tie(formula, inputs, outputs, is_moore) = parse_tlsf(spec_descr.file_name);

    // TODO: what happens when TLSF does have inputs/outputs but the formula doesn't mention them?
    //       (should still have it)

    INF("\n" <<
        "  tlsf: " << spec_descr.file_name << "\n" <<
        "  formula: " << formula << "\n" <<
        "  inputs: " << join(", ", inputs) << "\n" <<
        "  outputs: " << join(", ", outputs) << "\n" <<
        "  is_moore: " << is_moore);
    INF("checking " << (spec_descr.check_unreal ? "UN" : "") << "realizability");

    aiger* model;
    bool game_is_real;
    game_is_real = spec_descr.check_unreal?
            synthesize_formula(SpecDescr2(spot::formula::Not(formula), outputs, inputs, !is_moore, spec_descr.extract_model), k_to_iterate, model):
            synthesize_formula(SpecDescr2(formula, inputs, outputs, is_moore, spec_descr.extract_model), k_to_iterate, model);

    if (!game_is_real)
    {   // game is won by Adam, but it does not mean the invoked spec is unrealizable (due to k-reduction)
        cout << SYNTCOMP_STR_UNKNOWN << endl;
        return SYNTCOMP_RC_UNKNOWN;
    }

    // game is won by Eve, so the (original or dualized) spec is realizable

    if (spec_descr.check_unreal)
        INF("(the game is won by Eve but the spec was dualized)");

    cout << (spec_descr.check_unreal ? SYNTCOMP_STR_UNREAL : SYNTCOMP_STR_REAL) << endl;

    if (spec_descr.extract_model)
    {
        if (!spec_descr.output_file_name.empty())
        {
            INF("writing a model to " << spec_descr.output_file_name);
            int res = (spec_descr.output_file_name == "stdout") ?
                      aiger_write_to_file(model, aiger_ascii_mode, stdout):
                      aiger_open_and_write_to_file(model, spec_descr.output_file_name.c_str());
            MASSERT(res, "Could not write the model to file");
        }
        aiger_reset(model);
    }

    return (spec_descr.check_unreal ? SYNTCOMP_RC_UNREAL : SYNTCOMP_RC_REAL);
}


bool sdf::synthesize_atm(const SpecDescr2<spot::twa_graph_ptr>& spec_descr,
                         const std::vector<uint>& k_to_iterate,
                         aiger*& model)
{
    for (auto k: k_to_iterate)
    {
        INF("trying k = " << k);
        auto k_aut = k_reduce(spec_descr.spec, k);

        INF("automaton before iterative merging states/edges: "
            << k_aut->num_states() << " states, "
            << k_aut->num_edges() << " edges");
        merge_atm(k_aut);
        INF("merged k-automaton: " << k_aut->num_states() << " states, " << k_aut->num_edges() << " edges");

        {   // debug
            stringstream ss;
            spot::print_dot(ss, k_aut);
            DEBUG("\n" << ss.str());
        }

        GameSolver solver(spec_descr.is_moore, spec_descr.inputs, spec_descr.outputs, k_aut, 3600);
        if (spec_descr.extract_model)
        {
            model = solver.synthesize();
            if (model != nullptr)
                return true;
        }
        else
        {
            if (solver.check_realizability())
                return true;
        }
    }

    return false;
}


bool sdf::synthesize_formula(const SpecDescr2<spot::formula>& spec_descr,
                             const std::vector<uint>& k_to_iterate,
                             aiger*& model)
{
    spot::formula neg_formula = spot::formula::Not(spec_descr.spec);
    spot::translator translator;
    translator.set_type(spot::postprocessor::BA);
    translator.set_pref(spot::postprocessor::SBAcc);
    translator.set_level(spot::postprocessor::Medium);
    // On some examples the high optimization is the bottleneck
    // while Medium seems to be good enough. Eamples: try_ack_arbiter, lift
    // The results of SYNTCOMP'21 confirm that Medium performs better by a noticeable margin, so we use Medium.

    Timer timer;
    spot::twa_graph_ptr aut = translator.run(neg_formula);
    INF("LTL->UCW translation took (sec.): " << timer.sec_restart());
    INF("UCW automaton size (states): " << aut->num_states());

    {   // debug
        stringstream ss;
        spot::print_dot(ss, aut);
        DEBUG("\n" << ss.str());
    }

    return synthesize_atm(SpecDescr2(aut, spec_descr.inputs, spec_descr.outputs, spec_descr.is_moore, spec_descr.extract_model),
                          k_to_iterate,
                          model);
}
