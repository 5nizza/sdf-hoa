#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <vector>
#include <algorithm>
#include <spdlog/spdlog.h>


#define BDD spotBDD
#include <spot/twa/twagraph.hh>
#include <spot/twa/bddprint.hh>
#include <spot/twa/formula2bdd.hh>
#undef BDD


#include "Synth.hpp"


#define IS_NEGATED(l) ((l & 1) == 1)
#define STRIP_LIT(lit) (lit & ~1)
#define NEGATED(lit) (lit ^ 1)


#define L_INF(message) {spdlog::get("console")->info() << message;}


#define hmap unordered_map
typedef unsigned uint;
typedef vector<uint> VecUint;
typedef unordered_set<uint> SetUint;



BDD Synth::get_bdd_for_sign_lit(uint lit) {
    /* lit is an AIGER variable index with a 'sign' */

    if (bdd_by_aiger_unlit.find(STRIP_LIT(lit)) != bdd_by_aiger_unlit.end()) {
        BDD res = bdd_by_aiger_unlit[STRIP_LIT(lit)];
        if (IS_NEGATED(lit))
            return ~res;
    }

    uint stripped_lit = STRIP_LIT(lit);
    BDD res;

    if (stripped_lit == 0) {
        res = cudd.bddZero();
    }
    else if (aiger_is_input(aiger_spec, stripped_lit) ||
             aiger_is_latch(aiger_spec, stripped_lit)) {
        res = cudd.ReadVars(cudd_by_aiger[stripped_lit]);
//        MASSERT(res.NodeReadIndex() == stripped_lit/2, "that bug again: impossible: " << res.NodeReadIndex() << " vs " << stripped_lit/2 );
    }
    else { // aiger_and
        aiger_and *and_ = aiger_is_and(aiger_spec, stripped_lit);
        res = get_bdd_for_sign_lit(and_->rhs0) & get_bdd_for_sign_lit(and_->rhs1);
    }

    bdd_by_aiger_unlit[stripped_lit] = res;

    return IS_NEGATED(lit) ? (~res):res;
}


vector<BDD> Synth::get_controllable_vars_bdds() {
    vector<BDD> result;
    for (ulong i = inputs.size(); i < inputs_outputs.size(); ++i) {
        BDD var_bdd = cudd.bddVar(static_cast<int>(i));
        result.push_back(var_bdd);
    }
    return result;
}


vector<BDD> Synth::get_uncontrollable_vars_bdds() {
    vector<BDD> result;
    for (uint i = 0; i < inputs.size(); ++i) {
        BDD var_bdd = cudd.bddVar(i);
        result.push_back(var_bdd);
    }
    return result;
}


void Synth::introduce_error_bdd() {
    // Error BDD is true on reaching any of the accepting states.
    // Assumptions:
    // - acceptance is state-based,
    // - state is accepting => state has a self-loop with true.
    L_INF("introduce_error_bdd..");

    MASSERT(aut->is_sba() == spot::trival::yes_value, "");
    MASSERT(aut->prop_terminal() == spot::trival::yes_value, "");

    error = cudd.bddZero();
    for (auto s = 0u; s < aut->num_states(); ++s)
        if (aut->state_is_accepting(s))
            error |= cudd.bddVar(s + NOF_SIGNALS);
}


void Synth::compose_init_state_bdd() {
    L_INF("compose_init_state_bdd..");

    // Initial state is 'the latch of the initial state is 1, others are 0'
    init = cudd.bddOne();
    for (auto s = 0u; s < aut->num_states(); s++)
        if (s != aut->get_init_state_number())
            init &= ~cudd.bddVar(s + NOF_SIGNALS);
        else
            init &= cudd.bddVar(s + NOF_SIGNALS);
}


