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


#define hmap unordered_map
#define hset unordered_set


// TODO: this is ugly!
#define DEBUG(message) spdlog::get("console")->debug()<<message
#define INF(message) spdlog::get("console")->info()<<message



vector<uint> get_hoaIDs_of_outputs(const spot::twa_graph_ptr& atm,
                                   const set<spot::formula>& outputs);


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

    args::Group group(parser, "SPOT postprocessing (default none):", args::Group::Validators::AtMostOne);
    args::Flag ppm(group, "ppm", "Medium postprocessing", {"ppm"});
    args::Flag pph(group, "pph", "High postprocessing", {"pph"});

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
    const string STDOUT = "stdout";
    string output_file_name(output_hoa_name ? output_hoa_name.Get() : STDOUT);
    uint b(b_uint_arg.Get());

    INF("hoa_file: " << hoa_file_name << ", " <<
        "b (bound on # of registers): " << b << ", " <<
        "output_hoa_file: " << output_file_name);

    auto [reg_ucw, inputs, outputs, is_moore] = read_ehoa(hoa_file_name);
    INF("input automaton: nof_states = " << reg_ucw->num_states() << ", nof_edges = " << reg_ucw->num_edges());

    auto [classical_ucw, sysTst, sysAsgn, sysOutR] = reduce(reg_ucw, b);
    DEBUG("completed: unprocessed: nof_states = " << classical_ucw->num_states() << ", nof_edges = " << classical_ucw->num_edges());

    classical_ucw->merge_edges();
    classical_ucw->merge_states();
    DEBUG("merged states and edges: nof_states = " << classical_ucw->num_states() << ", nof_edges = " << classical_ucw->num_edges());

    // we still need postprocessing to ensure BA and SBAcc
    spot::postprocessor post;
    post.set_type(spot::postprocessor::BA);
    post.set_pref(spot::postprocessor::SBAcc);
    DEBUG("postprocessing: level " << (pph ? "high" : (ppm ? "medium" : "low")));
    post.set_level(pph? spot::postprocessor::High : (ppm ? spot::postprocessor::Medium : spot::postprocessor::Low));
    // since SPOT may remove unnecessary APs, we copy the original ones
    auto result_atm = post.run(classical_ucw);
    result_atm->copy_ap_of(classical_ucw);
    if (!pph and !ppm) // other optimizations can remove states
        result_atm->copy_state_names_from(classical_ucw);
    INF("result: nof_states = " << result_atm->num_states() << ", nof_edges = " << result_atm->num_edges());

    INF("outputting to " << output_file_name);
    if (output_file_name == STDOUT)
        spot::print_dot(cout, result_atm);
    else
    {
        ofstream fs(output_file_name); MASSERT(fs.is_open(), "");

        if (output_file_name.substr(output_file_name.length() - 4, 4) == ".dot")
            spot::print_dot(fs, result_atm);
        else
        {
            stringstream ss;
            spot::print_hoa(ss, result_atm);

            for (string l; getline(ss, l);)
            {
                if (l == "--BODY--")  // inject 'controllable-AP' before
                    fs << CONTROLLABLE_AP_TKN << ": "
                       << join(" ", get_hoaIDs_of_outputs(result_atm,
                                                          a_union_b(outputs, sysAsgn, sysOutR)))
                       << endl;
                fs << l << endl;
            }
        }
        fs.close();
    }
    return 0;
}


hmap<int, uint> compute_spotID_to_hoaID(const spot::twa_graph_ptr& atm)
{
    // this code was taken from spot `print_hoa`
    hmap<int, uint> spotID_to_hoaID;
    auto last_hoaID = 0;
    for (auto all = atm->ap_vars(); all != bddtrue; all = bdd_high(all))
    {
        auto spotID = bdd_var(all);
        spotID_to_hoaID.insert({spotID, last_hoaID});
        ++last_hoaID;
    }
    return spotID_to_hoaID;
}

vector<uint> get_hoaIDs_of_outputs(const spot::twa_graph_ptr& atm,
                                   const set<spot::formula>& outputs)
{
    vector<uint> list_of_hoaIDs;
    for (const auto& spot_hoa : compute_spotID_to_hoaID(atm))
    {
        auto f = atm->get_dict()->bdd_map[spot_hoa.first].f;
        if (contains(outputs, f))
            list_of_hoaIDs.push_back(spot_hoa.second);
    }
    return list_of_hoaIDs;
}












































