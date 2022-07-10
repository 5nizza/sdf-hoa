#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include "gtest/gtest.h"

#include <vector>

#include "reg/types.hpp"
#include "reg/partition.hpp"


using namespace sdf;
using namespace std;
using namespace graph;

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


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#pragma clang diagnostic pop