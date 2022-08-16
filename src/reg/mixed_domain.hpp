#pragma once

#include "reg/partition.hpp"
#include "reg/types.hpp"
#include "reg/tst_asgn.hpp"

#define BDD spotBDD
    #include "spot/twa/twagraph.hh"
#undef BDD


namespace sdf
{

enum DomainName { order, equality };


class MixedDomain
{
private:
    const DomainName background_domain;

public:
    using P = Partition;

    /** @param background_domain is the domain in which all registers -- system and automaton -- reside. */
    explicit
    MixedDomain(const DomainName& background_domain): background_domain(background_domain) {}

    /** Convert "≥" into two edges "= or >"; ≠ stays unchanged. */  // TODO: remove this method (don't mention spot here)
    spot::twa_graph_ptr
    preprocess(const spot::twa_graph_ptr&);

    /** We start from the partition 'true' (no relations). */
    P
    build_init_partition(const string_hset & sysR,
                         const string_hset & atmR);

    /**
     * Refine a given partition wrt. test atoms.
     * @returns: true if refinement results in a consistent partition,
     *           false if not
     * @note: has side-effects on p
     */
    bool
    refine(P& p, const std::unordered_set<TstAtom>& tst_atoms);

    /** Update a given partial partition by performing the assignment. */
    void
    update(P& p, const Asgn& asgn);

    /** Add IN and OUT vertices to p. */
    void
    add_io_to_p(P& p);

    /** Remove IN and OUT vertices from p. */
    void
    remove_io_from_p(P& p);

    /**
     * @param p_io: a partition complete for atm registers and tests
     * @param reg_descr: mapping sys_reg_name -> domain name (> or =).
     *                   Absent registers mean no info about it.
     * @return pairs (partition, sys_tst),
     *         where partition corresponds to the refinement of p_io wrt. sys_tst,
     *         and each sys_tst is allowed by tst_desc
     * Complexity: exponential (unavoidable).
     * TODO: check that reg_descr does not use the domain stronger than the background domain
     */
    std::vector<std::pair<P, std::unordered_set<TstAtom>>>
    all_possible_sys_tst(const P& p_io,
                         const std::unordered_map<std::string,DomainName>& reg_descr);

};


}  // namespace sdf
