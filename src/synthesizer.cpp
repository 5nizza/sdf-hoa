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
    #include <spot/twaalgos/simulation.hh>
//    #include <spot/parseaut/public.hh>
#undef BDD


extern "C"
{
    #include <aiger.h>
}

#include <fstream>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ostream.h>


using namespace std;
using namespace sdf;


#define hset unordered_set


template <> struct fmt::formatter<spot::formula> : ostream_formatter {};  // reason: to be able to pass to spdlog::info(..) the object of spot::formula which implements "<<"

int sdf::run_tlsf(const SpecDescr& spec_descr,
                  const std::vector<uint>& k_to_iterate)
{
    spot::formula formula;
    hset<spot::formula> inputs, outputs;
    bool is_moore;
    tie(formula, inputs, outputs, is_moore) = parse_tlsf(spec_descr.file_name);

    // TODO: what happens when TLSF does have inputs/outputs but the formula doesn't mention them?
    //       (should still have it)

    spdlog::info("\n"
        "  tlsf: {}\n"
        "  formula: {}\n"
        "  inputs: {}\n"
        "  outputs: {}\n"
        "  is_moore: {}\n",
        spec_descr.file_name, formula, join(", ", inputs), join(", ", outputs), is_moore);

    unordered_map<string,string> models;
    bool game_is_real;
    game_is_real = spec_descr.check_unreal?
            synthesize_formula(SpecDescr2(spot::formula::Not(formula), outputs, inputs, !is_moore, spec_descr.extract_model, spec_descr.do_reach_optim, spec_descr.do_var_group_optim), k_to_iterate, models):
            synthesize_formula(SpecDescr2(formula, inputs, outputs, is_moore, spec_descr.extract_model, spec_descr.do_reach_optim, spec_descr.do_var_group_optim), k_to_iterate, models);

    if (!game_is_real)
    {   // game is won by Adam, but it does not mean the invoked spec is unrealizable (due to k-reduction)
        cout << SYNTCOMP_STR_UNKNOWN << endl;
        return SYNTCOMP_RC_UNKNOWN;
    }

    // game is won by Eve, so the (original or dualized) spec is realizable

    if (spec_descr.check_unreal)
        spdlog::info("(the game is won by Eve but the spec was dualized)");

    cout << (spec_descr.check_unreal ? SYNTCOMP_STR_UNREAL : SYNTCOMP_STR_REAL) << endl;

    if (!spec_descr.output_file_name.empty())
    {
        for (auto const& [file_type, file_content] : models)
        {
            auto file_extension = file_type == "aiger"? "solution.aag" : file_type;
            auto file_name = format("{}.{}", spec_descr.output_file_name, file_extension);
            spdlog::info("writing {} model to {}", file_type, file_name);
            ofstream out(file_name);
            out << file_content;
        }
    }

    return (spec_descr.check_unreal ? SYNTCOMP_RC_UNREAL : SYNTCOMP_RC_REAL);
}


bool sdf::synthesize_atm(const SpecDescr2<spot::twa_graph_ptr>& spec_descr,
                         const std::vector<uint>& k_to_iterate,
                         unordered_map<string,string>& output_models)
{
    for (auto k: k_to_iterate)
    {
        spdlog::info("trying k = {}", k);
        auto k_aut = k_reduce(spec_descr.spec, k);

        MASSERT(k_aut->is_sba() == spot::trival::yes_value, "is the automaton with Buchi-state acceptance?");
        MASSERT(k_aut->prop_terminal() == spot::trival::yes_value, "is the automaton terminal?");

        spdlog::info("automaton before sim/cosim reduction: {} states, {} edges", k_aut->num_states(), k_aut->num_edges());
        auto reduced_k_aut = spot::reduce_iterated_sba(k_aut);
        reduced_k_aut->copy_named_properties_of(k_aut);    // TODO: strange: bug?: ask Ald about this (on lilydemo13.tlsf, the properties are not copied)
        reduced_k_aut->copy_acceptance_of(k_aut);          // TODO: strange: bug?: ask Ald about this
        k_aut = reduced_k_aut;
        MASSERT(k_aut->is_sba() == spot::trival::yes_value, "is the automaton with Buchi-state acceptance?");
        MASSERT(k_aut->prop_terminal() == spot::trival::yes_value, "is the automaton terminal?");
        spdlog::info("... after sim/cosim reduction: {} states, {} edges", k_aut->num_states(), k_aut->num_edges());

        {   // debug
            stringstream ss;
            spot::print_dot(ss, k_aut);
            spdlog::debug("\n{}", ss.str());
        }

        GameSolver solver(spec_descr.is_moore, spec_descr.inputs, spec_descr.outputs, k_aut,
                          spec_descr.do_reach_optim && (k_aut->num_states()<=R_OPTIM_BOUND),
                          spec_descr.do_var_grouping_optim,
                          3600);

        aiger* aiger_model = nullptr;
        auto realizable = solver.generate_qbf_for_relation_determinization(spec_descr.extract_model, output_models, aiger_model);

        if (realizable)
        {
            if (spec_descr.extract_model)
            {
                for (int c_str_len = 1024 * 1024, res = 0; c_str_len <= 512 * 1024 * 1024 and res != 1; c_str_len *= 2)  // 512MB limit
                {
                    auto raw_c_str = (char*) malloc(c_str_len);
                    if (raw_c_str != nullptr)
                    {
                        res = aiger_write_to_string(aiger_model, aiger_mode::aiger_ascii_mode, raw_c_str, c_str_len);
                        if (res == 1)
                            output_models["aiger"] = string(raw_c_str);
                        free(raw_c_str);
                    }
                    else
                        spdlog::warn("could not allocate memory for AIGER string");
                }
                if (spec_descr.extract_model and output_models["aiger"].empty())
                    spdlog::warn("could not create AIGER string");

                aiger_reset(aiger_model);
            }
            return true;
        }
    }

    return false;
}


bool sdf::synthesize_formula(const SpecDescr2<spot::formula>& spec_descr,
                             const std::vector<uint>& k_to_iterate,
                             unordered_map<string,string>& output_models)
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
    spdlog::info("LTL->UCW translation took (sec.): {}", timer.sec_restart());
    spdlog::info("UCW automaton size (states): {}", aut->num_states());

    {   // debug
        stringstream ss;
        spot::print_dot(ss, aut);
        spdlog::debug("\n{}", ss.str());
    }

    return synthesize_atm(SpecDescr2(aut, spec_descr.inputs, spec_descr.outputs, spec_descr.is_moore, spec_descr.extract_model, spec_descr.do_reach_optim, spec_descr.do_var_grouping_optim),
                          k_to_iterate,
                          output_models);
}
