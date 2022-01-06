#include "reg_reduction.hpp"

#include "ehoa_parser.hpp"
#include "utils.hpp"
#include "partition_tst_asgn.hpp"
#include "graph.hpp"
#include "graph_algo.hpp"

// for tests in tmp()
#include <spot/tl/parse.hh>


#define BDD spotBDD
    #include <spot/twaalgos/dot.hh>
    #include <spot/twa/formula2bdd.hh>
#undef BDD


using namespace std;
using namespace sdf;
using GA = graph::GraphAlgo;
using formula = spot::formula;
using twa_graph_ptr = spot::twa_graph_ptr;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}  // NOLINT(bugprone-macro-parentheses)
#define INF(message)   {spdlog::get("console")->info()  << message;}  // NOLINT(bugprone-macro-parentheses)

#define hmap unordered_map
#define hset unordered_set

static const string TST_CMP_TOKENS[] = {"=","≠","<",">","≤","≥"};  // NOLINT

static const string IN = "in";    // NOLINT
static const string OUT = "out";  // NOLINT

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


/* Assignments have form in↓rX | out↓rX */
bool is_atm_asgn(const string& name)
{
    return name.find("↓") != string::npos;
}


// ASSUMPTION: one data input and one data output
// APs naming convention for system tst/asgn/R
formula ctor_sys_tst_inp_smaller_r(const string& r) { return formula::ap(IN + "<" + r); }
formula ctor_sys_tst_inp_equal_r(const string& r)   { return formula::ap(IN + "=" + r); }
formula ctor_sys_asgn_r(const string& r)        { return formula::ap(IN + "↓" + r); }
formula ctor_sys_out_r(const string& r)         { return formula::ap("↑" + r); }


/**
The encoding is:

- system tests:
  for each register r, we introduce two Boolean variables, in<r and in=r.
  - the atm construction must ensure there are no transitions labeled in<r & in=r,
  - all other combinations are possible:
       in<r & !(in=r) encodes "*<r",
    !(in<r) & !(in=r) encodes "*>r",
    !(in<r) & in=r    encodes "*=r".

- system assignments:
  for each register r, introduce a Boolean variable in↓r

- system outputs:
  for each register, introduce a Boolean variable ↑r, and
  ensure that having ↑r1 & ↑r2 leads to a rejecting sink.
*/
void introduce_sysActionAPs(const hset<string>& sysR,
                            twa_graph_ptr classical_ucw, // NOLINT
                            set<formula>& sysTst,
                            set<formula>& sysAsgn,
                            set<formula>& sysOutR)
{
    // create variables for sysTst, sysAsgn, R
    for (const auto& r : sysR)
    {
        // sysTst variables for register r
        classical_ucw->register_ap(ctor_sys_tst_inp_smaller_r(r));
        classical_ucw->register_ap(ctor_sys_tst_inp_equal_r(r));

        sysTst.insert(ctor_sys_tst_inp_equal_r(r));
        sysTst.insert(ctor_sys_tst_inp_smaller_r(r));

        // sysAsgn
        classical_ucw->register_ap(ctor_sys_asgn_r(r));
        sysAsgn.insert(ctor_sys_asgn_r(r));

        // vars to output r in R: unary encoding: introduce |R| many of vars, then require MUTEXCL
        classical_ucw->register_ap(ctor_sys_out_r(r));
        sysOutR.insert(ctor_sys_out_r(r));
    }
}

void introduce_labelAPs(const twa_graph_ptr old_ucw,  // NOLINT
                        twa_graph_ptr new_ucw         // NOLINT
                        )
{
    for (const auto& ap : old_ucw->ap())
        if (!is_atm_tst(ap.ap_name()) && !is_atm_asgn(ap.ap_name()))
            new_ucw->register_ap(ap);
}


hset<string> build_sysR(uint nof_sys_regs)
{
    hset<string> result;
    for (uint i = 1; i<= nof_sys_regs; ++i)
        result.insert("rs" + to_string(i));
    return result;
}


