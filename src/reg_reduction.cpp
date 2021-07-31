#include "reg_reduction.hpp"

#include "ehoa_parser.hpp"
#include "utils.hpp"
#include "partition_tst_asgn.hpp"
#include "graph.hpp"
#include "graph_algo.hpp"


#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
    #include <spot/twa/formula2bdd.hh>
#undef BDD


using namespace std;
using namespace sdf;
using GA = graph::GraphAlgo;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}  // NOLINT(bugprone-macro-parentheses)
#define INF(message)   {spdlog::get("console")->info()  << message;}  // NOLINT(bugprone-macro-parentheses)

#define hmap unordered_map
#define hset unordered_set

static const string TST_CMP_TOKENS[] = {"=","≠","<",">","≤","≥"};  // NOLINT

/*
 * A test atom has the form:
 *   atom := in◇rX | out◇rX | in◇out
 * where ◇ is one of =,<,>,≠,≤,≥
 */
bool is_atm_tst(const string& name)
{
    for (const auto& tok : TST_CMP_TOKENS)  // NOLINT
        // we have to use `find` because some of the tokens consist of multiple chars
        if (name.find(tok) != string::npos)
            return true;

    return false;
}

bool is_i_atm_tst(const string& name)
{
    return is_atm_tst(name) &&
           name.find("in") != string::npos &&
           name.find("out") == string::npos;
}

bool is_o_atm_tst(const string& name)
{
    return is_atm_tst(name) &&
           name.find("in") == string::npos &&
           name.find("out") != string::npos;
}

bool is_io_atm_tst(const string& name)
{
    return is_atm_tst(name) &&
           name.find("in") != string::npos &&
           name.find("out") != string::npos;
}

/* Assignments have form in↓rX | out↓rX */
bool is_atm_asgn(const string& name)
{
    return name.find("↓") != string::npos;
}

bool is_i_atm_asgn(const string& name)
{
    return is_atm_asgn(name) &&
           name.find("in") != string::npos;
}

bool is_o_atm_asgn(const string& name)
{
    return is_atm_asgn(name) &&
           name.find("out") != string::npos;
}


set<spot::formula> get_non_actionAPs_from(const spot::twa_graph_ptr& reg_atm)
{
    set<spot::formula> non_regAPs;
    for (const auto& ap : reg_atm->ap())
        if (!is_atm_tst(ap.ap_name()) and !is_atm_asgn(ap.ap_name()))
            non_regAPs.insert(ap);
    return non_regAPs;
}


// TODO: assumes one data input and one data output
// APs naming convention for system tst/asgn/R
spot::formula ctor_sys_tst_greater_r(const string& r) { return spot::formula::ap("*>" + r); }
spot::formula ctor_sys_tst_smaller_r(const string& r) { return spot::formula::ap("*<" + r); }
spot::formula ctor_sys_tst_equal_r(const string& r)   { return spot::formula::ap("*=" + r); }
spot::formula ctor_sys_asgn_r(const string& r)        { return spot::formula::ap("↓" + r); }
spot::formula ctor_sys_out_r(const string& r)         { return spot::formula::ap("↑" + r); }


void introduce_sysActionAPs(const hset<string>& sysR,
                            spot::twa_graph_ptr& classical_ucw,
                            set<spot::formula>& sysTst,
                            set<spot::formula>& sysAsgn,
                            set<spot::formula>& sysOutR)
{
    // create variables for sysTst, sysAsgn, R
    for (const auto& r : sysR)
    {
        // sysTst variables for register r (TODO: exactly one of them must hold)

        classical_ucw->register_ap(ctor_sys_tst_greater_r(r));
        classical_ucw->register_ap(ctor_sys_tst_smaller_r(r));
        classical_ucw->register_ap(ctor_sys_tst_equal_r(r));

        sysTst.insert(ctor_sys_tst_greater_r(r));
        sysTst.insert(ctor_sys_tst_equal_r(r));
        sysTst.insert(ctor_sys_tst_smaller_r(r));

        // sysAsgn
        classical_ucw->register_ap(ctor_sys_asgn_r(r));
        sysAsgn.insert(ctor_sys_asgn_r(r));

        // vars to output r in R: introduce |R| many of vars, later require MUTEXCL (TODO: MUTEXCL)
        classical_ucw->register_ap(ctor_sys_out_r(r));
        sysOutR.insert(ctor_sys_out_r(r));
    }
}


