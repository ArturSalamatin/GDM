#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("Smoke: simulator creates and runs", "[smoke]") {
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator simulator{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    simulator.numPrm.set_initial_schemeTau(10.0);
    simulator.numPrm.set_currentMoment(0.0);

    simulator.Solve({0.0, 10.0});

    REQUIRE(simulator.OilTotal() > 0.0);
    REQUIRE(simulator.WaterTotal() > 0.0);
}

TEST_CASE("Smoke: saturation bounds", "[smoke]") {
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator simulator{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    simulator.numPrm.set_initial_schemeTau(10.0);
    simulator.numPrm.set_currentMoment(0.0);

    simulator.Solve({0.0, 10.0});

    auto Sw = simulator.GetWaterSaturationField();
    auto So = simulator.GetOilSaturationField();
    for (size_t i = 0; i < Sw.size(); ++i) {
        REQUIRE(Sw[i] >= 0.0);
        REQUIRE(Sw[i] <= 1.0);
        REQUIRE(So[i] >= 0.0);
        REQUIRE(So[i] <= 1.0);
        REQUIRE_THAT(Sw[i] + So[i],
            Catch::Matchers::WithinAbs(1.0, 1e-12));
    }
}
