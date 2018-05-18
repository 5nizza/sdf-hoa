#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

#include "Synth.hpp"
#include "ArgsParser.hpp"


using namespace std;


void print_usage_exit() {
    cout << "<tool> <input_aiger_file> [-f] [-v] [-moore] [-h]" << endl
         << "-f      you want a full model" << endl
         << "        (with outputs defined along with the `bad` output)" << endl
         << "-moore  synthesize Moore machines (by default I synthesize Mealy)" << endl
         << "-v      verbose output (default silent)" << endl
         << "-h      this help message" << endl;
    exit(0);
}


int main(int argc, char *argv[]) {
    ArgsParser parser(argc, argv, 2);
    if (ArgsParser(argc, argv, 1).cmdOptionExists("-h") || argc < 2)
        print_usage_exit();

    // setup logging
    auto console = spdlog::stdout_logger_mt("console", false);
    spdlog::set_pattern("%H:%M:%S %v ");
    if (!parser.cmdOptionExists("-v"))
        spdlog::set_level(spdlog::level::off);
    // end of logger setup

    string input_file_name = argv[1];
    bool print_full_model = parser.cmdOptionExists("-f");
    string output_file_name = parser.cmdOptionExists("-o") ? parser.getCmdOption("-o") : string("stdout");
    bool is_moore = parser.cmdOptionExists("-moore");
    spdlog::get("console")->info()
            << "input_file: " << input_file_name << ", "
            << "is_moore: " << is_moore << ", "
            << "full_model: " << print_full_model << ", "
            << "output_file: " << output_file_name;

    Synth synthesizer(is_moore, input_file_name, output_file_name, print_full_model, 3600);
    bool is_realizable = synthesizer.run();

    return is_realizable? 10:20;
}
