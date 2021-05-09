#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <args.hxx>

#include "utils.hpp"
#include "synthesizer.hpp"
#include "reg_reduction.hpp"


using namespace std;
using namespace sdf;


// TODO: this is ugly!
#define DEBUG(message) spdlog::get("console")->debug()<<message
#define INF(message) spdlog::get("console")->info()<<message


int main(int argc, const char *argv[])
{
    auto logger = spdlog::stdout_logger_mt("console", false);
    logger->set_pattern("%H:%M:%S %v ");

    args::ArgumentParser parser("Register-Transducer Synthesizer from reg-UCW (in HOA format)");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::Positional<string> hoa_arg
        (parser, "hoa",
         "File with HOA specification",
         args::Options::Required);

    args::Flag check_real_only_flag
            (parser,
             "real",
             "do not extract the model (check realizability only)",
             {'r', "real"});

    args::ValueFlag<uint> b_uint_arg
            (parser,
             "b",
             "the bound on the number of transducer registers (default: 2)",
             {'b'},
             2);

    args::ValueFlagList<uint> k_list_arg
            (parser,
             "k",
             "the maximum number of times the same bad state can be visited "
             "(thus, it is reset between SCCs). "
             "If you provide it twice (e.g. -k 1 -k 5), then I will try all values in that range. "
             "Default: 4.",
             {'k'},
             {4});

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
    string hoa_file_name(hoa_arg.Get());
    string output_file_name(output_name ? output_name.Get() : "stdout");
    uint b(b_uint_arg.Get());
    vector<uint> k_list(k_list_arg.Get());
    bool check_real_only(check_real_only_flag.Get());

    INF("hoa_file: " << hoa_file_name << ", " <<
        "b (bound on # of registers): " << ", " <<
        "k: " << "[" << join(", ", k_list) << "], " <<
        "output_file: " << output_file_name);

    vector<uint> k_to_iterate = (k_list.size() == 2?
                                 range(k_list[0], k_list[1]+1):
                                 (k_list.size()>2? k_list: range(k_list[0], k_list[0]+1)));


    return sdf::run(hoa_file_name, k_to_iterate, !check_real_only, output_file_name);
}

