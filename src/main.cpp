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

#define BDD spotBDD
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twa/bddprint.hh>
#include <spot/twaalgos/translate.hh>
#undef BDD


#define RC_REALIZABLE 10
#define RC_UNREALIZABLE 20


using namespace std;


bool check_real(spot::twa_graph_ptr ucw_aut,
                uint k,
                bool is_moore,
                const vector<spot::formula>& inputs,
                const vector<spot::formula>& outputs,
                const string& output_file_name)
{
    auto k_aut = k_reduce(ucw_aut, k);

    cout << k_aut->ap().size() << endl;

    Synth synthesizer(is_moore, inputs, outputs, k_aut, output_file_name, 3600);
    bool is_realizable = synthesizer.run();
    return is_realizable;
}


int main(int argc, const char *argv[])
{
    args::ArgumentParser parser("Synthesizer from LTL (TLSF format)");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::HelpFlag help
            (parser,
             "help",
             "Display this help menu",
             {'h', "help"});

    args::Positional<string> tlsf_arg
            (parser, "tlsf",
             "File with TLSF",
             args::Options::Required);
    args::Flag is_moore_flag
            (parser,
             "moore",
             "synthesize Moore machines (by default I synthesize Mealy)",
             {"moore"});
    args::ValueFlagList<uint> k_list_arg
            (parser,
             "k",
             "the maximum number of times the same bad state can be visited"
             "(thus, it is reset between SCCs)."
             "If you provide it twice (e.g. -k 1 -k 5), then I will try all values in that range."
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

    spdlog::get("console")->info()
            << "tlsf_file: " << tlsf_file_name << ", "
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

    spot::formula neg_formula = spot::formula::Not(formula);
    spot::translator translator;
    translator.set_type(spot::postprocessor::BA);
    translator.set_pref(spot::postprocessor::SBAcc);
    translator.set_level(spot::postprocessor::Medium);  // on some examples the high optimization is the bottleneck (TODO: double-check)
    spot::twa_graph_ptr aut = translator.run(neg_formula);
    cout << aut->ap().size() << endl;  // TODO: remove me

    for (auto k: (k_list.size() == 2?
                  range(k_list[0], k_list[1]+1):
                  (k_list.size()>2? k_list: range(k_list[0], k_list[0]+1))))
    {
        auto is_realizable = check_real(aut, k, is_moore, inputs, outputs, output_file_name);
        if (is_realizable)
            return RC_REALIZABLE;
    }
    // CURRENT:
    // - I implemented the arguments for k
    // - noticed a strange thing about AP of the new automaton (it has none), and asked a question
    // NEXT:
    // - fix that strange issue with APs (no need to do any checks, so probably just remove them)
    // - implement the final tool
    return RC_UNREALIZABLE;
}

