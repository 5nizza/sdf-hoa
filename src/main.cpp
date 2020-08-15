#include <fstream>
#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <args.hxx>

#include "Synth.hpp"
#include "k_reduce.hpp"

#define BDD spotBDD
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twa/bddprint.hh>
#undef BDD


using namespace std;


vector<string> tokenize_line(uint nof_tokens_to_skip, const string& line);


void assert_do_not_intersect(vector<string> inputs, vector <string> outputs);



int main(int argc, const char *argv[]) {
    args::ArgumentParser parser("Synthesizer from UCW automata specs");
    parser.helpParams.width = 100;
    parser.helpParams.helpindent = 26;

    args::HelpFlag help(parser, "help", "Display this help menu", {'h', "help"});

    args::Positional<std::string> hoa_arg(parser, "hoa", "HOA automaton file (universal co-reachability)",
                                          args::Options::Required);
    args::Positional<std::string> signals_arg(parser,
                                              "signals",
                                              "part file with two lines \n(example: first line: `.inputs i1 i2`, second line: `.outputs o1 o2`)",
                                              args::Options::Required);
    args::ValueFlag<uint> k_arg(parser,
                                "k",
                                "the maximum number of times the same bad state can be visited (thus, it is reset between SCCs)"
                                "(default:8)",
                                {'k'},
                                8);
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
    string hoa_file_name(hoa_arg.Get());
    string signals_file_name(signals_arg.Get());
    string output_file_name(output_name ? output_name.Get() : "stdout");
    uint k(k_arg.Get());

    spdlog::get("console")->info()
            << "hoa_file: " << hoa_file_name << ", "
            << "signals_file: " << signals_file_name << ", "
            << "k: " << k << ", "
            << "output_file: " << output_file_name;

    std::ifstream signals_file(signals_file_name);
    string line_inputs, line_outputs;
    if (!getline(signals_file, line_inputs) || !getline(signals_file, line_outputs) ) {
        cout << "bad file with inputs/outputs" << endl;
        exit(1);
    }
    vector<string> inputs = tokenize_line(1, line_inputs);
    vector<string> outputs = tokenize_line(1, line_outputs);
    MASSERT(!outputs.empty(), "outputs are empty");
    assert_do_not_intersect(inputs, outputs);

    spot::parsed_aut_ptr pa = parse_aut(hoa_file_name, spot::make_bdd_dict());
    MASSERT(pa->format_errors(cerr) == 0, "error while reading HOA file");
    MASSERT(pa->aborted==0, "could not read HOA file: it is terminated with 'ABORT'");

    auto k_aut = k_reduce(pa->aut, k);

    Synth synthesizer(inputs, outputs, k_aut, output_file_name, 3600);
    bool is_realizable = synthesizer.run();

    return is_realizable? 10:20;
}


void assert_do_not_intersect(vector<string> inputs, vector <string> outputs) {
    sort(inputs.begin(), inputs.end());
    sort(outputs.begin(), outputs.end());

    vector<string> intersection;
    set_intersection(inputs.begin(), inputs.end(), outputs.begin(), outputs.end(),
                     back_inserter(intersection));

    if (!intersection.empty()) {
        cerr << "Error: inputs/outputs have common signals:" << endl;
        for (const auto& s: intersection)
            cerr << "  " << s << endl;
        MASSERT(0, "");
    }
}


vector<string> tokenize_line(uint nof_tokens_to_skip, const string& line) {
    istringstream iss(line);
    string token;
    vector<string> tokens;
    uint skipped = 0;
    while (iss >> token) {
        if (nof_tokens_to_skip > 0 && skipped++ < nof_tokens_to_skip)
            continue;
        tokens.push_back(token);
    }
    return tokens;
}
