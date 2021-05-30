#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <args.hxx>

#include "utils.hpp"
#include "synthesizer.hpp"
#include "reg_reduction.hpp"
#include "ehoa_parser.hpp"

#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
#undef BDD


using namespace std;
using namespace sdf;


// TODO: this is ugly!
#define DEBUG(message) spdlog::get("console")->debug()<<message
#define INF(message) spdlog::get("console")->info()<<message


int main(int argc, const char *argv[])
{
    auto logger = spdlog::get("console");

    args::ArgumentParser parser("Reductor from reg-UCW to classical UCW (in HOA format)");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::Positional<string> hoa_arg
        (parser, "hoa",
         "File with HOA specification",
         args::Options::Required);

    args::ValueFlag<uint> b_uint_arg
            (parser,
             "b",
             "the bound on the number of transducer registers (default: 2)",
             {'b'},
             2);

    args::ValueFlag<string> output_hoa_name
            (parser,
             "o",
             "file name for the output SHOA automaton",
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
        logger->set_level(spdlog::level::off);
    if (verbose_flag)
        logger->set_level(spdlog::level::debug);

    // parse args
    string hoa_file_name(hoa_arg.Get());
    string output_file_name(output_hoa_name ? output_hoa_name.Get() : "stdout");
    uint b(b_uint_arg.Get());

   INF("hoa_file: " << hoa_file_name << ", " <<
       "b (bound on # of registers): " << ", " <<
       "output_hoa_file: " << output_file_name);

    set<spot::formula> inputs, outputs;
    spot::twa_graph_ptr reg_ucw;
    bool is_moore;
    tie(reg_ucw, inputs, outputs, is_moore) = read_ehoa(hoa_file_name);

    spot::twa_graph_ptr classical_ucw;
    std::set<spot::formula> sysTst;
    std::set<spot::formula> sysAsgn;
    std::set<spot::formula> sysR;

    reduce(reg_ucw, b, classical_ucw, sysTst, sysAsgn, sysR);

    {   // debug
        ofstream fs("classical.dot");
        MASSERT(fs.is_open(), "");
        spot::print_dot(fs, classical_ucw);
        fs.close();

        stringstream ss;
        spot::print_dot(ss, classical_ucw);
//        spot::print_dot(ss, reg_ucw);
        DEBUG("\n" << ss.str());
    }

    // TODO: save SHOA into file
}

