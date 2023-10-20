#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <spdlog/spdlog.h>


#define BDD spotBDD
    #include <spot/twa/twagraph.hh>
    #include <spot/twa/bddprint.hh>
    #include <spot/twa/formula2bdd.hh>
#undef BDD


#include "game_solver.hpp"
#include "utils.hpp"


extern "C" {
    #include <cuddInt.h>  // useful for debugging to access the reference count
}


#define NEGATED(lit) (lit ^ 1)


#define DEBUG(message) spdlog::get("console")->debug()<<message
#define INF(message) spdlog::get("console")->info()<<message

// TODO: use temporary object that logs time on destruction
#define log_time(message) spdlog::get("console")->info() << message << " took (sec): " << timer.sec_restart()


using namespace std;


#define hmap unordered_map
typedef vector<uint> VecUint;
typedef unordered_set<uint> SetUint;


vector<BDD> sdf::GameSolver::get_controllable_vars_bdds()
{
    vector<BDD> result;
    for (uint i = inputs.size(); i < inputs_outputs.size(); ++i)
        result.push_back(cudd.ReadVars(i));
    return result;
}


vector<BDD> sdf::GameSolver::get_uncontrollable_vars_bdds()
{
    vector<BDD> result;
    for (uint i = 0; i < inputs.size(); ++i)
        result.push_back(cudd.ReadVars(i));
    return result;
}


// This function may be currently not used but useful for debugging.
void dumpBddAsDot(Cudd& cudd, const BDD& model, const string& name)
{
    const char* inames[cudd.ReadSize()];
    string inames_str[cudd.ReadSize()];  // we have to store those strings for the time DumpDot is operating
    for (uint i = 0; i < (uint)cudd.ReadSize(); ++i)
    {
        inames_str[i] = cudd.getVariableName(i);
        inames[i] = inames_str[i].c_str();
    }

    FILE *file = fopen((name + ".dot").c_str(), "w");
    const char *onames[] = {name.c_str()};

    cudd.DumpDot((vector<BDD>) {model}, inames, onames, file);
    fclose(file);
}


void sdf::GameSolver::build_error_bdd()
{
    // Error BDD is true on reaching any of the accepting states.
    // Assumptions:
    // - acceptance is state-based,
    // - state is accepting => state has a self-loop with true.
    INF("build_error_bdd..");

    MASSERT(aut->is_sba() == spot::trival::yes_value, "is the automaton with Buchi-state acceptance?");
    MASSERT(aut->prop_terminal() == spot::trival::yes_value, "is the automaton terminal?");

    error = cudd.bddZero();
    for (auto s = 0u; s < aut->num_states(); ++s)
        if (aut->state_is_accepting(s))
            error |= cudd.bddVar(s + NOF_SIGNALS);  // NOLINT(cppcoreguidelines-narrowing-conversions)

//    dumpBddAsDot(cudd, error, "error");
}


void sdf::GameSolver::build_init_state_bdd()
{
    INF("build_init_state_bdd..");

    // Initial state is 'the latch of the initial state is 1, others are 0'
    // (there is only one initial state)
    init = cudd.bddOne();
    for (auto s = 0u; s < aut->num_states(); s++)
        if (s != aut->get_init_state_number())
            init &= ~cudd.bddVar(s + NOF_SIGNALS); // NOLINT(cppcoreguidelines-narrowing-conversions)
        else
            init &= cudd.bddVar(s + NOF_SIGNALS);  // NOLINT(cppcoreguidelines-narrowing-conversions)
}


BDD bdd_from_ap(const spot::formula& ap,
                const vector<spot::formula>& inputs_outputs,
                Cudd& cudd)
{
    long idx = distance(inputs_outputs.begin(),
                        find(inputs_outputs.begin(), inputs_outputs.end(), ap));
    MASSERT(idx < (long) inputs_outputs.size(), "idx is out of range for proposition: " << ap);
    return cudd.bddVar((int) idx);
}


