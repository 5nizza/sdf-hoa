#include "generate_qcir.hpp"
#include "bdd_visitor.hpp"
#include "utils.hpp"

#include <unordered_map>
#include <format>


using namespace sdf;
using namespace std;


#define hset unordered_set
#define hmap unordered_map


uint create_ite(string& gates_def, uint& next_free_var,
                int a, int t, int e)
{
    auto gate_var = next_free_var++;
    gates_def += format("{} = ite({}, {}, {})\n", gate_var, a, t, e);
    return gate_var;
};

int get_optimized_and(string& gates_def, uint& next_free_var, int true_lit, int false_lit,
                      int a, int b)
{
    if (a == false_lit || b == false_lit)
        return false_lit;

    if (a == true_lit && b == true_lit)
        return true_lit;

    if (a == true_lit)
        return b;

    if (b == true_lit)
        return a;

    // a_lit and b_lit are non-constants
    auto and_lit = next_free_var++;
    gates_def += format("{} = and({}, {})\n", and_lit, a, b);
    return (int)and_lit;
}

int create_ite_via_and(string& gates_def, uint& next_free_var, const BDDVisitor::FnNegateType& op_neg, int true_lit, int false_lit,
                       int a, int t, int e)
{
    // ite(a, then, else)
    // = a*then + !a*else
    // = !(!(a*then) * !(!a*else))

    auto op_and = [&](int x, int y){ return get_optimized_and(gates_def, next_free_var, true_lit, false_lit,
                                                              x, y);
                                   };
    return op_neg(op_and(op_neg(op_and(a, t)),
                         op_neg(op_and(op_neg(a), e))));
};

QcirObject sdf::generate_qcir_implication(const Cudd& cudd,
                                          const BDD& premise,
                                          const BDD& conclusion,
                                          const set<uint>& exists_indices,
                                          bool preserve_structure,
                                          bool do_not_share_cache,
                                          bool create_cleansed_qcir)  // cudd indices
{
    uint next_free_var = 1;  // skip 0 to simplify the negation

    hmap<uint,uint> qcir_by_cudd;  // QCIR variable by Cudd variable

    // create literal true and map it in qcir_by_cudd
    MASSERT(Cudd_Regular(cudd.bddOne().getNode()) == cudd.bddOne().getNode(), "We rely on Cudd 3.0 defining bddOne as the authentic node and bddZero as its negation.");
    qcir_by_cudd[Cudd_NodeReadIndex(cudd.bddOne().getNode())] = next_free_var;
    string false_true_def = format("{} = and()\n", next_free_var);
    int lit_true = (int)next_free_var;
    ++next_free_var;

    // prepare to walk the BDD
    string gates_def;
    BDDVisitor::FnEnterType fn_enter = [](DdNode*) { };
    BDDVisitor::FnNegateType fn_negate = [](int l) { return -l; };
    BDDVisitor::FnCreateIteType fn_create_ite;
    if (create_cleansed_qcir)
        fn_create_ite = [&](int a, int t, int e) { return create_ite_via_and(gates_def, next_free_var, fn_negate, lit_true, fn_negate(lit_true), a, t, e); };
    else
        fn_create_ite = [&](int a, int t, int e) { return create_ite(gates_def, next_free_var, a, t, e); };

    int formula_lit;
    set<uint> all_cudd_indices;  // we use `set` instead of `hset` because `set` has a deterministic order which is convenient

    // Depending on the requested shape of QCIR,
    // we walk premise->conclusion or walk separately premise and conclusion.
    // The definition of all_cudd_indices also differs.
    if (preserve_structure)
    {
        for (const auto& f: {premise.SupportIndices(), conclusion.SupportIndices()})
            all_cudd_indices.insert(f.begin(), f.end());

        for (auto v : all_cudd_indices)
            qcir_by_cudd[v] = next_free_var++;

        int premise_lit;
        int conclusion_lit;

        if (do_not_share_cache)
        {
            // the two visitors do not share the cache, hence the formulas do not share the gates
            auto premise_visitor = BDDVisitor(cudd, qcir_by_cudd, fn_enter, fn_create_ite, fn_negate);
            auto conclusion_visitor = BDDVisitor(cudd, qcir_by_cudd, fn_enter, fn_create_ite, fn_negate);

            premise_lit = premise_visitor.walk(premise.getNode());
            conclusion_lit = conclusion_visitor.walk(conclusion.getNode());
        }
        else
        {
            auto visitor = BDDVisitor(cudd, qcir_by_cudd, fn_enter, fn_create_ite, fn_negate);

            premise_lit = visitor.walk(premise.getNode());
            conclusion_lit = visitor.walk(conclusion.getNode());
        }

        formula_lit = fn_create_ite(premise_lit, conclusion_lit, lit_true);
    }
    else
    {
        auto formula = ~premise | conclusion;
        all_cudd_indices = to_container<set<uint>>(formula.SupportIndices());
        for (auto v: all_cudd_indices)
            qcir_by_cudd[v] = next_free_var++;

        formula_lit = BDDVisitor(cudd, qcir_by_cudd, fn_enter, fn_create_ite, fn_negate).walk(formula.getNode());
    }

    auto used_exists_indices = a_intersection_b(exists_indices, all_cudd_indices);  // some vars of exists_indices might be not used in the formula
    auto used_forall_indices = a_minus_b(all_cudd_indices, used_exists_indices);

    string qcir;
    qcir += format("#QCIR-G14 {}\n", next_free_var-1);
    qcir += format("forall({})\n", join(", ", used_forall_indices, [&qcir_by_cudd](auto v){ return qcir_by_cudd.at(v); }));
    qcir += format("exists({})\n", join(", ", used_exists_indices, [&qcir_by_cudd](auto v){ return qcir_by_cudd.at(v); }));
    qcir += format("output({})\n", formula_lit);
    qcir += false_true_def;
    qcir += gates_def;

    return QcirObject{.qcir_str=qcir,
                      .used_forall_indices=used_forall_indices,
                      .used_exists_indices=used_exists_indices,
                      .qcir_by_cudd=qcir_by_cudd};
}

