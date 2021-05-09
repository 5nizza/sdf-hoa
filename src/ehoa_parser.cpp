#include "ehoa_parser.hpp"
#include "my_assert.hpp"
#include "utils.hpp"
#include <iostream>
#include <algorithm>
#include <tuple>
#include <set>


#define BDD spotBDD
#include <spot/tl/formula.hh>
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/hoa.hh>
#undef BDD


using namespace std;
using namespace sdf;


// helpers
vector<string> get_controllable_APs(const string& hoa_file_name);
bool get_synt_moore_flag(const string& hoa_file_name);
//


std::tuple<spot::twa_graph_ptr, std::set<spot::formula>, std::set<spot::formula>, bool>
sdf::read_ehoa(const string& hoa_file_name)
{
    // the parser assumes that the hoa file contains the additional lines:
    // "controllable-AP: 1 2 3"  <-- this lines describes the indices of controllable APs
    // "synt-moore: true" [optional] <-- describes if we need to synthesise Moore machines (also: "true" can be "false")
    std::set<spot::formula> inputs, outputs;
    spot::twa_graph_ptr aut;
    bool synt_moore;
    //

    spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    spot::parsed_aut_ptr pa = parse_aut(hoa_file_name, dict);
    { MASSERT(!pa->aborted, ""); stringstream ss; if (pa->format_errors(ss)) MASSERT(0, ss.str()) }
    aut = pa->aut;

    vector<string> controllableAPs = get_controllable_APs(hoa_file_name);
    for (auto& ap : aut->ap())
        if (contains(controllableAPs, ap.ap_name()))
            outputs.insert(ap);
        else
            inputs.insert(ap);

    synt_moore = get_synt_moore_flag(hoa_file_name);

    return make_tuple(aut, inputs, outputs, synt_moore);
}

vector<string> get_APs(const string& hoa_file_name)
{
    // find the line and turn it into vector:
    // AP: 4 "g_0" "r_0" "g_1" "r_1"
    // the result respects the original order
    ifstream f(hoa_file_name);
    vector<string> AP_names;
    for (string l; getline(f, l);) {
        l = trim_spaces(l);
        if (l.find("AP") == 0)
        {
            auto tokens = split_by_space(l);
            // the tokens are "AP:", "4", '"g_0"', etc.
            MASSERT(tokens.size()>2, "names must be present");

            auto remove_quotes = [](const string& e){return substituteAll(e, "\"", "");};
            transform(tokens.begin()+2, tokens.end(),
                      back_inserter(AP_names), remove_quotes);
            return AP_names;
        }
    }
    MASSERT(0, "unreachable");
}

vector<string> get_controllable_APs(const string& hoa_file_name)
{
    vector<string> APs = get_APs(hoa_file_name);

    vector<string> controllableAPs;
    ifstream f(hoa_file_name);
    for (string l; getline(f, l);)
    {
        l = trim_spaces(l);
        if (l.find("controllable-AP") == 0)
        {
            auto tokens = split_by_space(l);  // tokens are: "controllable-AP:", "0", "2", etc.
            for (auto it = tokens.begin() + 1; it != tokens.end(); ++it)
                controllableAPs.push_back(APs[stoi(*it)]);
            return controllableAPs;
        }
    }
    MASSERT(0, "unreachable");
}

bool get_synt_moore_flag(const string& hoa_file_name)
{
    ifstream f(hoa_file_name);
    for (string l; getline(f, l);)
    {
        l = trim_spaces(l);
        if (l.find("synt-moore") == 0)
        {
            auto tokens = split_by_space(l);
            MASSERT(tokens.size() == 2, "expected the line 'synt-moore: value'");

            if (tokens[1] == "false")
                return false;
            else if (tokens[1] == "true")
                return true;
            else
                MASSERT(0, "");
        }
    }
    return false;  // default is false
}