BDD translate_formula_into_cuddBDD(const spot::formula& formula,
                                   vector<spot::formula>& inputs_outputs,
                                   Cudd& cudd)
{
    // formula is the sum of products (i.e., in DNF)
    if (formula.is(spot::op::Or))
    {
        BDD cudd_bdd = cudd.bddZero();
        for (auto l: formula)
            cudd_bdd |= translate_formula_into_cuddBDD(l, inputs_outputs, cudd);
        return cudd_bdd;
    }

    if (formula.is(spot::op::And))
    {
        BDD cudd_bdd = cudd.bddOne();
        for (auto l: formula)
            cudd_bdd &= translate_formula_into_cuddBDD(l, inputs_outputs, cudd);
        return cudd_bdd;
    }

    if (formula.is(spot::op::ap))
        return bdd_from_ap(formula, inputs_outputs, cudd);

    if (formula.is(spot::op::Not))
    {
        MASSERT(formula[0].is(spot::op::ap), formula);
        return ~bdd_from_ap(formula[0], inputs_outputs, cudd);
    }

    if (formula.is(spot::op::tt))
        return cudd.bddOne();  // no restriction

    MASSERT(0, "unexpected type of f: " << formula);
}

void sdf::GameSolver::build_pre_trans_func()
{
    // This function ensures: for each state, cuddIdx = state+NOF_SIGNALS
    INF("build_pre_trans_func..");

    const spot::bdd_dict_ptr& spot_bdd_dict = aut->get_dict();

    {   //debug info
        stringstream ss;
        for (const spot::formula& ap: aut->ap())
        {
            ss << ap << "(" << spot_bdd_dict->varnum(ap) << ')';
            if (ap != aut->ap().back())
                ss << ", ";
        }
        DEBUG("Atomic propositions explicitly used by the automaton (" << aut->ap().size() << "): " << ss.str());
    }

    // assumption: in the automaton, states are numbered from 0 to n-1

    hmap<uint, vector<spot::twa_graph::edge_storage_t>> in_edges_by_state;
    for (uint s = 0; s < aut->num_states(); ++s)
        for (auto &t: aut->edges())
            if (t.dst == s)
                in_edges_by_state[s].push_back(t);

    for (uint s = 0; s < aut->num_states(); ++s)
    {
        BDD s_transitions = cudd.bddZero();
        for (auto &t: in_edges_by_state[s])
        {   // t has src, dst, cond, acc
            //INF("  edge: " << t.src << " -> " << t.dst << ": " << spot::bdd_to_formula(t.cond, spot_bdd_dict) << ": " << t.acc);

            BDD s_t = cudd.ReadVars(t.src + NOF_SIGNALS)  // NOLINT(cppcoreguidelines-narrowing-conversions)
                      & translate_formula_into_cuddBDD(spot::bdd_to_formula(t.cond, spot_bdd_dict), inputs_outputs, cudd);
            s_transitions |= s_t;
        }
        pre_trans_func[s + NOF_SIGNALS] = s_transitions;

//        dumpBddAsDot(cudd, s_transitions, "s" + to_string(s));
    }
}


vector<BDD> sdf::GameSolver::get_substitution()
{
    vector<BDD> substitution;

    for (uint i = 0; i < (uint)cudd.ReadSize(); ++i)
        if (i >= inputs_outputs.size())  // is a state variable
            substitution.push_back(pre_trans_func.at(i));
        else
            substitution.push_back(cudd.ReadVars(i));

    return substitution;
}


vector<string>& split(const string &s, char delim, vector<string> &elems)
{
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim))
        elems.push_back(item);

    return elems;
}


vector<string> split(const string &s, char delim)
{
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}


VecUint get_order(Cudd &cudd)
{
    VecUint order;
    for (uint i = 0; i < (uint)cudd.ReadSize(); ++i)
        order.push_back(cudd.ReadInvPerm(i));
    return order;
}

string string_set(const SetUint& v)
{
    stringstream ss;
    for(auto const el : v)
        ss << el << ",";
    return ss.str();
}

template<typename Container>
struct container_hash
{
    std::size_t operator()(Container const & v) const
    {
        size_t hash = 0;
        for (auto const el: v)
            hash ^= el;
        return hash;
    }
};


bool do_intersect(const SetUint& s1, const SetUint& s2)
{
    for (auto el1 : s1)
        if (s2.find(el1) != s2.end())
            return true;
    return false;
}


