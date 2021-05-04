//
// Created by ayrat on 23/06/18.
//

#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "syntcomp_constants.hpp"
#include "synthesizer.hpp"
#include "utils.hpp"


// TODO: add config file
#define MC_PATH string("/home/art/software/tlsf_model_checker/mc.sh")


using namespace std;
using namespace sdf;


struct SpecParam
{
    const string name;
    const bool is_real;

    SpecParam(string name_, bool is_real_) : name(move(name_)), is_real(is_real_) { }
};


/**
  * Checking realisability (without model extraction)
**/

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
    SpecParam("simple_arbiter_3.tlsf", true),
    SpecParam("detector.tlsf", true),
    SpecParam("full_arbiter.tlsf", true),
    SpecParam("load_balancer.tlsf", true),
    SpecParam("round_robin_arbiter.tlsf", true),
    SpecParam("round_robin_arbiter2.tlsf", true),
    SpecParam("prioritized_arbiter.tlsf", true),
    SpecParam("mealy_moore_real.tlsf", true),
};

class RealCheckFixture : public ::testing::TestWithParam<SpecParam> { };

TEST_P(RealCheckFixture, check_unreal)  // TODO: during test execution, test names are shown ugly (SpecParam should be updated)
{
    auto spec = GetParam();
    auto status = run(true, "./specs/" + spec.name, {4});
    if (spec.is_real)
        ASSERT_EQ(SYNTCOMP_RC_UNKNOWN, status);
    else
        ASSERT_EQ(SYNTCOMP_RC_UNREAL, status);
}

TEST_P(RealCheckFixture, check_real)
{
    auto spec = GetParam();
    auto status = run(false, "./specs/" + spec.name, {4});
    if (spec.is_real)
        ASSERT_EQ(SYNTCOMP_RC_REAL, status);
    else
        ASSERT_EQ(SYNTCOMP_RC_UNKNOWN, status);
}

INSTANTIATE_TEST_SUITE_P(RealUnreal, RealCheckFixture, ::testing::ValuesIn(specs));


/**
  * Checking Synthesis: extract and model check the models
**/

// they are all realisable by definition
const vector<string> specs_for_mc =
{
    "load_balancer_real2.tlsf",
    "simple_arbiter.tlsf",
    "simple_arbiter_3.tlsf",
    "detector.tlsf",
    "full_arbiter.tlsf",
    "load_balancer.tlsf",
    "round_robin_arbiter.tlsf",
//    "round_robin_arbiter2.tlsf",  // MC takes 4s
    "prioritized_arbiter.tlsf",
    "mealy_moore_real.tlsf"
};

class SyntWithMCFixture: public ::testing::TestWithParam<string>
{
public:
    const string tmpFolder;

    SyntWithMCFixture() : tmpFolder(create_tmp_folder())
    {
        cout << "(TEST) Using tmp folder: " << tmpFolder << endl;
    }
    ~SyntWithMCFixture() override
    {
        // TODO: rewrite: hackish: we need to remove the non-empty dir so `rmdir` does not work
        string cmd = "rm -rf " + tmpFolder;
        int res = system(cmd.c_str());
        MASSERT(res == 0, "could not remove the tmp folder");
    }
};

TEST_P(SyntWithMCFixture, synt_and_verify)
{
    auto spec = GetParam();
    auto specPath = "./specs/" + spec;
    auto modelPath = tmpFolder + "/" + spec + ".aag";
    cout << "(TEST) SYNTHESIS..." << endl;
    auto status = sdf::run(false, specPath, {2}, true, modelPath);
    ASSERT_EQ(SYNTCOMP_RC_REAL, status);
    cout << "(TEST) SYNTHESIS: SUCCESS!" << endl;

    cout << "(TEST) VERIFICATION..." << endl;
    int rc;
    string out, err;
    tie(rc, out, err) = execute(MC_PATH + " " + modelPath + " " + specPath);
    if (rc != 0)
    {
        cout << "(TEST) MC failed: (rc!=0): " << rc << ", spec: " << specPath;
        cout << "stdout was: " << endl << out << endl;
        cout << "stderr was: " << endl << err << endl;
        FAIL();
    }

    cout << out << endl;
    cout << "(TEST) VERIFICATION: SUCCESS!" << endl;
}

INSTANTIATE_TEST_SUITE_P(SyntWithMC,
                         SyntWithMCFixture,
                         ::testing::ValuesIn(specs_for_mc));


/// For future: good to check the exact values of parameter k that makes specs realizable.

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
