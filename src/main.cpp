#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <spdlog/spdlog.h>

#include "Synth.hpp"
#include "ArgsParser.hpp"

#define BDD spotBDD
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twa/bddprint.hh>
#undef BDD


using namespace std;


vector<string> tokenize_line(const string& line);


void print_usage_exit() {
    cout << endl
         << "<tool>  <hoa_file> -s <inputs_outputs_file> [-f] [-v] [-moore] [-o] [-h]" << endl << endl
         << "-s      file with space-separated signals into inputs/outputs (first line: inputs, second line: outputs)" << endl
         << "-f      you want a full model" << endl
         << "        (with outputs defined along with the `bad` output)" << endl
         << "-moore  synthesize Moore machines (by default I synthesize Mealy)" << endl
         << "-o      output file for a model" << endl
         << "-v      verbose output (default silent)" << endl
         << "-h      this help message" << endl;
    exit(0);
}


int main(int argc, char *argv[]) {
    ArgsParser parser(argc, argv, 2);
    if (parser.cmdOptionExists("-h") || argc < 4 || !parser.cmdOptionExists("-s"))
        print_usage_exit();

    // setup logging
    auto console = spdlog::stdout_logger_mt("console", false);
    spdlog::set_pattern("%H:%M:%S %v ");
    if (!parser.cmdOptionExists("-v"))
        spdlog::set_level(spdlog::level::off);

    // parse args
    auto hoa_file_name = string(argv[1]);
    auto to_print_full_model = parser.cmdOptionExists("-f");
    auto signals_file_name = parser.getCmdOption("-s");
    auto output_file_name = parser.cmdOptionExists("-o") ? parser.getCmdOption("-o") : string("stdout");
    auto is_moore = parser.cmdOptionExists("-moore");

    spdlog::get("console")->info()
            << "hoa_file: " << hoa_file_name << ", "
            << "signals_file: " << signals_file_name << ", "
            << "is_moore: " << is_moore << ", "
            << "full_model: " << to_print_full_model << ", "
            << "output_file: " << output_file_name;

    spot::parsed_aut_ptr pa = parse_aut(hoa_file_name, spot::make_bdd_dict());
    MASSERT(pa->format_errors(cerr) == 0, "error while reading HOA file");
    MASSERT (pa->aborted==0, "could not read HOA file: it is terminated with 'ABORT'");

    std::ifstream signals_file(signals_file_name);
    string line_inputs, line_outputs;
    if (!getline(signals_file, line_inputs) || !getline(signals_file, line_outputs) ) {
        cout << "bad file with inputs/outputs" << endl;
        exit(1);
    }
    vector<string> inputs = tokenize_line(line_inputs);
    vector<string> outputs = tokenize_line(line_outputs);

    Synth synthesizer(is_moore, inputs, outputs, pa->aut, output_file_name, to_print_full_model, 3600);
    bool is_realizable = synthesizer.run();
    return is_realizable? 10:20;
}

vector<string> tokenize_line(const string& line) {
    istringstream iss(line);
    string token;
    vector<string> tokens;
    while (iss >> token)
        tokens.push_back(token);
    return tokens;
}
