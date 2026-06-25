#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Solver/Grids/Cells/AbstractCells.h"

using namespace reservoir_simulator::cell;
using Catch::Approx;

// --- SomeDimCell ---

TEST_CASE("SomeDimCell: default has zero volume",
          "[unit][level2][physics][SomeDimCell]") {
    SomeDimCell c;
    CHECK(c.Volume() == Approx(0.0));
}

TEST_CASE("SomeDimCell: volume is product of sizes",
          "[unit][level2][physics][SomeDimCell]") {
    SomeDimCell c({1.0, 2.0}, {3.0, 4.0});
    CHECK(c.Volume() == Approx(12.0));
}

// --- Dim1Cell ---

TEST_CASE("Dim1Cell: X and StepX",
          "[unit][level2][physics][Dim1Cell]") {
    Dim1Cell c({5.0}, {2.0});
    CHECK(c.X() == Approx(5.0));
    CHECK(c.StepX() == Approx(2.0));
    CHECK(c.Volume() == Approx(2.0));
}

// --- Dim2Cell ---

TEST_CASE("Dim2Cell: X, Y, StepX, StepY",
          "[unit][level2][physics][Dim2Cell]") {
    Dim2Cell c({3.0, 7.0}, {2.0, 5.0});
    CHECK(c.X() == Approx(3.0));
    CHECK(c.Y() == Approx(7.0));
    CHECK(c.StepX() == Approx(2.0));
    CHECK(c.StepY() == Approx(5.0));
    CHECK(c.Volume() == Approx(10.0));
}

// --- Dim3Cell ---

TEST_CASE("Dim3Cell: X, Y, Z, StepZ, Volume",
          "[unit][level2][physics][Dim3Cell]") {
    Dim3Cell c({1.0, 2.0, 3.0}, {4.0, 5.0, 6.0});
    CHECK(c.X() == Approx(1.0));
    CHECK(c.Y() == Approx(2.0));
    CHECK(c.Z() == Approx(3.0));
    CHECK(c.StepX() == Approx(4.0));
    CHECK(c.StepY() == Approx(5.0));
    CHECK(c.StepZ() == Approx(6.0));
    CHECK(c.Volume() == Approx(120.0));
}

TEST_CASE("Dim3Cell: default has zero volume",
          "[unit][level2][physics][Dim3Cell]") {
    Dim3Cell c;
    CHECK(c.Volume() == Approx(0.0));
}

// --- PhysPropCell static array ---

TEST_CASE("PhysPropCell: set and get ConstantPointProperties",
          "[unit][level2][physics][PhysPropCell]") {
    std::array<double, 8> props = {9.81, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
    PhysPropCell::set_constantPointProperties(props);
    auto got = PhysPropCell::GetConstPointProperties();
    for (int i = 0; i < 8; i++)
        CHECK(got[i] == Approx(props[i]));
}
