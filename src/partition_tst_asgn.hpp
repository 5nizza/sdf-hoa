#pragma once
#pragma clang diagnostic push
#pragma ide diagnostic ignored "modernize-pass-by-value"


#include <string>
#include <utility>
#include <vector>
#include <map>
#include <set>
#include <ostream>
#include <list>

#include "my_assert.hpp"
#include "graph.hpp"
#include "utils.hpp"


namespace sdf
{
struct TstAtom;
struct Asgn;
struct Partition;
}


namespace sdf
{
using EC = std::unordered_set<std::string>;
using Graph = graph::Graph;
using V = Graph::V;

// TODO: fix visibility (struct or class?)

/*
 * Partition is a graph, where v1 -> v2 means "v1>v2".
 * Each vertex describes an equivalence class, using v_to_ec mapping.
 */
struct Partition
{
    const Graph g;
    const std::unordered_map<V, EC> v_to_ec;
    Partition()=delete;  // with const above, it makes no sense

    explicit Partition(const Graph& g, const std::unordered_map<V, EC>& v_to_ec): g(g), v_to_ec(v_to_ec), hash_(calc_hash()) { }

    // Useful for hash containers. NB: non-intelligent verbatim comparison.
    bool operator==(const Partition& rhs) const { return g == rhs.g and v_to_ec == rhs.v_to_ec; }
    bool operator!=(const Partition& rhs) const { return !(*this == rhs); }

    friend struct std::hash<Partition>;
    friend std::ostream& operator<<(std::ostream& out, const Partition& p);

private:
    const size_t hash_;
   [[nodiscard]]
   size_t calc_hash() const;
};

/* TstAtom has the form a ◁ b where ◁ in {<,≤,=,≠} */
struct TstAtom
{
    const std::string t1, t2, cmp;

    /* Construct the test (change ≥ to ≤ if necessary) */
    static TstAtom constructTst(const std::string& t1, const std::string& t2, const std::string& cmp)
    {
        if (cmp == ">")
            return TstAtom(t2, t1, "<");

        if (cmp == "≥")
            return TstAtom(t2, t1, "≤");

        if (cmp == "<" or cmp == "≤")
            return TstAtom(t1, t2, cmp);

        // = or ≠
        return TstAtom(min(t1, t2), max(t1, t2), cmp);
    }

     // Useful for hash containers:
     bool operator==(const TstAtom& rhs) const { return std::tie(t1, t2, cmp) == std::tie(rhs.t1, rhs.t2, rhs.cmp); }
     bool operator!=(const TstAtom& rhs) const { return !(rhs == *this); }

    friend struct std::hash<TstAtom>;
    friend std::ostream& operator<<(std::ostream& out, const TstAtom& tst) { return out << tst.t1 << tst.cmp << tst.t2; }

private:
    const size_t hash_;

    explicit TstAtom(const std::string& t1, const std::string& t2, const std::string& cmp): t1(t1), t2(t2), cmp(cmp), hash_(calc_hash())
    {
        MASSERT(t1 != t2, "probably bug");
        std::set<std::string> allowed_cmp = {"<", "=", "≠", "≤"};
        MASSERT(contains(allowed_cmp, cmp), "unknown cmp: " << cmp);
    }

    [[nodiscard]]
    size_t calc_hash() const
    {
        std::vector<std::string> elements = {t1, t2, cmp};
        return hash_ordered(elements, std::hash<std::string>());
    }
};

/* Mapping : Var->SetOfReg  describing where to store a value of given variable. */
struct Asgn
{
    const std::unordered_map<std::string, std::unordered_set<std::string>> asgn;  // d is stored into {r1,r2,...}

    explicit Asgn(const std::unordered_map<std::string, std::unordered_set<std::string>>& asgn): asgn(asgn) { }

    // Useful for hash containers: TODO: no need?
//    bool operator==(const Asgn& rhs) const { return asgn == rhs.asgn; }
//    bool operator!=(const Asgn& rhs) const { return !(rhs == *this); }

    friend std::ostream& operator<<(std::ostream& os, const Asgn& asgn_);
};

}

namespace std
{
template<> struct hash<sdf::Partition> { size_t operator()(const sdf::Partition& p) const { return p.hash_; } };
template<> struct hash<sdf::TstAtom>       { size_t operator()(const sdf::TstAtom& tst) const     { return tst.hash_; } };
//template<> struct hash<sdf::Asgn>;
}


//namespace std
//{
//template<>
//struct hash<sdf::Partition>
//{
//    size_t operator()(const sdf::Partition& p) const
//    {
//        return pair_hash()(make_pair(x.state, x.k));
//    }
//};
//}


#pragma clang diagnostic pop
