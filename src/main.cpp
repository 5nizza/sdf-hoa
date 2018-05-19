#include <fstream>
#include <iostream>

#include <spdlog/spdlog.h>

#include "Synth.hpp"
#include "ArgsParser.hpp"

#define BDD spotBDD
#include <spot/parseaut/public.hh>
#include <spot/twaalgos/hoa.hh>
#include <spot/twa/bddprint.hh>
#undef BDD


using namespace std;


void print_usage_exit() {
    cout << endl
         << "<tool>  <hoa_file> [-f] [-v] [-moore] [-o] [-h]" << endl << endl
         << "-f      you want a full model" << endl
         << "        (with outputs defined along with the `bad` output)" << endl
         << "-moore  synthesize Moore machines (by default I synthesize Mealy)" << endl
         << "-o      output file for a model" << endl
         << "-v      verbose output (default silent)" << endl
         << "-h      this help message" << endl;
    exit(0);
}


void custom_print(std::ostream& out, spot::twa_graph_ptr& aut);


int main(int argc, char *argv[]) {
    ArgsParser parser(argc, argv, 2);
    if (ArgsParser(argc, argv, 1).cmdOptionExists("-h") || argc < 2)
        print_usage_exit();

    // setup logging
    auto console = spdlog::stdout_logger_mt("console", false);
    spdlog::set_pattern("%H:%M:%S %v ");
    if (!parser.cmdOptionExists("-v"))
        spdlog::set_level(spdlog::level::off);

    // parse args
    auto hoa_file_name = string(argv[1]);
    auto to_print_full_model = parser.cmdOptionExists("-f");
    auto output_file_name = parser.cmdOptionExists("-o") ? parser.getCmdOption("-o") : string("stdout");
    auto is_moore = parser.cmdOptionExists("-moore");

    spdlog::get("console")->info()
            << "hoa_file: " << hoa_file_name << ", "
            << "is_moore: " << is_moore << ", "
            << "full_model: " << to_print_full_model << ", "
            << "output_file: " << output_file_name;

    spot::parsed_aut_ptr pa = parse_aut(hoa_file_name, spot::make_bdd_dict());
    MASSERT(pa->format_errors(std::cerr) == 0, "error while reading HOA file");
    MASSERT (pa->aborted==0, "could not read HOA file: it is terminated with 'ABORT'");

    vector<string> inputs = {"r1", "r2"};   // TODO
    vector<string> outputs = {"g1", "g2"};       // TODO

    Synth synthesizer(is_moore, inputs, outputs, pa->aut, output_file_name, to_print_full_model, 3600);
    bool is_realizable = synthesizer.run();
    return is_realizable? 10:20;
}


void custom_print(std::ostream& out, spot::twa_graph_ptr& aut)
{
    // We need the dictionary to print the BDDs that label the edges
    const spot::bdd_dict_ptr& dict = aut->get_dict();

    // Some meta-data...
    out << "Acceptance: " << aut->get_acceptance() << '\n';
    out << "Number of sets: " << aut->num_sets() << '\n';
    out << "Number of states: " << aut->num_states() << '\n';
    out << "Number of edges: " << aut->num_edges() << '\n';
    out << "Initial state: " << aut->get_init_state_number() << '\n';
    out << "Atomic propositions:";
    for (spot::formula ap: aut->ap())
        out << ' ' << ap << " (=" << dict->varnum(ap) << ')';
    out << '\n';

    // Arbitrary data can be attached to automata, by giving them
    // a type and a name.  The HOA parser and printer both use the
    // "automaton-name" to name the automaton.
    if (auto name = aut->get_named_prop<std::string>("automaton-name"))
        out << "Name: " << *name << '\n';

    // For the following prop_*() methods, the return value is an
    // instance of the spot::trival class that can represent
    // yes/maybe/no.  These properties correspond to bits stored in the
    // automaton, so they can be queried in constant time.  They are
    // only set whenever they can be determined at a cheap cost: for
    // instance an algorithm that always produces deterministic automata
    // would set the deterministic property on its output.  In this
    // example, the properties that are set come from the "properties:"
    // line of the input file.
    out << "Complete: " << aut->prop_complete() << '\n';
    out << "Deterministic: " << (aut->prop_universal()
                                 && aut->is_existential()) << '\n';
    out << "Unambiguous: " << aut->prop_unambiguous() << '\n';
    out << "State-Based Acc: " << aut->prop_state_acc() << '\n';
    out << "Terminal: " << aut->prop_terminal() << '\n';
    out << "Weak: " << aut->prop_weak() << '\n';
    out << "Inherently Weak: " << aut->prop_inherently_weak() << '\n';
    out << "Stutter Invariant: " << aut->prop_stutter_invariant() << '\n';

    // States are numbered from 0 to n-1
    unsigned n = aut->num_states();
    for (unsigned s = 0; s < n; ++s)
    {
        out << "State " << s << ":\n";

        // The out(s) method returns a fake container that can be
        // iterated over as if the contents was the edges going
        // out of s.  Each of these edges is a quadruplet
        // (src,dst,cond,acc).  Note that because this returns
        // a reference, the edge can also be modified.
        for (auto& t: aut->out(s))
        {
            out << "  edge(" << t.src << " -> " << t.dst << ")\n    label = ";
            spot::bdd_print_formula(out, dict, t.cond);
            out << "\n    acc sets = " << t.acc << '\n';
        }
    }
}


