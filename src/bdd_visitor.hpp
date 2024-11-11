#pragma once

#include <unordered_map>
#include <functional>

#include <cuddObj.hh>


namespace sdf
{

/**
 * Backwards-exploration game solver using BDDs.
 */
class BDDVisitor
{
public:
    using FnEnterType = std::function<void(DdNode*)>;
    using FnCreateIteType = std::function<int(int,int,int)>;
    using FnNegateType = std::function<int(int)>;

public:
    BDDVisitor(const Cudd& cudd,
               const std::unordered_map<uint, uint>& lit_by_cudd,
               const FnEnterType& fn_enter,          // NOLINT(*-pass-by-value)
               const FnCreateIteType& fn_create_ite, // NOLINT(*-pass-by-value)
               const FnNegateType& fn_negate):       // NOLINT(*-pass-by-value)
               cudd(cudd),
               var_by_cudd(lit_by_cudd),
               fn_enter(fn_enter),
               fn_create_ite(fn_create_ite),
               fn_negate(fn_negate)
    {
    }

    /**
     * Can be called several times. The cache persists between the calls.
     */
    int walk(DdNode*);

private:
    const Cudd& cudd;
    /**
     * Mapping from cudd indexes to regular (non-complemented) literals.
     * Note: the mapping maps only inputs, outputs, latches.
     */
    const std::unordered_map<uint, uint> var_by_cudd;

    FnEnterType fn_enter;
    FnCreateIteType fn_create_ite;
    FnNegateType fn_negate;

private:
    std::unordered_map<DdNode*, int> cache;
};


} // namespace sdf
