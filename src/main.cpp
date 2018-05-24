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


vector<string> tokenize_line(uint nof_tokens_to_skip, const string& line);


void _assert_do_not_intersect(vector<string> inputs, vector <string> outputs);

void print_usage_exit() {
    cout << endl
         << "<tool>  <hoa_file> -s <inputs_outputs_file> [-f] [-v] [-moore] [-o] [-h]" << endl << endl
         << "-s      part file with two lines (example: first line: `.inputs i1 i2`, second line: `.outputs o1 o2`)" << endl
         << "-moore  synthesize Moore machines (by default I synthesize Mealy)" << endl
         << "-o      output file for a model (not yet supported)" << endl
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
    auto signals_file_name = parser.getCmdOption("-s");
    auto output_file_name = parser.cmdOptionExists("-o") ? parser.getCmdOption("-o") : string("stdout");
    auto is_moore = parser.cmdOptionExists("-moore");

    spdlog::get("console")->info()
            << "hoa_file: " << hoa_file_name << ", "
            << "signals_file: " << signals_file_name << ", "
            << "is_moore: " << is_moore << ", "
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
    _assert_do_not_intersect(inputs, outputs);

    spot::parsed_aut_ptr pa = parse_aut(hoa_file_name, spot::make_bdd_dict());
    MASSERT(pa->format_errors(cerr) == 0, "error while reading HOA file");
    MASSERT (pa->aborted==0, "could not read HOA file: it is terminated with 'ABORT'");

    Synth synthesizer(is_moore, inputs, outputs, pa->aut, output_file_name, 3600);
    bool is_realizable = synthesizer.run();
    return is_realizable? 10:20;
}


void _assert_do_not_intersect(vector<string> inputs, vector <string> outputs) {

    sort(inputs.begin(), inputs.end());
    sort(outputs.begin(), outputs.end());

    vector<string> intersection;
    set_intersection(inputs.begin(), inputs.end(), outputs.begin(), outputs.end(),
                     back_inserter(intersection));

    if (!intersection.empty()) {
        cerr << "Error: inputs/outputs have common signals:" << endl;
        for (auto s: intersection)
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
