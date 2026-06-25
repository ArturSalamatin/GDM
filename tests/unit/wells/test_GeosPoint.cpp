#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Data/wells/GeosPoint.h"

using namespace GeosShell;
using Catch::Approx;

TEST_CASE("SimplePoint: default constructor gives zero",
          "[unit][level0][wells][GeosPoint]") {
    SimplePoint sp;
    CHECK(sp.getX() == 0.0);
    CHECK(sp.getY() == 0.0);
}

TEST_CASE("SimplePoint: parameterized constructor",
          "[unit][level0][wells][GeosPoint]") {
    SimplePoint sp(3.5, 7.2);
    CHECK(sp.getX() == 3.5);
    CHECK(sp.getY() == 7.2);
}

TEST_CASE("GeosPoint: default constructor",
          "[unit][level0][wells][GeosPoint]") {
    GeosPoint gp;
    CHECK(gp.get()->getX() == 0.0);
    CHECK(gp.get()->getY() == 0.0);
}

TEST_CASE("GeosPoint: parameterized constructor",
          "[unit][level0][wells][GeosPoint]") {
    GeosPoint gp(10.0, 20.0);
    CHECK(gp.get()->getX() == 10.0);
    CHECK(gp.get()->getY() == 20.0);
}

TEST_CASE("GeosPoint: copy shares underlying SimplePoint",
          "[unit][level0][wells][GeosPoint]") {
    GeosPoint gp1(5.0, 15.0);
    GeosPoint gp2 = gp1;
    CHECK(gp1.get().use_count() == 2);
    CHECK(gp2.get()->getX() == 5.0);
    CHECK(gp2.get()->getY() == 15.0);
}

TEST_CASE("SimplePoint: negative coordinates",
          "[unit][level0][wells][GeosPoint]") {
    SimplePoint sp(-100.5, -200.3);
    CHECK(sp.getX() == -100.5);
    CHECK(sp.getY() == -200.3);
}

TEST_CASE("GeosPoint: shared_ptr mutation visible through copies",
          "[unit][level0][wells][GeosPoint]") {
    GeosPoint gp1(1.0, 2.0);
    GeosPoint gp2 = gp1;
    CHECK(gp1.get().get() == gp2.get().get());
}
