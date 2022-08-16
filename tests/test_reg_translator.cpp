#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include "gtest/gtest.h"

#include <vector>

#include "reg/types.hpp"
#include "reg/partition.hpp"
#include "reg/mixed_domain.hpp"
#include "reg/atm_parsing_naming.hpp"


using namespace sdf;
using namespace std;
using namespace graph;
using namespace spot;

#define hset unordered_set
#define Vset unordered_set<V>
#define hmap unordered_map


TEST(PartitionTest, GetCanonical1)
{
    // We test: sorting, unnecessary-edges removal, vertex renaming:
    // 1->2->3, 1->3, 1≠3, 1≠2; 1->{z1,z2}, 2->{y}, 3->{x,z3}
    // after canonize should become:
    // 0<-1<-2, 0->{x,z3}, 1->{y}, 2->{z1,z2}
    auto p = Partition(SpecialGraph({{1,2},{2,3},{1,3}},  // ->
                                    {{1,3},{1,2}}),       // ≠
                       {{1,{"z1","z2"}}, {2,{"y"}}, {3,{"x","z3"}}});

    auto p_expected = Partition(SpecialGraph({{2,1},{1,0}}, {}),
                                {{2,{"z1","z2"}}, {1,{"y"}}, {0,{"x","z3"}}});

    auto p_actual = PartitionCanonical(p);
    ASSERT_EQ(p_expected, p_actual.p);
}

TEST(PartitionTest, GetCanonical2)
{
    // Test the corner case (no edges at all); so only sorting and renaming should kick in.
    // 1->{z1,z2}, 2->{y}, 3->{x,z3}
    // after canonize should become:
    // 0->{x,z3}, 1->{y}, 2->{z1,z2}
    auto p = Partition(SpecialGraph({1,2,3}),
                       {{1,{"z1","z2"}}, {2,{"y"}}, {3,{"x","z3"}}});

    auto p_expected = Partition(SpecialGraph({0,1,2}),
                                {{2,{"z1","z2"}}, {1,{"y"}}, {0,{"x","z3"}}});

    auto p_actual = PartitionCanonical(p);
    ASSERT_EQ(p_expected, p_actual.p);
}

TEST(PartitionTest, PartitionCanonicalEqual)
{
    auto p1 = Partition(SpecialGraph({},{{1,2}}),
                        {{1,{"a","b"}}, {2,{"c"}}});

    auto p2 = Partition(SpecialGraph({},{{20,1}}),
                        {{20,{"a","b"}}, {1,{"c"}}});

    ASSERT_TRUE(PartitionCanonical(p1) == PartitionCanonical(p2));

    p1.graph.add_vertex(3);
    p1.v_to_ec.insert({3,{"d"}});

    ASSERT_FALSE(PartitionCanonical(p1) == PartitionCanonical(p2));
}

#define E MixedDomain(DomainName::equality)
#define O MixedDomain(DomainName::order)

TEST(MixedDomainTest, all_possible_sys_tst1)
{
    /// one atm register, one system register, and IN;
    /// r>IN, rs
    auto rs = ctor_sys_reg(1);
    auto r = ctor_atm_reg(1);
    auto p_io = Partition(SpecialGraph({{1,2}},{}), {{1,{r}},
                                                     {2,{IN}},
                                                     {3,{rs}}});
    p_io.graph.add_vertex(3);  // (for 'rs')

    /// sys tests is T, hence only one possible partition
    ASSERT_EQ(1,
              O.all_possible_sys_tst(p_io, {}).size());

    /// sys test is =, hence 2 partitions: rs=IN, rs≠IN (plus the rest)
    ASSERT_EQ(2,
              O.all_possible_sys_tst(p_io, {{rs, DomainName::equality}}).size());

    /// sys test is <, hence 3 partitions: rs<IN, rs>IN, rs=IN (plus the rest)
    ASSERT_EQ(3,
              O.all_possible_sys_tst(p_io, {{rs, DomainName::order}}).size());

    /// the results are the same when instead of rs we have rs1=rs2
    /// (no matter if we compare only one or both registers)
    auto rs1 = ctor_sys_reg(1), rs2 = ctor_sys_reg(2);
    p_io.v_to_ec[3] = {rs1, rs2};

    ASSERT_EQ(1, O.all_possible_sys_tst(p_io, {}).size());
    ASSERT_EQ(2, O.all_possible_sys_tst(p_io, {{rs1, DomainName::equality}}).size());
    ASSERT_EQ(3, O.all_possible_sys_tst(p_io, {{rs1, DomainName::order}}).size());
    ASSERT_EQ(1, O.all_possible_sys_tst(p_io, {}).size());
    ASSERT_EQ(2, O.all_possible_sys_tst(p_io, {{rs1, DomainName::equality}, {rs2, DomainName::equality}}).size());
    ASSERT_EQ(3, O.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::order}}).size());
    ASSERT_EQ(3, O.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::equality}}).size());
}