bool is_input_name(const string& name) { return name.substr(0,2) == IN; }
bool is_output_name(const string& name) { return name.substr(0,3) == OUT; }
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

    return {t1,t2,cmp};
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
    return {var_name, reg};
}

hset<string> build_atmR(const twa_graph_ptr& reg_atm)
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


// Assumes that f is in DNF.
// NB: if f = True returns a single cube `True`
vector<formula> get_cubes(const formula& f)
{
    if (f.is_tt())
        return {f};

    MASSERT(!f.is_ff() && !f.is_tt(), f);

    if (f.is(spot::op::And) or f.is(spot::op::ap) or f.is(spot::op::Not))
        return { f };

    MASSERT(f.is(spot::op::Or), f);

    vector<formula> cubes;
    for (const auto& it : f)
        cubes.push_back(it);
    return cubes;
}


void classify_literal(const formula& f,
                      hset<formula>& APs,
                      hset<formula>& cmp_atoms,
                      hset<formula>& asgn_atoms)
{
    MASSERT(f.is_literal(), f);
    if (f.is(spot::op::Not))
    {
        auto a_name = f.get_child_of(spot::op::Not).ap_name();
        MASSERT(!is_atm_tst(a_name) and !is_atm_asgn(a_name),
                "negated actions not allowed (negated APs are OK): " << f);
        APs.insert(f);
        return;
    }

    if (is_atm_tst(f.ap_name()))
        cmp_atoms.insert(f);
    else if (is_atm_asgn(f.ap_name()))
        asgn_atoms.insert(f);
    else
        APs.insert(f);
}

/* @return finite-alphabet APs, cmp_atoms, asgn_atoms */
tuple<hset<formula>, hset<formula>, hset<formula>>
separate(const formula& cube)
{
    if (cube.is_tt())
        return {{}, {}, {}};

    hset<formula> APs, cmp_atoms, asgn_atoms;

    if (cube.is_literal())
        classify_literal(cube, APs, cmp_atoms, asgn_atoms);
    else
        for (formula f : cube)
            classify_literal(f, APs, cmp_atoms, asgn_atoms);

    return {APs, cmp_atoms, asgn_atoms};
}


template<typename Container>
Asgn compute_asgn(const Container& atm_asgn_atoms)
{
    hmap<string, hset<string>> asgn_data;  // e.g.: { i -> {r1,r2}, o -> {r3,r4} }
    for (const auto& atom : atm_asgn_atoms)
    {
        auto [var, reg] = parse_asgn_atom(atom.ap_name());
        asgn_data[var].insert(reg);
    }
    return Asgn(asgn_data);
}


V add_vertex(Graph& g, hmap<V, EC>& v_to_ec, const string& var_name)
{
    for (const auto& v_ec : v_to_ec)
        MASSERT(!v_ec.second.count(var_name), "the vertex is already present: " << var_name);

    auto vertices = g.get_vertices();
    auto new_v = vertices.empty()? 0 : 1 + *max_element(vertices.begin(), vertices.end());
    g.add_vertex(new_v);
    v_to_ec.insert({new_v, {var_name}});
    return new_v;
}


V get_vertex_of(const string& var,
                const Graph& g,
                const hmap<V,EC>& v_to_ec)
{
    for (const auto& v : g.get_vertices())
        if (v_to_ec.at(v).count(var))
            return v;

    MASSERT(0,
            "not possible: var: " << var << ", v_to_ec: " <<
                                  join(", ", v_to_ec,
                                       [](const auto& v_ec)
                                       {
                                           return to_string(v_ec.first) + " -> {" + join(",", v_ec.second) + "}";
                                       }));
}


/* Returns the partial partition constructed from p and atm_tst_atoms (if possible).
 * Assumes atm_tst_atoms are of the form "<" or "=", or the test is True, but not "≤" nor "≠".  */
