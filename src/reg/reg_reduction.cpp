#include "reg_reduction.hpp"

#include "asgn.hpp"
#include "ehoa_parser.hpp"
#include "reg_utils.hpp"
#include "atm_parsing_naming.hpp"


#define BDD spotBDD
    #include "spot/twa/formula2bdd.hh"
    #include "spot/tl/parse.hh"  // for tests in tmp()
#undef BDD


using namespace std;
using namespace sdf;
using formula = spot::formula;
using twa_graph_ptr = spot::twa_graph_ptr;
using P = Partition;
using PH = PartitionCanonical;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}  // NOLINT(bugprone-macro-parentheses)
#define INF(message)   {spdlog::get("console")->info()  << message;}  // NOLINT(bugprone-macro-parentheses)

#define hmap unordered_map
#define hset unordered_set


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
    // Note: the empty assignment (e.g. appears in label "1")
    //       means 'do no assignments' (recall that assignments are treated in a special way)
    hmap<string, hset<string>> asgn_data;  // e.g.: { i -> {r1,r2}, o -> {r3,r4} }
    for (const auto& atom : atm_asgn_atoms)
    {
        auto [var, reg] = parse_asgn_atom(atom.ap_name());
        asgn_data[var].insert(reg);
    }
    return Asgn(asgn_data);
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


pair<set<formula>, set<formula>>
construct_AsgnOut(const hset<string>& sysR)
{
    set<formula> sysAsgn, sysOutR;
    for (const auto& r : sysR)
    {
        sysAsgn.insert(ctor_sys_asgn_r(r));
        // vars to output r in R: unary encoding: introduce |R| many of vars, then require MUTEXCL
        sysOutR.insert(ctor_sys_out_r(r));
    }
    return {sysAsgn, sysOutR};
}


tuple<twa_graph_ptr,  // new_ucw
      set<formula>,   // sysTst
      set<formula>,   // sysAsgn
      set<formula>>   // sysOutR
