#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "Solver/Grids/PropertyDescriptor.h"

using namespace reservoir_simulator;
using Catch::Approx;

TEST_CASE("CustomUnitConverter: viscosity conversion factor",
          "[unit][level0][physics][PropertyDescriptor]") {
    CHECK(CustomUnitConverter::viscosityConverter() == Approx(1.0 / 86400.0 / 1000.0));
}

TEST_CASE("PhaseProperties: constructor applies viscosity conversion",
          "[unit][level0][physics][PhaseProperties]") {
    double visc_mPas = GENERATE(0.5, 1.0, 3.15, 10.0);
    CAPTURE(visc_mPas);

    WaterPhaseProperty w(visc_mPas, 1000.0, 0.0, 0.2, 1e5);
    CHECK(w.Viscosity() == Approx(visc_mPas / 86400.0 / 1000.0));
}

TEST_CASE("PhaseProperties: all getters return constructor values",
          "[unit][level0][physics][PhaseProperties]") {
    OilPhaseProperty oil(2.0, 800.0, 1e-9, 0.15, 2e7);
    CHECK(oil.Density() == 800.0);
    CHECK(oil.Compressibility() == 1e-9);
    CHECK(oil.ResidualFraction() == 0.15);
    CHECK(oil.ReferencePressure() == 2e7);
}

TEST_CASE("PhaseProperties: default constructor",
          "[unit][level0][physics][PhaseProperties]") {
    WaterPhaseProperty w;
    CHECK(w.ReferencePressure() == Approx(1e5));
    CHECK(w.Density() == Approx(1000.0));
    CHECK(w.Compressibility() == 0.0);
    CHECK(w.ResidualFraction() == 0.0);
}

TEST_CASE("OtherProperties: default values",
          "[unit][level0][physics][OtherProperties]") {
    OtherProperties op;
    CHECK(op.g == Approx(9.81));
    CHECK(op.extPressure == Approx(101325.0));
}
