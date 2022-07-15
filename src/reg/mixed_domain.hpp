#include "reg/data_domain_interface.hpp"
#include "reg/partition.hpp"
#include "reg/types.hpp"


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
     * The input partition must be complete for Ratm, but can be partial for Rsys.
     * Given a partition including partial atm tests on input and output,
     * return all possible completed atm tests.
     * In reality, this method does not return the tests and instead
     * returns partitions where values of input/output are fully related with atm registers,
     * and so they can be extracted from each such partition if needed.
     * @param partial_p_io
     * @return partitions with full information on input/output wrt. atm registers
     */
    std::vector<P>
    all_possible_atm_tst(const P& atm_sys_p, const std::unordered_set<spot::formula>& atm_tst_atoms) override;

    /**
     * @param p_io: a partition complete for atm registers and tests
     * @param sys_tst_descr: mapping sys_reg_name -> relation_type.
     *                       Absent registers mean no info about it.
     *                       For instance: {"rs1" -> Relation::less, "rs2" -> Relation::equal}.
     * @return pairs (partition, sys_tst),
     *         where partition corresponds to the refinement of p_io wrt. sys_tst,
     *         and each sys_tst is an allowed test wrt. tst_desc
     */
    std::vector<std::pair<P, spot::formula>>
    all_possible_sys_tst(const P& p_io,
                         const std::unordered_map<std::string,Relation>& sys_tst_descr) override;

    void
    update(P& p, const Asgn& asgn) override;

    void
    remove_io_from_p(P& p) override;


    /**
     * From a given partial partition,
     * compute system registers compatible with OUT.
     * This method simply returns the system registers that reside in the same EC as OUT.
     * Such registers for sure have the same value as OUT.
     * Note: if sys reg and OUT are in different ECs, c1 and c2, and c1 c2 are not related,
     *       we do not know whether c1 and c2 are equal or not,
     *       and do not return such a reg.
     */
    string_hset
    pick_all_r(const P& p_io) override;

    bool
    out_is_implementable(const P& partition) override;

    // ----------------------------------------------------------------------------------------- //

    /**
     * @param sysR system registers (NB: we assume throughout that they satisfy is_sys_reg_name)
     * @return newly created system-test atomic propositions
     */
    std::set<spot::formula>
    construct_sysTstAP(const string_hset & sysR) override;

};


} // namespace sdf
