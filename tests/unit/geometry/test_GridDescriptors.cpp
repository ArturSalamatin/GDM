#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Grids/GridDescriptors.h"

using namespace reservoir_simulator;
using Catch::Approx;

TEST_CASE("GridBounds: constructor computes lengths from min/max",
          "[unit][level0][geometry][GridBounds]") {
    GridBounds gb(100.0, 200.0, 500.0, 600.0);
    CHECK(gb.x_length == Approx(400.0));
    CHECK(gb.y_length == Approx(400.0));
    CHECK(gb.x_min == 100.0);
    CHECK(gb.y_min == 200.0);
    CHECK(gb.x_max == 500.0);
    CHECK(gb.y_max == 600.0);
}

TEST_CASE("GridBounds: negative coords produce positive lengths",
          "[unit][level0][geometry][GridBounds]") {
    GridBounds gb(-100.0, -200.0, 100.0, 200.0);
    CHECK(gb.x_length == Approx(200.0));
    CHECK(gb.y_length == Approx(400.0));
}

TEST_CASE("GridBounds: default constructor gives zero dimensions",
          "[unit][level0][geometry][GridBounds]") {
    GridBounds gb;
    CHECK(gb.x_length == 0.0);
    CHECK(gb.y_length == 0.0);
    CHECK(gb.x_min == 0.0);
    CHECK(gb.y_min == 0.0);
    CHECK(gb.x_max == 0.0);
    CHECK(gb.y_max == 0.0);
}

TEST_CASE("GridBounds: inverted min/max still gives positive length",
          "[unit][level0][geometry][GridBounds]") {
    GridBounds gb(500.0, 600.0, 100.0, 200.0);
    CHECK(gb.x_length == Approx(400.0));
    CHECK(gb.y_length == Approx(400.0));
}

TEST_CASE("GridSize: aggregate initialization",
          "[unit][level0][geometry][GridSize]") {
    GridSize gs{10, 20, 3};
    CHECK(gs.Nx == 10);
    CHECK(gs.Ny == 20);
    CHECK(gs.Nz == 3);
}

TEST_CASE("BlockSize: aggregate initialization",
          "[unit][level0][geometry][BlockSize]") {
    BlockSize bs{25.0, 50.0};
    CHECK(bs.step_x == 25.0);
    CHECK(bs.step_y == 50.0);
}