hset<string> build_sysR(uint nof_sys_regs)
{
    hset<string> result;
    for (uint i = 1; i<= nof_sys_regs; ++i)
        result.insert("r" + to_string(i));
    return result;
}


bool is_input_name(const string& name) { return name[0] == 'i'; }
bool is_reg_name(const string& name) { return name[0] == 'r'; }


/* Extract t1,t2,◇ from  t1◇t2 */
tuple<string,string,string>
parse_tst(const string& action_name)
{
    MASSERT(is_atm_tst(action_name), action_name);

    uint index_cmp = 0;
    size_t pos_cmp;
    for (; index_cmp < sizeof TST_CMP_TOKENS; ++index_cmp)
        if ((pos_cmp = action_name.find(TST_CMP_TOKENS[index_cmp])) != string::npos)
            break;

    auto cmp = TST_CMP_TOKENS[index_cmp];
    auto t1 = trim_spaces(action_name.substr(0, pos_cmp));
    auto t2 = trim_spaces(action_name.substr(pos_cmp+cmp.length(), string::npos));

    return tuple(t1,t2,cmp);
}

/* Returns pair <variable_name, register> */
pair<string, string> parse_asgn_atom(const string& asgn_atom)
{
    // Examples: "in↓r1", "out ↓ r2"
    auto trimmed_action_name = trim_spaces(asgn_atom);
    auto i = trimmed_action_name.find("↓");
    auto var_name = trim_spaces(trimmed_action_name.substr(0, i));
    auto reg = trim_spaces(trimmed_action_name.substr(i + string("↓").length(), string::npos));
    MASSERT(reg[0] == 'r', reg);
    return make_pair(var_name, reg);
}

hset<string> build_atmR(const spot::twa_graph_ptr& reg_atm)
{
    hset<string> result;
    for (const auto& ap : reg_atm->ap())
    {
        const auto& ap_name = ap.ap_name();

        if (!is_atm_tst(ap_name) and !is_atm_asgn(ap_name))
            continue;

        if (is_atm_tst(ap_name))
        {
            auto [t1,t2,_] = parse_tst(ap_name);
            if (is_reg_name(t1))
                result.insert(t1);
            if (is_reg_name(t2))
                result.insert(t2);
        }
        else  // is_atm_asgn(ap_name)
            result.insert(parse_asgn_atom(ap_name).second);
    }
    return result;
}


// assumes that f is in DNF (also not tt nor ff)
vector<spot::formula> get_cubes(const spot::formula& f)
{
    MASSERT(!f.is_ff() && !f.is_tt(), f);

    if (f.is(spot::op::And))
        return { f };

    MASSERT(f.is(spot::op::Or), f);

    vector<spot::formula> cubes;
    for (auto it = f.begin(); it != f.end(); ++it)
        cubes.push_back(*it);
    return cubes;
}

//// TODO: add support for io_tst_atoms

template<typename Container>
tuple<set<spot::formula>, set<spot::formula>, set<spot::formula>>
separate(const Container& cube)
{
    set<spot::formula> APs, cmp_atoms, asgn_atoms;

    for (spot::formula f : cube)
    {
        // f is either positive 'a' or negated '!a' or an action
        if (f.is(spot::op::Not))
            f = f.get_child_of(spot::op::Not);

        MASSERT(f.is(spot::op::ap), f);

        if (is_atm_tst(f.ap_name()))
            cmp_atoms.insert(f);
        else if (is_atm_asgn(f.ap_name()))
            asgn_atoms.insert(f);
        else
            APs.insert(f);
    }

    return tuple(APs, cmp_atoms, asgn_atoms);
}


