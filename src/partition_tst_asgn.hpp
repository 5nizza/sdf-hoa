#pragma once
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-pass-by-value"


#include <string>
#include <vector>
#include <map>
#include <set>
#include <ostream>


namespace sdf
{

/*
 * "r1<r2=r3<r4"
 * Internally, this is represented by vector<set<string>>:
 *   [{r1}, {r2,r3}, {r4}]
 */
class Partition
{
private:
    std::vector<std::set<std::string>> repr;

public:
    explicit Partition(const std::vector<std::set<std::string>>& repr_): repr(repr_) { }

    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& out, const Partition& p) { out << p.to_str(); return out; }
};


/*
 * "r1<d<r2"
 * "d=r3"
 */
class Cmp
{
private:
    std::string d;
    std::string r;
    std::string r2;

public:
    // "d=r"
    Cmp(std::string d, std::string r): d(std::move(d)), r(std::move(r)) {}
    // "r<d<r2"
    Cmp(std::string r, std::string d, std::string r2): d(std::move(d)), r(std::move(r)), r2(std::move(r2)) {}
public:
    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& out, const Cmp& cmp) { out << cmp.to_str(); return out; }
};

/*
 * "(i<o, i=r2, r1<o<r2)"
 * Thus, a test contains:
 * - i<o: the partition of data (among themselves)
 *   Internally, we use Partition over data
 * - Each data maps to Cmp:
 *   i -> i=r2
 *   o -> r1<o<r2
 *   Internally, we use map<string,Cmp>
 */
class Tst
{
private:
    Partition data_partition;
    std::map<std::string, Cmp> data_to_cmp;
public:
    Tst(const Partition& data_partition, const std::map<std::string, Cmp>& data_to_cmp):
            data_partition(data_partition), data_to_cmp(data_to_cmp)
    { }

    std::string to_str() const;
    friend std::ostream& operator<<(std::ostream& os, const Tst& tst) { os << tst.to_str(); return os; }
};

}

#pragma clang diagnostic pop
