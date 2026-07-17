#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Math/MathRoutines.h"
#include <cmath>

using namespace math_routines;
using Catch::Approx;

// --- LinearInterp (scalar) ---

TEST_CASE("LinearInterp: midpoint gives average",
          "[unit][level1][geometry][MathRoutines]") {
    double v = MathRoutines::LinearInterp(0.5, 0.0, 1.0, 10.0, 20.0);
    CHECK(v == Approx(15.0));
}

TEST_CASE("LinearInterp: at t1 returns v1",
          "[unit][level1][geometry][MathRoutines]") {
    double v = MathRoutines::LinearInterp(0.0, 0.0, 1.0, 10.0, 20.0);
    CHECK(v == Approx(10.0));
}

TEST_CASE("LinearInterp: at t2 returns v2",
          "[unit][level1][geometry][MathRoutines]") {
    double v = MathRoutines::LinearInterp(1.0, 0.0, 1.0, 10.0, 20.0);
    CHECK(v == Approx(20.0));
}

TEST_CASE("LinearInterp: t1 == t2 returns average of v1 and v2",
          "[unit][level1][geometry][MathRoutines]") {
    double v = MathRoutines::LinearInterp(5.0, 3.0, 3.0, 10.0, 20.0);
    CHECK(v == Approx(15.0));
}

TEST_CASE("LinearInterp: extrapolation beyond [t1,t2]",
          "[unit][level1][geometry][MathRoutines]") {
    double v = MathRoutines::LinearInterp(2.0, 0.0, 1.0, 0.0, 10.0);
    CHECK(v == Approx(20.0));
}

// --- LinearInterp (pair) ---

TEST_CASE("LinearInterp pair: interpolates both components",
          "[unit][level1][geometry][MathRoutines]") {
    auto v = MathRoutines::LinearInterp(
        0.5, 0.0, 1.0,
        std::make_pair(0.0, 100.0),
        std::make_pair(10.0, 200.0));
    CHECK(v.first == Approx(5.0));
    CHECK(v.second == Approx(150.0));
}

// --- BilinearInterp ---

TEST_CASE("BilinearInterp: center of unit square with uniform values",
          "[unit][level1][geometry][MathRoutines]") {
    using reservoir_simulator::phasePortrait::Point;
    Point query(0.5, 0.5);
    std::array<Point, 4> P = {
        Point(0, 0), Point(0, 1), Point(1, 1), Point(1, 0)
    };
    std::array<double, 4> vals = {1.0, 1.0, 1.0, 1.0};
    double v = MathRoutines::BilinearInterp(query, P, vals);
    CHECK(v == Approx(1.0));
}

TEST_CASE("BilinearInterp: center of square with varying values",
          "[unit][level1][geometry][MathRoutines]") {
    using reservoir_simulator::phasePortrait::Point;
    Point query(0.5, 0.5);
    std::array<Point, 4> P = {
        Point(0, 0), Point(0, 1), Point(1, 1), Point(1, 0)
    };
    std::array<double, 4> vals = {0.0, 10.0, 20.0, 30.0};
    double v = MathRoutines::BilinearInterp(query, P, vals);
    CHECK(v == Approx(15.0));
}

// --- LowerPointUniformMesh ---

TEST_CASE("LowerPointUniformMesh: query at mesh node",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 1.0, 2.0, 3.0};
    CHECK(MathRoutines::LowerPointUniformMesh(mesh, 2.0) == 2);
}

TEST_CASE("LowerPointUniformMesh: query between nodes",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 1.0, 2.0, 3.0};
    CHECK(MathRoutines::LowerPointUniformMesh(mesh, 1.5) == 1);
}

TEST_CASE("LowerPointUniformMesh: query at origin",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 1.0, 2.0};
    CHECK(MathRoutines::LowerPointUniformMesh(mesh, 0.0) == 0);
}

TEST_CASE("LowerPointUniformMesh: query below mesh returns negative",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 1.0, 2.0};
    CHECK(MathRoutines::LowerPointUniformMesh(mesh, -0.5) < 0);
}

// --- LowerPointNonUniformMesh ---

TEST_CASE("LowerPointNonUniformMesh: query between nodes",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 0.5, 1.5, 4.0, 10.0};
    CHECK(MathRoutines::LowerPointNonUniformMesh(mesh, 2.0) == 2);
}

TEST_CASE("LowerPointNonUniformMesh: query at first node",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 1.0, 3.0};
    CHECK(MathRoutines::LowerPointNonUniformMesh(mesh, 0.0) == 0);
}

TEST_CASE("LowerPointNonUniformMesh: query below mesh returns negative",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 0.5, 1.5, 4.0, 10.0};
    CHECK(MathRoutines::LowerPointNonUniformMesh(mesh, -1.0) < 0);
}

TEST_CASE("LowerPointNonUniformMesh: query above mesh returns negative",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 0.5, 1.5, 4.0, 10.0};
    CHECK(MathRoutines::LowerPointNonUniformMesh(mesh, 15.0) < 0);
}

// --- InterpFieldConstTime ---

TEST_CASE("InterpFieldConstTime: interpolates on 3x3 grid",
          "[unit][level1][geometry][MathRoutines]") {
    using reservoir_simulator::phasePortrait::Point;

    std::vector<double> x_mesh = {0.0, 1.0, 2.0};
    std::vector<double> y_mesh = {0.0, 1.0, 2.0};
    std::vector<std::vector<double>> field = {
        {1.0, 2.0, 3.0},  // y=0
        {4.0, 5.0, 6.0},  // y=1
        {7.0, 8.0, 9.0}   // y=2
    };

    Point center(0.5, 0.5);
    double v = MathRoutines::InterpFieldConstTime(center, x_mesh, y_mesh, field);
    CHECK(v == Approx(3.0));
}

TEST_CASE("InterpFieldConstTime: out-of-bounds returns NaN",
          "[unit][level1][geometry][MathRoutines]") {
    using reservoir_simulator::phasePortrait::Point;

    std::vector<double> x_mesh = {0.0, 1.0, 2.0};
    std::vector<double> y_mesh = {0.0, 1.0, 2.0};
    std::vector<std::vector<double>> field = {
        {1.0, 2.0, 3.0},
        {4.0, 5.0, 6.0},
        {7.0, 8.0, 9.0}
    };

    Point outside(-1.0, 0.5);
    double v = MathRoutines::InterpFieldConstTime(outside, x_mesh, y_mesh, field);
    CHECK(std::isnan(v));
}