sdf::reduce(DataDomainInterface& domain,
            const twa_graph_ptr& reg_ucw,
            const hset<string>& sysR,
            const hmap<string,Relation>& sys_tst_descr)
{
    // create new_ucw
    twa_graph_ptr new_ucw;
    auto bdd_dict = spot::make_bdd_dict();
    new_ucw = spot::make_twa_graph(bdd_dict);
    new_ucw->copy_acceptance_of(reg_ucw);  // note that the acceptance can become transition-based

    // register names
    auto atmR = extract_atmR(reg_ucw);
    assert_no_intersection(sysR, atmR);

    // create sysTst, sysAsgn, sysOutR
    auto sysTst = domain.construct_sysTstAP(sysR);
    auto [sysAsgn,sysOutR] = construct_AsgnOut(sysR);

    // introduce APs
    for (const auto& vars : {sysTst, sysAsgn, sysOutR})
        for (const auto& v : vars)
            new_ucw->register_ap(v);
    for (const auto& ap : reg_ucw->ap())
        if (!is_atm_tst(ap.ap_name()) && !is_atm_asgn(ap.ap_name()))
            new_ucw->register_ap(ap);

    // -- the main part --

    // (technical detail) define hash function and equality for partitions
    using QP = pair<uint, PartitionCanonical>;
    auto qp_hasher = [](const QP& qp)
            {
                 auto uint_hasher = [](const uint& i) {return std::hash<uint>()(i);};
                 auto p_hasher = [](const PH& p) {return p.hash;};
                 return do_hash_pair(qp, uint_hasher, p_hasher);
            };
    auto qp_equal_to = [](const QP& qp1, const QP& qp2)
    {
        return qp1.first==qp2.first && qp1.second == qp2.second;
    };

    auto qp_to_c = hmap<QP, uint,
                        decltype(qp_hasher),
                        decltype(qp_equal_to)>
                   (0, qp_hasher, qp_equal_to);

    auto get_c = [&qp_to_c, &new_ucw](const QP& qp)
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
                    ps << qp.second.p;
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

    using QPHashSet = hset<QP, decltype(qp_hasher), decltype(qp_equal_to)>;
    auto qp_processed = QPHashSet(0, qp_hasher, qp_equal_to);
    auto qp_todo = QPHashSet(0, qp_hasher, qp_equal_to);

    auto init_p = domain.build_init_partition(sysR, atmR);
    cout << "init_p: " << init_p << endl;
    QP init_qp = {reg_ucw->get_init_state_number(), PartitionCanonical(init_p)};
    new_ucw->set_init_state(get_c(init_qp));
    qp_todo.insert(init_qp);

    // How are pure Equality and Order domains handled by this algorithm?
    // The following methods depend on the domain type:
    // `preprocess`
    // `all_possible_atm_tst`
    // `all_possible_sys_tst` (?)
    // The Order domain can be handled within Mixed Domain,
    // by providing the complete set of system predicates.
    // However, for the Equality domain, the function `all_possible_atm_tst`
    // should account for the fact that T (wrt. a single reg) means = or ≠
    // rather than > < or =. The function `preprocess` should also be changed:
    // in the Equality, there is no need to translate ≠ into '< or >'.

    /*
    while qp_todo is not empty:
        let (q,p) = qp_todo.pop()
        add (q,p) to qp_processed
        for (lab, atm_tst, atm_asgn, q') in reg_ucw.out(q):
            for (p_io,full_atm_tst) in all_possible_atm_tst(p,atm_tst):        // p_io is a refinement of p_io(=p+atm_tst) wrt. full_atm_tst
                for (p_io', sys_tst) in all_possible_sys_tst(p_io, sys_pred):  // p_io' is a refinement of p_io wrt. sys_tst (2nd step hidden: only allowed sys_tst are used)
                    for every sys_asgn:                                        // p_io' is partial since sys_tst is not complete in general
                        let p_io'' = update(p_io', sys_asgn)      // update Rs
                        let all_r = pick_all_r(p_io'', sysR)
                        if all_r is empty: skip
                        let p_io''' = update(p_io'', atm_asgn)    // update Ra
                        let p_succ = remove i and o from p_io'''
                        add to classical_ucw the edge (q,p) -> (q_succ, p_succ) labelled (sys_tst & lab, sys_asgn, OR(all_r))
                        make p_succ canonical
                        if (q_succ, p_succ) not in qp_processed:
                            add (q_succ, p_succ) to c_todo
    */

    /// CONTINUE IMPLEMENTING METHODS (of the domain)
    while (!qp_todo.empty())
    {
        DEBUG("qp_todo.size() = " << qp_todo.size() << ", "
              "qp_processed.size() = " << qp_processed.size());

        auto qp = pop_first<QP>(qp_todo);
        auto c_of_qp = get_c(qp);
        qp_processed.insert(qp);

        for (const auto& e: reg_ucw->out(qp.first))
        {
            auto e_f = spot::bdd_to_formula(e.cond, reg_ucw->get_dict());  // e_f is true or a DNF formula
            for (const auto& cube : get_cubes(e_f))                        // iterate over individual edges
            {
                auto [APs, atm_tst_atoms, atm_asgn_atoms] = separate(cube);
                auto atm_asgn = compute_asgn(atm_asgn_atoms);

                auto p = Partition(qp.second.p);  // (a modifiable copy)

                for (const auto& p_io : domain.all_possible_atm_tst(p,atm_tst_atoms))
                {
                    for (auto& [p_sys, sys_tst] : domain.all_possible_sys_tst(p_io, sys_tst_descr)) // p_sys is a refinement of p_io wrt. sys_tst (2nd step hidden: only allowed sys_tst are used)
                    {
                        // early break: if o does not belong to the class of i or some sys_reg, then o cannot be realised
//                    if (!domain.out_is_implementable(p_io)) continue;

//                    vector<vector<formula>> all_assignments;  // NB: this requires adding mutexcl edges
//                    all_assignments.emplace_back();  // empty assignment
//                    for (auto&&  reg : sysAsgn)
//                        all_assignments.push_back({reg});
//                    for (const auto& sys_asgn_atoms : all_assignments)  // this give 2x speedup _only_
                        for (const auto& sys_asgn_atoms : all_subsets<formula>(sysAsgn))
                        {
                            auto sys_asgn = compute_asgn(sys_asgn_atoms);
                            domain.update(p_sys, sys_asgn);              // update Rs
                            auto all_r = domain.pick_all_r(p_sys);
                            if (all_r.empty())
                                continue;  // skip if `o` not in any ECs with system regs in it

                            domain.update(p_sys, atm_asgn);              // update Ra
                            domain.remove_io_from_p(p_sys);
                            auto p_succ = PartitionCanonical(p_sys);
                            QP qp_succ = {e.dst, p_succ};

                            /* add the edge (q,p) -> (q_succ, p_succ) labelled (sys_tst & ap(cube), sys_asgn, out_r) */
                            auto cond = formula::And({asgn_to_formula(sys_asgn, sysAsgn),
                                                      sys_tst,
                                                      R_to_formula(all_r),
                                                      formula::And(vector<formula>(APs.begin(), APs.end()))});
                            new_ucw->new_edge(c_of_qp, get_c(qp_succ),
                                              spot::formula_to_bdd(cond, new_ucw->get_dict(), new_ucw),
                                              e.acc);

                            if (!qp_processed.count(qp_succ))
                                qp_todo.insert(qp_succ);
                        }
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
    // NB: when use singleton assignments, ADD mutexcl edges here forbiding storing into ≥2 registers

    return {new_ucw, sysTst, sysAsgn, sysOutR};

    // int i=0, j=0;
    // DEBUG("#iterations: " << j << ", #breaks: " << i << ", use ratio: " << ((float)(j-i))/j);
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


/** return "none are true" OR "≥2 are true" */
formula create_MUTEXCL_violation(const set<formula>& props_)
{
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

//    cout << p1.equal_to(p2) << endl;

}



















