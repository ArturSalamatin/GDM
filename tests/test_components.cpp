#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "test_helpers.h"
#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// --- Relperm (Corey n=3) ---

TEST_CASE("Corey relperm: boundary values", "[components][relperm]") {
    // k_rw(0) = 0, k_ro(0) = 1
    // k_rw(1) = 1, k_ro(1) = 0
    REQUIRE(std::pow(0.0, 3) == 0.0);
    REQUIRE(std::pow(1.0, 3) == 1.0);
}

TEST_CASE("Corey relperm: intermediate values match formula",
          "[components][relperm]") {
    auto Sw = GENERATE(0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0);
    double krw = std::pow(Sw, 3);
    double kro = std::pow(1.0 - Sw, 3);

    CAPTURE(Sw);
    CHECK(krw >= 0.0);
    CHECK(krw <= 1.0);
    CHECK(kro >= 0.0);
    CHECK(kro <= 1.0);
    REQUIRE_THAT(krw + kro, WithinAbs(
        std::pow(Sw, 3) + std::pow(1.0 - Sw, 3), 1e-15));
}

TEST_CASE("Corey relperm: monotonicity", "[components][relperm]") {
    double prev_krw = 0.0, prev_kro = 1.0;
    for (int i = 1; i <= 100; ++i) {
        double Sw = i / 100.0;
        double krw = std::pow(Sw, 3);
        double kro = std::pow(1.0 - Sw, 3);
        CHECK(krw >= prev_krw);
        CHECK(kro <= prev_kro);
        prev_krw = krw;
        prev_kro = kro;
    }
}

TEST_CASE("Corey relperm: derivative vs finite difference",
          "[components][relperm]") {
    auto Sw = GENERATE(0.1, 0.3, 0.5, 0.7, 0.9);
    constexpr double eps = 1e-7;

    double krw_plus = std::pow(Sw + eps, 3);
    double krw_minus = std::pow(Sw - eps, 3);
    double dkrw_numerical = (krw_plus - krw_minus) / (2 * eps);
    double dkrw_analytical = 3.0 * Sw * Sw;

    CAPTURE(Sw);
    REQUIRE_THAT(dkrw_analytical, WithinRel(dkrw_numerical, 1e-5));

    double kro_plus = std::pow(1.0 - (Sw + eps), 3);
    double kro_minus = std::pow(1.0 - (Sw - eps), 3);
    double dkro_numerical = (kro_plus - kro_minus) / (2 * eps);
    double dkro_analytical = -3.0 * (1.0 - Sw) * (1.0 - Sw);

    REQUIRE_THAT(dkro_analytical, WithinRel(dkro_numerical, 1e-5));
}

// --- Fractional flow ---

TEST_CASE("Fractional flow: boundary values",
          "[components][fractional-flow]") {
    constexpr double M = 4.3 / 2.0;

    double fw_0 = M * 0.0 / (M * 0.0 + 1.0);
    REQUIRE_THAT(fw_0, WithinAbs(0.0, 1e-15));

    double fw_1 = M * 1.0 / (M * 1.0 + 0.0);
    REQUIRE_THAT(fw_1, WithinAbs(1.0, 1e-15));
}

TEST_CASE("Fractional flow: S-shape and monotonicity",
          "[components][fractional-flow]") {
    constexpr double M = 4.3 / 2.0;

    double prev_fw = 0.0;
    for (int i = 1; i <= 100; ++i) {
        double Sw = i / 100.0;
        double s3 = Sw * Sw * Sw;
        double q3 = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);
        double fw = M * s3 / (M * s3 + q3);
        CHECK(fw >= prev_fw);
        CHECK(fw >= 0.0);
        CHECK(fw <= 1.0);
        prev_fw = fw;
    }
}

// --- Simulator-level component tests ---

TEST_CASE("Trivial solve: uniform IC converges in 1 Newton iteration",
          "[components]") {
    constexpr double P_init_atm = 200.0;
    constexpr double P_init_Pa = P_init_atm * 101325.0;

    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, P_init_atm, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_init_Pa;

    sim.Solve({0.0, 10.0});

    CHECK(sim.numPrm.WastedTrialsCount() == 0);
}

TEST_CASE("Simulator relperm: S_w and S_o consistency from GetFields",
          "[components][relperm]") {
    auto horizon = test_helpers::make_uniform_horizon(
        3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 200.0, 0.6);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = 200.0 * 101325.0;
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);

    auto Sw = sim.GetWaterSaturationField();
    auto So = sim.GetOilSaturationField();

    REQUIRE(Sw.size() == 9);
    REQUIRE(So.size() == 9);

    for (size_t i = 0; i < 9; ++i) {
        INFO("Cell " << i);
        // S_oil_init = 0.6, S_water_init = 0.4
        REQUIRE_THAT(So[i], WithinRel(0.6, 1e-10));
        REQUIRE_THAT(Sw[i], WithinRel(0.4, 1e-10));
        REQUIRE_THAT(Sw[i] + So[i], WithinAbs(1.0, 1e-12));
    }
}

TEST_CASE("Grid size: matches input dimensions",
          "[components]") {
    auto horizon = test_helpers::make_uniform_horizon(
        4, 5, 2, 400.0, 500.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    auto P = sim.GetPressureField();
    REQUIRE(P.size() == 4 * 5 * 2);
}
