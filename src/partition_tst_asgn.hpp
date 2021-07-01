#pragma once
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-pass-by-value"


#include <string>
#include <vector>
#include <map>
#include <set>
#include <ostream>
#include <list>
#include "my_assert.hpp"


namespace sdf
{

// TODO: fix visibility (struct or class?)

struct Tst;
struct Asgn;
struct Partition;

/*
 * Example: "r1<r2=r3<r4"
 * Internally, this is represented by list<set<string>>:
 *   [{r1}, {r2,r3}, {r4}]
 */
struct Partition
{
    using EC = std::set<std::string>;
    std::list<EC> repr;

    Partition() = default;
    explicit Partition(const std::list<EC>& repr_): repr(repr_) { }

    std::set<std::string>& get_class_of(const std::string& r);

    [[nodiscard]] std::set<std::string> getR() const;  // return non-primed registers of the partition

    [[nodiscard]] std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& out, const Partition& p) { out << p.to_str(); return out; }

    friend struct Algo;

    // these are needed to be able to insert Partition into a set
    bool operator<(const Partition &rhs) const;
    bool operator>(const Partition &rhs) const;
    bool operator<=(const Partition &rhs) const;
    bool operator>=(const Partition &rhs) const;
    bool operator==(const Partition &rhs) const;
    bool operator!=(const Partition &rhs) const;
};

/*
 * Examples:
 * "r1<d<r2"
 * "d=r3"
 */
struct Cmp
{
//    enum CmpType {
//        EQUAL,
//        BELOW,
//        ABOVE,
//        BETWEEN
//    } type;

    /* the implicit invariant:
     *   rL=rU=""                    type is TRUE (no constraint)
     *   rL==rU:                     type is EQUAL (*=r)
     *   rL != rU and both nonempty: type is BETWEEN (rL<*<rU)
     *   rL != "" and rU == "":      type is ABOVE (rL<*)
     *   rL == "" and rU != "":      type is BELOW (*<rU)
     */
    const std::string rL;
    const std::string rU;

    //
    static Cmp True()
    {
        return Cmp("", "");
    }

    // "*=r1"
    static Cmp Equal(const std::string& r)
    {
        MASSERT(!r.empty(), r);
        return Cmp(r, r);
    }

    // "r1<*<r2"
    static Cmp Between(const std::string& rL, const std::string& rU)
    {
        MASSERT(!rL.empty() && !rU.empty() && rL != rU, "rL='" << rL << "', rU='" << rU << "'");
        return Cmp(rL, rU);
    }

    // "*<r1" (note: r1 must be the smallest register)
    static Cmp Below(const std::string& rU)
    {
        MASSERT(!rU.empty(), "rU='" << rU << "'");
        return Cmp("", rU);
    }

    // "r2<*" (note: r2 must be the largest register)
    static Cmp Above(const std::string& rL)
    {
        MASSERT(!rL.empty(), "rL='" << rL << "'");
        return Cmp(rL, "");
    }

    [[nodiscard]] bool is_TRUE() const    { return rL.empty() && rU.empty(); }
    [[nodiscard]] bool is_ABOVE() const   { return !rL.empty() && rU.empty(); }
    [[nodiscard]] bool is_BELOW() const   { return rL.empty() && !rU.empty(); }
    [[nodiscard]] bool is_EQUAL() const   { return rL == rU && !rL.empty(); }
    [[nodiscard]] bool is_BETWEEN() const { return rL != rU && !rL.empty() && !rU.empty(); }

    // (to be able to add to set)
    bool operator==(const Cmp &rhs) const;
    bool operator!=(const Cmp &rhs) const;
    bool operator<(const Cmp &rhs) const;
    bool operator>(const Cmp &rhs) const;
    bool operator<=(const Cmp &rhs) const;
    bool operator>=(const Cmp &rhs) const;

private:
    explicit Cmp(const std::string& rL, const std::string& rU): rL(rL), rU(rU) { }

public:
    [[nodiscard]] std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& out, const Cmp& cmp) { out << cmp.to_str(); return out; }

    friend struct Algo;
};

/*
 * Example: "(i<o, i=r2, r1<o<r2)"
 * Thus, a test contains:
 * - i<o: the partition of data (among themselves)
 *   Internally, we use Partition over data
 * - Each data maps to Cmp:
 *   i -> i=r2
 *   o -> r1<o<r2
 *   Internally, we use map<string,Cmp>
 */
struct Tst
{
    Partition data_partition;                // e.g. "i1<i2<o"  // TODO: can a test be incomplete? If yes, then what is data_partition?
    std::map<std::string, Cmp> data_to_cmp;  // guarantees that all variables are present in this map

    explicit Tst(const Partition& data_partition, const std::map<std::string, Cmp>& data_to_cmp):
            data_partition(data_partition), data_to_cmp(data_to_cmp)
    {
        // TODO: add check: if i=o then data_to_cmp.at(i) = data_to_cmp.at(o)
    }

    [[nodiscard]] std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& os, const Tst& tst) { os << tst.to_str(); return os; }

    // these are needed to be able to insert Tst into a set
    bool operator==(const Tst &rhs) const;
    bool operator!=(const Tst &rhs) const;
    bool operator<(const Tst &rhs) const;
    bool operator>(const Tst &rhs) const;
    bool operator<=(const Tst &rhs) const;
    bool operator>=(const Tst &rhs) const;
};


/*
 * Example: "{i->{r1,r2}, d->{r3}} and r4 and r5 are missing" (and output `o` is not mapped at all)
 * An assignment is a partial mapping : R -> DataVariables.
 */
struct Asgn
{
    std::map<std::string, std::set<std::string>> asgn;

    Asgn() = default;
    explicit Asgn(const std::map<std::string, std::set<std::string>>& asgn): asgn(asgn) { }
    [[nodiscard]] std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& os, const Asgn& asgn_) { os << asgn_.to_str(); return os; }
    friend struct Algo;
};

struct Algo
{
    static Partition compute_next(const Partition& p, const Tst&, const Asgn&);

    static void add_data(const Tst& tst, Partition& p);

    static void add_primed(const Asgn& asgn, Partition& p_with_data);

    static void project_on_primed(Partition& p);

    static void unprime(Partition& p);
};

}


#pragma clang diagnostic pop
