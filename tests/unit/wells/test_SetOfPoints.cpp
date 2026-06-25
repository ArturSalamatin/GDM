#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/Well/SetOfPoints.h"

using namespace set_of_points;
using Catch::Approx;

// --- Construction ---

TEST_CASE("SetOfPoints: single open segment",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob job(10.0, 30.0, true);
    SetOfPoints sop(job);
    CHECK(sop.totalLength() == Approx(20.0));
    auto& segs = sop.getCurrentConfiguration();
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].start == Approx(10.0));
    CHECK(segs[0].end == Approx(30.0));
}

TEST_CASE("SetOfPoints: two non-overlapping open segments",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob job1(10.0, 20.0, true);
    SetOfPoints sop(job1);
    sop.AddSegment(WellJob(30.0, 40.0, true));
    CHECK(sop.totalLength() == Approx(20.0));
    CHECK(sop.getCurrentConfiguration().size() == 2);
}

TEST_CASE("SetOfPoints: overlapping open segments merge",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob job1(10.0, 30.0, true);
    SetOfPoints sop(job1);
    sop.AddSegment(WellJob(20.0, 40.0, true));
    CHECK(sop.totalLength() == Approx(30.0));
    auto& segs = sop.getCurrentConfiguration();
    REQUIRE(segs.size() == 1);
    CHECK(segs[0].start == Approx(10.0));
    CHECK(segs[0].end == Approx(40.0));
}

TEST_CASE("SetOfPoints: close removes part of open segment",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob open_job(0.0, 100.0, true);
    SetOfPoints sop(open_job);
    sop.AddSegment(WellJob(30.0, 60.0, false));
    auto& segs = sop.getCurrentConfiguration();
    REQUIRE(segs.size() == 2);
    CHECK(segs[0].start == Approx(0.0));
    CHECK(segs[0].end == Approx(30.0));
    CHECK(segs[1].start == Approx(60.0));
    CHECK(segs[1].end == Approx(100.0));
    CHECK(sop.totalLength() == Approx(70.0));
}

TEST_CASE("SetOfPoints: close entire segment leaves empty",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob open_job(10.0, 30.0, true);
    SetOfPoints sop(open_job);
    sop.AddSegment(WellJob(10.0, 30.0, false));
    CHECK(sop.totalLength() == Approx(0.0));
    CHECK(sop.getCurrentConfiguration().empty());
}

TEST_CASE("SetOfPoints: totalLength sums all segments",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob job1(0.0, 10.0, true);
    SetOfPoints sop(job1);
    sop.AddSegment(WellJob(20.0, 35.0, true));
    sop.AddSegment(WellJob(50.0, 80.0, true));
    CHECK(sop.totalLength() == Approx(55.0));
}

TEST_CASE("SetOfPoints: operator== for identical configurations",
          "[unit][level1][wells][SetOfPoints]") {
    WellJob job(10.0, 30.0, true);
    SetOfPoints a(job);
    SetOfPoints b(job);
    CHECK(a == b);
}

TEST_CASE("SetOfPoints: operator== for different sizes returns false",
          "[unit][level1][wells][SetOfPoints]") {
    SetOfPoints a(WellJob(10.0, 30.0, true));
    SetOfPoints b(WellJob(10.0, 30.0, true));
    b.AddSegment(WellJob(40.0, 50.0, true));
    CHECK_FALSE(a == b);
}
