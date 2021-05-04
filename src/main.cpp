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
    args::ArgumentParser parser("Synthesizer from LTL (TLSF format)");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::Positional<string> tlsf_arg
        (parser, "tlsf",
         "File with TLSF specification",
         args::Options::Required);

    args::Flag check_dual_flag
            (parser,
             "dual",
             "check the dualized spec (unrealizability)",
             {'d', "dual"});

    args::Flag check_real_only_flag
            (parser,
             "real",
             "do not extract the model (check realizability only)",
             {'r', "real"});

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
             "file name for the synthesized model",
             {'o', "output"});

    args::Flag silence_flag
            (parser,
             "s",
             "silent mode (the printed output adheres to SYNTCOMP)",
             {'s', "silent"});

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
    if (silence_flag)
        spdlog::set_level(spdlog::level::off);
    if (verbose_flag)
        spdlog::set_level(spdlog::level::debug);

    // parse args
    string tlsf_file_name(tlsf_arg.Get());
    string output_file_name(output_name ? output_name.Get() : "stdout");
    vector<uint> k_list(k_list_arg.Get());
    if (k_list.empty())
        k_list.push_back(4);
    bool check_dual_spec(check_dual_flag.Get());
    bool check_real_only(check_real_only_flag.Get());

    spdlog::get("console")->info()
            << "tlsf_file: " << tlsf_file_name << ", "
            << "check_dual_spec: " << check_dual_spec << ", "
            << "k: " << "[" << join(", ", k_list) << "], "
            << "output_file: " << output_file_name;

    vector<uint> k_to_iterate = (k_list.size() == 2?
                                 range(k_list[0], k_list[1]+1):
                                 (k_list.size()>2? k_list: range(k_list[0], k_list[0]+1)));

    return sdf::run(check_dual_spec, tlsf_file_name, k_to_iterate, !check_real_only, output_file_name);
}

