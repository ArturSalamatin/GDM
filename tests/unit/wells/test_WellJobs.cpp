#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/Well/WellJobs.h"

using namespace reservoir_simulator;
using namespace set_of_points;
using Catch::Approx;

TEST_CASE("WellJobs: construct from single layer jobs",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W1";
    JobsInLayer jobs = {
        WellJobTime(10.0, 30.0, true, 100.0),
        WellJobTime(40.0, 60.0, true, 200.0)
    };
    WellJobs wj(name, jobs);
    CHECK(wj.Name() == "W1");
    CHECK_FALSE(wj.IsEmpty());
}

TEST_CASE("WellJobs: AccumulatePerforations produces correct layers",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W2";
    JobsInLayer layer0 = {
        WellJobTime(0.0, 50.0, true, 100.0),
        WellJobTime(60.0, 80.0, true, 200.0)
    };
    JobsInLayer layer1 = {
        WellJobTime(10.0, 40.0, true, 150.0)
    };
    WellJobsPerLayer layers = {layer0, layer1};
    WellJobs wj(name, layers);

    auto perfs = wj.AccumulatePerforations();
    CHECK(perfs.size() == 2);
    CHECK(perfs.count(0) == 1);
    CHECK(perfs.count(1) == 1);
}

TEST_CASE("WellJobs: empty layer does not appear in AccumulatePerforations",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W3";
    JobsInLayer layer0 = {};
    JobsInLayer layer1 = {
        WellJobTime(10.0, 40.0, true, 100.0)
    };
    WellJobsPerLayer layers = {layer0, layer1};
    WellJobs wj(name, layers);

    auto perfs = wj.AccumulatePerforations();
    CHECK(perfs.count(0) == 0);
    CHECK(perfs.count(1) == 1);
}

TEST_CASE("WellJobs: AccumulatePerforations respects time ordering",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W4";
    JobsInLayer jobs = {
        WellJobTime(0.0, 100.0, true, 50.0),
        WellJobTime(30.0, 60.0, false, 150.0)
    };
    WellJobs wj(name, {jobs});
    auto perfs = wj.AccumulatePerforations();

    REQUIRE(perfs.count(0) == 1);
    auto& ap = perfs.at(0);
    auto& final_perf = ap.getPerforations(200.0);
    CHECK(final_perf.TotalLength() == Approx(70.0));
}

TEST_CASE("WellJobs: jobsInLayer accessor",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W5";
    JobsInLayer layer0 = {
        WellJobTime(10.0, 30.0, true, 100.0)
    };
    WellJobs wj(name, {layer0});
    CHECK(wj.jobsInLayer(0).size() == 1);
}

TEST_CASE("WellJobs: IsEmpty returns true when all layers empty",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W_empty";
    WellJobsPerLayer layers = {
        JobsInLayer{},
        JobsInLayer{},
        JobsInLayer{}
    };
    WellJobs wj(name, layers);
    CHECK(wj.IsEmpty());
}

TEST_CASE("WellJobs: IsEmpty returns false when one layer has jobs",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W_partial";
    WellJobsPerLayer layers = {
        JobsInLayer{},
        JobsInLayer{ WellJobTime(0.0, 50.0, true, 100.0) },
        JobsInLayer{}
    };
    WellJobs wj(name, layers);
    CHECK_FALSE(wj.IsEmpty());
}

TEST_CASE("WellJobs: IsEmpty returns true when zero layers",
          "[unit][level3][wells][WellJobs]") {
    std::string name = "W_zero";
    WellJobsPerLayer layers = {};
    WellJobs wj(name, layers);
    CHECK(wj.IsEmpty());
}
