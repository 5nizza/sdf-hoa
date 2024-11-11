#pragma once

#include <string>
#include <set>
#include <unordered_map>

#include <cuddObj.hh>


namespace sdf
{
    struct QcirObject
    {
        std::string qcir_str;
        std::set<uint> used_forall_indices;
        std::set<uint> used_exists_indices;
        std::unordered_map<uint,uint> qcir_by_cudd;  // QCIR variable by Cudd variable (for forall/exists variables only, i.e., for input/output/state variables)
    };

    /**
     * Generate QCIR representation for formula
     *   ∀forall_vars.∃exists_vars: premise -> conclusion
     * When @param preserve_structure is true, encode premise and conclusion separately (preserving the implication structure),
     * otherwise first create a BDD for premise->conclusion, then encode it into QCIR.
     * When @param do_not_share_nodes is true, the premise and conclusion do not share intermediate gate variables
     * (this is achieved by walking their BDDs with BDD visitors that do not share the cache).
     * The function uses only those exists_indices that are used by the used formula (the set can shrink: for instance, when preserve_structure=false).
     */
    QcirObject generate_qcir_implication(const Cudd& cudd,
                                         const BDD& premise, const BDD& conclusion,
                                         const std::set<uint>& exists_indices,
                                         bool preserve_structure,
                                         bool do_not_share_nodes,
                                         bool create_cleansed_qcir);  // the last parameter is used as output only

    /**
     * Generate QCIR representation for formula
     *   ∀forall_vars.∃exists_vars: W(t) -> ¬error(t,i,o) ∧ W(t') ∧ (t' <-> trans_function_for_t)
     * The function uses only those exists_indices that are used by the used formula.
     */
    QcirObject generate_qcir_implication2(const Cudd& cudd,
                                          const BDD& win_region, const BDD& error, const std::unordered_map<uint,BDD>& pre_trans_func,
                                          const std::set<uint>& exists_indices,
                                          bool create_cleansed_qcir);  // the last parameter is used as output only
}  // namespace sdf
