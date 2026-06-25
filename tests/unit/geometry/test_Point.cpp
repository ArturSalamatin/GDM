#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Anomaly/FlowField/Point.h"
#include <cmath>
#include <limits>

using namespace reservoir_simulator::phasePortrait;
using Catch::Approx;

// --- Point ---

TEST_CASE("Point: constructor and accessors",
          "[unit][level0][geometry][Point]") {
    Point p(3.5, 7.2);
    CHECK(p.x() == 3.5);
    CHECK(p.y() == 7.2);
}

TEST_CASE("Point: distance to another point",
          "[unit][level0][geometry][Point]") {
    Point p1(0.0, 0.0);
    Point p2(3.0, 4.0);
    CHECK(p1.distance(p2) == Approx(5.0));
}

TEST_CASE("Point: distance to self is zero",
          "[unit][level0][geometry][Point]") {
    Point p(10.0, 20.0);
    CHECK(p.distance(p) == Approx(0.0));
}

TEST_CASE("Point: Area static method",
          "[unit][level0][geometry][Point]") {
    CHECK(Point::Area(Point(0, 0), Point(3, 4)) == Approx(12.0));
    CHECK(Point::Area(Point(1, 1), Point(1, 1)) == Approx(0.0));
}

TEST_CASE("Point: Area member method",
          "[unit][level0][geometry][Point]") {
    Point p1(0, 0);
    Point p2(5, 3);
    CHECK(p1.Area(p2) == Approx(15.0));
}

TEST_CASE("Point: Area with 4-point array",
          "[unit][level0][geometry][Point]") {
    Point center(2, 2);
    std::array<Point, 4> corners = {
        Point(0, 0), Point(4, 0), Point(0, 4), Point(4, 4)
    };
    auto areas = Point::Area(center, corners);
    CHECK(areas[0] == Approx(4.0));
    CHECK(areas[1] == Approx(4.0));
    CHECK(areas[2] == Approx(4.0));
    CHECK(areas[3] == Approx(4.0));
}

// --- trPoint ---

TEST_CASE("trPoint: constructor and accessors",
          "[unit][level0][geometry][trPoint]") {
    trPoint tp(10.0, 5.0, 1.0, 2.0);
    CHECK(tp.t() == 10.0);
    CHECK(tp.s() == 5.0);
    CHECK(tp.x() == 1.0);
    CHECK(tp.y() == 2.0);
}

TEST_CASE("trPoint: Move applies direction * dt",
          "[unit][level0][geometry][trPoint]") {
    trPoint tp(0.0, 0.0, 1.0, 2.0);
    Direction dir = {1.0, 0.5, -0.5};
    auto moved = tp.Move(dir, 2.0);
    CHECK(moved.t() == Approx(2.0));
    CHECK(moved.s() == Approx(2.0));
    CHECK(moved.x() == Approx(2.0));
    CHECK(moved.y() == Approx(1.0));
}

TEST_CASE("trPoint: is_normal true for finite values",
          "[unit][level0][geometry][trPoint]") {
    trPoint tp(1.0, 2.0, 3.0, 4.0);
    CHECK(tp.is_normal());
}

TEST_CASE("trPoint: is_normal false for infinity",
          "[unit][level0][geometry][trPoint]") {
    trPoint bad(std::numeric_limits<double>::infinity(), 0, 0, 0);
    CHECK_FALSE(bad.is_normal());
}

TEST_CASE("trPoint: is_normal false for NaN",
          "[unit][level0][geometry][trPoint]") {
    trPoint bad(std::numeric_limits<double>::quiet_NaN(), 0, 0, 0);
    CHECK_FALSE(bad.is_normal());
}

TEST_CASE("trPoint: operator+ sums components",
          "[unit][level0][geometry][trPoint]") {
    trPoint a(1.0, 2.0, 3.0, 4.0);
    trPoint b(10.0, 20.0, 30.0, 40.0);
    auto c = a + b;
    CHECK(c.t() == Approx(11.0));
    CHECK(c.s() == Approx(22.0));
    CHECK(c.x() == Approx(33.0));
    CHECK(c.y() == Approx(44.0));
}

TEST_CASE("trPoint: operator/ divides components",
          "[unit][level0][geometry][trPoint]") {
    trPoint a(10.0, 20.0, 30.0, 40.0);
    auto b = a / 2.0;
    CHECK(b.t() == Approx(5.0));
    CHECK(b.s() == Approx(10.0));
    CHECK(b.x() == Approx(15.0));
    CHECK(b.y() == Approx(20.0));
}

// --- Contour ---

TEST_CASE("Contour: size matches input vectors",
          "[unit][level0][geometry][Contour]") {
    Contour c({1, 2, 3}, {4, 5, 6});
    CHECK(c.size() == 3);
    CHECK(c.x().size() == 3);
    CHECK(c.y().size() == 3);
}

TEST_CASE("Contour: default is empty",
          "[unit][level0][geometry][Contour]") {
    Contour c;
    CHECK(c.size() == 0);
}

// --- geos_polygon (stub) ---

TEST_CASE("geos_polygon: stub methods return safe defaults",
          "[unit][level0][geometry][geos_polygon]") {
    geos_polygon p1;
    geos_polygon p2;
    CHECK_FALSE(p1.contains(p2));
    CHECK_FALSE(p1.intersects(p2));
    CHECK(p1.vertecies_nmbr() == 0);
}
