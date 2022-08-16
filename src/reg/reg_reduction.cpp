#include "reg_reduction.hpp"

#include "tst_asgn.hpp"
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
        MASSERT(!is_tst(a_name) and !is_atm_asgn(a_name),
                "negated actions not allowed (negated APs are OK): " << f);
        APs.insert(f);
        return;
    }

    if (is_tst(f.ap_name()))
        cmp_atoms.insert(f);
    else if (is_atm_asgn(f.ap_name()))
        asgn_atoms.insert(f);
    else
        APs.insert(f);
}

/** @return finite-alphabet APs, cmp_atoms, asgn_atoms */
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

Asgn to_asgn(const hset<formula>& atm_asgn_atoms)
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

formula asgn_to_formula(const Asgn& sys_asgn,
                        const hset<formula>& sysAsgnAP)
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
                formula::Not(formula::Or(to_vector(a_minus_b(sysAsgnAP, positive_atoms))))  // !(a|b|...) means (!a&!b&...)
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

formula create_MUTEXCL_violation(const hset<formula>& props_, bool forbid_empty);

/* Add an edge labelled `label` into `dst_state` from every state except `dst_state` itself. */
void add_edge_to_every_state(twa_graph_ptr& atm,
                             uint dst_state,
                             const formula& label);


pair<hset<formula>, hset<formula>>
construct_Asgn_Out_AP(const hset<string>& sysR)
{
    hset<formula> sysAsgn, sysOutR;
    for (const auto& r : sysR)
    {
        sysAsgn.insert(ctor_sys_asgn_r(r));
        // vars to output r in R: unary encoding: introduce |R| many of vars, then require MUTEXCL
        sysOutR.insert(ctor_sys_out_r(r));
    }
    return {sysAsgn, sysOutR};
}

/**
 * For every sys reg s in `sys_pred_decr`:
 *
 * - If the register has the equality domain,
 *   then introduce one Boolean proposition "s=IN". The encoding is:
 *   "*=s":  (s=IN),
 *   "*≠s": !(s=IN).
 *
 * - If the register has the order domain,
 *   then introduce two Boolean propositions: IN<s and IN=s.
 *   Then, the atm definition must correctly handle -- ignore -- tests labeled "IN<s & IN=s".
 *   Then the encoding is:
 *   "*<s":  (IN<s) & !(IN=s),
 *   "*>s": !(IN<s) & !(IN=s),
 *   "*=s": !(IN<s) &  (IN=s).
 */
hset<formula>
construct_sysTstAP(const hmap<string,DomainName>& sys_pred_descr)
{
    auto sysTstAP = hset<formula>();
    for (const auto& [s,dom] : sys_pred_descr)
    {
        if (dom == DomainName::equality)
        {
            sysTstAP.insert(ctor_tst_inp_equal_r(s));
        }
        else if (dom == DomainName::order)
        {
            sysTstAP.insert(ctor_tst_inp_equal_r(s));
            sysTstAP.insert(ctor_tst_inp_smaller_r(s));
        }
        else
            UNREACHABLE();
    }
    return sysTstAP;
}

/** Uses the encoding introduced in `construct_sysTstAP`. */
formula
sys_tstAtom_to_formula(const TstAtom& tst_atom,
                       const DomainName& domain_name)
{
    auto s = tst_atom.t1 != IN? tst_atom.t1 : tst_atom.t2;
    if (domain_name == DomainName::order)
    {
        if (tst_atom.relation == TstAtom::equal)
            // the case "input=s"
            return formula::And({ctor_tst_inp_equal_r(s),
                                 formula::Not(ctor_tst_inp_smaller_r(s))});
        else if (tst_atom.relation == TstAtom::less && tst_atom.t1 == IN)
            // the case "input<s"
            return formula::And({ctor_tst_inp_smaller_r(s),
                                 formula::Not(ctor_tst_inp_equal_r(s))});
        else if (tst_atom.relation == TstAtom::less && tst_atom.t2 == IN)
            // the case "input>s"
            return formula::And({formula::Not(ctor_tst_inp_smaller_r(s)),
                                 formula::Not(ctor_tst_inp_equal_r(s))});
        else
            UNREACHABLE();
    }
    else if (domain_name == DomainName::equality)
    {
        switch (tst_atom.relation)
        {
            case TstAtom::equal: return ctor_tst_inp_equal_r(s);
            case TstAtom::nequal: return formula::Not(ctor_tst_inp_equal_r(s));
            default: UNREACHABLE();
        }
    }
    else
        UNREACHABLE();
}

