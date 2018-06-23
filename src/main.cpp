#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>
#include <args.hxx>

#include "Synth.hpp"
#include "k_reduce.hpp"
#include "ltl_parser.hpp"
#include "utils.hpp"
#include "syntcomp_constants.hpp"

#define BDD spotBDD
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twa/bddprint.hh>
#include <spot/twaalgos/translate.hh>
#undef BDD


using namespace std;


bool check_real_atm(spot::twa_graph_ptr ucw_aut,
                    uint k,
                    bool is_moore,
                    const vector<spot::formula> &inputs,
                    const vector<spot::formula> &outputs,
                    const string &output_file_name)
{
    auto k_aut = k_reduce(ucw_aut, k);

    Synth synthesizer(is_moore, inputs, outputs, k_aut, output_file_name, 3600);
    bool is_realizable = synthesizer.run();
    return is_realizable;
}


bool check_real_formula(const spot::formula& formula,
                        const vector<spot::formula>& inputs,
                        const vector<spot::formula>& outputs,
                        const vector<uint>& k_to_iterate,
                        bool is_moore,
                        const string& output_file_name)  // TODO: replace file name with less low-level
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


int main(int argc, const char *argv[])
{
    args::ArgumentParser parser("Synthesizer from LTL (TLSF format)");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::Positional<string> tlsf_arg
        (parser, "tlsf",
         "File with TLSF",
         args::Options::Required);

    args::Flag is_moore_flag
            (parser,
             "moore",
             "synthesize Moore machines (by default I synthesize Mealy)",
             {"moore"});
    args::Flag check_unreal_flag
            (parser,
             "unreal",
             "check unrealizability (instead of (default) realizability)",
             {'u', "unreal"});
    args::ValueFlagList<uint> k_list_arg
            (parser,
             "k",
             "the maximum number of times the same bad state can be visited "
             "(thus, it is reset between SCCs). "
             "If you provide it twice (e.g. -k 1 -k 5), then I will try all values in that range. "
             "Default: 4.",
             {'k'});

    args::ValueFlag<string> output_name
            (parser,
             "o",
             "the output file name (not yet supported)",
             {'o'});

    args::Flag verbose_flag
            (parser,
             "v",
             "verbose mode (the default is silent)",
             {'v', "verbose"});

    args::HelpFlag help
        (parser,
         "help",
         "Display this help menu",
         {'h', "help"});

    try
    {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help&)
    {
        cout << parser;
        return 0;
    }
    catch (args::ParseError& e)
    {
        cerr << e.what() << endl;
        cerr << parser;
        return 1;
    }
    catch (args::ValidationError& e)
    {
        cerr << e.what() << endl;
        cerr << parser;
        return 1;
    }

    // setup logging
    auto console = spdlog::stdout_logger_mt("console", false);
    spdlog::set_pattern("%H:%M:%S %v ");
    if (!verbose_flag)
        spdlog::set_level(spdlog::level::off);

    // parse args
    string tlsf_file_name(tlsf_arg.Get());
    string output_file_name(output_name ? output_name.Get() : "stdout");
    vector<uint> k_list(k_list_arg.Get());
    if (k_list.empty()) k_list.push_back(4);
    bool is_moore(is_moore_flag? true:false);
    bool check_unreal(check_unreal_flag? true:false);

    spdlog::get("console")->info()
            << "tlsf_file: " << tlsf_file_name << ", "
            << "check_unreal: " << check_unreal << ", "
            << "k: " << "[" << join(", ", k_list) << "], "
            << "is_moore: " << is_moore << ", "
            << "output_file: " << output_file_name;

    spot::formula formula;
    vector<spot::formula> inputs, outputs;
    tie(formula, inputs, outputs) = parse_tlsf(tlsf_file_name);

    spdlog::get("console")->info() << "\n"
            << "formula: " << formula << "\n"
            << "inputs: " << join(", ", inputs) << "\n"
            << "outputs: " << join(", ", outputs);
    spdlog::get("console")->info() << "checking " << (check_unreal? "UN":"") << "realizability";

    vector<uint> k_to_iterate = (k_list.size() == 2?
                                 range(k_list[0], k_list[1]+1):
                                 (k_list.size()>2? k_list: range(k_list[0], k_list[0]+1)));

    bool check_status;
    if (check_unreal)
        // dualize the spec
        check_status = check_real_formula(spot::formula::Not(formula), outputs, inputs, k_to_iterate, !is_moore, output_file_name);
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

    // PAST:
    // - implemented non-parallel version of the tool
    // - added tests framework
    /// CURRENT:
    /// - finish tests (see tests.cpp)
    // FUTURE:
    // - implement the parallel version of the tool
    // - extract AIGER models
}

