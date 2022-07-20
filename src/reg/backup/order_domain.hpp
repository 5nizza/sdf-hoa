#include "data_domain_interface.hpp"
#include "ord_partition.hpp"
#include "eq_partition.hpp"


namespace sdf
{



class OrderDomain: public DataDomainInterface<OrdPartition>
{
public:
    using P = OrdPartition;

    // Convert "≥" into "= or >", convert "≠" into "< or >".
    spot::twa_graph_ptr
    preprocess(const spot::twa_graph_ptr&);

    P
    build_init_partition(const std::unordered_set<std::string>& sysR,
                         const std::unordered_set<std::string>& atmR) override;

    /* Returns the partial partition constructed from p and atm_tst_atoms (if possible).
     * Assumes atm_tst_atoms are of the form "<" or "=", or the test is True, but not "≤" nor "≠". */
    std::optional<P>
    compute_partial_p_io(const P& p,
                         const std::unordered_set<spot::formula>& atm_tst_atoms) override;

    std::vector<P>
    compute_all_p_io(const P& partial_p_io) override;

    P
    update(const P& p, const Asgn& asgn) override;

    P
    remove_io_from_p(const P& p) override;

    std::unordered_set<std::string>
    pick_R(const P& p_io) override;

    bool
    out_is_implementable(const P& partition) override;

    // ----------------------------------------------------------------------------------------- //

    std::set<spot::formula>
    construct_sysTstAP(const std::unordered_set<std::string>& sysR) override;

    spot::formula
    extract_sys_tst_from_p(const P& p) override;

    // ----------------------------------------------------------------------------------------- //

    size_t
    hash(const P&) override;

    // assumes the partitions are total
    bool
    total_equal(const P& total_p1, const P& total_p2) override;
};


} // namespace sdf