optional<Partition> compute_partial_p_io(const Partition& p, const hset<formula>& atm_tst_atoms)
{
    // copy
    Graph g = p.g;
    hmap<V, EC> v_to_ec = p.v_to_ec;

    add_vertex(g, v_to_ec, IN);
    add_vertex(g, v_to_ec, OUT);

    for (const auto& tst_atom : atm_tst_atoms)
    {
        auto [t1, t2, cmp] = parse_tst(tst_atom.ap_name());
        MASSERT(cmp != "≥" && cmp != "≤" && cmp != "≠", "should be handled before");
        MASSERT(! (is_reg_name(t1) && is_reg_name(t2)), "not supported");

        auto v1 = get_vertex_of(t1, g, v_to_ec),
             v2 = get_vertex_of(t2, g, v_to_ec);

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


Partition build_init_partition(const hset<string>& sysR, const hset<string>& atmR)
{
    Graph g;
    g.add_vertex(0);
    hmap<V, EC> v_to_ec;
    v_to_ec.insert({0, a_union_b(sysR, atmR)});

    return Partition(g, v_to_ec);
}

// convert "≥" into "= or >", similarly for "≠"
// note: the test "True" is handled later
twa_graph_ptr preprocess(const twa_graph_ptr& reg_ucw)
{
    twa_graph_ptr g = spot::make_twa_graph(reg_ucw->get_dict());
    g->copy_ap_of(reg_ucw);
    g->copy_acceptance_of(reg_ucw);

    hmap<uint, uint> new_by_old;  // (technical) relate states in the old an new automaton (they have the same states up to renaming)
    auto s = [&g, &new_by_old](const uint& reg_ucw_s)
            {
                if (new_by_old.find(reg_ucw_s) == new_by_old.end())
                    new_by_old.insert({reg_ucw_s, g->new_state()});
                return new_by_old.at(reg_ucw_s);
            };

    g->set_init_state(s(reg_ucw->get_init_state_number()));

    for (const auto& e : reg_ucw->edges())
    {
        auto vars_as_conj = spot::bdd_to_formula(bdd_support(e.cond), g->get_dict());

        MASSERT(vars_as_conj.is_constant() or
                vars_as_conj.is(spot::op::And) or
                vars_as_conj.is(spot::op::ap),
                "unexpected");

        vector<formula> aps;

        if (vars_as_conj.is(spot::op::And))
            for (const auto& it : vars_as_conj)
                aps.push_back(it);
        else if (vars_as_conj.is(spot::op::ap))
            aps.push_back(vars_as_conj);
        // else aps is empty

        bdd new_cond = e.cond;
        for (const auto& ap : aps)
        {
            if (!is_atm_tst(ap.ap_name()))
                continue;

            auto [t1,t2,cmp] = parse_tst(ap.ap_name());
            if (cmp == ">" or cmp == "<" or cmp == "=")
                continue;

            formula ap1, ap2;
            if (cmp == "≥")
            {
                ap1 = formula::ap(t1 + ">" + t2);  // NOLINT
                ap2 = formula::ap(t1 + "=" + t2);  // NOLINT
            }
            if (cmp == "≤")
            {
                ap1 = formula::ap(t1 + "<" + t2);  // NOLINT
                ap2 = formula::ap(t1 + "=" + t2);  // NOLINT
            }
            if (cmp == "≠")
            {
                ap1 = formula::ap(t1 + "<" + t2);  // NOLINT
                ap2 = formula::ap(t1 + ">" + t2);  // NOLINT
            }

            g->register_ap(ap1);
            g->register_ap(ap2);

            bdd subst = formula_to_bdd(formula::Or({ap1, ap2}), g->get_dict(), g);

            auto bdd_id_of_ap = g->register_ap(ap);  // helps us to get BDD id of the ap (otherwise does nothing)
            new_cond = bdd_compose(new_cond, subst, bdd_id_of_ap); // NB: new_cond may be equivalent to false due to conflicting tests like "out>r & out=r"
        }

        g->new_edge(s(e.src), s(e.dst), new_cond, e.acc);
    }

    return g;
}

vector<Partition> compute_all_p_io(const Partition& partial_p_io)
{
    vector<Partition> result;
    for (auto&& [new_g,mapper] : GA::all_topo_sorts2(partial_p_io.g))
    {
        hmap<V,EC> new_v_to_ec;
        for (auto&& [new_v,set_of_old_v] : mapper)
            for (auto&& old_v : set_of_old_v)
                for (auto&& r : partial_p_io.v_to_ec.at(old_v))
                    new_v_to_ec[new_v].insert(r);
        result.emplace_back(new_g, new_v_to_ec);
    }
    return result;
}

/*
  From a given partition (which includes positions of i and o),
  compute system registers compatible with o.
  ASSUMPTION: there is only one output `o`.
  ASSUMPTION: p_io is complete
  NB: the partition is assumed to be complete, yet there might still be several different possible r:
      when they all reside in the same EC.
*/
hset<string> pick_R(const Partition& p_io, const hset<string>& sysR)
{
    hset<string> result;
    for (const auto& [v,ec] : p_io.v_to_ec)
        if (find_if(ec.begin(), ec.end(), is_output_name) != ec.end())  // assumes there is only one o
            return a_intersection_b(ec, sysR);
    return {};
}

Partition update(const Partition& p, const Asgn& asgn)
{
    auto new_g = p.g;  // copy
    auto new_v_to_ec = p.v_to_ec;

    for (const auto& [io, regs] : asgn.asgn)
    {
        auto v_io = get_vertex_of(io, new_g, new_v_to_ec);
        for (const auto& r : regs)
        {
            auto v_r = get_vertex_of(r, new_g, new_v_to_ec);
            if (v_r == v_io)
                continue;  // store the same value; has no effect on partition

            new_v_to_ec.at(v_io).insert(r);

            new_v_to_ec.at(v_r).erase(r);
            if (new_v_to_ec.at(v_r).empty())
            {
                GA::close_vertex(new_g, v_r);
                new_v_to_ec.erase(v_r);
            }
        }
    }

    return Partition(new_g, new_v_to_ec);
}


Partition remove_io_from_p(const Partition& p)
{
    auto new_v_to_ec = p.v_to_ec;
    auto new_g = p.g;

    for (const auto& var : {IN, OUT})
    {
        auto v = get_vertex_of(var, new_g, new_v_to_ec);
        new_v_to_ec.at(v).erase(var);
        if (new_v_to_ec.at(v).empty())
        {
            GA::close_vertex(new_g, v);
            new_v_to_ec.erase(v);
        }
    }
    return Partition(new_g, new_v_to_ec);
}

// Extract the full system test from a given partition.
formula extract_sys_tst_from_p(const Partition& p, const hset<string>& sysR)
{
    auto v_IN = get_vertex_of(IN, p.g, p.v_to_ec);
    auto ec_IN = p.v_to_ec.at(v_IN);

    auto result = formula::tt();

    // components for *=r
    {
        for (const auto& r: a_intersection_b(sysR, ec_IN))
            result = formula::And({result, ctor_sys_tst_inp_equal_r(r)});
    }

    // getting system registers below *
    {
        hset<V> descendants;
        GA::get_descendants(p.g, v_IN, [&descendants](const V& v) { descendants.insert(v); });
        for (const auto& v: descendants)
            for (const auto& r: a_intersection_b(sysR, p.v_to_ec.at(v)))
                // r<* is encoded as !(in=r) & !(in<r)
                result = formula::And({result,
                                       formula::Not(ctor_sys_tst_inp_equal_r(r)),
                                       formula::Not(ctor_sys_tst_inp_smaller_r(r))
                                      });
    }

    // getting system registers above *
    {
        hset<V> ancestors;
        GA::get_ancestors(p.g, v_IN, [&ancestors](const V& v) { ancestors.insert(v); });
        for (const auto& v: ancestors)
            for (const auto& r: a_intersection_b(sysR, p.v_to_ec.at(v)))
                // r>* is encoded as !(in=r) & (in<r)
                result = formula::And({result,
                                       formula::Not(ctor_sys_tst_inp_equal_r(r)),
                                       ctor_sys_tst_inp_smaller_r(r)
                                      });
    }

    return result;
}

formula asgn_to_formula(const Asgn& sys_asgn, const set<formula>& sysAsgnAtoms)
{
    // find sysAsgnAtom for each r in Asgn.at(i), set it to True, and all others to False
    // note: only i is stored into registers

    vector<formula> positive_atoms;
    if (sys_asgn.asgn.count(IN))
        for (const auto& r: sys_asgn.asgn.at(IN))
            positive_atoms.push_back(ctor_sys_asgn_r(r));

    return formula::And(
            {
                formula::And(positive_atoms),
                formula::Not(formula::Or(to_vector(a_minus_b(sysAsgnAtoms, positive_atoms))))  // !(a|b|...) means (!a&!b&...)
            });
}

/* return the formula "any of all_r is true" */
formula R_to_formula(const hset<string>& all_r)
{
    vector<formula> R_atoms;
    transform(all_r.begin(), all_r.end(), back_inserter(R_atoms), ctor_sys_out_r);
    return formula::Or(R_atoms);
}

struct QPHashEqual
{
    // operator `hash`
    size_t operator()(const pair<uint, Partition>& qp) const
    {
        return pair_hash<uint,
                         Partition,
                         std::hash<uint>,
                         FullPartitionHelper>()(qp);
    }

    // operator `equal`
    bool operator()(const pair<uint, Partition>& qp1, const pair<uint, Partition>& qp2) const
    {
        return (qp1.first == qp2.first && FullPartitionHelper::equal(qp1.second, qp2.second));
    }
};

void assert_no_intersection(const hset<string>& set1, const hset<string>& set2)
{
    for (const auto& e1 : set1)
        MASSERT(!set2.count(e1),
                "nonzero intersection: " << join(", ", set1) << " vs " << join(", ", set2));
}

formula create_MUTEXCL_violation(const set<formula>& props_);

/* Add an edge labelled `label` into `dst_state` from every state except `dst_state` itself. */
void add_edge_to_every_state(twa_graph_ptr& atm,
                             uint dst_state,
                             const formula& label);

tuple<twa_graph_ptr,  // new_ucw
      set<formula>,   // sysTst
      set<formula>,   // sysAsgn
      set<formula>>   // sysOutR
sdf::reduce(const twa_graph_ptr& reg_ucw_, uint nof_sys_regs)
{
    // TODO: make sure ap "i>r" and "i > r" are treated the same

    // result
    twa_graph_ptr new_ucw;
    set<formula> sysTst, sysAsgn, sysOutR;  // Boolean variables

    twa_graph_ptr reg_ucw = preprocess(reg_ucw_);

    auto bdd_dict = spot::make_bdd_dict();
    new_ucw = spot::make_twa_graph(bdd_dict);

    new_ucw->copy_acceptance_of(reg_ucw);
    if (reg_ucw->prop_state_acc())
        new_ucw->prop_state_acc(true);

    auto sysR = build_sysR(nof_sys_regs);
    auto atmR = build_atmR(reg_ucw);
    assert_no_intersection(sysR, atmR);

    introduce_sysActionAPs(sysR, new_ucw, sysTst, sysAsgn, sysOutR);
    introduce_labelAPs(reg_ucw, new_ucw);

    // -- the main part --
    // The register-naming convention (hard-coded) is:
    // - system registers are rs1, rs2, ..., rsK
    // - automaton registers are r1, r2, ... (NOT TRUE: I JUST EXTRACT WHATEVER THE AUTOMATON USES!)

    hmap<pair<uint, Partition>, uint, QPHashEqual, QPHashEqual> qp_to_c;
    auto get_c = [&qp_to_c, &new_ucw](const pair<uint, Partition>& qp)
            {
                // helper function: get (and create if necessary) a state
                if (auto it = qp_to_c.find(qp); it != qp_to_c.end())
                    return it->second;
                auto new_c = new_ucw->new_state();
                qp_to_c.insert({qp, new_c});

                {   // setting state names (useful for debugging):
                    auto names = new_ucw->get_or_set_named_prop<vector<string>>("state-names");
                    for (auto i = names->size(); i < new_c; ++i)
                        names->push_back(string());  // a dummy name for not yet used states (should not happen, 'just in case')

                    stringstream ps;
                    ps << qp.second;
                    auto p_str = substituteAll(ps.str(), " ", "");
                    stringstream ss;
                    ss << qp.first << ", " << p_str;
                    if (names->size() == new_c)
                        names->push_back(ss.str());
                    else
                        (*names)[new_c] = ss.str();
                }

                return new_c;
            };

    unordered_set<pair<uint, Partition>,QPHashEqual,QPHashEqual> qp_processed;
    hset<pair<uint, Partition>,QPHashEqual,QPHashEqual> qp_todo;

    auto init_qp = make_pair(reg_ucw->get_init_state_number(), build_init_partition(sysR, atmR));
    new_ucw->set_init_state(get_c(init_qp));
    qp_todo.insert(init_qp);

    while (!qp_todo.empty())
    {
        DEBUG("qp_todo.size() = " << qp_todo.size() << ", "
              "qp_processed.size() = " << qp_processed.size());

        auto qp = pop_first<pair<uint, Partition>>(qp_todo);
        auto c_of_qp = get_c(qp);
        qp_processed.insert(qp);

        for (const auto& e: reg_ucw->out(qp.first))
        {
//            cout << e.src << " -" << e.cond << "-> " << e.dst << endl;
            auto e_f = spot::bdd_to_formula(e.cond, reg_ucw->get_dict());
            // e_f is in DNF or true

            // iterate over individual edges
            for (const auto& cube : get_cubes(e_f))
            {
                /*  // NB: we assume that partitions are complete, which simplifies the algorithm.
                    let partial_p_io = compute_partial_p_io(p, atm_tst_atoms) // partition is complete but tst is partial
                    let all_p_io = compute_all_p_io(partial_p_io)             // <-- (should be possible to incorporate sys_asgn-out_r enumeration)
                    for every p_io in all_p_io:
                        for every sys_asgn:
                            let p_io' = update(p_io, sys_asgn)                // update Rs
                            let all_r = pick_all_r(p_io', sysR)
                            let p_io'' = update(p_io', atm_asgn)              // update Ra
                            let p_succ = remove i and o from p_io''
                            add to classical_ucw the edge (q,p) -> (q_succ, p_succ) labelled (sys_tst(p_io) & ap(cube), sys_asgn, OR(all_r))
                            if (q_succ, p_succ) not in qp_processed:
                                add (q_succ, p_succ) to c_todo
                */

                auto [APs, atm_tst_atoms, atm_asgn_atoms] = separate(cube);
                auto atm_asgn = compute_asgn(atm_asgn_atoms);

                auto partial_p_i = compute_partial_p_io(qp.second, atm_tst_atoms);
                if (!partial_p_i.has_value())  // means atm_tst_atoms are not consistent
                    continue;
                for (const auto& p_io : compute_all_p_io(partial_p_i.value()))
                {
//                    cout << "p_io: " << p_io << ", sys_tst: " << extract_sys_tst_from_p(p_io, sysR) << endl;
                    for (const auto& sys_asgn_atoms : all_subsets<formula>(sysAsgn))
                    {
                        auto sys_asgn = compute_asgn(sys_asgn_atoms);
                        auto p_io_ = update(p_io, sys_asgn);
                        auto all_r = pick_R(p_io_, sysR);
//                        cout << "sys_asgn: " << sys_asgn << endl;
//                        cout << "all_r: " << join(", ", all_r) << endl;
//                        cout << endl;
                        // p_io is complete, hence we should continue only if `o` lies in one of ECs with system regs in it
                        if (all_r.empty())
                            continue;
                        auto p_io__ = update(p_io_, atm_asgn);  // NOLINT(bugprone-reserved-identifier)
                        auto p_succ = remove_io_from_p(p_io__);

                        auto qp_succ = make_pair(e.dst, p_succ);

                        /* add to classical_ucw the edge (q,p) -> (q_succ, p_succ) labelled (sys_tst(p_io) & ap(cube), sys_asgn, out_r) */
                        auto cond = formula::And({asgn_to_formula(sys_asgn, sysAsgn),
                                                  extract_sys_tst_from_p(p_io, sysR),
                                                  R_to_formula(all_r),
                                                  formula::And(vector<formula>(APs.begin(), APs.end()))});
                        new_ucw->new_edge(c_of_qp, get_c(qp_succ),
                                          spot::formula_to_bdd(cond, new_ucw->get_dict(), new_ucw),
                                          e.acc);

                        if (!qp_processed.count(qp_succ))
                            qp_todo.insert(qp_succ);
                    }
                }
            } // for (cube : get_cubes(e_f))
        } // for (e : rec_ucw->out(qp.first))
    } // while (!qp_todo.empty())

    // To every state,
    // we add an outgoing edge leading to the rejecting sink when the assumptions on _system_ are violated.
    // Assumptions on system:
    // exactly one of ↑r holds (MUTEXCL)

    // first create a rejecting sink
    auto rej_sink = new_ucw->new_state();
    new_ucw->new_acc_edge(rej_sink, rej_sink,
                          spot::formula_to_bdd(formula::tt(), new_ucw->get_dict(), new_ucw));
    // now add the edges
    add_edge_to_every_state(new_ucw, rej_sink, create_MUTEXCL_violation(sysOutR));

    return {new_ucw, sysTst, sysAsgn, sysOutR};
}

/* Add an edge labelled `label` into `dst_state` from every state except `dst_state` itself. */
void add_edge_to_every_state(twa_graph_ptr& atm,
                             uint dst_state,
                             const formula& label)
{
    for (uint s = 0; s < atm->num_states(); ++s)
    {
        if (s == dst_state)
            continue;
        atm->new_edge(s, dst_state, spot::formula_to_bdd(label, atm->get_dict(), atm));
    }
}

formula create_MUTEXCL_violation(const set<formula>& props_)
{
    // return "none are true" OR "≥2 are true"
    vector<formula> props(props_.begin(), props_.end());
    vector<formula> big_OR;
    big_OR.push_back(formula::Not(formula::Or(props)));
    for (uint i1 = 0; i1 < props.size(); ++i1)
        for (uint i2 = i1+1; i2 < props.size(); ++i2)
            big_OR.push_back(formula::And({props.at(i1), props.at(i2)}));
    return formula::Or(big_OR);
}

void sdf::tmp()
{
    Graph g1;
    g1.add_vertex(1);
    auto v_to_ec1 = hmap<Graph::V, hset<string>>(
            {{1, {"b", "a"}}});

    Graph g2;
    g2.add_vertex(1);
    auto v_to_ec2 = hmap<Graph::V, hset<string>>(
            {{1, {"a", "b"}}});

    auto p1 = Partition(g1, v_to_ec1);
    auto p2 = Partition(g2, v_to_ec2);

    cout << FullPartitionHelper::calc_hash(p1) << endl << FullPartitionHelper::calc_hash(p2) << endl;
    cout << FullPartitionHelper::equal(p1, p2) << endl;


    /*
    Graph g1 ({{1,2},{2,3}});
    auto v_to_ec1 = hmap<Graph::V, hset<string>>(
            {{1, {"a", "b"}},
             {2, {"c"}},
             {3, {"d"}}});

    auto p1 = Partition(g1, v_to_ec1);

    Graph g2 ({{10,20},{20,30}});
    auto v_to_ec2 =  hmap<Graph::V, hset<string>>(
            {{10, {"a", "b"}},
             {20, {"c"}},
             {30, {"d"}}});

    auto p2 = Partition(g2, v_to_ec2);

    cout << FullPartitionHelper::calc_hash(p1) << endl << FullPartitionHelper::calc_hash(p2) << endl;
    cout << FullPartitionHelper::equal(p1, p2) << endl;
    */

//    hset<pair<uint, Partition>,pair_hash> qp_todo;
//    qp_todo.insert({10,p1});
//    qp_todo.insert({10,p2});
//    cout << qp_todo.count({10,p1}) << endl;
//    cout << p1.calc_hash() << endl;
//    cout << p2.calc_hash() << endl;
//    cout << (p1 == p2) << endl;


}






























