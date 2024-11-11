#include "bdd_visitor.hpp"


using namespace std;


int sdf::BDDVisitor::walk(DdNode* node)
{
    /**
    Walk given DdNode node (recursively).
    If a given node requires intermediate AND gates for its representation, this function adds them.
    Literal representing given input node is `not` added to the spec.

    :returns: literal representing input node
    **/
    fn_enter(node);

    auto cached_lit = cache.find(Cudd_Regular(node));
    if (cached_lit != cache.end())
        return Cudd_IsComplement(node) ? fn_negate(cached_lit->second) : cached_lit->second;

    if (Cudd_IsConstant(node))
    {
        // Cudd represents both bddOne and bddZero by a single node,
        // we don't know which one is the authentic node and which one is the complement.
        // The mapping var_by_cudd maps the authentic node only.
        DdNode* authentic_node = (Cudd_Regular(cudd.bddZero().getNode()) == cudd.bddZero().getNode())?
                cudd.bddZero().getNode():
                cudd.bddOne().getNode();
        auto authentic_var = var_by_cudd.at(Cudd_NodeReadIndex(authentic_node));

        if (node == authentic_node)
            return (int)authentic_var;
        else
            return fn_negate((int)authentic_var);
    }

    DdNode *t_bdd = Cudd_T(node);
    DdNode *e_bdd = Cudd_E(node);

    int t_lit = walk(t_bdd);
    int e_lit = walk(e_bdd);
    uint a_reg_lit = var_by_cudd.at(Cudd_NodeReadIndex(node));  // a_reg_lit is a regular (non-negated) literal

    int res = fn_create_ite((int)a_reg_lit, t_lit, e_lit);

    cache[Cudd_Regular(node)] = res;

    if (Cudd_IsComplement(node))
        res = fn_negate(res);

    return res;
}