vector<SetUint>
get_group_candidates(const vector<VecUint>& orders,
                     uint window_size)
{
    hmap<SetUint, uint, container_hash<SetUint>>
            group_freq;

    for (auto const & order : orders)
        for (uint idx=0; idx < order.size() - window_size; ++idx)
        {
            SetUint sub_group(order.begin() + idx,
                              order.begin() + idx + window_size);
            ++group_freq[sub_group];
        }

    vector<SetUint> candidates;
    for (auto const & it: group_freq)
        if (((float)it.second/orders.size()) >= 0.8)  // appears 'often'
            candidates.push_back(it.first);
    return candidates;
}


void remove_intersecting(const SetUint& group,
                         vector<SetUint>& groups)
{
    auto it = groups.begin();
    while (it != groups.end())
        if (do_intersect(group, *it))
            it = groups.erase(it);
        else
            ++it;
}


uint get_var_of_min_order_position(Cudd& cudd, const SetUint& group)
{
    uint min_var = *group.begin();
    uint min_pos = (uint)cudd.ReadPerm(min_var);
    for (auto const var : group)
        if ((uint)cudd.ReadPerm(var) < min_pos) {
            min_var = var;
            min_pos = (uint)cudd.ReadPerm(var);
        }
    return min_var;
}


void introduce_group_into_cudd(Cudd &cudd, const SetUint& group)
{
    INF("adding variable group to cudd: " << string_set(group));
    uint first_var_pos = get_var_of_min_order_position(cudd, group);
    cudd.MakeTreeNode(first_var_pos, (uint) group.size(), MTR_FIXED);
}


void _do_grouping(Cudd &cudd,
                  hmap<uint, vector<SetUint>>& groups_by_length,  // we modify its values
                  uint cur_group_length,
                  const VecUint& cur_order)
{
    INF("fixing groups of size " << cur_group_length << ". The number of groups = " << groups_by_length[cur_group_length].size());

    auto cur_groups = groups_by_length[cur_group_length];

    for (uint i = 0; i+cur_group_length < cur_order.size(); ++i)
    {
        SetUint candidate;
        for (uint j = 0; j < cur_group_length; ++j)
            candidate.insert(cur_order[i+j]);

        if (find(cur_groups.begin(), cur_groups.end(), candidate) != cur_groups.end())
        {
            for (uint l = 2; l < cur_group_length; ++l)
                remove_intersecting(candidate, groups_by_length[l]);  //remove from smaller groups
            introduce_group_into_cudd(cudd, candidate);
        }
    }
}


void do_grouping(Cudd& cudd,
                 const vector<VecUint>& orders)
{
    INF("trying to group vars..");

    if (orders.empty() || orders[0].size() < 5)  // window size is too big
        return;

    hmap<uint, vector<SetUint>> groups_by_length;
    groups_by_length[2] = get_group_candidates(orders, 2);
    groups_by_length[3] = get_group_candidates(orders, 3);
    groups_by_length[4] = get_group_candidates(orders, 4);
    groups_by_length[5] = get_group_candidates(orders, 5);

    INF("# of group candidates: of size 2 -- " << groups_by_length[2].size());
    for (auto const& g : groups_by_length[2])
        INF(string_set(g));

    INF("# of group candidates: of size 3 -- " << groups_by_length[3].size());
    for (auto const& g : groups_by_length[3])
        INF(string_set(g));

    INF("# of group candidates: of size 4 -- " << groups_by_length[4].size());
    INF("# of group candidates: of size 5 -- " << groups_by_length[5].size());

    auto cur_order = orders.back();    // we fix only groups present in the current order (because that is easier to implement)

    for (uint i = 5; i>=2; --i)  // decreasing order!
        if (!groups_by_length[i].empty())
            _do_grouping(cudd, groups_by_length, i, cur_order);
}


void update_order_if(Cudd& cudd, vector<VecUint>& orders)
{
    static uint last_nof_orderings = 0;  // FIXME: get rid of static variables!!!

    if (last_nof_orderings != cudd.ReadReorderings())
        orders.push_back(get_order(cudd));

    last_nof_orderings = cudd.ReadReorderings();
}


