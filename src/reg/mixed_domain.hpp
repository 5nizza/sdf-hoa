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

    /** Convert "≥" into two edges "= or >"; ≠ stays unchanged. */
    spot::twa_graph_ptr
    preprocess(const spot::twa_graph_ptr&);

    /**
     * We start from the partition {ri=rj | ri,rj ∈ Ra},
     * i.e., sys registers are unrelated.
     */
    P
    build_init_partition(const string_hset & sysR,
                         const string_hset & atmR);

    /**
     * Enumerates all possible refinements of a given test for a given partition.
     * @param atm_sys_p : partition complete for atm registers
     * @param atm_tst_atoms : a (partial) automaton test
     * @param reg_descr : mapping atm_register -> DomainName
     */
    std::vector<std::pair<P, std::unordered_set<TstAtom>>>
    all_possible_atm_tst(const P& atm_sys_p,
                         const std::unordered_set<TstAtom>& atm_tst_atoms);

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

    /** Update a given partial partition by performing the assignment. */
    void
    update(P& p, const Asgn& asgn);

    /**
     * From a given partial partition,
     * compute system registers compatible with OUT.
     * This method simply returns the system registers that reside in the same EC as OUT.
     * Such registers for sure have the same value as OUT.
     * Note: if sys reg and OUT are in different ECs c1 and c2, and c1 c2 are not related,
     *       we do not know whether c1 and c2 are equal or not,
     *       and do not return such a reg.
     */
    string_hset
    pick_all_r(const P& p_io);

    /** Remove IN and OUT from p. */
    void
    remove_io_from_p(P& p);

    bool
    out_is_implementable(const P& partition);

};


} // namespace sdf