Asgn compute_asgn(const set<spot::formula>& atm_asgn_atoms)
{
    hmap<string, hset<string>> asgn_data;  // e.g.: { i -> {r1,r2}, o -> {r3,r4} }
    for (const auto& atom : atm_asgn_atoms)
    {
        auto var_reg = parse_asgn_atom(atom.ap_name());
        asgn_data[var_reg.first].insert(var_reg.second);
    }
    return Asgn(asgn_data);
}


V get_add_vertex(Graph& g, hmap<V, EC>& v_to_ec, const string& var_name)
{
    for (const auto& v_ec : v_to_ec)
        if (contains(v_ec.second, var_name))
            return v_ec.first;

    auto vertices = g.get_vertices();
    auto new_v = vertices.empty()? 0 : 1 + *max_element(vertices.begin(), vertices.end());
    g.add_vertex(new_v);
    v_to_ec.insert({new_v, {var_name}});
    return new_v;
}


/* Returns the partial partition constructed from p and atm_tst_atoms (if possible). */
optional<Partition> compute_partial_p_io(const Partition& p, const set<spot::formula>& atm_tst_atoms)
{
    // copy
    Graph g = p.g;
    hmap<V, EC> v_to_ec = p.v_to_ec;

    for (const auto& tst_atom : atm_tst_atoms)
    {
        auto [t1, t2, cmp] = parse_tst(tst_atom.ap_name());
        MASSERT(cmp != "≥" && cmp != "≤" && cmp != "≠", "should be handled before");
        MASSERT(! (is_reg_name(t1) && is_reg_name(t2)), "not supported");

        auto v1 = get_add_vertex(g, v_to_ec, t1),
             v2 = get_add_vertex(g, v_to_ec, t2);

        if (cmp == "=")
        {
            if (v1 == v2)
                continue;

            GA::merge_v1_into_v2(g, v1, v2);
            auto v1_ec = v_to_ec.at(v1);
            v_to_ec.at(v2).insert(v1_ec.begin(), v1_ec.end());
            v_to_ec.erase(v1);
        }
        else if (cmp == ">")  // t1 > t2
            g.add_edge(v1, v2);
        else if (cmp == "<")
            g.add_edge(v2, v1);
        else
            MASSERT(0, "unreachable: " << cmp);
    }

    if (GA::has_cycles(g))
        return {};

    return Partition(g, v_to_ec);
}

Partition compute_succ_partition(const Partition& partition,
                                 const TstAtom& i_tst, const Asgn& i_asgn,
                                 const TstAtom& o_tst, const Asgn& o_asgn)
{
    // TODO
}


/*
 * Compare two bounds. Both a and b can be empty, meaning:
 *   +∞  if treat_empty_as_plus_infinity is true
 *   -∞  when it is false
 */
bool a_is_ge_b(const string& a, const string& b, const Partition& p, bool treat_empty_as_plus_infinity)
{
    /////if (treat_empty_as_plus_infinity)
    /////{
    /////    if (a.empty()) return true;  // a=∞ or a=b=∞
    /////    if (b.empty()) return false; // a<∞ and b=∞
    /////}
    /////else
    /////{
    /////    if (b.empty()) return true;
    /////    if (a.empty()) return false;
    /////}

    /////for (const auto& c : p.repr)
    /////{
    /////    if (contains(c, b))  // b appears strictly before a or in the same equivalence class
    /////        return true;
    /////    if (contains(c, a))  // a appears strictly before b
    /////        return false;
    /////}

    /////MASSERT(0, "unreachable");
}

bool a_is_less_b(const string& a, const string& b, const Partition& p, bool treat_empty_as_plus_infinity)
{
    return !a_is_ge_b(a, b, p, treat_empty_as_plus_infinity);
}