formula
sys_tst_to_formula(const hset<TstAtom>& sys_tst,
                   const hmap<string,DomainName>& sys_pred_descr)
{
    auto conjuncts = vector<formula>();
    for (const auto& tst_atom : sys_tst)
        conjuncts.push_back(sys_tstAtom_to_formula(tst_atom,
                                                   sys_pred_descr.at(tst_atom.t1 != IN? tst_atom.t1 : tst_atom.t2)));
    return formula::And(conjuncts);  // (note: as expected, returns true if the vector is empty)
}

hset<TstAtom>
to_tst(const hset<formula>& tst_atoms)
{
    auto result = hset<TstAtom>();
    for (const auto& f : tst_atoms)
    {
        auto [t1,t2,cmp] = parse_tst(f.ap_name());
        MASSERT(! (is_reg_name(t1) && is_reg_name(t2)), "not supported");

        if (cmp == "<")
            result.emplace(t1, TstAtom::less, t2);
        else if (cmp == ">")
            result.emplace(t2, TstAtom::less, t1);  // (swap t1 and t2)
        else if (cmp == "=")
            result.emplace(t1, TstAtom::equal, t2);
        else if (cmp == "≠")
            result.emplace(t1, TstAtom::nequal, t2);
        else
            MASSERT(0, "only <,>,=,≠ is supported internally; the others should be preprocessed");
            // moreover, ≠ is only supported for the equality domain but not in the order domain
    }
    return result;
}

tuple<twa_graph_ptr,  // new_ucw
      hset<formula>,  // sysTst
      hset<formula>,  // sysAsgn
      hset<formula>>  // sysOutR
