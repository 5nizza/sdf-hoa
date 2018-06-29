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


/// @returns: return_code of the tool execution
int execute_tool(const string& name, bool do_unreal_check, uint k)
{
    return sdf::run("./specs/" + name, do_unreal_check, {k}, "");
}


class TestSpec : public ::testing::TestWithParam<SpecParam> { };


TEST_P(TestSpec, check_unreal)
{
    auto spec = GetParam();

    auto status = execute_tool(spec.name, true, 4);

    if (spec.is_real)
        EXPECT_EQ(SYNTCOMP_RC_UNKNOWN, status);

    if (!spec.is_real)
        EXPECT_EQ(SYNTCOMP_RC_UNREAL, status);
}


TEST_P(TestSpec, check_real)
{
    auto spec = GetParam();

    auto status = execute_tool(spec.name, false, 4);

    if (spec.is_real)
        EXPECT_EQ(SYNTCOMP_RC_REAL, status);

    if (!spec.is_real)
        EXPECT_EQ(SYNTCOMP_RC_UNKNOWN, status);
}


INSTANTIATE_TEST_CASE_P(RealAndUnreal,
                        TestSpec,
                        ::testing::ValuesIn(specs));


// PAST:
// - just added the tools, using parameterized tests, see:
//   https://stackoverflow.com/questions/19160244/create-tests-at-run-time-google-test
//   https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#value-parameterized-tests
//   https://www.youtube.com/watch?v=8Up5eNZ0FLw
/// CURRENT:
/// - the testing code itself is not finished:
///   - run the tool (either via command line or directly)
///   - check dual cases (run unreal check for real and vice versa)
///  Note: may be rewrite instantiations, using only one, and introduce SpecParams struct



int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
