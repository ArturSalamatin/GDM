#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("Mass balance: closed reservoir conserves mass",
          "[balance][conservation]") {
    constexpr double P_init_atm = 200.0;
    constexpr double P_init_Pa = P_init_atm * 101325.0;

    auto horizon = test_helpers::make_uniform_horizon(
        20, 20, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, P_init_atm, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_init_Pa;

    double oil_0 = sim.OilTotal();
    double water_0 = sim.WaterTotal();

    INFO("Initial oil = " << oil_0 << " kg, water = " << water_0 << " kg");
    REQUIRE(oil_0 > 0.0);
    REQUIRE(water_0 > 0.0);

    sim.Solve({0.0, 10.0, 50.0, 100.0});

    REQUIRE_THAT(sim.OilTotal(), WithinRel(oil_0, 1e-10));
    REQUIRE_THAT(sim.WaterTotal(), WithinRel(water_0, 1e-10));
}

TEST_CASE("Mass balance: expected initial mass values",
          "[balance][conservation]") {
    constexpr double P_init_atm = 200.0;
    constexpr double P_init_Pa = P_init_atm * 101325.0;
    constexpr double poro = 0.2;
    constexpr double oil_sat = 0.8;
    constexpr double rho_oil = 800.0;
    constexpr double rho_water = 1000.0;
    constexpr double V_total = 1000.0 * 1000.0 * 10.0;

    auto horizon = test_helpers::make_uniform_horizon(
        20, 20, 1, 1000.0, 1000.0, 10.0, 100.0, poro, P_init_atm, oil_sat);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double expected_oil = poro * oil_sat * rho_oil * V_total;
    double expected_water = poro * (1.0 - oil_sat) * rho_water * V_total;

    REQUIRE_THAT(sim.OilTotal(), WithinRel(expected_oil, 1e-3));
    REQUIRE_THAT(sim.WaterTotal(), WithinRel(expected_water, 1e-3));
}

TEST_CASE("Mass balance: open boundary flow decreases mass",
          "[balance][conservation]") {
    constexpr double P_init_atm = 250.0;
    constexpr double P_ref_atm = 200.0;

    auto horizon = test_helpers::make_uniform_horizon(
        20, 20, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, P_init_atm, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_ref_atm * 101325.0;

    double oil_prev = sim.OilTotal();

    std::vector<double> times = {0.0, 10.0, 50.0, 100.0};
    for (size_t step = 1; step < times.size(); ++step) {
        sim.Solve({times[step - 1], times[step]});

        double oil_cur = sim.OilTotal();
        INFO("Step " << step << ": t=" << times[step]
             << " oil=" << oil_cur);
        CHECK(oil_cur <= oil_prev + 1.0);

        oil_prev = oil_cur;
    }
}

TEST_CASE("Physical constraints: S_w + S_o = 1, P > 0",
          "[balance][conservation]") {
    auto horizon = test_helpers::make_uniform_horizon(
        15, 15, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);

    sim.Solve({0.0, 10.0, 50.0, 100.0});

    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();
    auto So = sim.GetOilSaturationField();

    for (size_t i = 0; i < P.size(); ++i) {
        INFO("Cell " << i);
        CHECK(P[i] > 0.0);
        CHECK(Sw[i] >= -1e-12);
        CHECK(Sw[i] <= 1.0 + 1e-12);
        CHECK(So[i] >= -1e-12);
        CHECK(So[i] <= 1.0 + 1e-12);
        REQUIRE_THAT(Sw[i] + So[i], WithinAbs(1.0, 1e-12));
    }
}
