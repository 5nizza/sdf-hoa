#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include <vector>

#include "gtest/gtest.h"

#include "graph.hpp"
#include "graph_algo.hpp"

using namespace std;
using namespace graph;

#define hset unordered_set

TEST(GraphTest, AddEdge)
{
    auto g = Graph();
    g.add_edge(1, 2);
    ASSERT_EQ(g.get_vertices(), hset({1u,2u}));
}

TEST(GraphTest, RemoveVertex)
{
    auto g = Graph();
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    g.add_edge(2, 2);
    g.add_edge(1, 3);
    g.remove_vertex(2);
    ASSERT_EQ(g.get_vertices(), hset({1u,3u}));
    ASSERT_EQ(g.get_children(1), hset({3u}));
}

TEST(GraphTest, MergeVerticesSelfLoop1)
{
    auto g = Graph();
    g.add_edge(1, 1);
    g.add_vertex(2);
    GraphAlgo::merge_v1_into_v2(g, 1, 2);
    ASSERT_EQ(g.get_vertices(), hset({2u}));
    ASSERT_EQ(g.get_parents(2), hset({2u}));
    ASSERT_EQ(g.get_children(2), hset({2u}));
}

TEST(GraphTest, MergeVerticesSelfLoop2)
{
    Graph g1, g2;
    g1.add_edge(1, 2);
    g2.add_edge(2, 1);
    for (auto g : {g1, g2})
    {
        GraphAlgo::merge_v1_into_v2(g, 1, 2);
        ASSERT_EQ(g.get_vertices(), hset({2u}));
        ASSERT_EQ(g.get_parents(2), hset({2u}));
        ASSERT_EQ(g.get_children(2), hset({2u}));
    }
}

TEST(GraphTest, MergeVerticesSelfLoop3)
{
    auto g = Graph();
    g.add_edge(1, 2);
    g.add_edge(2, 3);
    GraphAlgo::merge_v1_into_v2(g,1,3);
    ASSERT_EQ(g.get_vertices(), hset({2u,3u}));
    ASSERT_EQ(g.get_parents(3), hset({2u}));
    ASSERT_EQ(g.get_children(3), hset({2u}));
    ASSERT_EQ(g.get_parents(2), hset({3u}));
    ASSERT_EQ(g.get_children(2), hset({3u}));
}

TEST(GraphTest, GetDescendants)
{
    Graph g;
    g.add_edge(1,1);
    g.add_edge(1,2);
    g.add_edge(2,3);
    g.add_edge(3,4);
    g.add_vertex(5);
    ASSERT_EQ(GraphAlgo::get_descendants(g,1), hset({1u,2u,3u,4u}));
    ASSERT_EQ(GraphAlgo::get_descendants(g,2), hset({3u,4u}));
    ASSERT_TRUE(GraphAlgo::get_descendants(g,4).empty());
}

TEST(GraphTest, GetAncestors)
{
    Graph g;
    g.add_edge(1,2);
    g.add_edge(2,2);
    g.add_edge(2,3);
    g.add_edge(3,4);
    g.add_vertex(5);
    ASSERT_TRUE(GraphAlgo::get_ancestors(g,1).empty());
    ASSERT_EQ(GraphAlgo::get_ancestors(g,4), hset({1u,2u,3u}));
    ASSERT_EQ(GraphAlgo::get_ancestors(g,2), hset({1u,2u}));
}

TEST(GraphTest, HasCycles)
{
    Graph g;
    g.add_vertex(1);
    g.add_vertex(2);
    ASSERT_FALSE(GraphAlgo::has_cycles(g));
    g.add_edge(1,2);
    ASSERT_FALSE(GraphAlgo::has_cycles(g));
    g.add_edge(2,1);
    ASSERT_TRUE(GraphAlgo::has_cycles(g));
    g.remove_edge(2,1);
    ASSERT_FALSE(GraphAlgo::has_cycles(g));
    g.add_edge(1,1);
    ASSERT_TRUE(GraphAlgo::has_cycles(g));
}


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#pragma clang diagnostic pop
