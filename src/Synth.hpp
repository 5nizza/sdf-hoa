//
// Created by ayrat on yestoday.
//

#ifndef SDF_SYNTH_H
#define SDF_SYNTH_H


#include <vector>
#include <string>
#include <unordered_map>


#define BDD spotBDD
#include <spot/twa/twa.hh>
#include <spot/tl/formula.hh>
#undef BDD


extern "C" {
//#include <aiger.h>
#include <mtr.h>
#include <cudd.h>
};

#include <cuddObj.hh>
#include "myassert.hpp"
#include "Timer.hpp"


class Synth {

public:
    /// NOTE: time_limit_sec is used for heuristics (but I won't stop on reaching it)
    Synth(bool is_moore_,
          const std::vector<spot::formula>& inputs_,
          const std::vector<spot::formula>& outputs_,
          spot::twa_graph_ptr &aut_,
          const std::string &output_file_name_,
          unsigned time_limit_sec_=3600):
            is_moore(is_moore_),
            inputs(inputs_),
            outputs(outputs_),
            aut(aut_),
            output_file_name(output_file_name_),
            time_limit_sec(time_limit_sec_) {
        inputs_outputs.insert(inputs_outputs.end(), inputs.begin(), inputs.end());
        inputs_outputs.insert(inputs_outputs.end(), outputs.begin(), outputs.end());
        NOF_SIGNALS = (uint)inputs_outputs.size();
    }

    bool run();  // -> returns 'is realizable'
    ~Synth();


private:
    Synth(const Synth &other);
    Synth &operator=(const Synth &other);


private:
    const bool is_moore;
    const std::vector<spot::formula> inputs;
    const std::vector<spot::formula> outputs;
    std::vector<spot::formula> inputs_outputs;
    uint NOF_SIGNALS;
    spot::twa_graph_ptr aut;

    const std::string output_file_name;
    const uint time_limit_sec;


private:
    Timer timer;
    Cudd cudd;
//    aiger *aiger_spec;

    std::unordered_map<unsigned, BDD> transition_func;       // automaton state (without the shift) to BDD
    BDD init;
    BDD error;
    BDD win_region;
    BDD non_det_strategy;

    std::unordered_map<unsigned, BDD> bdd_by_aiger_unlit;   // this is specially for amba2match benchmarks

    std::unordered_map<unsigned, unsigned> cudd_by_aiger;   // mapping from aiger stripped literals to cudd indexes
    std::unordered_map<unsigned, unsigned> aiger_by_cudd;   // mapping from cudd indexes to aiger stripped literals


private:
    std::vector<BDD> get_controllable_vars_bdds();
    std::vector<BDD> get_uncontrollable_vars_bdds();

    void introduce_error_bdd();

    void compose_init_state_bdd();

    void compose_transition_vector();

    BDD pre_sys(BDD dst);  // also ensures that error is not violated

    BDD calc_win_region();

    BDD get_nondet_strategy();

    std::unordered_map<unsigned, BDD> extract_output_funcs();

    std::vector<BDD> get_substitution();
};


#endif //SDF_SYNTH_H
