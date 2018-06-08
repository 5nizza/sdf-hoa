#include "ltl_parser.hpp"
#include "myassert.hpp"
#include "utils.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <tuple>


#define BDD spotBDD
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#undef BDD

using namespace std;


void assert_do_not_intersect(vector<string> &inputs, vector<string> &outputs)
{
    sort(inputs.begin(), inputs.end());
    sort(outputs.begin(), outputs.end());

    vector<string> intersection;
    set_intersection(inputs.begin(), inputs.end(), outputs.begin(), outputs.end(),
                     back_inserter(intersection));

    if (!intersection.empty())
        MASSERT(0, "Error: inputs/outputs have common signals:" + join(" ", intersection));
}


string
to_str(int rc, const string& out, const string& err)
{
    stringstream ss;
    ss << "rc: " << rc << endl
       << "out: \n" << out << endl
       << "err: \n" << err << endl;
    return ss.str();
}


tuple<spot::formula, vector<spot::formula>, vector<spot::formula>>
parse_tlsf(const string& tlsf_file_name)
{
    int rc;
    string out, err;

    tie(rc, out, err) = execute("syfco -f ltlxba -m fully " + tlsf_file_name);
    MASSERT(rc == 0 && err.empty(),
            "syfco exited with non-zero status or non-empty stderr: " + to_str(rc, out, err));

    auto parsed_formula = spot::parse_infix_psl(out);
    if (!parsed_formula.errors.empty())
    {
        stringstream ss;
        parsed_formula.format_errors(ss);
        MASSERT(0, "Could not parse the formula: " << out << endl << ss.str());
    }

    tie(rc, out, err) = execute("syfco -ins " + tlsf_file_name);
    MASSERT(rc == 0 && err.empty(), "syfco exited with non-zero status or non-empty stderr: " << to_str(rc, out, err));
    auto str_inputs = split_by_space(substituteAll(out, ",", " "));

    tie(rc, out, err) = execute("syfco -outs " + tlsf_file_name);
    MASSERT(rc == 0 && err.empty(),
            "syfco exited with non-zero status or non-empty stderr: " + to_str(rc, out, err));
    auto str_outputs = split_by_space(substituteAll(out, ",", " "));

    assert_do_not_intersect(str_inputs, str_outputs);

    vector<spot::formula> aps;
    parsed_formula.f.traverse(
            [&aps](spot::formula f)
            {
                if (f.is(spot::op::ap))
                    aps.push_back(f);
                return false;  // always continue traversing
            });

    // NOTE: syfco lowers all signal names in props
    // The check below ensures that all APs appearing in the formula are from inputs or outputs
    // (that also ensures that the above note does not inflict)

    vector<spot::formula> inputs, outputs;
    for (const auto& ap: aps)
    {
        if (contains(str_inputs, ap.ap_name()))
            inputs.push_back(ap);
        else if (contains(str_outputs, ap.ap_name()))
            outputs.push_back(ap);
        else
            MASSERT(0, "unknown AP: " << ap);
    }

    return tuple<spot::formula, vector<spot::formula>, vector<spot::formula>>(parsed_formula.f, inputs, outputs);
}






