/*
  Pre-conditions:
  - p is complete
  - atm_i_tst can be incomplete
  - |data_inputs|=1
  Ensures:
  - each partition in the result set is complete and adheres to tst for i
*/
set<Partition> compute_all_p_with_i(const Partition& p, const TstAtom& atm_tst)
{
    /////// check pre-condition |inputs|=1
    /////set<string> inputs;
    /////for (const auto& var_cmp : atm_tst.data_to_cmp)
    /////    if (is_input_name(var_cmp.first))
    /////        inputs.insert(var_cmp.first);
    /////MASSERT(inputs.size() == 1, "function pre-condition violation");

    /////string i_name = *inputs.begin();
    /////auto i_cmp = atm_tst.data_to_cmp.at(i_name);
    /////if (i_cmp.is_EQUAL())
    /////{
    /////    auto p_result = p;  // copy
    /////    p_result.get_class_of(i_cmp.rU).insert(i_name);
    /////    return { p_result };
    /////}

    //////*
    /////  -∞ < r0 < r1 < r2 < r3 < +∞
    /////       ^    ^    ^    ^
    /////  Enumeration happens as noted above.
    /////  For each register, we check two tests:
    /////  (prev,cur) and [cur].
    /////  Hence:
    /////  (-∞,r0) and [r0];  (r0,r1) and [r1];  (r1,r2) and [r2];  (r2,r3), [r3].
    /////  and after the loop we check (r3,+∞).
    /////*/

    /////set<Partition> all_p_with_i;
    /////for (const auto& ec : p.repr)
    /////{
    /////    auto cur_reg = *(ec.begin());

    /////    // we try two tests: (prev_reg,cur_reg) and [cur_reg]

    /////    // if (i_cmp.rU < cur_reg)
    /////    if (a_is_less_b(i_cmp.rU, cur_reg,  p, true))
    /////        return all_p_with_i;  // we are done

    /////    // if (i_cmp.rL ≥ cur_reg)
    /////    if (a_is_ge_b(i_cmp.rL, cur_reg, p, false))
    /////        continue;  // didn't reach yet

    /////    // now: i_cmp.rL < cur_reg ≤ i_cmp.rU

    /////    // The case [cur_reg]:
    /////    if (a_is_less_b(cur_reg, i_cmp.rU, p, true))
    /////    {
    /////        auto new_p = p;  // copy
    /////        new_p.get_class_of(cur_reg).insert(i_name);
    /////        all_p_with_i.insert(new_p);
    /////    }

    /////    // The case (prev_reg, cur_reg):
    /////    // since rL < cur_reg ≤ rU  and  rL ≤ prev_reg,
    /////    // we have (prev_reg, cur_reg) ⊆ (rL, rU), hence no if-checks
    /////    auto new_p = p;
    /////    auto it_of_cur_ec = find(new_p.repr.begin(), new_p.repr.end(), ec);
    /////    new_p.repr.insert(it_of_cur_ec, {i_name});  // insert before it_of_cur_ec, creating prev_reg<i<cur_reg
    /////    all_p_with_i.insert(new_p);
    /////}

    /////if (i_cmp.rU.empty())    // if unrestricted from above
    /////{                        // then put i as the last element of p
    /////    auto new_p = p;
    /////    new_p.repr.push_back({i_name});
    /////    all_p_with_i.insert(new_p);
    /////}

    /////return all_p_with_i;
}

Partition build_init_partition(const hset<string>& sysR, const hset<string>& atmR)
{
    Graph g;
    g.add_vertex(0);
    hmap<V, EC> v_to_ec;
    v_to_ec[0] = a_union_b(sysR, atmR);

    return Partition(g, v_to_ec);
}

