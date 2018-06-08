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


using namespace std;


int main(int argc, const char *argv[]) {
    args::ArgumentParser parser("Synthesizer from LTL (TLSF format)");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

    args::Positional<std::string> tlsf_arg(parser, "tlsf",
                                           "File with TLSF",
                                           args::Options::Required);
    args::ValueFlag<uint> k_arg(parser,
                                "k",
                                "the maximum number of times the same bad state can be visited (thus, it is reset between SCCs)"
                                "(default:8)",
                                {'k'},
                                8);
    args::Flag is_moore_flag(parser, "moore", "synthesize Moore machines (by default I synthesize Mealy)", {"moore"});
    args::Flag verbose_flag(parser, "v", "verbose mode (the default is silent)", {'v'});

    args::ValueFlag<string> output_name(parser,
                                        "o",
                                        "the output file name (not yet supported)",
                                        {'o'});

    try {
        parser.ParseCLI(argc, argv);
    }
    catch (args::Help&) {
        std::cout << parser;
        return 0;
    }
    catch (args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    catch (args::ValidationError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
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
    uint k(k_arg.Get());
    bool is_moore(is_moore_flag? true:false);

    spdlog::get("console")->info()
            << "tlsf_file: " << tlsf_file_name << ", "
            << "k: " << k << ", "
            << "is_moore: " << is_moore << ", "
            << "output_file: " << output_file_name;

    spot::formula formula;
    vector<spot::formula> inputs, outputs;
    tie(formula, inputs, outputs) = parse_tlsf(tlsf_file_name);

    cout << "formula: " << formula << endl;
    cout << "inputs: " << join(", ", inputs) << endl;
    cout << "outputs: " << join(", ", outputs) << endl;

    spot::formula neg_formula = spot::formula::Not(formula);
    spot::translator translator;
    translator.set_type(spot::postprocessor::BA);
    translator.set_pref(spot::postprocessor::SBAcc);
    translator.set_level(spot::postprocessor::Medium);  // on some examples the high optimization is the bottleneck (TODO: double-check)
    spot::twa_graph_ptr aut = translator.run(neg_formula);

    auto k_aut = k_reduce(aut, k);

    Synth synthesizer(is_moore, inputs, outputs, k_aut, output_file_name, 3600);
    bool is_realizable = synthesizer.run();

    return is_realizable? 10:20;
}

