//
// Created by ayrat on 23/06/18.
//

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "syntcomp_constants.hpp"
#include "synthesizer.hpp"


using namespace std;


const vector<string> unreal_specs =
{
    "round_robin_arbiter_unreal1.tlsf",
    "round_robin_arbiter_unreal2.tlsf",
    "prioritized_arbiter_unreal1.tlsf",
    "prioritized_arbiter_unreal2.tlsf",
    "prioritized_arbiter_unreal3.tlsf",
    "parameterized/load_balancer_unreal1.tlsf",
    "mealy_moore_unreal.tlsf",
    "simple_arbiter_unreal1.tlsf",
    "simple_arbiter_unreal2.tlsf",
    "simple_arbiter_unreal3.tlsf",
    "detector_unreal.tlsf",
    "full_arbiter_unreal1.tlsf",
    "full_arbiter_unreal2.tlsf"
};


const vector<string> real_specs =
{
    "load_balancer_real2.tlsf",
    "simple_arbiter.tlsf",
    "detector.tlsf",
    "full_arbiter.tlsf",
    "load_balancer.tlsf",
    "round_robin_arbiter.tlsf",
    "round_robin_arbiter2.tlsf",
    "prioritized_arbiter.tlsf",
    "mealy_moore_real.tlsf"
};


/// @returns: return_code of the tool execution
bool execute_tool(const string& name)
{
    auto rc = sdf::run("./specs/" + name, true, {1, 2, 3}, "");
    cout << name << ", rc: " << rc << endl;
    return true;
}


class TestUnreal : public ::testing::TestWithParam<string> { };
class TestReal : public ::testing::TestWithParam<string> { };


TEST_P(TestUnreal, check_unreal)
{   /// ensure it returns realizable when checking unrealizability of unrealizable specs
    EXPECT_TRUE(execute_tool(GetParam()));
}


TEST_P(TestUnreal, check_real)
{   /// ensure it returns unknown when checking realizability of unrealizable specs
    EXPECT_TRUE(execute_tool(GetParam()));
}


INSTANTIATE_TEST_CASE_P(Unreal,
                        TestUnreal,
                        ::testing::ValuesIn(unreal_specs));


INSTANTIATE_TEST_CASE_P(Real,
                        TestReal,
                        ::testing::ValuesIn(real_specs));
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