BDD bdd_from_p_name(const string& p_name,
                    const vector<string>& inputs_outputs,
                    Cudd& cudd) {
    long idx = distance(inputs_outputs.begin(),
                        find(inputs_outputs.begin(), inputs_outputs.end(), p_name));
    MASSERT(idx < (long) inputs_outputs.size(), "idx is out of range for p_name: " << p_name);
    BDD p_bdd = cudd.bddVar((int) idx);
    return p_bdd;
}


BDD translate_formula_into_cuddBDD(const spot::formula &formula,
                                   vector<string> &inputs_outputs,
                                   Cudd &cudd) {
    // formula is the sum of products (i.e., in DNF)
    if (formula.is(spot::op::Or)) {
        BDD cudd_bdd = cudd.bddZero();
        for (auto l: formula)
            cudd_bdd |= translate_formula_into_cuddBDD(l, inputs_outputs, cudd);
        return cudd_bdd;
    }

    if (formula.is(spot::op::And)) {
        BDD cudd_bdd = cudd.bddOne();
        for (auto l: formula)
            cudd_bdd &= translate_formula_into_cuddBDD(l, inputs_outputs, cudd);
        return cudd_bdd;
    }

    if (formula.is(spot::op::ap))
        return bdd_from_p_name(formula.ap_name(), inputs_outputs, cudd);

    if (formula.is(spot::op::Not)) {
        MASSERT(formula[0].is(spot::op::ap), formula);
        return ~bdd_from_p_name(formula[0].ap_name(), inputs_outputs, cudd);
    }

    if (formula.is(spot::op::tt))
        return cudd.bddOne();  // no restriction

    MASSERT(0, "unexpected type of f: " << formula);
}


void Synth::compose_transition_vector() {
    L_INF("compose_transition_vector..");

    const spot::bdd_dict_ptr& spot_bdd_dict = aut->get_dict();

    L_INF("Atomic propositions explicitly used by the automaton:");
    for (const spot::formula& ap: aut->ap())
        L_INF(' ' << ap << " (=" << spot_bdd_dict->varnum(ap) << ')');

    unordered_map<uint, vector<spot::twa_graph::edge_storage_t>> in_edges_by_state;
    for (uint s = 0; s < aut->num_states(); ++s)
        for (auto &t: aut->edges())
            if (t.dst == s)
                in_edges_by_state[s].push_back(t);

    // States are numbered from 0 to n-1.
    for (uint s = 0; s < aut->num_states(); ++s) {
        L_INF("Processing state " << s << ":");

        BDD s_transitions = cudd.bddZero();
        for (auto &t: in_edges_by_state[s]) {
            // t has src, dst, cond, acc
            L_INF("  edge: " << t.src << " -> " << t.dst << ": " << spot::bdd_to_formula(t.cond, spot_bdd_dict) << ": " << t.acc);

            BDD s_t = cudd.bddVar(t.src + NOF_SIGNALS)
                      & translate_formula_into_cuddBDD(spot::bdd_to_formula(t.cond, spot_bdd_dict), inputs_outputs, cudd);
            s_transitions |= s_t;
        }
        transition_func[s] = s_transitions;
//        cudd.DumpDot((vector<BDD>){s_transitions});
    }
}


vector<BDD> Synth::get_substitution() {
    vector<BDD> substitution;

    for (uint i = 0; i < (uint)cudd.ReadSize(); ++i) {
        if (i >= inputs_outputs.size()) // is state variable
            substitution.push_back(transition_func.find(i - (uint)inputs_outputs.size())->second);
        else
            substitution.push_back(cudd.ReadVars(i));
    }

    return substitution;
}


vector<string> &split(const string &s, char delim, vector<string> &elems) {
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}

vector<string> split(const string &s, char delim) {
    vector<string> elems;
    split(s, delim, elems);
    return elems;
}
VecUint get_order(Cudd &cudd) {
    string order_str = cudd.OrderString();
    auto str_vars = split(order_str, ' ');                                MASSERT(str_vars.size() == (uint) cudd.ReadSize(), "");
    VecUint order;
    for (auto const & str_v : str_vars) {
        order.push_back((uint) stoi(str_v.substr(1)));
    }
    return order;
}

