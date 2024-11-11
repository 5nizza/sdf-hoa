#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <args.hxx>

#include "utils.hpp"
#include "synthesizer.hpp"


using namespace std;
using namespace sdf;


int main(int argc, const char *argv[])
{
    args::ArgumentParser parser("Generate QCIR/BDD for strategy-determinization task for synthesis from LTL (in TLSF format); also generate BDD/AIGER model if requested.");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::Positional<string> tlsf_arg
        (parser, "tlsf",
         "File with TLSF specification",
         args::Options::Required);

    args::Flag generate_only_flag
            (parser,
             "real",
             "only generate QBF problems; do not determinize the strategy myself",
             {'r', "real"});

    args::ValueFlagList<uint> k_list_arg
            (parser,
             "k",
             "the maximum number of times the same bad state can be visited "
             "(thus, it is reset between SCCs). "
             "If you provide it several times (e.g. -k 1 -k 4 -k 8), then all those values will be tried, in that order. "
             "Default: 4.",
             {'k'},
             {4});

    args::ValueFlag<string> output_name
            (parser,
             "o",
             "file name prefix for output QCIR/AIGER files",
             {'o', "output"});

    args::Flag verbose_flag
            (parser,
             "v",
             "verbose mode (default: informational)",
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
    spdlog::set_pattern("%H:%M:%S %v ");
    if (verbose_flag)
        spdlog::set_level(spdlog::level::debug);

    // parse args
    string tlsf_file_name(tlsf_arg.Get());
    string output_file_name(output_name ? output_name.Get() : "stdout");
    vector<uint> k_list(k_list_arg.Get());

    bool generate_only(generate_only_flag.Get());

    spdlog::info("tlsf_file: {}, generate_only: {}, k: {}, output_file_prefix: {}",
                 tlsf_file_name, generate_only, join(", ", k_list), output_file_name);

    return sdf::run_tlsf(SpecDescr(false, tlsf_file_name, !generate_only, false, false, output_file_name),
                         k_list);
}

