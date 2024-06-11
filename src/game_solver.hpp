#pragma once

#include <utility>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>


#define BDD spotBDD
    #include <spot/twa/twa.hh>
    #include <spot/tl/formula.hh>
#undef BDD


extern "C"
{
    #include <aiger.h>
    #include <mtr.h>
    #include <cudd.h>
}

#include <cuddObj.hh>
#include "my_assert.hpp"
#include "timer.hpp"

namespace sdf
{

/**
 * Backwards-exploration game solver using BDDs.
 */
class GameSolver
{

public:
    /// NOTE: time_limit_sec is used for heuristics (but I won't stop on reaching it)
    GameSolver(bool is_moore_,
               const std::unordered_set<spot::formula>& inputs_,
               const std::unordered_set<spot::formula>& outputs_,
               const spot::twa_graph_ptr& aut_,
               const bool do_reach_optim,
               const bool do_var_grouping_optimization,
               uint time_limit_sec_ = 3600) :
        is_moore(is_moore_),
        inputs(inputs_.begin(), inputs_.end()),
        outputs(outputs_.begin(), outputs_.end()),
        NOF_SIGNALS(inputs.size()+outputs.size()),
        aut(aut_),
        do_reach_optim(do_reach_optim),
        do_var_grouping_optimization(do_var_grouping_optimization),
        time_limit_sec(time_limit_sec_)
    {
        inputs_outputs.insert(inputs_outputs.end(), inputs.begin(), inputs.end());      // NB: inputs, not inputs_
        inputs_outputs.insert(inputs_outputs.end(), outputs.begin(), outputs.end());
    }

    /**
     * @return true iff the game realizable
     * (can be called only once!)
     */
    bool check_realizability();
    /**
     * @return model in the AIGER format if the game is realizable otherwise return NULL
     * (Beware of internal state: can be called only once!)
     */
    aiger* synthesize();

private:
    GameSolver(const GameSolver& other);
    GameSolver& operator=(const GameSolver& other);

private:
    const bool is_moore;
    const std::vector<spot::formula> inputs;             // (ordered)
    const std::vector<spot::formula> outputs;            // (ordered)
    /*const*/ std::vector<spot::formula> inputs_outputs; // (ordered)
    const uint NOF_SIGNALS;
    spot::twa_graph_ptr aut; // TODO: make const
    const bool do_reach_optim;
    const bool do_var_grouping_optimization;

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
    aiger* aiger_lib = nullptr;
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

    BDD compute_reachable(const BDD& T);

    BDD compute_monolithic_T();
};


} // namespace sdf
