#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/Well/SetOfPoints.h"
#include <limits>

using namespace set_of_points;
using Catch::Approx;

TEST_CASE("SetOfPerforations: default is closed with min time",
          "[unit][level1][wells][SetOfPerforations]") {
    SetOfPerforations sp;
    CHECK(sp.curTime() == -std::numeric_limits<double>::max());
    CHECK(sp.TotalLength() == Approx(0.0));
}

TEST_CASE("SetOfPerforations: construct from WellJobTime",
          "[unit][level1][wells][SetOfPerforations]") {
    WellJobTime job(10.0, 30.0, true, 100.0);
    SetOfPerforations sp(job);
    CHECK(sp.curTime() == 100.0);
    CHECK(sp.TotalLength() == Approx(20.0));
}

TEST_CASE("SetOfPerforations: AddNewJob updates time and perforations",
          "[unit][level1][wells][SetOfPerforations]") {
    WellJobTime job1(10.0, 30.0, true, 100.0);
    SetOfPerforations sp(job1);

    WellJobTime job2(40.0, 60.0, true, 200.0);
    sp.AddNewJob(job2);
    CHECK(sp.curTime() == 200.0);
    CHECK(sp.TotalLength() == Approx(40.0));
}

TEST_CASE("SetOfPerforations: MoveDate changes time without affecting perforations",
          "[unit][level1][wells][SetOfPerforations]") {
    WellJobTime job(10.0, 30.0, true, 100.0);
    SetOfPerforations sp(job);
    double len_before = sp.TotalLength();

    sp.MoveDate(50.0);
    CHECK(sp.curTime() == 50.0);
    CHECK(sp.TotalLength() == Approx(len_before));
}

TEST_CASE("SetOfPerforations: close job reduces TotalLength",
          "[unit][level1][wells][SetOfPerforations]") {
    WellJobTime open_job(0.0, 100.0, true, 10.0);
    SetOfPerforations sp(open_job);
    CHECK(sp.TotalLength() == Approx(100.0));

    WellJobTime close_job(30.0, 60.0, false, 20.0);
    sp.AddNewJob(close_job);
    CHECK(sp.TotalLength() == Approx(70.0));
}

TEST_CASE("SetOfPerforations: operator== compares perforation geometry",
          "[unit][level1][wells][SetOfPerforations]") {
    WellJobTime job(10.0, 30.0, true, 100.0);
    SetOfPerforations a(job);
    SetOfPerforations b(job);
    CHECK(a == b);
}

TEST_CASE("SetOfPerforations: JoinPerforations merges two sets",
          "[unit][level1][wells][SetOfPerforations]") {
    WellJobTime job1(0.0, 50.0, true, 100.0);
    SetOfPerforations sp1(job1);

    WellJobTime job2(60.0, 80.0, true, 200.0);
    SetOfPerforations sp2(job2);

    auto joined = sp1.JoinPerforations(sp2);
    CHECK(joined.TotalLength() == Approx(70.0));
    CHECK(joined.curTime() == std::min(sp1.curTime(), sp2.curTime()));
}
