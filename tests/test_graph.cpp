#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include <vector>

#include "gtest/gtest.h"

#include "reg/special_graph.hpp"
#include "reg/graph_algo.hpp"

using namespace sdf;
using namespace std;
using namespace graph;

using V = SpecialGraph::V;
using GA = GraphAlgo;

#define Vset unordered_set<V>
#define hmap unordered_map

TEST(GraphTest, AddEdge)
{
    auto g = SpecialGraph();
    g.add_dir_edge(1, 2);
    ASSERT_EQ(g.get_vertices(), Vset({1,2}));

    g.add_neq_edge(3, 4);
    ASSERT_EQ(g.get_vertices(), Vset({1,2,3,4}));
}

TEST(GraphTest, RemoveVertex)
{
    // 1->2->3, 2<->2, 1->3, 2≠3, 1≠3
    auto g = SpecialGraph({{1,2}, {2,3}, {2,2}, {1,3}}, {{2,3}, {1,3}});
    g.remove_vertex(2);
    ASSERT_EQ(g.get_vertices(), Vset({1,3}));
    ASSERT_EQ(g.get_children(1), Vset({3}));
    ASSERT_EQ(g.get_distinct(1), Vset({3}));
}

TEST(GraphTest, MergeVertices0)
{
    auto g = SpecialGraph({1,2});  // two independent vertices
    GraphAlgo::merge_v1_into_v2(g, 1, 2);
    ASSERT_EQ(g.get_vertices(), Vset({2}));
    ASSERT_TRUE(g.get_children(2).empty());
    ASSERT_TRUE(g.get_parents(2).empty());
    ASSERT_TRUE(g.get_distinct(2).empty());
}

TEST(GraphTest, MergeVerticesMainCase)
{
    // 1->2->3, 4->1
    // after merging(2,4) becomes
    // 1 <-> 4 -> 3
    auto g = SpecialGraph({{1,2}, {2,3}, {4,1}}, {});
    GraphAlgo::merge_v1_into_v2(g, 2, 4);
    ASSERT_EQ(g.get_vertices(), Vset({1,4,3}));
    ASSERT_EQ(g.get_children(1), Vset({4}));
    ASSERT_EQ(g.get_children(4), Vset({1,3}));
    ASSERT_TRUE(g.get_children(3).empty());
}

TEST(GraphTest, MergeVerticesMainCase_Distinct)
{
    // 1-2, 3
    // after merging(2,3) becomes
    // 1 - 3
    auto g = SpecialGraph({}, {{1,2}});
    g.add_vertex(3);
    GraphAlgo::merge_v1_into_v2(g, 2, 3);

    ASSERT_EQ(g.get_vertices(), Vset({1,3}));

    ASSERT_EQ(g.get_distinct(1), Vset({3}));
    ASSERT_EQ(g.get_distinct(3), Vset({1}));

    ASSERT_EQ(g.get_children(1), Vset({}));
    ASSERT_EQ(g.get_children(3), Vset({}));
}

TEST(GraphTest, MergeVerticesSelfLoop_Distinct1)
{
    auto g = SpecialGraph();
    g.add_neq_edge(1, 1);
    g.add_vertex(2);
    GraphAlgo::merge_v1_into_v2(g, 1, 2);
    ASSERT_EQ(g.get_vertices(), Vset({2}));
    ASSERT_EQ(g.get_distinct(2), Vset({2}));
    ASSERT_EQ(g.get_parents(2), Vset({}));
    ASSERT_EQ(g.get_children(2), Vset({}));
}

TEST(GraphTest, MergeVerticesSelfLoop_Distinct2)
{
    auto g = SpecialGraph();
    g.add_neq_edge(1, 2);
    GraphAlgo::merge_v1_into_v2(g, 1, 2);
    ASSERT_EQ(g.get_vertices(), Vset({2}));
    ASSERT_EQ(g.get_distinct(2), Vset({2}));
    ASSERT_EQ(g.get_parents(2), Vset({}));
    ASSERT_EQ(g.get_children(2), Vset({}));
}

TEST(GraphTest, MergeVerticesSelfLoop1)
{
    auto g = SpecialGraph({{1,1}},{});
    g.add_vertex(2);
    GraphAlgo::merge_v1_into_v2(g, 1, 2);
    ASSERT_EQ(g.get_vertices(), Vset({2}));
    ASSERT_EQ(g.get_parents(2), Vset({2}));
    ASSERT_EQ(g.get_children(2), Vset({2}));
}