BDD sdf::GameSolver::pre_sys(BDD dst) {
    /**
    Calculate predecessor states of given states.

        ∀u ∃c ∃t': tau(t,u,c,t') & dst(t') & ~error(t,u,c)

    We use the direct substitution optimization (since t' <-> BDD(t,u,c)), thus:

        ∀u ∃c: (!error(t,u,c)  &  (dst(t)[t <- bdd_next_t(t,u,c)]))

    or for Moore machines:

        ∃c ∀u: (!error(t,u,c)  &  (dst(t)[t <- bdd_next_t(t,u,c)]))

    Note that we do not replace t variables in the error bdd.

    @returns: BDD of the predecessor states
    **/

    // NOTE: I tried considering two special cases: error(t,u,c) and error(t),
    //       and move error(t) outside of quantification ∀u ∃c.
    //       It slowed down..
    // TODO: try again: on the driver example

    static vector<VecUint> orders;  // FIXME: get rid of static variables!!!
    static bool did_grouping = false;

    if (!did_grouping && timer.sec_from_origin() > time_limit_sec/4)  // TODO: prove it works or remove
    {   // at 0.25*time_limit we fix the order
        do_grouping(cudd, orders);
        did_grouping = true;
    }

    dst = dst.VectorCompose(get_substitution());                            update_order_if(cudd, orders);

    if (is_moore)  // we use this for checking unrealizability (i.e. realizability by env of the dual spec)
    {
        BDD result = dst.And(~error);

        vector<BDD> uncontrollable = get_uncontrollable_vars_bdds();
        if (!uncontrollable.empty())
        {
            BDD uncontrollable_cube = cudd.bddComputeCube(uncontrollable.data(),
                                                          nullptr,
                                                          (int)uncontrollable.size());
            result = result.UnivAbstract(uncontrollable_cube);                 update_order_if(cudd, orders);
        }

        // ∃c ∀u  (...)
        vector<BDD> controllable = get_controllable_vars_bdds();
        BDD controllable_cube = cudd.bddComputeCube(controllable.data(),
                                                    nullptr,
                                                    (int)controllable.size());
        result = result.ExistAbstract(controllable_cube);                      update_order_if(cudd, orders);
        return result;
    }

    // the case of Mealy machines
    vector<BDD> controllable = get_controllable_vars_bdds();
    BDD controllable_cube = cudd.bddComputeCube(controllable.data(),
                                                nullptr,
                                                (int)controllable.size());
    BDD result = dst.AndAbstract(~error, controllable_cube);                   update_order_if(cudd, orders);

    vector<BDD> uncontrollable = get_uncontrollable_vars_bdds();
    if (!uncontrollable.empty())
    {
        // ∀u ∃c (...)
        BDD uncontrollable_cube = cudd.bddComputeCube(uncontrollable.data(),
                                                      nullptr,
                                                      (int)uncontrollable.size());
        result = result.UnivAbstract(uncontrollable_cube);                     update_order_if(cudd, orders);
    }
    return result;
}


BDD sdf::GameSolver::calc_win_region()
{
    /** Calculate the winning region of a safety game:
            win = greatest_fix_point.X [not_error & pre_sys(X)]
        :return: BDD representing the winning region
    **/

    BDD new_ = cudd.bddOne();
    for (uint i = 1; ; ++i)
    {
        INF("calc_win_region: iteration " << i << ": node count " << cudd.ReadNodeCount());

        BDD curr = new_;

        new_ = pre_sys(curr);

        if ((init & new_) == cudd.bddZero())  // intersection => we look for at least one win state from init (vs. all init states are winning); but if init describes a single state, then the same.
            return cudd.bddZero();

        if (new_ == curr)
            return new_;
    }
}


/**
 * Get non-deterministic strategy from the winning region.
 * If the system outputs controllable values that satisfy this non-deterministic strategy,
 * then the system wins.
 * I.e., a non-deterministic strategy describes for each state all possible output values
 * (below is assuming W excludes error states)
 *
 *     strategy(t,u,c) = ∃t' W(t) & T(t,i,c,t') & W(t')
 *
 * But since t' <-> bdd(t,i,o), (and since we use error=error(t,u,c)), we use:
 *
 *     strategy(t,u,c) = ~error(t,u,c) & W(t) & W(t)[t <- bdd_next_t(t,u,c)]
 *
 * @returns: non-deterministic strategy bdd
 * @note: The strategy is non-deterministic -- determinization is done later.
 */
