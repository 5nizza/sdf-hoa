#include "reg_reduction.hpp"

#include "ehoa_parser.hpp"
#include "utils.hpp"
#include "partition_tst_asgn.hpp"

#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
#include <spot/twa/formula2bdd.hh>

#undef BDD


using namespace std;
using namespace sdf;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}  // NOLINT(bugprone-macro-parentheses)
#define INF(message)   {spdlog::get("console")->info()  << message;}  // NOLINT(bugprone-macro-parentheses)

static const string TST_CMP_TOKENS[] = {"=","≠","<",">","≤","≥"};  // NOLINT

/*
 * A test atom has the form:
 *   atom := in◇rX | out◇rX | in◇out
 * where ◇ is one of =,<,>,≠,≤,≥
 */
bool is_i_atm_tst(const string& name)
{
    for (const auto& tok : TST_CMP_TOKENS)  // NOLINT
        // we have to use `find` because some of the tokens consist of multiple chars
        if (name.find(tok) != string::npos)
            return true;

    return false;
}

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


bool is_atm_asgn(const string& name)
{
    return name.find("↓") != string::npos;
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


void introduce_sysActionAPs(const set<string>& sysR,
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


set<string> get_sysR(uint nof_sys_regs)
{
    set<string> result;
    for (uint i = 1; i<= nof_sys_regs; ++i)
        result.insert("r" + to_string(i));
    return result;
}

/* Extract t1,t2,◇ from
 *   in◇rX | out◇rX | in◇out
 * and classify them:
 *    in◇rX: t_in -> in, r -> rX, out -> ""
 *   out◇rX: t_in -> "", r -> rX, out -> out
 *   in◇out: t_in -> in, r -> "", out -> out
 */
void sdf::parse_tst(const string& action_name,
               string& t_in,
               string& t_out,
               string& r,
               string& cmp)
{
    MASSERT(is_atm_tst(action_name), action_name);

    uint index_cmp = 0;
    size_t pos_cmp;
    for (; index_cmp < sizeof TST_CMP_TOKENS; ++index_cmp)
        if ((pos_cmp = action_name.find(TST_CMP_TOKENS[index_cmp])) != string::npos)
            break;

    cmp = TST_CMP_TOKENS[index_cmp];
    auto t1 = trim_spaces(action_name.substr(0, pos_cmp));
    auto t2 = trim_spaces(action_name.substr(pos_cmp+cmp.length(), string::npos));

    t_in = t_out = r = "";

    for (auto t : {t1, t2})
        switch(t[0])
        {
            case 'r':
                MASSERT(r.empty(), "forbidden: t1 and t2 are registers: " << t1 << " : " << t2);
                r = t;
                break;
            case 'i':
                MASSERT(t_in.empty(), "forbidden: t1 and t2 are inputs: " << t1 << " : " << t2);
                t_in = t;
                break;
            case 'o':
                MASSERT(t_out.empty(), "forbidden: t1 and t2 are outputs: " << t1 << " : " << t2);
                t_out = t;
                break;
        }
}

/*
 * A test atom `action_name` is of the form
 *   atom := in◇rX | out◇rX | in◇out
 * Hence:
 *   in◇r | out◇r --> return r
 *   out◇in       --> return ""
 */
string extract_reg_from_tst(const string& action_name)
{
    string t1, t2, cmp;
    //parse_tst(action_name, t1, t2, cmp); // TODO: current

    // Return the term whose name starts with 'r';
    // for in◇out, return ""
    if (t1[0] == 'r')
        return t1;
    else if (t2[0] == 'r')
        return t2;
    else
        return "";
}


string extract_reg_from_asgn(const string& action_name)
{
    // ↓r1
    auto trimmed_action_name = trim_spaces(action_name);
    auto i = trimmed_action_name.find("↓");
    auto result = trim_spaces(trimmed_action_name.substr(i+string("↓").length(), string::npos));
    MASSERT(result[0] == 'r', result);
    return result;
}


/*
 * It returns "" when the action is the test in ◇ out,
 * else it returns a single register.
 */
string sdf::extract_reg_from_action(const string& action_name)
{
    MASSERT(is_atm_tst(action_name) or is_atm_asgn(action_name), action_name);

    if (is_atm_tst(action_name))
        return extract_reg_from_tst(action_name);
    else if (is_atm_asgn(action_name))
        return extract_reg_from_asgn(action_name);

    // TODO: NOLINT No-Wreturn-type
}


set<string> get_atmR(const spot::twa_graph_ptr& reg_atm)
{
    set<string> result;
    for (const auto& ap : reg_atm->ap())
    {
        const auto& ap_name = ap.ap_name();
        if (is_atm_tst(ap_name) or is_atm_asgn(ap_name))
        {
            auto r = extract_reg_from_action(ap_name);
            if (!r.empty())
                result.insert(r);
        }
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

// TODO: add support for io_tsts
template<typename Container>
void separate(const Container& cube,
              set<spot::formula>& APs,
              set<spot::formula>& atm_i_tsts,
              set<spot::formula>& atm_o_tsts,
              set<spot::formula>& atm_i_asgns,
              set<spot::formula>& atm_o_asgns)
{
    for (spot::formula f : cube)
    {
        // f is either positive 'a' or negated '!a' or an action
        if (f.is(spot::op::Not))
            f = f.get_child_of(spot::op::Not);

        MASSERT(f.is(spot::op::ap), f);

        // TODO: current
//        if (is_i_atm_tst(f.ap_name()))
//            atm_i_tsts.insert(f);
//        else if (is_o_atm_tst(f.ap_name()))
//            atm_o_tsts.insert(f);
//        else if (is_i_atm_asgn(f.ap_name()))
//            atm_i_asgns.insert(f);
//        else if (is_o_atm_asgn(f.ap_name()))
//            atm_o_asgns.insert(f);
//        else
//            APs.insert(f);
    }
}

void sdf::reduce(const spot::twa_graph_ptr& reg_ucw, uint nof_sys_regs,
                 spot::twa_graph_ptr& classical_ucw,
                 set<spot::formula>& sysTst,
                 set<spot::formula>& sysAsgn,
                 set<spot::formula>& sysOutR)
{
    auto bdd_dict = spot::make_bdd_dict();
    classical_ucw = spot::make_twa_graph(bdd_dict);

    auto sysR = get_sysR(nof_sys_regs);
    auto atmR = get_atmR(reg_ucw);
    introduce_sysActionAPs(sysR, classical_ucw, sysTst, sysAsgn, sysOutR);

    // -- introduce Boolean APs --
    auto non_actionAPs = get_non_actionAPs_from(reg_ucw);
    for (const auto& ap : non_actionAPs)
        classical_ucw->register_ap(ap);

    // -- the main part --
    // The register-naming convention is:
    // - system registers are rs1, rs2, ..., rsK
    // - automaton registers are r1, r2, ... (NOT TRUE: I JUST EXTRACT WHATEVER THE AUTOMATON USES!)
    // This is hard-coded throughout the program.

    map<uint, uint> c_to_r;       // 'classical state' to 'reg-atm state'
    map<uint, Partition> c_to_p;  // recall that a state of a classical UCW is a pair (q,partition): 'state of spot atm' to 'partition'

    // create the initial state/partition
    auto r_init = reg_ucw->get_init_state_number();
    auto c_init = classical_ucw->new_state();

    c_to_r[c_init] = r_init;
    c_to_p[c_init] = Partition({ a_union_b(sysR, atmR) });

    set<uint> c_todo;
    c_todo.insert(c_init);

    while (!c_todo.empty())
    {
        auto c = pop(c_todo);
        auto r = c_to_r[c];
        auto p = c_to_p[c];
        for (const auto& t: reg_ucw->out(r))
        {
            cout << t.src << " -" << t.cond << "-> " << t.dst << endl;
            auto t_f = spot::bdd_to_formula(t.cond, reg_ucw->get_dict());
            // t_f is in DNF or true
            if (t_f.is_tt())  // TODO: handle
                continue;
            for (const auto& cube : get_cubes(t_f))
            {
//                cout << "cube: " << join(" & ", cube) << endl;
                set<spot::formula> APs, atm_i_tsts, atm_o_tsts, atm_i_asgns, atm_o_asgns;
                separate(cube,
                         APs,
                         atm_i_tsts, atm_o_tsts,  // TODO: allow for atm_io_tests
                         atm_i_asgns, atm_o_asgns);
                cout << "APs: " << join(",", APs) << endl;
                cout << "atm_i_tsts: " << join(",", atm_i_tsts) << endl;
                cout << "atm_o_tsts: " << join(",", atm_o_tsts) << endl;
                cout << "atm_i_asgn: " << join(",", atm_i_asgns) << endl;
                cout << "atm_o_asgn: " << join(",", atm_o_asgns) << endl;
            }


            // for all (sys_tst, sys_asgn, sys_out) in sysTst × sysAsgn × sysOutR:
            //     if (p,sys_tst,sys_out,atm_tst are compatible)
            //         succ_p = compute_succ_p(p,sys_tst,sys_asgn,sys_out,atm_tst,atm_asgn)
            //         add (q',succ_p) to_process
        }
    }
}