TEST(GraphTest, MergeVerticesSelfLoop2)
{
    SpecialGraph g1({{1,2}}, {}), g2({{2,1}}, {});
    for (auto g : {g1, g2})
    {
        GraphAlgo::merge_v1_into_v2(g, 1, 2);
        ASSERT_EQ(g.get_vertices(), Vset({2}));
        ASSERT_EQ(g.get_parents(2), Vset({2}));
        ASSERT_EQ(g.get_children(2), Vset({2}));
    }
}

TEST(GraphTest, MergeVerticesSelfLoop3)
{
    auto g = SpecialGraph({{1,2}, {2,3}}, {});
    GraphAlgo::merge_v1_into_v2(g,1,3);
    ASSERT_EQ(g.get_vertices(), Vset({2,3}));
    ASSERT_EQ(g.get_parents(3), Vset({2}));
    ASSERT_EQ(g.get_children(3), Vset({2}));
    ASSERT_EQ(g.get_parents(2), Vset({3}));
    ASSERT_EQ(g.get_children(2), Vset({3}));
}

TEST(GraphTest, GetDescendantsAncestors)
{
    // Since get_descentants and get_ancestors call the same implementation inside,
    // it suffices to test only one of them.
    SpecialGraph g({{1,1},{1,2},{2,3},{3,4}},{});
    g.add_vertex(5);

    Vset result;
    auto insert = [&result](const V& v){result.insert(v);};

    GraphAlgo::get_descendants(g,1,insert);
    ASSERT_EQ(result, Vset({1,2,3,4}));

    result.clear();
    GraphAlgo::get_descendants(g,2,insert);
    ASSERT_EQ(result, Vset({3,4}));

    result.clear();
    GraphAlgo::get_descendants(g,4, insert);
    ASSERT_TRUE(result.empty());
}

TEST(GraphTest, HasCycles)
{
    SpecialGraph g({1,2});
    ASSERT_FALSE(GraphAlgo::has_dir_cycles(g));
    g.add_dir_edge(1,2);
    ASSERT_FALSE(GraphAlgo::has_dir_cycles(g));
    g.add_dir_edge(2,1);
    ASSERT_TRUE(GraphAlgo::has_dir_cycles(g));
    g.remove_dir_edge(2,1);
    ASSERT_FALSE(GraphAlgo::has_dir_cycles(g));
    g.add_dir_edge(1,1);
    ASSERT_TRUE(GraphAlgo::has_dir_cycles(g));
}


template<class ContainerT1, class ContainerT2>
void assert_equal_content(const ContainerT1& c1, const ContainerT2& c2)
{
    for (const auto& e : c1)
        if (find(c2.begin(), c2.end(), e) == c2.end())
            ASSERT_TRUE(false) << "container 1 has a value not present in container 2" << endl;

    for (const auto& e : c2)
        if (find(c1.begin(), c1.end(), e) == c1.end())
            ASSERT_TRUE(false) << "container 2 has a value not present in container 1" << endl;
}


TEST(GraphTest, AllTopoSorts)
{
    using T = vector<SpecialGraph>;
    assert_equal_content(GA::all_topo_sorts(SpecialGraph({1,2})),  // two vertices 1,2, no edges
                         T{SpecialGraph({{1,2}},{}), SpecialGraph({{2,1}},{})});

    assert_equal_content(GA::all_topo_sorts(SpecialGraph({{1,2}},{})),  // one edge 1->2
                         T{SpecialGraph({{1,2}},{})});

    assert_equal_content(GA::all_topo_sorts(SpecialGraph({{1,2}, {1,3}},{})),
                         T{SpecialGraph({{1,2}, {2,3}},{}),
                           SpecialGraph({{1,3},{3,2}},{})});

    assert_equal_content(GA::all_topo_sorts(SpecialGraph({{1,2}, {10,20}},{})),
                         T{SpecialGraph({{1,2}, {2,10}, {10,20}},{}),
                           SpecialGraph({{1,10}, {10,2}, {2,20}},{}),
                           SpecialGraph({{1,10}, {10,20}, {20,2}},{}),
                           SpecialGraph({{10,1}, {1,20}, {20,2}},{}),
                           SpecialGraph({{10,20}, {20,1}, {1,2}},{}),
                           SpecialGraph({{10,1}, {1,2}, {2,20}},{})});
}