BDD sdf::GameSolver::get_nondet_strategy()
{
    INF("get_nondet_strategy..");

    // TODO: which produces smaller circuits?
    return ~error & win_region & win_region.VectorCompose(get_substitution());
//    return ~win_region | (~error & win_region.VectorCompose(get_substitution()));
}


hmap<uint,BDD> sdf::GameSolver::extract_output_funcs()
{
    /**
     * The procedure is:
     * for i in len(controllable)-1 ... 0:
     *     c_arena = nondet_strat.ExistAbstract(controllable[0:i))
     *     c_can_be_true   = c_arena(c=true)                            // over (t,i) variables
     *     c_can_be_false  = c_arena(c=false)
     *     c_must_be_true  = c_can_be_true & ~c_can_be_false
     *     c_must_be_false = c_can_be_false & ~c_can_be_true
     *     ...
     */

    INF("extract_output_funcs..");

    cudd.FreeTree();  // ordering that worked for win region computation might not work here

    hmap<uint,BDD> model_by_cuddidx;

    vector<BDD> controls = get_controllable_vars_bdds();

    while (!controls.empty())
    {
        BDD c = controls.back(); controls.pop_back();

        auto c_name = inputs_outputs[c.NodeReadIndex()].ap_name();
        INF("extracting BDD model for " << c_name << "...");
//        dumpBddAsDot(cudd, c, c_name);

        BDD c_arena;
        if (!controls.empty())
        {
            BDD cube = cudd.bddComputeCube(controls.data(), nullptr, (int)controls.size());
            c_arena = non_det_strategy.ExistAbstract(cube);
        }
        else //no other signals left
            c_arena = non_det_strategy;
        // Now we have: c_arena(t,u,c) = ∃c_others: nondet(t,u,c)
        // (i.e., c_arena talks about this particular c, about t and u)

        BDD c_can_be_true = c_arena.Cofactor(c);
        BDD c_can_be_false = c_arena.Cofactor(~c);

        BDD c_must_be_true = ~c_can_be_false & c_can_be_true;
        BDD c_must_be_false = c_can_be_false & ~c_can_be_true;
        // Note that we cannot use `c_must_be_true = ~c_can_be_false`,
        // since the negation can cause including tuples (t,i,o) that violate non_det_strategy.

        auto support_indices = cudd.SupportIndices({c_must_be_false, c_must_be_true});
        for (auto var_cudd_idx : support_indices)
        {
            // TODO: try the specific order where for output g_i the input r_i is abstracted last
            //       while non relevant variables are abstracted first
            //       (would be cool to calculate related variables and abstract them last)
            auto v = cudd.ReadVars((int)var_cudd_idx);
            auto new_c_must_be_false = c_must_be_false.ExistAbstract(v);
            auto new_c_must_be_true = c_must_be_true.ExistAbstract(v);

            if ((new_c_must_be_false & new_c_must_be_true) == cudd.bddZero())
            {
                c_must_be_false = new_c_must_be_false;
                c_must_be_true = new_c_must_be_true;
            }
        }

        // We use 'restrict' operation, but we could also just do:
        //     c_model = care_set -> must_be_true
        // but this is (presumably) less efficient (in time? in size?).
        // (intuitively, because we always set c_model to 1 if !care_set, but we could set it to 0)
        //
        // The result of restrict operation satisfies:
        //     on c_care_set: c_must_be_true <-> must_be_true.Restrict(c_care_set)

        BDD c_model = c_must_be_true.Restrict(c_must_be_true | c_must_be_false);

        model_by_cuddidx[c.NodeReadIndex()] = c_model;

        // killing node refs: TODO: check if it affects anything
        c_must_be_false = c_must_be_true = c_can_be_false = c_can_be_true = c_arena = cudd.bddZero();

        non_det_strategy = non_det_strategy.Compose(c_model, (int)c.NodeReadIndex());
        //non_det_strategy = non_det_strategy & ((c & c_model) | (~c & ~c_model));
    }

    return model_by_cuddidx;
}


void init_cudd(Cudd& cudd)
{
//    cudd.Srandom(827464282);  // for reproducibility
    cudd.AutodynEnable(CUDD_REORDER_SIFT);
//    cudd.EnableReorderingReporting();
}


