#pragma clang diagnostic push
#pragma ide diagnostic ignored "cert-err58-cpp"

#include <string>
#include <vector>

#include "partition_tst_asgn.hpp"

#include "gtest/gtest.h"


using namespace std;
using namespace sdf;

// TODO: add tests for Partition
// TODO: add tests for compute_partial_p_io

/*

struct Partition1: public testing::Test
{
    Partition p = Partition({{"r1"}, {"r2"}, {"r3"}});
};

TEST_F(Partition1, compute_partial_p_io)
{
    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({{"i"}, {"o"}}),
                           {{"i", Cmp::Between("r1", "r2")},
                        {"o", Cmp::Between("r2", "r3")}});
        Asgn a({{"i", {"r1", "r3"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r1=r3<r2");
    }

    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({{"i"}, {"o"}}),
                           {{"i", Cmp::Below("r1")},
                        {"o", Cmp::Above("r3")}});
        Asgn a({{"i", {"r3"}},
                {"o", {"r1"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r3<r2<r1");
    }

    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({{"i"}, {"o"}}),
                           {{"i", Cmp::Above("r3")},
                        {"o", Cmp::Above("r3")}});
        Asgn a({{"i", {"r3"}},
                {"o", {"r1"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r2<r3<r1");
    }

    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({{"i"},
                                      {"o"}}),
                           {{"i", Cmp::Equal("r3")},
                        {"o", Cmp::Equal("r1")}});
        Asgn a({{"i", {"r1"}},
                {"o", {"r3"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r3<r2<r1");
    }

    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({set<string>({"i", "o"})}),
                           {{"i", Cmp::Equal("r2")},
                        {"o", Cmp::Equal("r2")}});
        Asgn a({{"i", {"r1"}},
                {"o", {"r3"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r1=r2=r3");
    }
}


struct Partition2: public testing::Test { Partition p = Partition({{"r1", "r2", "r3"}}); };
TEST_F(Partition2, ComputeNext)
{
    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({{"o"}, {"i"}}),
                           {{"i", Cmp::Above("r2")},
                        {"o", Cmp::Below("r1")}});
        Asgn a({{"i", {"r3"}},
                {"o", {"r1"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r1<r2<r3");
    }

    {
        cout << "-------------" << endl;
        auto tst = TstAtom(Partition({{"o"}, {"i"}}),
                           {{"i", Cmp::Equal("r2")},
                        {"o", Cmp::Below("r1")}});
        Asgn a({{"i", {"r3"}},
                {"o", {"r1"}}});

        EXPECT_EQ(Algo::compute_next(p, tst, a).to_str(),
                  "r1<r2=r3");
    }
}
*/


int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


#pragma clang diagnostic pop