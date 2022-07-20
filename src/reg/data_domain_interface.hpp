#pragma once


#include <string>
#include <unordered_set>

#include "tst_asgn.hpp"
#include "reg/types.hpp"
#include "reg/partition.hpp"

#define BDD spotBDD
    #include "spot/tl/formula.hh"
    #include "spot/twa/twagraph.hh"
#undef BDD


namespace sdf
{

enum DomainName { order, equality };

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
    all_possible_atm_tst(const P& p, const std::unordered_set<TstAtom>& atm_tst_atoms) = 0;

    virtual
    std::vector<std::pair<P, std::unordered_set<TstAtom>>>
    all_possible_sys_tst(const P& p_io,
                         const std::unordered_map<std::string,DomainName>& sys_tst_descr) = 0;

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
};


} // namespace sdf
