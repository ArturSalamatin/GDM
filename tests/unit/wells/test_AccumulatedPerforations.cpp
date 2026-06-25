#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/Well/SetOfPoints.h"
#include <limits>

using namespace set_of_points;
using Catch::Approx;

TEST_CASE("AccumulatedPerforations: default has one empty SetOfPerforations",
          "[unit][level2][wells][AccumulatedPerforations]") {
    AccumulatedPerforations ap;
    CHECK(ap.getPerforationsSet().size() == 1);
    CHECK(ap.getPerforations(0.0).TotalLength() == Approx(0.0));
}

TEST_CASE("AccumulatedPerforations: construct from WellJobTime",
          "[unit][level2][wells][AccumulatedPerforations]") {
    WellJobTime job(10.0, 30.0, true, 100.0);
    AccumulatedPerforations ap(job);
    CHECK(ap.getPerforationsSet().size() == 2);
}

TEST_CASE("AccumulatedPerforations: AddNewJob at increasing times",
          "[unit][level2][wells][AccumulatedPerforations]") {
    AccumulatedPerforations ap;
    ap.AddNewJob(WellJobTime(0.0, 50.0, true, 100.0));
    ap.AddNewJob(WellJobTime(60.0, 80.0, true, 200.0));

    CHECK(ap.getPerforationsSet().size() == 3);
    CHECK(ap.getPerforations(150.0).TotalLength() == Approx(50.0));
    CHECK(ap.getPerforations(250.0).TotalLength() == Approx(70.0));
}

TEST_CASE("AccumulatedPerforations: getPerforations before any job returns empty",
          "[unit][level2][wells][AccumulatedPerforations]") {
    AccumulatedPerforations ap;
    ap.AddNewJob(WellJobTime(0.0, 50.0, true, 100.0));
    auto& perf = ap.getPerforations(50.0);
    CHECK(perf.TotalLength() == Approx(0.0));
}

TEST_CASE("AccumulatedPerforations: getPerforations after all jobs returns last config",
          "[unit][level2][wells][AccumulatedPerforations]") {
    AccumulatedPerforations ap;
    ap.AddNewJob(WellJobTime(0.0, 50.0, true, 100.0));
    ap.AddNewJob(WellJobTime(60.0, 80.0, true, 200.0));
    auto& perf = ap.getPerforations(1000.0);
    CHECK(perf.TotalLength() == Approx(70.0));
}

TEST_CASE("AccumulatedPerforations: AddNewJob at same time appends to same set",
          "[unit][level2][wells][AccumulatedPerforations]") {
    AccumulatedPerforations ap;
    ap.AddNewJob(WellJobTime(0.0, 30.0, true, 100.0));
    ap.AddNewJob(WellJobTime(40.0, 60.0, true, 100.0));
    CHECK(ap.getPerforationsSet().size() == 2);
    CHECK(ap.getPerforations(100.0).TotalLength() == Approx(50.0));
}
