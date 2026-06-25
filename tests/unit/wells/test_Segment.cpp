#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/Well/SetOfPoints.h"

using namespace set_of_points;
using Catch::Approx;

// --- Segment ---

TEST_CASE("Segment: constructor and length",
          "[unit][level0][wells][Segment]") {
    Segment s(10.0, 30.0);
    CHECK(s.length() == Approx(20.0));
    CHECK(s.start == 10.0);
    CHECK(s.end == 30.0);
}

TEST_CASE("Segment: segment() returns pair",
          "[unit][level0][wells][Segment]") {
    Segment s(5.0, 25.0);
    auto p = s.segment();
    CHECK(p.first == 5.0);
    CHECK(p.second == 25.0);
}

TEST_CASE("Segment: constructor from pair",
          "[unit][level0][wells][Segment]") {
    Segment s(std::make_pair(3.0, 7.0));
    CHECK(s.start == 3.0);
    CHECK(s.end == 7.0);
    CHECK(s.length() == Approx(4.0));
}

TEST_CASE("Segment: operator== for equal segments",
          "[unit][level0][wells][Segment]") {
    Segment s1(10.0, 30.0);
    Segment s2(10.0, 30.0);
    CHECK(s1 == s2);
}

TEST_CASE("Segment: operator== for different segments",
          "[unit][level0][wells][Segment]") {
    Segment s1(10.0, 30.0);
    Segment s2(10.0, 25.0);
    CHECK_FALSE(s1 == s2);
}

TEST_CASE("Segment: inverted endpoints get normalized",
          "[unit][level0][wells][Segment]") {
    Segment s(30.0, 10.0);
    CHECK(s.start == 10.0);
    CHECK(s.end == 30.0);
    CHECK(s.length() == Approx(20.0));
}

TEST_CASE("Segment: zero-length segment",
          "[unit][level0][wells][Segment]") {
    Segment s(10.0, 10.0);
    CHECK(s.length() == Approx(0.0));
    CHECK(s.start == s.end);
}

// --- WellJob ---

TEST_CASE("WellJob: open job",
          "[unit][level0][wells][WellJob]") {
    WellJob job(Segment(10.0, 30.0), true);
    CHECK(job.isOpen());
}

TEST_CASE("WellJob: close job",
          "[unit][level0][wells][WellJob]") {
    WellJob job(Segment(10.0, 30.0), false);
    CHECK_FALSE(job.isOpen());
}

TEST_CASE("WellJob: constructor from pair",
          "[unit][level0][wells][WellJob]") {
    WellJob job(std::make_pair(5.0, 15.0), true);
    CHECK(job.isOpen());
}

TEST_CASE("WellJob: constructor from doubles",
          "[unit][level0][wells][WellJob]") {
    WellJob job(5.0, 15.0, false);
    CHECK_FALSE(job.isOpen());
}

// --- WellJobTime ---

TEST_CASE("WellJobTime: stores time moment",
          "[unit][level0][wells][WellJobTime]") {
    WellJobTime jt(10.0, 30.0, true, 100.0);
    CHECK(jt.timeMoment == 100.0);
    CHECK(jt.isOpen());
}

TEST_CASE("WellJobTime: sortOperator orders by time",
          "[unit][level0][wells][WellJobTime]") {
    WellJobTime jt1(10.0, 30.0, true, 100.0);
    WellJobTime jt2(10.0, 30.0, true, 200.0);
    CHECK(WellJobTime::sortOperator(jt1, jt2));
    CHECK_FALSE(WellJobTime::sortOperator(jt2, jt1));
}

TEST_CASE("WellJobTime: TransferDate changes time",
          "[unit][level0][wells][WellJobTime]") {
    WellJobTime jt(10.0, 30.0, true, 100.0);
    jt.TransferDate(50.0);
    CHECK(jt.timeMoment == 50.0);
}
