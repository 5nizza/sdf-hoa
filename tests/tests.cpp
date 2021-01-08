//
// Created by ayrat on 23/06/18.
//

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "syntcomp_constants.hpp"
#include "synthesizer.hpp"


using namespace std;


struct SpecParam
{
    const string name;
    const bool is_real;

    SpecParam(const string& name_, bool is_real_) : name(name_), is_real(is_real_) { }
};


///TODO: fix this warning: see how to: https://github.com/google/googletest/blob/master/googletest/docs/advanced.md
const vector<SpecParam> specs =
{
    SpecParam("round_robin_arbiter_unreal1.tlsf", false),
    SpecParam("round_robin_arbiter_unreal2.tlsf", false),
    SpecParam("prioritized_arbiter_unreal1.tlsf", false),
    SpecParam("prioritized_arbiter_unreal2.tlsf", false),
    SpecParam("prioritized_arbiter_unreal3.tlsf", false),
    SpecParam("load_balancer_unreal1.tlsf", false),
    SpecParam("mealy_moore_unreal.tlsf", false),
    SpecParam("simple_arbiter_unreal1.tlsf", false),
    SpecParam("simple_arbiter_unreal2.tlsf", false),
    SpecParam("simple_arbiter_unreal3.tlsf", false),
    SpecParam("detector_unreal.tlsf", false),
    SpecParam("full_arbiter_unreal1.tlsf", false),
    SpecParam("full_arbiter_unreal2.tlsf", false),

    SpecParam("load_balancer_real2.tlsf", true),
    SpecParam("simple_arbiter.tlsf", true),
    SpecParam("detector.tlsf", true),
    SpecParam("full_arbiter.tlsf", true),
    SpecParam("load_balancer.tlsf", true),
    SpecParam("round_robin_arbiter.tlsf", true),
    SpecParam("round_robin_arbiter2.tlsf", true),
    SpecParam("prioritized_arbiter.tlsf", true),
    SpecParam("mealy_moore_real.tlsf", true)
};


class TestSpec : public ::testing::TestWithParam<SpecParam> { };


TEST_P(TestSpec, check_unreal)
{
    auto spec = GetParam();

    auto status = sdf::run(true,
                           "./specs/" + spec.name,
                           {4});

    if (spec.is_real)
        EXPECT_EQ(SYNTCOMP_RC_UNKNOWN, status);
    else
        EXPECT_EQ(SYNTCOMP_RC_UNREAL, status);
}


TEST_P(TestSpec, check_real)
{
    auto spec = GetParam();

    auto status = sdf::run(false,
                           "./specs/" + spec.name,
                           {4});

    if (spec.is_real)
        EXPECT_EQ(SYNTCOMP_RC_REAL, status);
    else
        EXPECT_EQ(SYNTCOMP_RC_UNKNOWN, status);
}


INSTANTIATE_TEST_SUITE_P(RealAndUnreal,
                         TestSpec,
                         ::testing::ValuesIn(specs));


/// For future: good to check the exact values of parameter k that makes specs realizable.

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
