#include "reg/data_domain_interface.hpp"
#include "reg/partition.hpp"
#include "reg/types.hpp"
#include "reg/tst_asgn.hpp"


namespace sdf
{


class MixedDomain: public DataDomainInterface
{
public:
    using P = Partition;

    /** Convert "≥" into two edges "= or >", note that ≠ stays unchanged. */
    spot::twa_graph_ptr
    preprocess(const spot::twa_graph_ptr&);

    /**
     * We start from the partition {ri=rj | ri,rj ∈ Ra},
     * i.e., sys registers are unrelated.
     */
    P
    build_init_partition(const string_hset & sysR,
                         const string_hset & atmR) override;

    /**
     * Given a partition including partial atm tests on input and output,
     * return all possible completed atm tests.
     * In reality, this method does not return the tests and instead
     * returns partitions where values of input/output are fully related with atm registers,
     * and so they can be extracted from each such partition if needed.
     * (The input partition must be complete for Ratm, but can be partial for Rsys.)
     * @param partial_p_io
     * @return partitions with full information on input/output wrt. atm registers
     */
    std::vector<P>
    all_possible_atm_tst(const P& atm_sys_p,
                         const std::unordered_set<TstAtom>& atm_tst_atoms) override;

    /**
     * @param p_io: a partition complete for atm registers and tests
     * @param sys_tst_descr: mapping sys_reg_name -> domain name (> or =).
     *                       Absent registers mean no info about it.
     * @return pairs (partition, sys_tst),
     *         where partition corresponds to the refinement of p_io wrt. sys_tst,
     *         and each sys_tst is allowed by tst_desc
     * Complexity: exponential (unavoidable).
     */
    std::vector<std::pair<P, std::unordered_set<TstAtom>>>
    all_possible_sys_tst(const P& p_io,
                         const std::unordered_map<std::string,DomainName>& sys_tst_descr) override;

    /** Update a given partial partition by performing the assignment. */
    void
    update(P& p, const Asgn& asgn) override;

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
    pick_all_r(const P& p_io) override;

    /** Remove IN and OUT from p. */
    void
    remove_io_from_p(P& p) override;

    bool
    out_is_implementable(const P& partition) override;

};


} // namespace sdf