TEST(MixedDomainTest, all_possible_sys_tst2)
{
    /// one atm register, two system registers, and IN;
    /// r>IN, rs1, rs2
    auto rs1 = ctor_sys_reg(1), rs2 = ctor_sys_reg(2);
    auto r = ctor_atm_reg(1);
    auto domain = MixedDomain(DomainName::order);
    auto p_io = Partition(SpecialGraph({{1, 2}}, {}), {{1, {r}},
                                                       {2, {IN}},
                                                       {3, {rs1}},
                                                       {4, {rs2}}});
    p_io.graph.add_vertex(3);  // (for 'rs1')
    p_io.graph.add_vertex(4);  // (for 'rs2')

    ASSERT_EQ(1, domain.all_possible_sys_tst(p_io, {}).size());
    ASSERT_EQ(2, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::equality}}).size());
    ASSERT_EQ(4, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::equality}, {rs2, DomainName::equality}}).size());  // (2*2)
    ASSERT_EQ(3, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::order}}).size());
    ASSERT_EQ(6, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::equality}, {rs2, DomainName::order}}).size());   // (2*3)
    ASSERT_EQ(9, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::order}}).size());    // (3*3)
}

TEST(MixedDomainTest, all_possible_sys_tst_constraining_sys1)
{
    /// one atm register, two system registers, and IN;
    /// rs1>r>IN, rs2
    auto rs1 = ctor_sys_reg(1), rs2 = ctor_sys_reg(2);
    auto r = ctor_atm_reg(1);
    auto domain = MixedDomain(DomainName::order);
    auto p_io = Partition(SpecialGraph({{1,2},{2,3}}, {}), {{1, {rs1}},
                                                            {2, {r}},
                                                            {3, {IN}},
                                                            {4, {rs2}}});
    p_io.graph.add_vertex(4);  // for rs2

    /// rs1 does not affect the result
    ASSERT_EQ(3, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::order}}).size());

    /// make rs2=IN impossible
    p_io.graph.add_neq_edge(3,4);
    ASSERT_EQ(2, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::order}}).size());
    p_io.graph.add_dir_edge(3,4);
    ASSERT_EQ(1, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::order}}).size());
}

TEST(MixedDomainTest, all_possible_sys_tst_constraining_sys2)
{
    /// one atm register, two system registers, and IN;
    /// r>IN, rs1>rs2
    auto rs1 = ctor_sys_reg(1), rs2 = ctor_sys_reg(2);
    auto r = ctor_atm_reg(1);
    auto domain = MixedDomain(DomainName::order);
    auto p_io = Partition(SpecialGraph({{1,2},{3,4}}, {}), {{1, {r}},
                                                            {2, {IN}},
                                                            {3, {rs1}},
                                                            {4, {rs2}}});

    ASSERT_EQ(3, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::equality}, {rs2, DomainName::equality}}).size());
    ASSERT_EQ(5, domain.all_possible_sys_tst(p_io, {{rs1, DomainName::order}, {rs2, DomainName::order}}).size());
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#pragma clang diagnostic pop




















