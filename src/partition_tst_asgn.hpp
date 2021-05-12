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
    std::list<std::set<std::string>> repr;

    Partition() = default;
    explicit Partition(const std::list<std::set<std::string>>& repr_): repr(repr_) { }

    std::set<std::string>& get_class_of(const std::string& r);

    std::set<std::string> getR() const;  // return non-primed registers of the partition

    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& out, const Partition& p) { out << p.to_str(); return out; }

    friend struct Algo;

};

/*
 * Examples:
 * "r1<d<r2"
 * "d=r3"
 */
struct Cmp
{
    enum CmpType {
        EQUAL,
        BELOW,
        ABOVE,
        BETWEEN
    } type;

    const std::string r1;
    const std::string r2;

    // "*=r"
    static Cmp Equal(const std::string& r1_)
    {
        return Cmp(CmpType::EQUAL, r1_, "");
    }

    // "r<*<r2"
    static Cmp Between(const std::string& r1, const std::string& r2)
    {
        return Cmp(CmpType::BETWEEN, r1, r2);
    }

    // "*<r2" (note: r2 must be the smallest register)
    static Cmp Below(const std::string& r1)
    {
        return Cmp(CmpType::BELOW, r1, "");
    }

    // "*>r2" (note: r2 must be the largest register)
    static Cmp Above(const std::string& r1)
    {
        return Cmp(CmpType::ABOVE, r1, "");
    }

private:
    explicit Cmp(CmpType type, std::string r1, std::string r2): type(type), r1(r1), r2(r2)
    {
        MASSERT(r1[0] == 'r', "");
        if (type == CmpType::BETWEEN)
        {
            MASSERT(!r2.empty(), "");
            MASSERT(r2[0] == 'r', "");
        }
    }

public:
    std::string to_str() const;
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
    Partition data_partition;
    std::map<std::string, Cmp> data_to_cmp;  // TODO: optimization: replace with EqClass -> Cmp

    explicit Tst(const Partition& data_partition, const std::map<std::string, Cmp>& data_to_cmp):
            data_partition(data_partition), data_to_cmp(data_to_cmp)
    {
        // TODO: add check: if i=o then data_to_cmp.at(i) = data_to_cmp.at(o)
    }

    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& os, const Tst& tst) { os << tst.to_str(); return os; }
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
    std::string to_str() const;
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