QcirObject sdf::generate_qcir_implication2(const Cudd& cudd,
                                           const BDD& win_region,
                                           const BDD& error,
                                           const unordered_map<uint,BDD>& pre_trans_func,
                                           const set<uint>& exists_indices,
                                           bool create_cleansed_qcir)  // cudd indices
{
    uint next_free_var = 1;  // skip 0 to simplify the negation

    hmap<uint,uint> qcir_by_cudd;  // QCIR variable by Cudd variable

    // create literal true and map it in qcir_by_cudd
    MASSERT(Cudd_Regular(cudd.bddOne().getNode()) == cudd.bddOne().getNode(), "We rely on Cudd 3.0 defining bddOne as the authentic node and bddZero as its negation.");
    qcir_by_cudd[Cudd_NodeReadIndex(cudd.bddOne().getNode())] = next_free_var;
    string false_true_def = format("{} = and()\n", next_free_var);
    int lit_true = (int)next_free_var;
    ++next_free_var;

    // prepare to walk the BDD
    string gates_def;
    BDDVisitor::FnEnterType fn_enter = [](DdNode*) { };
    BDDVisitor::FnNegateType fn_negate = [](int l) { return -l; };
    BDDVisitor::FnCreateIteType fn_create_ite;
    if (create_cleansed_qcir)
        fn_create_ite = [&gates_def, &next_free_var, &fn_negate, &lit_true](int a, int t, int e) { return create_ite_via_and(gates_def, next_free_var, fn_negate, lit_true, fn_negate(lit_true), a, t, e); };
    else
        fn_create_ite = [&gates_def, &next_free_var, &fn_negate, &lit_true](int a, int t, int e) { return create_ite(gates_def, next_free_var, a, t, e); };

    set<uint> all_cudd_indices;  // we use `set` instead of `hset` because `set` has a deterministic order, which is convenient
    insert_all(win_region.SupportIndices(), all_cudd_indices);
    insert_all(error.SupportIndices(), all_cudd_indices);
    for (const auto& [state_idx,state_func]: pre_trans_func)
        insert_all(state_func.SupportIndices(), all_cudd_indices);

    for (auto v : all_cudd_indices)
        qcir_by_cudd[v] = next_free_var++;

    auto visitor = BDDVisitor(cudd, qcir_by_cudd, fn_enter, fn_create_ite, fn_negate);

    int win_region_lit = visitor.walk(win_region.getNode());
    int error_lit = visitor.walk(error.getNode());

    // ∀forall_vars.∃exists_vars: W(t) -> ¬error(t,i,o) ∧ W[t <- trans_function_lit_for_t]

    // Walk win_region using state_trans_lit instead of the original state literals.
    // We cannot use the visitor used for walking the win_region because its cache memorizes wrt. non-primed variables.
    // We also update qcir_by_cudd: it re-maps cudd state variables to state_trans_lit literals.

    auto new_qcir_by_cudd = qcir_by_cudd;  // copy
    for (const auto& [state_idx,state_func]: pre_trans_func)
    {
        // use old visitor to create definitions (gates) for transition functions
        int state_trans_lit = visitor.walk(state_func.getNode());
        new_qcir_by_cudd[state_idx] = state_trans_lit;
    }
    auto fresh_visitor = BDDVisitor(cudd, new_qcir_by_cudd, fn_enter, fn_create_ite, fn_negate);  // assumption: fn functions do not depend on qcir_by_cudd
    int win_region_lit_primed = fresh_visitor.walk(win_region.getNode());

    int formula_lit = fn_create_ite(win_region_lit,
                                    get_optimized_and(gates_def, next_free_var, lit_true, fn_negate(lit_true),
                                                      fn_negate(error_lit), win_region_lit_primed),
                                    lit_true);

    auto used_exists_indices = a_intersection_b(exists_indices, all_cudd_indices);  // some vars of exists_indices might be not used in the formula
    auto used_forall_indices = a_minus_b(all_cudd_indices, used_exists_indices);

    string qcir;
    qcir += format("#QCIR-G14 {}\n", next_free_var-1);
    qcir += format("forall({})\n", join(", ", used_forall_indices, [&qcir_by_cudd](auto v){ return qcir_by_cudd.at(v); }));
    qcir += format("exists({})\n", join(", ", used_exists_indices, [&qcir_by_cudd](auto v){ return qcir_by_cudd.at(v); }));
    qcir += format("output({})\n", formula_lit);
    qcir += false_true_def;
    qcir += gates_def;

    return {.qcir_str=qcir,
            .used_forall_indices=used_forall_indices,
            .used_exists_indices=used_exists_indices,
            .qcir_by_cudd=qcir_by_cudd};
}