string string_set(const SetUint& v) {
    stringstream ss;
    for(auto const el : v)
        ss << el << ",";
    return ss.str();
}

template<typename Container>
struct container_hash {
    std::size_t operator()(Container const & v) const {
        size_t hash = 0;
        for (auto const el: v)
            hash ^= el;
        return hash;
    }
};


bool do_intersect(const SetUint& s1, const SetUint& s2) {
    for (auto el1 : s1)
        if (s2.find(el1) != s2.end())
            return true;
    return false;
}


vector<SetUint>
get_group_candidates(const vector<VecUint >& orders,
                     uint window_size)
{
    hmap<SetUint, uint, container_hash<SetUint>>
            group_freq;

    for (auto const & order : orders) {
        for (uint idx=0; idx < order.size() - window_size; ++idx) {
            SetUint sub_group(order.begin() + idx,
                              order.begin() + idx + window_size);
            ++group_freq[sub_group];
        }
    }

    vector<SetUint> candidates;
    for (auto const & it: group_freq) {
        if (((float)it.second/orders.size()) >= 0.8)  // appears 'often'
            candidates.push_back(it.first);
    }
    return candidates;
}


void remove_intersecting(SetUint group,
                         vector<SetUint> &groups)
{
    vector<SetUint >::iterator it = groups.begin();
    while (it != groups.end()) {
        if (do_intersect(group, *it))
            it = groups.erase(it);
        else
            ++it;
    }
}


uint get_var_of_min_order_position(Cudd& cudd, const SetUint& group)
{
    uint min_var = *group.begin();
    uint min_pos = (uint)cudd.ReadPerm(min_var);
    for (auto const var : group)
        if ((uint )cudd.ReadPerm(var) < min_pos) {
            min_var = var;
            min_pos = (uint)cudd.ReadPerm(var);
        }
    return min_var;
}


void introduce_group_into_cudd(Cudd &cudd, const SetUint& group)
{
    L_INF("adding variable group to cudd: " << string_set(group));
    auto first_var_pos = get_var_of_min_order_position(cudd, group);
    cudd.MakeTreeNode(first_var_pos, (uint) group.size(), MTR_FIXED);
}


void _do_grouping(Cudd &cudd,
                  hmap<uint, vector<SetUint>> &groups_by_length,  // we modify its values
                  uint cur_group_length,
                  const VecUint& cur_order)
{
    L_INF("fixing groups of size " << cur_group_length << ". The number of groups = " << groups_by_length[cur_group_length].size());

    auto cur_groups = groups_by_length[cur_group_length];

    for (uint i = 0; i+cur_group_length < cur_order.size(); ++i) {
        SetUint candidate;
        for (uint j = 0; j < cur_group_length; ++j)
            candidate.insert(cur_order[i+j]);

        if (find(cur_groups.begin(), cur_groups.end(), candidate) != cur_groups.end()) {
            for (uint l = 2; l < cur_group_length; ++l) {
                remove_intersecting(candidate, groups_by_length[l]);  //remove from smaller groups
            }
            introduce_group_into_cudd(cudd, candidate);
        }
    }
}


void do_grouping(Cudd& cudd,
                 const vector<VecUint>& orders)
{
    L_INF("trying to group vars..");

    if (orders[0].size() < 5)  // window size is too big
        return;

    hmap<uint, vector<SetUint>> groups_by_length;
    groups_by_length[2] = get_group_candidates(orders, 2);
    groups_by_length[3] = get_group_candidates(orders, 3);
    groups_by_length[4] = get_group_candidates(orders, 4);
    groups_by_length[5] = get_group_candidates(orders, 5);

    L_INF("# of group candidates: of size 2 -- " << groups_by_length[2].size());
    for (auto const& g : groups_by_length[2]) {
        L_INF(string_set(g));
    }
    L_INF("# of group candidates: of size 3 -- " << groups_by_length[3].size());
    for (auto const& g : groups_by_length[3]) {
        L_INF(string_set(g));
    }
    L_INF("# of group candidates: of size 4 -- " << groups_by_length[4].size());
    L_INF("# of group candidates: of size 5 -- " << groups_by_length[5].size());

    auto cur_order = orders.back();    // we fix only groups present in the current order (because that is easier to implement)

    for (uint i = 5; i>=2; --i)  // decreasing order!
        if (!groups_by_length[i].empty())
            _do_grouping(cudd, groups_by_length, i, cur_order);
}


