#pragma once


#include <string>
#include <unordered_set>

#include "asgn.hpp"
#include "reg/types.hpp"

#define BDD spotBDD
    #include "spot/tl/formula.hh"
    #include "spot/twa/twagraph.hh"
#undef BDD


namespace sdf
{


/** Interface required by function reduce */
template<typename Partition>
class DataDomainInterface
{
public:
    using P = Partition;

    virtual
    P
    build_init_partition(const string_hset& sysR,
                         const string_hset& atmR) = 0;

    virtual
    std::optional<P>
    compute_partial_p_io(const P& p,
                         const std::unordered_set<spot::formula>& atm_tst_atoms) = 0;

    virtual
    std::vector<P>
    compute_all_p_io(const P& partial_p_io) = 0;

    virtual
    P
    update(const P& p, const Asgn& asgn) = 0;

    virtual
    P
    remove_io_from_p(const P& p) = 0;

    virtual
    string_hset
    pick_R(const P& p_io,
           const string_hset& sysR) = 0;

    virtual
    bool
    out_is_implementable(const P& partition) = 0;

    // ----------------------------------------------------------------------------------------- //

    virtual
    void
    introduce_sysActionAPs(const string_hset& sysR,
                           spot::twa_graph_ptr classical_ucw,  // NOLINT
                           std::set<spot::formula>& sysTst,
                           std::set<spot::formula>& sysAsgn,
                           std::set<spot::formula>& sysOutR) = 0;

    virtual
    spot::formula
    extract_sys_tst_from_p(const P& p,
                           const string_hset& sysR) = 0;

    // ----------------------------------------------------------------------------------------- //

    virtual
    size_t hash(const P& p) = 0;

    // assumes the partitions are total
    virtual
    bool total_equal(const P& total_p1, const P& total_p2) = 0;
};


} // namespace sdf
