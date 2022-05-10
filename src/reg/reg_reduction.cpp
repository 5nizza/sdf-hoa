#include "reg_reduction.hpp"

#include "asgn.hpp"
#include "ehoa_parser.hpp"
#include "utils.hpp"
#include "atm_parsing_naming.hpp"


#define BDD spotBDD
    #include "spot/twa/formula2bdd.hh"
    #include "spot/tl/parse.hh"  // for tests in tmp()
#undef BDD


using namespace std;
using namespace sdf;
using formula = spot::formula;
using twa_graph_ptr = spot::twa_graph_ptr;


#define DEBUG(message) {spdlog::get("console")->debug() << message;}  // NOLINT(bugprone-macro-parentheses)
#define INF(message)   {spdlog::get("console")->info()  << message;}  // NOLINT(bugprone-macro-parentheses)

#define hmap unordered_map
#define hset unordered_set


void introduce_labelAPs(const twa_graph_ptr old_ucw,  // NOLINT
                        twa_graph_ptr new_ucw         // NOLINT
                        )
{
    for (const auto& ap : old_ucw->ap())
        if (!is_atm_tst(ap.ap_name()) && !is_atm_asgn(ap.ap_name()))
            new_ucw->register_ap(ap);
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

template<typename P>
tuple<twa_graph_ptr,  // new_ucw
      set<formula>,   // sysTst
      set<formula>,   // sysAsgn
      set<formula>>   // sysOutR
sdf::reduce(DataDomainInterface<P>& domain, const twa_graph_ptr& reg_ucw, uint nof_sys_regs)
{
    // result
    twa_graph_ptr new_ucw;
    set<formula> sysTst, sysAsgn, sysOutR;  // Boolean variables

    auto bdd_dict = spot::make_bdd_dict();
    new_ucw = spot::make_twa_graph(bdd_dict);

    new_ucw->copy_acceptance_of(reg_ucw);
    if (reg_ucw->prop_state_acc())
        new_ucw->prop_state_acc(true);

    auto sysR = build_sysR(nof_sys_regs);
    auto atmR = build_atmR(reg_ucw);
    assert_no_intersection(sysR, atmR);

    domain.introduce_sysActionAPs(sysR, new_ucw, sysTst, sysAsgn, sysOutR);
    introduce_labelAPs(reg_ucw, new_ucw);

    // -- the main part --
    // The register-naming convention (hard-coded) is:
    // - system registers are rs1, rs2, ..., rsK
    // - automaton registers are whatever the spec automaton uses! (I do check that Rs \cap Ra = empty)

    // (technical detail) define hash function and equality for partitions
    using QP = pair<uint, P>;
    auto qp_hasher = [&domain](const QP& qp)
            {
                 auto uint_hasher = [](const uint& i) {return std::hash<uint>()(i);};
                 auto p_hasher = [&domain](const P& p) {return domain.hash(p);};
                 return do_hash_pair(qp, uint_hasher, p_hasher);
            };
    auto qp_equal_to = [&domain](const QP& qp1, const QP& qp2)
            { return qp1.first==qp2.first && domain.total_equal(qp1.second, qp2.second); };

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

    using QPHashSet = hset<QP, decltype(qp_hasher), decltype(qp_equal_to)>;
    auto qp_processed = QPHashSet(0, qp_hasher, qp_equal_to);
    auto qp_todo = QPHashSet(0, qp_hasher, qp_equal_to);

    auto init_qp = make_pair(reg_ucw->get_init_state_number(), domain.build_init_partition(sysR, atmR));
    new_ucw->set_init_state(get_c(init_qp));
    qp_todo.insert(init_qp);

    int i=0, j=0;
    while (!qp_todo.empty())
    {
        DEBUG("qp_todo.size() = " << qp_todo.size() << ", "
              "qp_processed.size() = " << qp_processed.size());

        auto qp = pop_first<QP>(qp_todo);
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
                    let partial_p_io = compute_partial_p_io(p, atm_tst_atoms) // p is complete, tst is partial => result is a partial partition
                    let all_p_io = compute_all_p_io(partial_p_io)             // <-- (should be possible to incorporate sys_asgn-out_r enumeration, but good enough now)
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

                auto partial_p_i = domain.compute_partial_p_io(qp.second, atm_tst_atoms);
                if (!partial_p_i.has_value())  // means atm_tst_atoms are not consistent
                    continue;
                for (const auto& p_io : domain.compute_all_p_io(partial_p_i.value()))
                {
                    // early break:
                    // if o does not belong to the class of i or some sys_reg,
                    // then o cannot be realised
                    if (!domain.out_is_implementable(p_io))
                        continue;

//                    cout << "p_io: " << p_io << ", sys_tst: " << extract_sys_tst_from_p(p_io, sysR) << endl;
//                    vector<vector<formula>> all_assignments;
//                    all_assignments.emplace_back();  // means "emplace the default ctor element"
//                    for (auto&&  reg : sysAsgn)
//                        all_assignments.push_back({reg});
//                    for (const auto& sys_asgn_atoms : all_assignments)  // this give 2x speedup _only_
                    for (const auto& sys_asgn_atoms : all_subsets<formula>(sysAsgn))
                    {
                        j++;
                        auto sys_asgn = compute_asgn(sys_asgn_atoms);
                        auto p_io_ = domain.update(p_io, sys_asgn);
                        auto all_r = domain.pick_R(p_io_, sysR);
                        // p_io is complete, hence we should continue only if `o` lies in one of ECs with system regs in it
                        if (all_r.empty())
                        {
                            i++;
                            continue;
                        }
                        auto p_io__ = domain.update(p_io_, atm_asgn);  // NOLINT(bugprone-reserved-identifier)
                        auto p_succ = domain.remove_io_from_p(p_io__);

                        auto qp_succ = make_pair(e.dst, p_succ);

                        /* add to classical_ucw the edge (q,p) -> (q_succ, p_succ) labelled (sys_tst(p_io) & ap(cube), sys_asgn, out_r) */
                        auto cond = formula::And({asgn_to_formula(sys_asgn, sysAsgn),
                                                  domain.extract_sys_tst_from_p(p_io, sysR),
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

    cout << "#iterations: " << j << ", #breaks: " << i << ", use ratio: " << ((float)(j-i))/j << endl;

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


// Explicit instantiation of the known cases.
// We must explicitly instantiate the template function
// for the known cases, so that the compiler instantiates and constructs
// their definition. Unfortunately, it is not smart enough to
// automatically recognise all the instantiations used in the program (and construct their definitions).

#include "ord_partition.hpp"

template
std::tuple<spot::twa_graph_ptr,
        std::set<spot::formula>,
        std::set<spot::formula>,
        std::set<spot::formula>>
sdf::reduce<OrdPartition>(
        DataDomainInterface<OrdPartition>& domain,
        const spot::twa_graph_ptr& reg_ucw,
        uint nof_sys_regs);



#include "reg/eq_partition.hpp"

void sdf::tmp()
{

//    cout << p1.equal_to(p2) << endl;

}



















