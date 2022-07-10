#include "reg/data_domain_interface.hpp"
#include "reg/partition.hpp"
#include "reg/types.hpp"


namespace sdf
{


class MixedDomain: public DataDomainInterface
{
public:
    using P = Partition;

    // Convert "≥" into "= or >", convert "≠" into "< or >".
    spot::twa_graph_ptr
    preprocess(const spot::twa_graph_ptr&);

    /**
     * We start from the partition {ri=rj | ri,rj ∈ Ra},
     * i.e., sys registers are unrelated.
     */
    P
    build_init_partition(const string_hset & sysR,
                         const string_hset & atmR) override;

    bool
    compute_partial_p_io(P& p,
                         const std::unordered_set<spot::formula>& atm_tst_atoms) override;

    std::vector<std::pair<P, spot::formula>>
    all_possible_atm_tst(const P& partial_p_io, const std::unordered_set<spot::formula>& atm_tst_atoms) override;

    std::vector<std::pair<P, spot::formula>>
    all_possible_sys_tst(const P& p_io,
                         const std::unordered_map<std::string,Relation>& sys_tst_descr) override;

    void
    update(P& p, const Asgn& asgn) override;

    void
    remove_io_from_p(P& p) override;

    string_hset
    pick_all_r(const P& p_io) override;

    bool
    out_is_implementable(const P& partition) override;

    // ----------------------------------------------------------------------------------------- //

    std::set<spot::formula>
    construct_sysTstAP(const string_hset & sysR) override;

};


} // namespace sdf
