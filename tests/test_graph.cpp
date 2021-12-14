#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include <vector>

#include "gtest/gtest.h"

#include "graph.hpp"
#include "graph_algo.hpp"
#include "utils.hpp"

using namespace sdf;
using namespace std;
using namespace graph;

using V = Graph::V;
using GA = GraphAlgo;

#define hset unordered_set
#define hmap unordered_map

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

TEST(GraphTest, GetDescendantsAncestors)
{
    // Since get_descentants and get_ancestors call to the same implementation inside,
    // it suffices to test only one of them.
    Graph g;
    g.add_edge(1,1);
    g.add_edge(1,2);
    g.add_edge(2,3);
    g.add_edge(3,4);
    g.add_vertex(5);

    hset<V> result;
    auto insert = [&result](const V& v){result.insert(v);};

    GraphAlgo::get_descendants(g,1,insert);
    ASSERT_EQ(result, hset({1u,2u,3u,4u}));

    result.clear();
    GraphAlgo::get_descendants(g,2,insert);
    ASSERT_EQ(result, hset({3u,4u}));

    result.clear();
    GraphAlgo::get_descendants(g,4, insert);
    ASSERT_TRUE(result.empty());
}

TEST(GraphTest, HasCycles)
{
    Graph g({1,2});
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

namespace std { template<> struct hash<Graph> { size_t operator()(const Graph& g) const { return g.calc_hash(); } }; }

hset<Graph> set_ctor(const vector<Graph>& graphs) { return hset<Graph>(graphs.begin(), graphs.end()); }

TEST(GraphTest, AllTopoSorts)
{
    ASSERT_EQ(set_ctor(GA::all_topo_sorts(Graph{1,2})),
              hset<Graph>({Graph({{1,2}}), Graph({{2,1}})}));

    ASSERT_EQ(set_ctor(GA::all_topo_sorts(Graph({{1,2}}))),
              hset<Graph>({Graph({{1,2}})}));

    ASSERT_EQ(set_ctor(GA::all_topo_sorts(Graph({{1,2}, {1,3}}))),
              hset<Graph>({Graph({{1,2}, {2,3}}), Graph({{1,3},{3,2}})}));

    ASSERT_EQ(set_ctor(GA::all_topo_sorts(Graph({{1,2}, {10,20}}))),
              hset<Graph>({Graph({{1,2}, {2,10}, {10,20}}),
                           Graph({{1,10}, {10,2}, {2,20}}),
                           Graph({{1,10}, {10,20}, {20,2}}),
                           Graph({{10,1}, {1,20}, {20,2}}),
                           Graph({{10,20}, {20,1}, {1,2}}),
                           Graph({{10,1}, {1,2}, {2,20}})}));
}

void assert_equal_content(const vector<pair<Graph, hmap<V, hset<V>>>>& g_map_pairs1,
                          const vector<pair<Graph, hmap<V, hset<V>>>>& g_map_pairs2)
{
    for (const auto& g_map : g_map_pairs1)
        ASSERT_TRUE(find(g_map_pairs2.begin(),g_map_pairs2.end(), g_map) != g_map_pairs2.end());

    for (const auto& g_map : g_map_pairs2)
        ASSERT_TRUE(find(g_map_pairs1.begin(),g_map_pairs1.end(), g_map) != g_map_pairs1.end());
}

TEST(GraphTest, AllTopoSorts2)
{
    assert_equal_content(GA::all_topo_sorts2(Graph({1,2})),
              {
                {Graph({{1,2}}), {{1,{1}},{2,{2}}}},
                {Graph({{2,1}}), {{1,{1}},{2,{2}}}},
                {Graph({1}), {{1,{1,2}}}}
              });

    assert_equal_content(GA::all_topo_sorts2(Graph({{1,2}})),
            {
                {Graph({{1,2}}), {{1,{1}},{2,{2}}}}
            });

    assert_equal_content(GA::all_topo_sorts2(Graph({{1,2}, {1,3}})),
                         {
                                 {Graph({{1,2}, {2,3}}), {{1,{1}},{2,{2}}, {3,{3}}}},
                                 {Graph({{1,3}, {3,2}}), {{1,{1}},{2,{2}}, {3,{3}}}},
                                 {Graph({{1,2}}), {{1,{1}},{2,{2,3}}}}
                         });

    assert_equal_content(GA::all_topo_sorts2(Graph({{1,2}, {2,3}, {1,99}})),
                         {
                                 {Graph({{1,2}, {2,3}, {3,99}}), {{1,{1}},{2,{2}}, {3,{3}}, {99,{99}}}},
                                 {Graph({{1,2}, {2,99}, {99,3}}), {{1,{1}},{2,{2}}, {3,{3}}, {99,{99}}}},
                                 {Graph({{1,99}, {99,2}, {2,3}}), {{1,{1}},{2,{2}}, {3,{3}}, {99,{99}}}},
                                 {Graph({{1,2}, {2,3}}), {{1,{1}},{2,{2}}, {3,{3,99}}}},
                                 {Graph({{1,2}, {2,3}}), {{1,{1}},{2,{2,99}}, {3,{3}}}}
                         });

}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#pragma clang diagnostic pop