void update_order_if(Cudd& cudd, vector<VecUint >& orders)
{
    static uint last_nof_orderings = 0;

    if (last_nof_orderings != cudd.ReadReorderings())
        orders.push_back(get_order(cudd));

    last_nof_orderings = cudd.ReadReorderings();
}


BDD Synth::pre_sys(BDD dst) {
    /**
    Calculate predecessor states of given states.

        ∀u ∃c ∃t': tau(t,u,c,t') & dst(t') & ~error(t,u,c)

    We use the direct substitution optimization (since t' <-> BDD(t,u,c)), thus:

        ∀u ∃c: (!error(t,u,c)  &  (dst(t)[t <- bdd_next_t(t,u,c)]))

    or for Moore machines:

        ∃c ∀u: (!error(t,u,c)  &  (dst(t)[t <- bdd_next_t(t,u,c)]))

    Note that we do not replace t variables in the error bdd.

    :return: BDD of the predecessor states
    **/

    // NOTE: I tried considering two special cases: error(t,u,c) and error(t),
    //       and move error(t) outside of quantification ∀u ∃c.
    //       It slowed down..
    // TODO: try again: on the driver example

    static vector<VecUint> orders;
    static bool did_grouping = false;

    if (!did_grouping && timer.sec_from_origin() > time_limit_sec/4) { // at 0.25*time_limit we fix the order
        do_grouping(cudd, orders);
        did_grouping = true;
    }

    dst = dst.VectorCompose(get_substitution());                                update_order_if(cudd, orders);

    if (is_moore) {
        BDD result = dst.And(~error);

        vector<BDD> uncontrollable = get_uncontrollable_vars_bdds();
        if (!uncontrollable.empty()) {
            BDD uncontrollable_cube = cudd.bddComputeCube(uncontrollable.data(),
                                                          nullptr,
                                                          (int)uncontrollable.size());
            result = result.UnivAbstract(uncontrollable_cube);                  update_order_if(cudd, orders);
        }

        // ∃c ∀u  (...)
        vector<BDD> controllable = get_controllable_vars_bdds();
        BDD controllable_cube = cudd.bddComputeCube(controllable.data(),
                                                    nullptr,
                                                    (int)controllable.size());
        result = result.ExistAbstract(controllable_cube);                       update_order_if(cudd, orders);
        return result;
    }

    // the case of Mealy machines
    vector<BDD> controllable = get_controllable_vars_bdds();
    BDD controllable_cube = cudd.bddComputeCube(controllable.data(),
                                                nullptr,
                                                (int)controllable.size());
    BDD result = dst.AndAbstract(~error, controllable_cube);                   update_order_if(cudd, orders);

    vector<BDD> uncontrollable = get_uncontrollable_vars_bdds();
    if (!uncontrollable.empty()) {
        // ∀u ∃c (...)
        BDD uncontrollable_cube = cudd.bddComputeCube(uncontrollable.data(),
                                                      nullptr,
                                                      (int)uncontrollable.size());
        result = result.UnivAbstract(uncontrollable_cube);                    update_order_if(cudd, orders);
    }
    return result;
}


