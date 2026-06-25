#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Grids/RawHorizon.h"

using namespace reservoir_simulator;
using Catch::Approx;

TEST_CASE("RawHorizon: GridSize returns Nx*Ny*Nz",
          "[unit][level1][reservoir][RawHorizon]") {
    RawHorizon h;
    h.grid_size = {3, 4, 2};
    CHECK(h.GridSize() == 24);
}

TEST_CASE("RawHorizon: GridPlanarSize returns Nx*Ny",
          "[unit][level1][reservoir][RawHorizon]") {
    RawHorizon h;
    h.grid_size = {5, 6, 3};
    CHECK(h.GridPlanarSize() == 30);
}

TEST_CASE("RawHorizon: nx and ny accessors",
          "[unit][level1][reservoir][RawHorizon]") {
    RawHorizon h;
    h.grid_size = {7, 11, 1};
    CHECK(h.nx() == 7);
    CHECK(h.ny() == 11);
}

TEST_CASE("RawHorizon: initial_water_saturation = 1 - oil_saturation",
          "[unit][level1][reservoir][RawHorizon]") {
    RawHorizon h;
    h.grid_size = {2, 1, 1};
    h.initial_oil_saturation = {0.8, 0.6};

    auto Sw = h.initial_water_saturation();
    REQUIRE(Sw.size() == 2);
    CHECK(Sw[0] == Approx(0.2));
    CHECK(Sw[1] == Approx(0.4));
}

TEST_CASE("RawHorizon: initial_water_saturation for zero oil gives Sw=1",
          "[unit][level1][reservoir][RawHorizon]") {
    RawHorizon h;
    h.grid_size = {1, 1, 1};
    h.initial_oil_saturation = {0.0};

    auto Sw = h.initial_water_saturation();
    REQUIRE(Sw.size() == 1);
    CHECK(Sw[0] == Approx(1.0));
}

TEST_CASE("RawHorizon: GridSize for single-cell grid",
          "[unit][level1][reservoir][RawHorizon]") {
    RawHorizon h;
    h.grid_size = {1, 1, 1};
    CHECK(h.GridSize() == 1);
    CHECK(h.GridPlanarSize() == 1);
}
