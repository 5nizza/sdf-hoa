#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "my_assert.hpp"
#include "reg_utils.hpp"

namespace sdf
{

// FIXME: remove? it does not seem to be very useful

/* Mapping : Var->SetOfReg  describing where to store a value of given variable. */
struct Asgn
{
    const std::unordered_map<std::string, std::unordered_set<std::string>> asgn;  // io -> {r1,r2,...}

    explicit Asgn(const std::unordered_map<std::string, std::unordered_set<std::string>>& asgn) : asgn(asgn) {}

    friend std::ostream&
    operator<<(std::ostream& os, const Asgn& asgn_)  // have to define it here otherwise gets invisible to the linker
    {
        if (asgn_.asgn.empty())
            return os << "<keep>";

        os << "{ ";
        os << join(", ", asgn_.asgn, [](auto p) { return p.first + "↦" + "{" + join(",", p.second) + "}"; });
        os << " }";

        return os;
    }
};

} // namespace sdf