BDD Synth::calc_win_region() {
    /** Calculate a winning region for the safety game: win = greatest_fix_point.X [not_error & pre_sys(X)]
        :return: BDD representing the winning region
    **/

    BDD new_ = cudd.bddOne();
    for (uint i = 1; ; ++i) {                                                  L_INF("calc_win_region: iteration " << i << ": node count " << cudd.ReadNodeCount());
        BDD curr = new_;

        new_ = pre_sys(curr);

        if ((init & new_) == cudd.bddZero())
            return cudd.bddZero();

        if (new_ == curr)
            return new_;
    }
}


BDD Synth::get_nondet_strategy() {
    /**
    Get non-deterministic strategy from the winning region.
    If the system outputs controllable values that satisfy this non-deterministic strategy,
    then the system wins.
    I.e., a non-deterministic strategy describes for each state all possible plausible output values
    (below is assuming W excludes error states)

        strategy(t,u,c) = ∃t' W(t) & T(t,i,c,t') & W(t')

    But since t' <-> bdd(t,i,o), (and since we use error=error(t,u,c)), we use:

        strategy(t,u,c) = ~error(t,u,c) & W(t) & W(t)[t <- bdd_next_t(t,u,c)]

    :return: non-deterministic strategy bdd
    :note: The strategy is non-deterministic -- determinization is done later.
    **/

    L_INF("get_nondet_strategy..");

    // TODO: do we need win_region?
    return ~error & win_region & win_region.VectorCompose(get_substitution());
}


