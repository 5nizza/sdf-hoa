#pragma once


#include <string>
#include <unordered_set>

#include "asgn.hpp"
#include "reg/types.hpp"
#include "reg/partition.hpp"

#define BDD spotBDD
    #include "spot/tl/formula.hh"
    #include "spot/twa/twagraph.hh"
#undef BDD


namespace sdf
{

enum Relation { less = '<', equal = '=' };

/** Interface required by function reduce */
class DataDomainInterface
{
public:
    using P = Partition;

    virtual
    P
    build_init_partition(const std::unordered_set<std::string>& sysR,
                         const std::unordered_set<std::string>& atmR) = 0;

    /**
     * Note: modifies @param p in-place
     * @return false if atm_tst_atoms are not consistent, true otherwise
     */
    virtual
    bool
    compute_partial_p_io(P& p,
                         const std::unordered_set<spot::formula>& atm_tst_atoms) = 0;

    /**
     * (Here, partitions in general are partial.)
     * Given a partition including partial atm tests on input and output,
     * return all possible completed atm tests.
     * In reality, this method does not return the tests and instead
     * returns partitions where values of input/output are fully related with atm registers,
     * and so they can be extracted from each such partition if needed.
     * @param partial_p_io
     * @return partitions with full information on input/output wrt. atm registers
     */
    virtual
    std::vector<std::pair<P, spot::formula>>
    all_possible_atm_tst(const P& partial_p_io, const std::unordered_set<spot::formula>& atm_tst_atoms) = 0;

    /**
     * @param p_io: a partition complete for atm registers and tests
     * @param sys_tst_descr: mapping sys_reg_name -> relation_type.
     *                       Absent registers mean no info about it.
     *                       For instance: {"rs1" -> Relation::less, "rs2" -> Relation::equal}.
     * @return pairs (partition, sys_tst),
     *         where partition corresponds to the refinement of p_io wrt. sys_tst,
     *         and each sys_tst is an allowed test wrt. tst_desc
     */
    virtual
    std::vector<std::pair<P, spot::formula>>
    all_possible_sys_tst(const P& p_io,
                         const std::unordered_map<std::string,Relation>& sys_tst_descr) = 0;

    virtual
    void
    update(P& p, const Asgn& asgn) = 0;

    virtual
    void
    remove_io_from_p(P& p) = 0;

    /**
     * From a given partial partition,
     * compute system registers compatible with OUT.
     * This method simply returns the system registers that reside in the same EC as OUT.
     * Such registers for sure have the same value as OUT.
     * Note: if sys reg and OUT are in different ECs, c1 and c2, and c1 c2 are not related,
     *       we do not know whether c1 and c2 are equal or not,
     *       and do not return such a reg.
    */
    virtual
    string_hset
    pick_all_r(const P& p_io) = 0;

    virtual
    bool
    out_is_implementable(const P& partition) = 0;

    // ----------------------------------------------------------------------------------------- //

    /**
     * @param sysR system registers (NB: we assume throughout that they satisfy is_sys_reg_name)
     * @return newly created system-test atomic propositions
     */
    virtual
    std::set<spot::formula>
    construct_sysTstAP(const std::unordered_set<std::string>& sysR) = 0;
};


} // namespace sdf
