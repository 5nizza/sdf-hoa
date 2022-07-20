#include "reg/tst_asgn.hpp"

namespace sdf
{

std::ostream& operator<<(std::ostream& os, const Asgn& asgn_)
{
    if (asgn_.asgn.empty())
        return os << "<keep>";

    os << "{ ";
    os << join(", ", asgn_.asgn, [](auto p) { return p.first + "↦" + "{" + join(",", p.second) + "}"; });
    os << " }";

    return os;
}


std::ostream&
operator<<(std::ostream& os, const TstAtom& tst)
{
    std::string relation_str =
            tst.relation == TstAtom::less ? "<" :
            tst.relation == TstAtom::equal ? "=" :
            tst.relation == TstAtom::nequal ? "≠" :
            "unknown";

    os << tst.t1 << relation_str << tst.t2;
    return os;
}



}  // namespace sdf
