#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("Stationary pressure: trivial equilibrium",
          "[pressure][analytical]") {
    constexpr double P_init_atm = 200.0;
    constexpr double P_init_Pa = P_init_atm * 101325.0;

    auto horizon = test_helpers::make_uniform_horizon(
        10, 10, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, P_init_atm, 0.0);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_init_Pa;

    auto P_before = sim.GetPressureField();
    sim.Solve({0.0, 10.0, 50.0, 100.0});
    auto P_after = sim.GetPressureField();

    for (size_t i = 0; i < P_before.size(); ++i) {
        INFO("Cell " << i);
        REQUIRE_THAT(P_after[i], WithinRel(P_before[i], 1e-10));
    }
}

TEST_CASE("Stationary pressure: relaxation to boundary pressure",
          "[pressure][analytical]") {
    constexpr size_t Nx = 20;
    constexpr double Lx = 1000.0, Ly = 1000.0, hz = 10.0;
    constexpr double P_init_atm = 250.0;
    constexpr double P_ref_atm = 200.0;
    constexpr double P_ref_Pa = P_ref_atm * 101325.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Nx, 1, Lx, Ly, hz, 100.0, 0.2, P_init_atm, 0.0);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_ref_Pa;

    sim.Solve({0.0, 10.0, 50.0, 100.0, 500.0, 1000.0, 5000.0});

    auto P = sim.GetPressureField();

    double max_err = 0.0;
    for (size_t i = 0; i < P.size(); ++i) {
        double err = std::abs(P[i] - P_ref_Pa) / P_ref_Pa;
        if (err > max_err) max_err = err;
    }
    INFO("Max relative error = " << max_err);
    CHECK(max_err < 1e-3);
}

TEST_CASE("Stationary pressure: symmetry of uniform IC",
          "[pressure][analytical]") {
    constexpr size_t Nx = 10, Ny = 10;
    constexpr double P_init_atm = 200.0;
    constexpr double P_init_Pa = P_init_atm * 101325.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, P_init_atm, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_init_Pa;

    sim.Solve({0.0, 10.0});

    auto P = sim.GetPressureField();

    for (size_t j = 0; j < Ny; ++j) {
        for (size_t i = 0; i < Nx / 2; ++i) {
            size_t left = j * Nx + i;
            size_t right = j * Nx + (Nx - 1 - i);
            REQUIRE_THAT(P[left], WithinRel(P[right], 1e-12));
        }
    }
}
