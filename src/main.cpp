#include <iostream>
#include <string>

#include <spdlog/spdlog.h>
#include <args.hxx>

#include "utils.hpp"
#include "synthesizer.hpp"


using namespace std;
using namespace sdf;


<<<<<<< HEAD
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
=======
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
>>>>>>> dev_tlsf
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
<<<<<<< HEAD
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
=======
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
>>>>>>> dev_tlsf

    return sdf::run(check_dual_spec, tlsf_file_name, k_to_iterate, !check_real_only, output_file_name);
}

<<<<<<< HEAD

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
=======
>>>>>>> dev_tlsf