void sdf::reduce(const spot::twa_graph_ptr& reg_ucw, uint nof_sys_regs,
                 spot::twa_graph_ptr& classical_ucw,
                 set<spot::formula>& sysTst,
                 set<spot::formula>& sysAsgn,
                 set<spot::formula>& sysOutR)
{
    auto bdd_dict = spot::make_bdd_dict();
    classical_ucw = spot::make_twa_graph(bdd_dict);

    auto sysR = build_sysR(nof_sys_regs);
    auto atmR = build_atmR(reg_ucw);
    introduce_sysActionAPs(sysR, classical_ucw, sysTst, sysAsgn, sysOutR);

    // -- introduce Boolean APs --
    auto non_actionAPs = get_non_actionAPs_from(reg_ucw);
    for (const auto& ap : non_actionAPs)
        classical_ucw->register_ap(ap);

    // -- the main part --
    // The register-naming convention (hard-coded) is:
    // - system registers are rs1, rs2, ..., rsK
    // - automaton registers are r1, r2, ... (NOT TRUE: I JUST EXTRACT WHATEVER THE AUTOMATON USES!)

    // create the initial state and partition
    auto r_init = reg_ucw->get_init_state_number();
    auto c_init = classical_ucw->new_state();

    hmap<uint, pair<uint, Partition>> c_to_qp;  // 'classical state' to (reg-atm state, partition)
    c_to_qp.insert(make_pair(c_init, make_pair(r_init, build_init_partition(sysR, atmR))));

    set<uint> c_todo;
    c_todo.insert(c_init);
    hset<pair<uint, Partition>,pair_hash> qp_processed;

    // main loop
    while (!c_todo.empty())
    {
        auto c = pop_first(c_todo);
        auto [q,p] = c_to_qp.at(c);
        qp_processed.insert({q, p});

        for (const auto& t: reg_ucw->out(q))
        {
            cout << t.src << " -" << t.cond << "-> " << t.dst << endl;
            auto t_f = spot::bdd_to_formula(t.cond, reg_ucw->get_dict());

            // t_f is in DNF or true
            if (t_f.is_tt())  // TODO: handle
                continue;

            // iterate over individual edges
            for (const auto& cube : get_cubes(t_f))
            {
//                cout << "cube: " << join(" & ", cube) << endl;
                // TODO: below we assume that the tests and partitions are complete:
                //       The assumption on the completeness of the partitions simplifies the algorithm.


                /*  The algorithm:

                    let partial_p_io = compute_partial_p_io(p, atm_tst_atoms)
                    let all_p_io = compute_all_p_io(partial_p_io)             // <-- (should be possible to incorporate here sys_asgn-out_r enumeration)
                    for every p_io in all_p_io:
                        for every sys_asgn:
                            let p_io' = update(p_io, sys_asgn)                // update Rs
                            let all_p_io_r = compute_all_p_io_r(p_io')
                            for every p_io_r in all_p_io_r:
                                let p_io_r' = update(p_io_r, atm_asgn)        // update Ra
                                let p_succ = remove i and o from p_io_r'
                                if (q_succ, p_succ) not in qp_processed:
                                    add (q_succ, p_succ) to c_todo
                */

                auto [APs, atm_cmp_atoms, atm_asgn_atoms] = separate(cube);
                auto atm_asgn = compute_asgn(atm_asgn_atoms);
                auto partial_p_i = compute_partial_p_io(p, atm_cmp_atoms);  // TODO: ensure cmp OP is one of <,>,= (but not ≠ ≤ ≥)
                // todo: CURRENT: (i've finished implementing the graph algorithms)
                // todo: TEST the function compute_partial_p_io

                // T0D0: after writing compute_all_p_io, check how to incorporate there enumeration of sys_asgn-out_r
            }
        }
    }
}

void sdf::tmp()
{
    // TODO: test join_tst

    /////////Partition p = Partition({{"r1"}, {"r2"}, {"r3"}, {"r4"}, {"r5"}, {"r6"}, {"r7"}});

    /////////auto atm_tst = TstAtom(Partition({{"i"}, {"o"}}),
    /////////                       { {"i", Cmp::Above("r2")} });

    /////////cout << p << endl;
    /////////cout << atm_tst << endl;
    /////////cout << "------" << endl;
    /////////auto all_p_with_i = compute_all_p_with_i(p, atm_tst);
    /////////for (auto p : all_p_with_i)
    /////////    cout << p << endl;

//    Cmp c1 = Cmp::Equal("ra");
//    Cmp c2 = Cmp::Below("ra");
//    Cmp c3 = Cmp::Above("rb");
//    Cmp c4 = Cmp::Between("rb", "ra");
//
//    cout << c1 << endl;
//    cout << c2 << endl;
//    cout << c3 << endl;
//    cout << c4 << endl;
}
