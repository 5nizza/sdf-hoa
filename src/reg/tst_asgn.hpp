#pragma once

#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "my_assert.hpp"
#include "reg/reg_utils.hpp"


namespace sdf
{

/** Mapping : Var->SetOfReg  describing where to store a value of given variable. */
struct Asgn
{
    std::unordered_map<std::string, std::unordered_set<std::string>> asgn;  // io -> {r1,r2,...}

    explicit
    Asgn(const std::unordered_map<std::string, std::unordered_set<std::string>>& asgn) : asgn(asgn) {}  // NOLINT

    friend std::ostream&
    operator<<(std::ostream& os, const Asgn& asgn_);
};


/** t1 {<,=,≠} t2 */
struct TstAtom
{
    enum Relation { less, equal, nequal };

    std::string t1, t2;
    Relation relation;

    explicit
    TstAtom(const std::string& t1, const Relation& relation, const std::string& t2): t1(t1), t2(t2), relation(relation){}  // NOLINT

    bool operator==(const TstAtom& rhs) const
    {
        return t1 == rhs.t1 &&
               t2 == rhs.t2 &&
               relation == rhs.relation;
    }
    bool operator!=(const TstAtom& rhs) const
    {
        return !(rhs == *this);
    }

    friend std::ostream&
    operator<<(std::ostream& os, const TstAtom& tst);
};

} // namespace sdf


namespace std
{
    // specialisation to be able to store TstAtom in unordered_set
    template<>
    struct hash<sdf::TstAtom>
    {
        size_t
        operator()(const sdf::TstAtom& tst_atom) const
        {
            return hash<string>()(tst_atom.t1)
                    + 17*hash<string>()(tst_atom.t2)
                    + 31*hash<byte>()(static_cast<byte>(tst_atom.relation));
        }
    };
} // std




