sdf::reduce(MixedDomain& domain,
            const twa_graph_ptr& reg_ucw,
            const hset<string>& sysR,
            const hmap<string,DomainName>& sys_pred_descr)
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
    auto sysTstAP = construct_sysTstAP(sys_pred_descr);
    auto [sysAsgnAP,sysOutAP] = construct_Asgn_Out_AP(sysR);

    // introduce APs
    for (const auto& vars : {sysTstAP, sysAsgnAP, sysOutAP})
        for (const auto& v : vars)
            new_ucw->register_ap(v);
    for (const auto& ap : reg_ucw->ap())
        if (!is_tst(ap.ap_name()) && !is_atm_asgn(ap.ap_name()))
            new_ucw->register_ap(ap);

    // We will use single_reg_assignments
    vector<Asgn> single_reg_assignments;
    single_reg_assignments.push_back(to_asgn({}));          // empty assignment
    for (const auto& regAP : sysAsgnAP)
        single_reg_assignments.push_back(to_asgn({regAP}));

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
    QP init_qp = {reg_ucw->get_init_state_number(), PartitionCanonical(init_p)};
    new_ucw->set_init_state(get_c(init_qp));
    qp_todo.insert(init_qp);

    /*
    while qp_todo is not empty:
        let (q,p) = qp_todo.pop()
        add (q,p) to qp_processed
        for (lab, atm_tst, atm_asgn, q') in reg_ucw.out(q):
            p_io = refine_if_possible(p, atm_tst)                             // if not possible, skip
            for (p_io',sys_tst) in all_possible_sys_tst(p_io,sys_reg_descr):  // p_io' is a refinement of p_io wrt. sys_tst
                for every sys_asgn:                                           // 'single-reg' assignments (to handle buffer efficiently, we need this; see Aug'8 notes.)
                    let p_io'' = update(p_io', sys_asgn)                      // update Rs
                    for out_r in sysR:
                        let p_io''' = refine_if_possible(p_io'', out_r)       // if not possible, skip
                        let p_io'''' = update(p_io''', atm_asgn)              // update Ra
                        let p_succ = remove i and o from p_io''''
                        add to classical_ucw the edge (q,p) -> (q_succ, p_succ)
                                                      labelled (sys_tst & lab, sys_asgn, r)
                        make p_succ canonical                                 // to make checking "in qp_processed?" cheap: see explanation below
                        if (q_succ, p_succ) not in qp_processed:
                            add (q_succ, p_succ) to c_todo
    */

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
                auto atm_asgn = to_asgn(atm_asgn_atoms);

                auto p_io = qp.second.p;
                domain.add_io_to_p(p_io);

                if (!domain.refine(p_io, to_tst(atm_tst_atoms)))
                    continue;

                for (const auto& [p_sys, sys_tst] : domain.all_possible_sys_tst(p_io, sys_pred_descr))  // p_sys is a refinement of p_io wrt. sys_tst
                {
                    for (const auto& sys_asgn : single_reg_assignments)
                    {
                        auto p_sys_upd = p_sys;
                        domain.update(p_sys_upd, sys_asgn);              // update Rs

                        for (const auto& out_r : sysR)
                        {
                            auto p_sys_new = p_sys_upd;
                            if (!domain.refine(p_sys_new, {TstAtom(OUT, TstAtom::equal, out_r)}))  // skip if out_r does not satisfy the OUT test
                                continue;

                            domain.update(p_sys_new, atm_asgn);          // update Ra
                            domain.remove_io_from_p(p_sys_new);
                            auto qp_succ = QP{e.dst, PartitionCanonical(p_sys_new)};

                            /* add the edge (q,p) -> (q_succ, p_succ) labelled (sys_tst & ap(cube), sys_asgn, out_r) */
                            auto cond = formula::And({asgn_to_formula(sys_asgn, sysAsgnAP),
                                                      sys_tst_to_formula(sys_tst, sys_pred_descr),
                                                      R_to_formula({out_r}),
                                                      formula::And(vector<formula>(APs.begin(), APs.end()))});
                            new_ucw->new_edge(c_of_qp, get_c(qp_succ),
                                              spot::formula_to_bdd(cond, new_ucw->get_dict(), new_ucw),
                                              e.acc);

                            if (!qp_processed.count(qp_succ))
                                qp_todo.insert(qp_succ);
                        } // enumerate out_r
                    } // enumerate sys_asgn
                } // enumerate possible sys_tst
            } // enumerate simple edges
        } // enumerate super edges from qp.first
    } // while (!qp_todo.empty())

    // To every state,
    // we add an outgoing edge leading to the rejecting sink when the system violates its assumptions.
    // Assumptions on system:
    // - exactly one of ↑r holds
    // - at most one of ↓r holds

    // create a rejecting sink
    auto rej_sink = new_ucw->new_state();
    new_ucw->new_acc_edge(rej_sink, rej_sink,
                          spot::formula_to_bdd(formula::tt(), new_ucw->get_dict(), new_ucw));
    // MUTEXCL for sys output
    add_edge_to_every_state(new_ucw, rej_sink, create_MUTEXCL_violation(sysOutAP, true));
    // MUTEXCL for single-register assignment
    add_edge_to_every_state(new_ucw, rej_sink, create_MUTEXCL_violation(sysAsgnAP, false));

    return {new_ucw, sysTstAP, sysAsgnAP, sysOutAP};
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


/** @returns: formula for "≥2 are true" OR "none are true" (the latter only if @param forbid_empty is set) */
formula create_MUTEXCL_violation(const hset<formula>& props_, bool forbid_empty)
{
    vector<formula> props(props_.begin(), props_.end());
    vector<formula> big_OR;
    if (forbid_empty)
        big_OR.push_back(formula::Not(formula::Or(props)));
    for (uint i1 = 0; i1 < props.size(); ++i1)
        for (uint i2 = i1+1; i2 < props.size(); ++i2)
            big_OR.push_back(formula::And({props.at(i1), props.at(i2)}));
    return formula::Or(big_OR);
}


void sdf::tmp()
{
    cout << sys_tst_to_formula({TstAtom("rs", TstAtom::less, IN), TstAtom(IN, TstAtom::equal, "rs2")},
                               {{"rs", DomainName::order}, {"rs2", DomainName::order}})
         << endl;
}



