hmap<uint,BDD> Synth::extract_output_funcs() {
    /** The result vector respects the order of the controllable variables **/

    L_INF("extract_output_funcs..");

    cudd.FreeTree();    // ordering that worked for win region computation might not work here

    hmap<uint,BDD> model_by_cuddidx;

    vector<BDD> controls = get_controllable_vars_bdds();

    while (!controls.empty()) {
        BDD c = controls.back(); controls.pop_back();

        aiger_symbol *aiger_input = aiger_is_input(aiger_spec, aiger_by_cudd[c.NodeReadIndex()]);
        L_INF("getting output function for " << aiger_input->name);

        BDD c_arena;
        if (controls.size() > 0) {
            BDD cube = cudd.bddComputeCube(controls.data(), nullptr, (int)controls.size());
            c_arena = non_det_strategy.ExistAbstract(cube);
        }
        else { //no other signals left
            c_arena = non_det_strategy;
        }
        // Now we have: c_arena(t,u,c) = ∃c_others: nondet(t,u,c)
        // (i.e., c_arena talks about this particular c, about t and u)

        BDD c_can_be_true = c_arena.Cofactor(c);
        BDD c_can_be_false = c_arena.Cofactor(~c);

        BDD c_must_be_true = ~c_can_be_false & c_can_be_true;
        BDD c_must_be_false = c_can_be_false & ~c_can_be_true;
        // Note that we cannot use `c_must_be_true = ~c_can_be_false`,
        // since the negation can cause including tuples (t,i,o) that violate non_det_strategy.

        auto support_indices = cudd.SupportIndices(vector<BDD>({c_must_be_false, c_must_be_true}));
        for (auto const var_cudd_idx : support_indices) {
            auto v = cudd.ReadVars(var_cudd_idx);
            auto new_c_must_be_false = c_must_be_false.ExistAbstract(v);
            auto new_c_must_be_true = c_must_be_true.ExistAbstract(v);

            if ((new_c_must_be_false & new_c_must_be_true) == cudd.bddZero()) {
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

        //killing node refs
        c_must_be_false = c_must_be_true = c_can_be_false = c_can_be_true = c_arena = cudd.bddZero();

        //TODO: ak: strange -- the python version for the example amba_02_9n produces a smaller circuit (~5-10 times)!
        non_det_strategy = non_det_strategy.Compose(c_model, c.NodeReadIndex());
        //non_det_strategy = non_det_strategy & ((c & c_model) | (~c & ~c_model));
    }

    return model_by_cuddidx;
}


uint Synth::next_lit() {
    /* return: next possible to add to the spec literal */
    return (aiger_spec->maxvar + 1) * 2;
}


uint Synth::get_optimized_and_lit(uint a_lit, uint b_lit) {
    if (a_lit == 0 || b_lit == 0)
        return 0;

    if (a_lit == 1 && b_lit == 1)
        return 1;

    if (a_lit == 1)
        return b_lit;

    if (b_lit == 1)
        return a_lit;

    if (a_lit > 1 && b_lit > 1) {
        uint a_b_lit = next_lit();
        aiger_add_and(aiger_spec, a_b_lit, a_lit, b_lit);
        return a_b_lit;
    }

    MASSERT(0, "impossible");
}


uint Synth::walk(DdNode *a_dd) {
    /**
    Walk given DdNode node (recursively).
    If a given node requires intermediate AND gates for its representation, the function adds them.
        Literal representing given input node is `not` added to the spec.

    :returns: literal representing input node
    **/

    // caching
    static hmap<DdNode*, uint> cache;
    {
        auto cached_lit = cache.find(Cudd_Regular(a_dd));
        if (cached_lit != cache.end())
            return Cudd_IsComplement(a_dd) ? NEGATED(cached_lit->second) : cached_lit->second;
    }
    // end of caching

    if (Cudd_IsConstant(a_dd))
        return (uint) (a_dd == cudd.bddOne().getNode());  // in aiger: 0 is False and 1 is True

    // get an index of the variable
    uint a_lit = aiger_by_cudd[Cudd_NodeReadIndex(a_dd)];

    DdNode *t_bdd = Cudd_T(a_dd);
    DdNode *e_bdd = Cudd_E(a_dd);

    uint t_lit = walk(t_bdd);
    uint e_lit = walk(e_bdd);

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


void Synth::model_to_aiger(const BDD &c_signal, const BDD &func) {
    /// Update AIGER spec with a definition of `c_signal`

    uint c_lit = aiger_by_cudd[c_signal.NodeReadIndex()];
    string output_name = string(aiger_is_input(aiger_spec, c_lit)->name);  // save the name before it is freed

    uint func_as_aiger_lit = walk(func.getNode());

    aiger_redefine_input_as_and(aiger_spec, c_lit, func_as_aiger_lit, func_as_aiger_lit);

    if (print_full_model)
        aiger_add_output(aiger_spec, c_lit, output_name.c_str());
}


class Cleaner {
public:
    aiger* spec;
    Cleaner(aiger* spec): spec(spec) { }
    ~Cleaner() { aiger_reset(spec); }
};


void init_cudd(Cudd& cudd) {
//    cudd.Srandom(827464282);  // for reproducibility (?)
    cudd.AutodynEnable(CUDD_REORDER_SIFT);
//    cudd.EnableReorderingReporting();
}


bool Synth::run() {
    init_cudd(cudd);

    // create T from the spot automaton

    /* The variables order is as follows:
     * first come inputs and outputs, ordered accordingly,
     * then come states, shifted by NOF_SIGNALS (i.e., s=2 gets BDD index NOF_SIGNALS + 2) */
    vector<BDD> _tmp; // _tmp ensures that all var BDDs have positive refs (TODO: do we really need this?)
    for (uint i = 0; i < inputs.size() + outputs.size(); ++i)
        _tmp.push_back(cudd.bddVar(i));
    for (uint s = 0; s < aut->num_states(); ++s)
        _tmp.push_back(cudd.bddVar(s + NOF_SIGNALS));

    timer.sec_restart();
    compose_init_state_bdd();
    compose_transition_vector();
    introduce_error_bdd();
    L_INF("creating transition relation took (sec): " << timer.sec_restart());

    win_region = calc_win_region();
    L_INF("calc_win_region took (sec): " << timer.sec_restart());

    if (win_region.IsZero()) {
        cout << "UNREALIZABLE" << endl;
        return false;
    }

    cout << "REALIZABLE" << endl;
    return true;

    // solve the game
    // output the result

//    aiger_spec = aiger_init();
//    const char *err = aiger_open_and_read_from_file(aiger_spec, aiger_file_name.c_str());
//    MASSERT(err == nullptr, err);
//    Cleaner cleaner(aiger_spec);
//
//    // main part
//    L_INF("synthesize.. number of vars = " << aiger_spec->num_inputs + aiger_spec->num_latches);
//
//    // Create all variables. _tmp ensures that BDD have positive refs.
//    vector<BDD> _tmp;
//    for (uint i = 0; i < aiger_spec->num_inputs + aiger_spec->num_latches; ++i)
//        _tmp.push_back(cudd.bddVar(i));
//
//    for (uint i = 0; i < aiger_spec->num_inputs; ++i) {
//        auto aiger_strip_lit = aiger_spec->inputs[i].lit;
//        cudd_by_aiger[aiger_strip_lit] = i;
//        aiger_by_cudd[i] = aiger_strip_lit;
//    }
//    for (uint i = 0; i < aiger_spec->num_latches; ++i) {
//        auto aiger_strip_lit = aiger_spec->latches[i].lit;
//        auto cudd_idx = i + aiger_spec->num_inputs;
//        cudd_by_aiger[aiger_strip_lit] = cudd_idx;
//        aiger_by_cudd[cudd_idx] = aiger_strip_lit;
//    }
//
//    compose_init_state_bdd();
//    timer.sec_restart();
//    compose_transition_vector();
//    L_INF("calc_trans_rel took (sec): " << timer.sec_restart());
//    introduce_error_bdd();
//    L_INF("introduce_error_bdd took (sec): " << timer.sec_restart());
//
//    // no need for cache
//    bdd_by_aiger_unlit.clear();
//
//    timer.sec_restart();
//    win_region = calc_win_region();
//    L_INF("calc_win_region took (sec): " << timer.sec_restart());
//
//    if (win_region.IsZero()) {
//        cout << "UNREALIZABLE" << endl;
//        return 0;
//    }
//
//    cout << "REALIZABLE" << endl;
//
//    non_det_strategy = get_nondet_strategy();
//
//    //cleaning non-used bdds
//    win_region = cudd.bddZero();
//    transition_func.clear();
//    init = cudd.bddZero();
//    error = cudd.bddZero();
//    //
//
//    // TODO: set time limit on reordering? or even disable it if no time?
//    hmap<uint, BDD> model_by_cuddidx = extract_output_funcs();                   L_INF("extract_output_funcs took (sec): " << timer.sec_restart());
//
//    //cleaning non-used bdds
//    non_det_strategy = cudd.bddZero();
//    //
//
//    auto elapsed_sec = time_limit_sec - timer.sec_from_origin();
//    if (elapsed_sec > 100) {    // leave 100sec just in case
//        auto spare_time_sec = elapsed_sec - 100;
//        cudd.ResetStartTime();
//        cudd.IncreaseTimeLimit((unsigned long) (spare_time_sec * 1000));
//        cudd.ReduceHeap(CUDD_REORDER_SIFT_CONVERGE);
//        cudd.UnsetTimeLimit();
//        cudd.AutodynDisable();  // just in case -- cudd hangs on timeout
//    }
//
//    for (auto const it : model_by_cuddidx)
//        model_to_aiger(cudd.ReadVars((int)it.first), it.second);
//                                                                   L_INF("model_to_aiger took (sec): " << timer.sec_restart());
//                                                                   L_INF("circuit size: " << (aiger_spec->num_ands + aiger_spec->num_latches));
//
//    int res = 1;
//    if (output_file_name == "stdout")
//        res = aiger_write_to_file(aiger_spec, aiger_ascii_mode, stdout);
//    else if (!output_file_name.empty()) {                                       L_INF("writing a model to " << output_file_name);
//        res = aiger_open_and_write_to_file(aiger_spec, output_file_name.c_str());
//    }
//
//    MASSERT(res, "Could not write result file");
}

Synth::~Synth() {
}
