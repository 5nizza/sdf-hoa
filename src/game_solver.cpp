#include <iostream>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <set>   // for variable-order optimization only


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


// TODO: use spdlog's stopwatch?
#define log_time(message) spdlog::info("{} took (sec): {}", message, timer.sec_restart());


using namespace std;


#define hmap unordered_map




// ---------------------------------------------------------------------------------------------------------------------
// Group-ordering optimization

typedef vector<uint> VecUint;
typedef unordered_set<uint> SetUint;



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
    spdlog::debug("adding variable group to cudd: {}", string_set(group));
    uint first_var_pos = get_var_of_min_order_position(cudd, group);
    cudd.MakeTreeNode(first_var_pos, (uint) group.size(), MTR_FIXED);
}


void _do_grouping(Cudd &cudd,
                  hmap<uint, vector<SetUint>>& groups_by_length,  // we modify its values
                  uint cur_group_length,
                  const VecUint& cur_order)
{
    spdlog::debug("fixing groups of size {}. The number of groups = {}.",
                 cur_group_length, groups_by_length[cur_group_length].size());

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
    spdlog::debug("trying to group vars..");

    if (orders.empty() || orders[0].size() < 5)  // window size is too big
        return;

    hmap<uint, vector<SetUint>> groups_by_length;
    groups_by_length[2] = get_group_candidates(orders, 2);
    groups_by_length[3] = get_group_candidates(orders, 3);
    groups_by_length[4] = get_group_candidates(orders, 4);
    groups_by_length[5] = get_group_candidates(orders, 5);

    spdlog::debug("# of group candidates: of size 2 -- ", groups_by_length[2].size());
    for (auto const& g : groups_by_length[2])
        spdlog::debug(string_set(g));

    spdlog::debug("# of group candidates: of size 3 -- {}", groups_by_length[3].size());
    for (auto const& g : groups_by_length[3])
        spdlog::debug(string_set(g));

    spdlog::debug("# of group candidates: of size 4 -- ", groups_by_length[4].size());
    spdlog::debug("# of group candidates: of size 5 -- ", groups_by_length[5].size());

    auto cur_order = orders.back();    // we fix only groups present in the current order (because that is easier to implement)

    for (uint i = 5; i>=2; --i)  // decreasing order!
        if (!groups_by_length[i].empty())
            _do_grouping(cudd, groups_by_length, i, cur_order);
}
// ---------------------------------------------------------------------------------------------------------------------



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
    spdlog::info("build_error_bdd..");

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
    spdlog::info("build_init_state_bdd..");

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
    spdlog::info("build_pre_trans_func..");

    const spot::bdd_dict_ptr& spot_bdd_dict = aut->get_dict();

    {   //debug info
        stringstream ss;
        for (const spot::formula& ap: aut->ap())
        {
            ss << ap << "(" << spot_bdd_dict->varnum(ap) << ')';
            if (ap != aut->ap().back())
                ss << ", ";
        }
        spdlog::debug("Atomic propositions explicitly used by the automaton ({}):{}",
                      aut->ap().size(), ss.str());
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


BDD sdf::GameSolver::pre_sys(BDD dst)
{
    //TODO: add support for empty controllable (model checking)

    /**
    Calculate predecessor states of given states.

        ∀u ∃c: dst(t)[t <- bdd_pred(t,u,c)] & !error(t,u,c)

    or for Moore machines:

        ∃c ∀u: dst(t)[t <- bdd_pred(t,u,c)] & !error(t,u,c)

    @returns: BDD of the predecessor states
    **/

    // TODO: I tried considering two special cases: error(t,u,c) and error(t),
    //       and move error(t) outside of quantification ∀u ∃c.
    //       (this should be equivalent to is_moore case)
    //       It slowed down...
    //       Properly evaluate.

    dst = dst.VectorCompose(get_substitution());

    if (is_moore)  // we use this for checking unrealizability (i.e. realizability by env of the dual spec)
    {
        // TODO: use AndAbstract (and some negations)
        BDD result = dst.And(~error);

        vector<BDD> uncontrollable = get_uncontrollable_vars_bdds();
        if (!uncontrollable.empty())
        {
            BDD uncontrollable_cube = cudd.bddComputeCube(uncontrollable.data(),
                                                          nullptr,
                                                          (int)uncontrollable.size());
            result = result.UnivAbstract(uncontrollable_cube);
        }

        // ∃c ∀u  (...)
        vector<BDD> controllable = get_controllable_vars_bdds();
        BDD controllable_cube = cudd.bddComputeCube(controllable.data(),
                                                    nullptr,
                                                    (int)controllable.size());
        result = result.ExistAbstract(controllable_cube);
        return result;
    }

    // the case of Mealy machines
    vector<BDD> controllable = get_controllable_vars_bdds();
    BDD controllable_cube = cudd.bddComputeCube(controllable.data(),
                                                nullptr,
                                                (int)controllable.size());
    BDD result = dst.AndAbstract(~error, controllable_cube);

    vector<BDD> uncontrollable = get_uncontrollable_vars_bdds();
    if (!uncontrollable.empty())
    {
        // ∀u ∃c (...)
        BDD uncontrollable_cube = cudd.bddComputeCube(uncontrollable.data(),
                                                      nullptr,
                                                      (int)uncontrollable.size());
        result = result.UnivAbstract(uncontrollable_cube);
    }
    return result;
}


BDD sdf::GameSolver::calc_win_region()
{
    /** Calculate the winning region of a safety game:
            win = greatest_fix_point.X [not_error & pre_sys(X)]
        :return: BDD representing the winning region
    **/

    // -----------------------------------------------------------------------------------------------------------------
    // Group-ordering optimization
    vector<VecUint> orders;
    bool did_grouping = false;
    uint last_nof_orderings = 0;
    struct Cleaner { Cudd& cudd; explicit Cleaner(Cudd& cudd):cudd(cudd) {} ~Cleaner(){ cudd.FreeTree(); }};
    Cleaner cleaner(cudd);  // remove any restrictions on the order
    // -----------------------------------------------------------------------------------------------------------------

    BDD new_ = cudd.bddOne();
    for (uint i = 1; ; ++i)
    {
        spdlog::info("calc_win_region: iteration {}: node count {}",
                     i, cudd.ReadNodeCount());

        BDD curr = new_;

        new_ = pre_sys(curr);

        if ((init & new_) == cudd.bddZero())  // intersection => we look for at least one win state from init (vs. all init states are winning); but if init describes a single state, then the same.
            return cudd.bddZero();

        if (new_ == curr)
            return new_;

        // -------------------------------------------------------------------------------------------------------------
        // Group-ordering optimization

        if (do_var_grouping_optimization and !did_grouping)
        {
            // update orders
            if (last_nof_orderings != cudd.ReadReorderings())
            {
                spdlog::debug("update orders");
                orders.push_back(get_order(cudd));
                last_nof_orderings = cudd.ReadReorderings();
            }

            // fix the order
            if (timer.sec_from_origin() > time_limit_sec/4)
            {   // at 0.25*time_limit we fix the order
                do_grouping(cudd, orders);
                did_grouping = true;
            }
        }
        // -------------------------------------------------------------------------------------------------------------
    }
}

/**
 * Get nondeterministic strategy from the winning region.
 * A nondet strategy satisfies:
 * ∀t∈W.∀i.∀o: (t,i,o)∈nondet <-> Succ(t,i,o)∈W & (t,i,o)⊨¬error
 * A nondeterministic strategy is not unique, thanks to the restriction of the above property to W.
 * We compute it as
 *
 *     strategy(t,i,o) = ¬error(t,i,o) & W(t) & W(t)[t <- bdd_next_t(t,i,o)]
 *
 * @returns: nondeterministic strategy bdd
 */
BDD sdf::GameSolver::get_nondet_strategy()
{
    spdlog::info("get_nondet_strategy..");

    // this means:
    // every (x,i,o) such that x in W, from x the (i,o)-transition is safe and leads to W
    // (note that win_region.VectorCompose(get_substitution()) contains exactly all (x,i,o) such that from x the (i,o)-transition leads to W)
    return win_region & ~error & win_region.VectorCompose(get_substitution());
    // the above version seems (properly evaluate?) to yield smaller BDDs than the versions below
    // return (~error & win_region.VectorCompose(get_substitution()));
    // return (~error & win_region.VectorCompose(get_substitution())).Restrict(win_region);
}


BDD extract_one_func_via_squeeze(const Cudd& cudd, BDD& can_be_true, BDD& can_be_false)
{
    BDD c_must_be_true = ~can_be_false & can_be_true;
//    BDD c_must_be_false = ~can_be_true & can_be_false;

//    auto support_indicesF = cudd.SupportIndices(vector<BDD>{c_must_be_false});
//    auto support_indicesT = cudd.SupportIndices(vector<BDD>{c_must_be_true});

//    auto siF = set<uint>(support_indicesF.begin(), support_indicesF.end());
//    auto siT = set<uint>(support_indicesT.begin(), support_indicesT.end());

//    vector<uint> diff;
//    set_symmetric_difference(siF.begin(), siF.end(), siT.begin(), siT.end(), std::back_inserter(diff));
//    spdlog::info("number of variables present in one but not in the other: {}", diff.size());
//    copy(diff.begin(), diff.end(), std::ostream_iterator<uint>(std::cout, " "));
//    cout << endl;

    // Hm, I don't think it is worth? On two examples, it showed no benefit.
//    for (auto new_var_cudd_idx : diff)
//    {
//        auto new_v = cudd.ReadVars((int)new_var_cudd_idx);
//        c_must_be_false = c_must_be_false.ExistAbstract(new_v);
//        c_must_be_true = c_must_be_true.ExistAbstract(new_v);
//        can_be_true = can_be_true.ExistAbstract(new_v);
//    }

    // Note that we cannot use `c_must_be_true = ~c_can_be_false`,
    // since the negation can cause including tuples (t,i,o) that violate non_det_strategy.

    can_be_false = cudd.bddZero();
//    c_must_be_false = cudd.bddZero();

    return c_must_be_true.Squeeze(can_be_true);
}


BDD extract_one_func_via_abstraction(const Cudd& cudd, BDD& can_be_true, BDD& can_be_false, const BDD& reachable)
{
    BDD c_must_be_true = ~can_be_false & can_be_true;
    BDD c_must_be_false = can_be_false & ~can_be_true;
    // Note that we cannot use `c_must_be_true = ~c_can_be_false`,
    // since the negation can cause including tuples (t,i,o) that violate non_det_strategy.

    // Note: having reachable=bddOne is equivalent to having no reachability optimization

    can_be_false = can_be_true = cudd.bddZero();  // killing node refs

    // we now optimize c_must_be_true

    // NOTE: The following optimization takes a lot of time.
    //       It does reduce circuit size, but does it worth the increased time?
    auto support_indices = cudd.SupportIndices({c_must_be_false, c_must_be_true});

    auto vars_abstracted_away = vector<uint>();

    // Are some orders of variable abstraction better than others?
    // The following order is based on hypothesis that larger-index variables play less important role:
    // usually, the larger the automaton state the closer it is to the rejecting state,
    // whereas the inputs have the smallest BDD indices.
    // This is an heuristics. There are better ways for finding automata states that likely do not affect the output.
    std::sort(support_indices.begin(), support_indices.end(), std::greater<>());  // on abcg_arbiter, this shows substantial reduction (without -a)

    for (auto var_cudd_idx : support_indices)
    {
        auto v = cudd.ReadVars((int)var_cudd_idx);

        auto new_c_must_be_false = c_must_be_false.ExistAbstract(v);
        auto new_c_must_be_true = c_must_be_true.ExistAbstract(v);

        if (((new_c_must_be_false & new_c_must_be_true) & reachable) == cudd.bddZero())
        {
            c_must_be_false = new_c_must_be_false;
            c_must_be_true = new_c_must_be_true;
            vars_abstracted_away.push_back(var_cudd_idx);
        }
    }

    spdlog::info("extract model: the variables were quantified away: {}", sdf::join(", ", vars_abstracted_away));

    return c_must_be_true.Restrict((c_must_be_true | c_must_be_false) & reachable);
}

BDD sdf::GameSolver::compute_monolithic_T()
{
    spdlog::info("compute_monolithic_T: computing monolithic T(x,i,o,x')...");
    auto start_time = timer.sec_from_origin();

    auto getCuddIdxState       = [&](int s) { return (int)(NOF_SIGNALS + s); };
    auto getCuddIdxPrimedState = [&](int s) { return (int)(NOF_SIGNALS + aut->num_states() + s); };

    BDD T = cudd.bddOne();
    for (auto s = 0; s < (int)aut->num_states(); ++s)
    {
        T &= ~(cudd.bddVar(getCuddIdxPrimedState(s)) ^ pre_trans_func.at(getCuddIdxState(s)));

        string name = string("s'") + to_string(s);  // we create a name for a newly created BDD variable for the primed state variable
        cudd.pushVariableName(name);
    }

    spdlog::info("compute_monolithic_T took (sec.): {}", timer.sec_from_origin() - start_time);

    return T;
}

BDD sdf::GameSolver::compute_reachable(const BDD& T)
{
    // 1. build the transition relation T(x,i,o,x'),
    // 2. compute the set of reachable states.
    // Unfortunately, T(x,i,o,x') is one large monolithic relation and uses the primed variables.
    // (I don't know how to compute reachable states without using the primed variables and monolithic T.)

    auto start_time_sec = timer.sec_from_origin();
    spdlog::info("compute_reachable...");

    auto getCuddIdxState       = [&](int s) { return (int)(NOF_SIGNALS + s); };
    auto getCuddIdxPrimedState = [&](int s) { return (int)(NOF_SIGNALS + aut->num_states() + s); };

    vector<BDD> states;
    vector<BDD> primedStates;
    for (auto s = 0; s < (int)aut->num_states(); ++s)
    {
        states.push_back(cudd.bddVar(getCuddIdxState(s)));
        primedStates.push_back(cudd.bddVar(getCuddIdxPrimedState(s)));
    }

    vector<BDD> inp_out_state_vars = a_union_b(states, a_union_b(get_controllable_vars_bdds(), get_uncontrollable_vars_bdds()));
    BDD varsCube_to_quantify = cudd.bddComputeCube(inp_out_state_vars.data(), nullptr, (int)inp_out_state_vars.size());

    // dumpBddAsDot(cudd, varsCube_to_quantify, "cube_to_quantify");

    BDD reach = init;
    for (auto i = 0; ; ++i)
    {
        spdlog::info("compute_reachable: iteration {}: node count: {}", i, cudd.ReadNodeCount());
        // dumpBddAsDot(cudd, reach, "reach" + to_string(i));

        BDD reach_prev = reach;
        reach |= reach.AndAbstract(T, varsCube_to_quantify).SwapVariables(states, primedStates);  // SwapVariables results in unpriming of the state variables in the bdd

        if (reach == cudd.bddOne())
            spdlog::info("compute_reachable: EVERYTHING is reachable");

        if (reach_prev == reach)
        {
            spdlog::info("compute_reachability took (sec): {}", timer.sec_from_origin() - start_time_sec);
            return reach;
        }
    }
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

    spdlog::info("extract_output_funcs..");

    hmap<uint,BDD> model_by_cuddidx;

    vector<BDD> controls = get_controllable_vars_bdds();

    auto reachable = cudd.bddOne();
    if (do_reach_optim)
    {
        auto was_reordering_disabled = not cudd.ReorderingStatus(nullptr);

        cudd.AutodynEnable(CUDD_REORDER_SAME);

        auto T = compute_monolithic_T() & non_det_strategy & ~error;
        reachable = compute_reachable(T);

        if (was_reordering_disabled)
            cudd.AutodynDisable();
    }

    // the order of concretisation substantially affects the circuit size,
    // but how to choose a good order is unclear

    /*
    std::sort(controls.begin(), controls.end(),
              [&](const BDD& c1, const BDD& c2)
              {
                  return inputs_outputs[c1.NodeReadIndex()].ap_name() < inputs_outputs[c2.NodeReadIndex()].ap_name();
              });
    */

    while (!controls.empty())
    {
        BDD c = controls.back(); controls.pop_back();

        auto c_name = inputs_outputs[c.NodeReadIndex()].ap_name();
        spdlog::info("extracting BDD model for {}...", c_name);
        // dumpBddAsDot(cudd, c, c_name);

        BDD c_arena;
        if (!controls.empty())
        {
            BDD others_cube = cudd.bddComputeCube(controls.data(), nullptr, (int)controls.size());
            c_arena = non_det_strategy.ExistAbstract(others_cube);
        }
        else //no other signals left
            c_arena = non_det_strategy;

        // Now we have: c_arena(t,u,c) = ∃c_others: nondet(t,u,c)
        // (i.e., c_arena talks about this particular c, about t and u)

        BDD c_can_be_true = c_arena.Cofactor(c);
        BDD c_can_be_false = c_arena.Cofactor(~c);

        c_arena = cudd.bddZero();  // killing node refs

//        BDD c_model = extract_one_func_via_squeeze(cudd, c_can_be_true, c_can_be_false);

        BDD c_model = extract_one_func_via_abstraction(cudd, c_can_be_true, c_can_be_false, reachable);
        c_can_be_true = c_can_be_false = cudd.bddZero();  // killing refs if they weren't killed before

        model_by_cuddidx[c.NodeReadIndex()] = c_model;

        non_det_strategy = non_det_strategy.Compose(c_model, (int)c.NodeReadIndex());

        // Note: we could re-compute the set of reachable states after each concretisation, but
        // 1. it is expensive
        // 2. does not seem to yield substantial circuit reduction
        //
        // if (do_reach_optim)
        // {
        //     T = T.Compose(c_model, (int)c.NodeReadIndex());
        //     reachable = compute_reachable(T);  // the reachable set shrinks as we concretize output functions
        // }
    }

    return model_by_cuddidx;
}


void init_cudd(Cudd& cudd)
{
    cudd.Srandom(827464282);  // for reproducibility
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
        cudd.bddVar(i);  // NOLINT(*-narrowing-conversions)
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
    {
        spdlog::info("BDD node count after check_realizability: {}", cudd.ReadNodeCount());
        return nullptr;
    }

    // now we have win_region and compute a nondet strategy

    cudd.AutodynDisable();  // disabling re-ordering greatly helps on some examples (arbiter, load_balancer), but on others (prioritised_arbiter) it worsens things.

    non_det_strategy = get_nondet_strategy();    // note: this introduces a really lot of BDD nodes
    log_time("get_nondet_strategy");
    spdlog::info("BDD node count after get_nondet_strategy: {}", cudd.ReadNodeCount());

    // cleaning non-used BDDs
    win_region = cudd.bddZero();
    if (!do_reach_optim)
    {
        init = cudd.bddZero();
        error = cudd.bddZero();
    }

    // note: pre_trans_func is needed to define how latches evolve in the impl (anyway, pre_trans_func is small compared to output functions)

    outModel_by_cuddIdx = extract_output_funcs();
    log_time("extract_output_funcs");
    spdlog::info("BDD node count after extract_output_funcs: {}", cudd.ReadNodeCount());

    // cleaning non-used BDDs
    non_det_strategy = cudd.bddZero();
    init = error = cudd.bddZero();

    spdlog::info("BDD node count of det strategy: {}", cudd.ReadNodeCount());
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
    spdlog::info("BDD node count of det strategy after reordering: {}", cudd.ReadNodeCount());

    model_to_aiger();
    log_time("model_to_aiger");
    spdlog::info("circuit size: {}", (aiger_lib->num_ands + aiger_lib->num_latches));

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
