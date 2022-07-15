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

class DataDomainInterface
{
public:
    using P = Partition;

    virtual
    P
    build_init_partition(const std::unordered_set<std::string>& sysR,
                         const std::unordered_set<std::string>& atmR) = 0;

    virtual
    std::vector<P>
    all_possible_atm_tst(const P& p, const std::unordered_set<spot::formula>& atm_tst_atoms) = 0;

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

    virtual
    string_hset
    pick_all_r(const P& p_io) = 0;

    virtual
    bool
    out_is_implementable(const P& partition) = 0;

    // ----------------------------------------------------------------------------------------- //

    virtual
    std::set<spot::formula>
    construct_sysTstAP(const std::unordered_set<std::string>& sysR) = 0;
};


} // namespace sdf