bool sdf::GameSolver::check_realizability()
{
    init_cudd(cudd);

    /* The CUDD variables index is as follows:
     * first come inputs and outputs, ordered accordingly,
     * then come variables of automaton states
     * (thus, cuddIdx = state + NOF_SIGNALS) */

    for (uint i = 0; i < inputs_outputs.size(); ++i)
    {
        cudd.bddVar(i);
        cudd.pushVariableName(inputs_outputs[i].ap_name());
    }
    for (uint s = 0; s < aut->num_states(); ++s)
    {
        cudd.bddVar(s + NOF_SIGNALS);  // NOLINT(cppcoreguidelines-narrowing-conversions)
        string name = string("s") + to_string(s);
        cudd.pushVariableName(name);
    }

    timer.sec_restart();
    build_init_state_bdd();
    build_pre_trans_func();
    build_error_bdd();
    log_time("creating transition relation");

    win_region = calc_win_region();
    log_time("calc_win_region");

    return !win_region.IsZero();
}


aiger* sdf::GameSolver::synthesize()
{
    if (!check_realizability())
        return nullptr;

    // now we have win_region and compute a nondet strategy

    cudd.AutodynDisable();  // TODO: disabling re-ordering greatly helps on some examples (load_balancer, lift(?)), but on others (prioritised_arbiter) it worsens things.

    non_det_strategy = get_nondet_strategy();

    log_time("get_nondet_strategy");

    // cleaning non-used BDDs
    init = error = win_region = cudd.bddZero();
    // we can't clear pre_trans_func bc we use it to define how latches (states) evolve in the impl

    /// FUTURE: set time limit on reordering? or even disable it if there is not enough time?
    outModel_by_cuddIdx = extract_output_funcs();
    log_time("extract_output_funcs");

    // cleaning non-used BDDs
    non_det_strategy = cudd.bddZero();
    /// FUTURE: we could also clear variables of trans_func that are not used in the model

    auto elapsed_sec = time_limit_sec - timer.sec_from_origin();
    if (elapsed_sec > 100)
    {   // leave 100sec for writing to AIGER
        auto spare_time_sec = elapsed_sec - 100;
        cudd.ResetStartTime();
        cudd.IncreaseTimeLimit((unsigned long) (spare_time_sec * 1000));
        cudd.ReduceHeap(CUDD_REORDER_SIFT_CONVERGE);
        cudd.UnsetTimeLimit();
        cudd.AutodynDisable();  // just in case -- cudd hangs on timeout
    }
    log_time("reordering before aigerizing");

    model_to_aiger();
    log_time("model_to_aiger");
    INF("circuit size: " << (aiger_lib->num_ands + aiger_lib->num_latches));

    return aiger_lib;
}