TEST(GraphTest, AllTopoSorts2)
{
    using T = vector<pair<SpecialGraph, hmap<V, Vset>>>;
    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({1,2})),
              T{
                {SpecialGraph({{1,2}},{}), {{1,{1}},{2,{2}}}},
                {SpecialGraph({{2,1}},{}), {{1,{1}},{2,{2}}}},
                {SpecialGraph({1}), {{1,{1,2}}}}
              });

    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({{1,2}},{})),
            T{
                {SpecialGraph({{1,2}},{}), {{1,{1}},{2,{2}}}}
            });

    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({{1,2}, {1,3}},{})),
                         T{
                                 {SpecialGraph({{1,2}, {2,3}},{}), {{1,{1}},{2,{2}}, {3,{3}}}},
                                 {SpecialGraph({{1,3}, {3,2}},{}), {{1,{1}},{2,{2}}, {3,{3}}}},
                                 {SpecialGraph({{1,2}},{}), {{1,{1}},{2,{2,3}}}}
                         });

    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({{1,2}, {2,3}, {1,99}},{})),
                         T{
                                 {SpecialGraph({{1,2}, {2,3}, {3,99}},{}), {{1,{1}},{2,{2}}, {3,{3}}, {99,{99}}}},
                                 {SpecialGraph({{1,2}, {2,99}, {99,3}},{}), {{1,{1}},{2,{2}}, {3,{3}}, {99,{99}}}},
                                 {SpecialGraph({{1,99}, {99,2}, {2,3}},{}), {{1,{1}},{2,{2}}, {3,{3}}, {99,{99}}}},
                                 {SpecialGraph({{1,2}, {2,3}},{}), {{1,{1}},{2,{2}}, {3,{3,99}}}},
                                 {SpecialGraph({{1,2}, {2,3}},{}), {{1,{1}},{2,{2,99}}, {3,{3}}}}
                         });
}

TEST(GraphTest, AllTopoSorts2_check_distinct)
{
    using T = vector<pair<SpecialGraph, hmap<V, Vset>>>;
    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({},{{1,2}})),  // 1≠2
                         T{
                                 {SpecialGraph({{1,2}},{}), {{1,{1}},{2,{2}}}},
                                 {SpecialGraph({{2,1}},{}), {{1,{1}},{2,{2}}}}
                         });

    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({{1,2},{1,3}},{{2,3}})),  // 1->(2≠3)
                         T{
                                 {SpecialGraph({{1,2},{2,3}},{}), {{1,{1}},{2,{2}},{3,{3}}}},
                                 {SpecialGraph({{1,3},{3,2}},{}), {{1,{1}},{2,{2}},{3,{3}}}}
                         });

    assert_equal_content(GA::all_topo_sorts2(SpecialGraph({},{{1,2},{2,3}})),  // 1≠2, 2≠3
                         T{
                                 {SpecialGraph({{1,2},{2,3}},{}), {{1,{1}},{2,{2}},{3,{3}}}},
                                 {SpecialGraph({{1,3},{3,2}},{}), {{1,{1}},{2,{2}},{3,{3}}}},
                                 {SpecialGraph({{2,1},{1,3}},{}), {{1,{1}},{2,{2}},{3,{3}}}},
                                 {SpecialGraph({{2,3},{3,1}},{}), {{1,{1}},{2,{2}},{3,{3}}}},
                                 {SpecialGraph({{3,1},{1,2}},{}), {{1,{1}},{2,{2}},{3,{3}}}},
                                 {SpecialGraph({{3,2},{2,1}},{}), {{1,{1}},{2,{2}},{3,{3}}}},

                                 {SpecialGraph({{1,2}},{}), {{1,{1,3}},{2,{2}}}},
                                 {SpecialGraph({{2,1}},{}), {{1,{1,3}},{2,{2}}}}
                         });
}

TEST(GraphTest, Reach)
{
    using T = hmap<V,Vset>;

    ASSERT_EQ(GA::get_reach(SpecialGraph({1,2})),  // 1,2
              T());

    ASSERT_EQ(GA::get_reach(SpecialGraph({{1,2},{2,3}},{})),  // 1->2->3
              T({{1,{3}}}));

    ASSERT_EQ(GA::get_reach(SpecialGraph({{1,2},{2,3},{3,4}},{})),  // 1->2->3->4
              T({{1,{3,4}},{2,{4}}}));

    ASSERT_EQ(GA::get_reach(SpecialGraph({{1,2},{2,3},{2,4}},{})),  // 1->2->{3,4}
              T({{1,{3,4}}}));

    ASSERT_EQ(GA::get_reach(SpecialGraph({{1,2},{2,3},{2,4},{1,4}},{})),  // 1->2->{3,4}, 1->4
              T({{1,{3,4}}}));

    ASSERT_EQ(GA::get_reach(SpecialGraph({{1,2},{2,3},{4,2}},{})),  // {1,4}->2->3
              T({{1,{3}},{4,{3}}}));

    // cycle
    ASSERT_EQ(GA::get_reach(SpecialGraph({{1,2},{2,1}},{})),  // 1<->2
              T({{1,{1,2}},{2,{1,2}}}));
}


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#pragma clang diagnostic pop
