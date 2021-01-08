//
// Created by ayrat on yestoday.
//

#pragma once

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>


#define BDD spotBDD
    #include <spot/twa/twa.hh>
    #include <spot/tl/formula.hh>
#undef BDD


extern "C" {
#include <aiger.h>
#include <mtr.h>
#include <cudd.h>
};

#include <cuddObj.hh>
#include "my_assert.hpp"
#include "timer.hpp"

namespace sdf
{

class GameSolver
{

public:
    /// NOTE: time_limit_sec is used for heuristics (but I won't stop on reaching it)
    GameSolver(bool is_moore_,
               const std::set<spot::formula>& inputs_,
               const std::set<spot::formula>& outputs_,
               spot::twa_graph_ptr& aut_,
               uint time_limit_sec_ = 3600) :
        is_moore(is_moore_),
        inputs(inputs_.begin(), inputs_.end()),
        outputs(outputs_.begin(), outputs_.end()),
        NOF_SIGNALS(inputs.size()+outputs.size()),
        aut(aut_),
        time_limit_sec(time_limit_sec_)
    {
        inputs_outputs.insert(inputs_outputs.end(), inputs.begin(), inputs.end());
        inputs_outputs.insert(inputs_outputs.end(), outputs.begin(), outputs.end());
    }

    /**
     * @return true iff the game realizable
     * (can be called only once!)
     */
    bool check_realizability();
    /**
     * @return model in in the AIGER format if the game is realizable otherwise return NULL
     * (Beware of internal state: can be called only once!)
     */
    aiger* synthesize();

private:
    GameSolver(const GameSolver& other);
    GameSolver& operator=(const GameSolver& other);

private:
    const bool is_moore;
    const std::vector<spot::formula> inputs;  // I use vector instead of set bcz I need ordered variables
    const std::vector<spot::formula> outputs; // (ordered)
    /*const*/ std::vector<spot::formula> inputs_outputs; // (ordered)
    const uint NOF_SIGNALS;
    spot::twa_graph_ptr aut; // TODO: make const

    const uint time_limit_sec;

private:
    Timer timer;
    Cudd cudd;

    std::unordered_map<uint, BDD> pre_trans_func;  // cudd variable index -> BDD (Note: cuddIdx = state + NOF_SIGNALS)
    BDD init;
    BDD error;
    BDD win_region;
    BDD non_det_strategy;
    std::unordered_map<uint, BDD> outModel_by_cuddIdx;

private:
    aiger* aiger_lib = nullptr;  // TODO: don't keep it in the internal state
    uint next_lit = 2;  // next available literal to be used in aiger; incremented by 2
    std::unordered_map<uint, uint> aiger_by_cudd;   // mapping from cudd indexes to aiger stripped literals
    std::unordered_map<DdNode*, uint> cache;        // used in AIGER model construction


private:
    std::vector<BDD> get_controllable_vars_bdds();
    std::vector<BDD> get_uncontrollable_vars_bdds();

    void build_error_bdd();

    void build_init_state_bdd();

    void build_pre_trans_func();

    BDD pre_sys(BDD dst);  // also ensures that error is not violated

    BDD pre_sys2(BDD src);

    BDD calc_win_region();

    BDD get_nondet_strategy();

    std::unordered_map<uint, BDD> extract_output_funcs();

    std::vector<BDD> get_substitution();

    uint walk(DdNode *a_dd, std::set<uint>&);

    uint get_optimized_and_lit(uint a_lit, uint b_lit);

    void model_to_aiger();

    uint generate_lit()
    {
        auto curr = next_lit;
        next_lit += 2;
        return curr;
    }
};


} // namespace sdf