void sdf::GameSolver::model_to_aiger()
{
    aiger_lib = aiger_init();

    // aiger: add inputs
    for (uint i = 0; i < inputs.size(); ++i)  // assumes the i->i mapping of cudd indices to inputs
        aiger_add_input(aiger_lib, (aiger_by_cudd[i] = generate_lit()), inputs[i].ap_name().c_str());

    // allocate literals for latches, but their implementations will be added to AIGER later
    for (const auto& it : pre_trans_func)
        aiger_by_cudd[it.first] = generate_lit();

    // aiger: add outputs
    // Note: the models of outputs can depend on input and latch variables,
    //       but they do not depend on the output variables

    set<uint> cuddIdxStatesUsed;
    for (const auto& it : outModel_by_cuddIdx)
    {
        uint bddVarIdx = it.first;
        BDD bddModel = it.second;

        uint aigerLit = walk(bddModel.getNode(), cuddIdxStatesUsed);
        aiger_by_cudd[bddVarIdx] = aigerLit;
        auto outName = inputs_outputs[bddVarIdx].ap_name();  // hm, I don't like this implicit knowledge
        aiger_add_output(aiger_lib, aigerLit, outName.c_str());
    }

    // aiger: add latches, but only those that are referenced in the output models (+implied dependencies).
    // The latch implementations may reference inputs and outputs,
    // but for outputs they refer to their models rather than to their original variables.
    // This is ensured by using the proper values in aiger_by_cudd (we prepared aiger_by_cudd for this).

    if (cuddIdxStatesUsed.empty())
        return;  // this model is combinatorial (does not rely on states)

    set<uint> processed;
    while (!cuddIdxStatesUsed.empty())
    {
        auto stateAsCuddIdx = pop_first<uint>(cuddIdxStatesUsed);
        set<uint> new_to_process;
        uint aigerLit = aiger_by_cudd[stateAsCuddIdx];
        uint aigerLitNext = walk(pre_trans_func[stateAsCuddIdx].getNode(), new_to_process);
        aiger_add_latch(aiger_lib, aigerLit, aigerLitNext, ("state " + to_string(stateAsCuddIdx - NOF_SIGNALS)).c_str());

        processed.insert(stateAsCuddIdx);
        for (const auto &e : new_to_process)
            if (!contains(processed, e))
                cuddIdxStatesUsed.insert(e);
    }

    // By default, aiger latches are initialized to 0,
    // but the latch of the initial state has to start in 1, so ensure this:
    uint s0 = aut->get_init_state_number();
    uint s0_asCuddIdx = s0 + NOF_SIGNALS;
    uint s0_asAigerLit = aiger_by_cudd[s0_asCuddIdx];
    aiger_add_reset(aiger_lib, s0_asAigerLit, 1);
    MASSERT(contains(processed, s0_asCuddIdx), "states must depend on the initial state");
}


uint sdf::GameSolver::walk(DdNode* a_dd, set<uint>& cuddIdxStatesUsed)  // TODO: using DdNode is bad
{
    /**
    Walk given DdNode node (recursively).
    If a given node requires intermediate AND gates for its representation, this function adds them.
        Literal representing given input node is `not` added to the spec.

    :returns: literal representing input node
    **/

    auto cached_lit = cache.find(Cudd_Regular(a_dd));
    if (cached_lit != cache.end())
        return Cudd_IsComplement(a_dd) ? NEGATED(cached_lit->second) : cached_lit->second;

    if (Cudd_IsConstant(a_dd))
        return (uint) (a_dd == cudd.bddOne().getNode());  // in aiger: 0 is False and 1 is True

    uint a_dd_cuddIdx = Cudd_NodeReadIndex(a_dd);
    if (a_dd_cuddIdx >= inputs_outputs.size())  // this is a state variable
        cuddIdxStatesUsed.insert(a_dd_cuddIdx);

    MASSERT(aiger_by_cudd.find(a_dd_cuddIdx) != aiger_by_cudd.end(), "");
    uint a_lit = aiger_by_cudd[a_dd_cuddIdx];

    DdNode *t_bdd = Cudd_T(a_dd);
    DdNode *e_bdd = Cudd_E(a_dd);

    uint t_lit = walk(t_bdd, cuddIdxStatesUsed);
    uint e_lit = walk(e_bdd, cuddIdxStatesUsed);

    // ite(a_bdd, then_bdd, else_bdd)
    // = a*then + !a*else
    // = !(!(a*then) * !(!a*else))
    // -> in general case we need 3 more ANDs

    uint a_t_lit = get_optimized_and_lit(a_lit, t_lit);
    uint na_e_lit = get_optimized_and_lit(NEGATED(a_lit), e_lit);

    uint n_a_t_lit = NEGATED(a_t_lit);
    uint n_na_e_lit = NEGATED(na_e_lit);

    uint and_lit = get_optimized_and_lit(n_a_t_lit, n_na_e_lit);

    uint res = NEGATED(and_lit);

    cache[Cudd_Regular(a_dd)] = res;

    if (Cudd_IsComplement(a_dd))
        res = NEGATED(res);

    return res;
}


uint sdf::GameSolver::get_optimized_and_lit(uint a_lit, uint b_lit)
{
    if (a_lit == 0 || b_lit == 0)
        return 0;

    if (a_lit == 1 && b_lit == 1)
        return 1;

    if (a_lit == 1)
        return b_lit;

    if (b_lit == 1)
        return a_lit;

    // a_lit > 1 and b_lit > 1
    uint and_lit = generate_lit();
    aiger_add_and(aiger_lib, and_lit, a_lit, b_lit);
    return and_lit;
}
