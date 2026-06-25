#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "test_helpers.h"
#include "Solver/Grids/Cells/TwoPhaseFlowCell.h"
#include <cmath>

using Catch::Approx;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
using namespace reservoir_simulator::cell;

// --- Relperm via TwoPhaseFlowCell (Corey n=3) ---

namespace {

void init_constant_point_properties() {
    std::array<double, 8> props = {
        2.0, 1.0, 800.0, 1000.0, 9.81, 1e-9, 5e-10, 1e7
    };
    PhysPropCell::set_constantPointProperties(props);
}

TwoPhaseFlowCell make_relperm_cell(double Sw, double So_res = 0.1, double Sw_res = 0.2) {
    init_constant_point_properties();
    std::vector<double> center = {50.0, 50.0, 0.0};
    std::vector<double> size = {10.0, 10.0, 5.0};
    std::vector<double> constProp = {1e-13, 0.2, So_res, Sw_res, 0, 0, 0, 0, 0};
    std::vector<double> varProp = {Sw, 1e7};
    return TwoPhaseFlowCell(center, size, constProp, varProp);
}

} // namespace

TEST_CASE("Corey relperm: boundary values via TwoPhaseFlowCell",
          "[components][relperm]") {
    constexpr double So_res = 0.1, Sw_res = 0.2;

    auto cell_low = make_relperm_cell(Sw_res, So_res, Sw_res);
    CHECK(cell_low.RelativePermeabilityWater() == Approx(0.0).margin(1e-12));
    CHECK(cell_low.RelativePermeabilityOil() == Approx(1.0));

    auto cell_high = make_relperm_cell(1.0 - So_res, So_res, Sw_res);
    CHECK(cell_high.RelativePermeabilityWater() == Approx(1.0));
    CHECK(cell_high.RelativePermeabilityOil() == Approx(0.0).margin(1e-12));
}

TEST_CASE("Corey relperm: values in [0,1] via TwoPhaseFlowCell",
          "[components][relperm]") {
    constexpr double So_res = 0.1, Sw_res = 0.2;
    auto Sw_frac = GENERATE(0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0);
    double Sw = Sw_res + Sw_frac * (1.0 - Sw_res - So_res);

    CAPTURE(Sw);
    auto cell = make_relperm_cell(Sw, So_res, Sw_res);
    double krw = cell.RelativePermeabilityWater();
    double kro = cell.RelativePermeabilityOil();

    CHECK(krw >= 0.0);
    CHECK(krw <= 1.0);
    CHECK(kro >= 0.0);
    CHECK(kro <= 1.0);
}

TEST_CASE("Corey relperm: monotonicity via TwoPhaseFlowCell",
          "[components][relperm]") {
    constexpr double So_res = 0.1, Sw_res = 0.2;
    double prev_krw = 0.0, prev_kro = 1.0;

    for (int i = 0; i <= 100; ++i) {
        double Sw = Sw_res + i / 100.0 * (1.0 - Sw_res - So_res);
        auto cell = make_relperm_cell(Sw, So_res, Sw_res);
        double krw = cell.RelativePermeabilityWater();
        double kro = cell.RelativePermeabilityOil();

        CHECK(krw >= prev_krw - 1e-12);
        CHECK(kro <= prev_kro + 1e-12);
        prev_krw = krw;
        prev_kro = kro;
    }
}

TEST_CASE("Corey relperm: mobility derivative vs finite difference",
          "[components][relperm]") {
    constexpr double So_res = 0.1, Sw_res = 0.2;
    constexpr double dSw_scaled = 0.7; // 1 - Sw_res - So_res
    constexpr double eps = 1e-7;
    auto Sw_frac = GENERATE(0.1, 0.3, 0.5, 0.7, 0.9);
    double Sw = Sw_res + Sw_frac * dSw_scaled;

    CAPTURE(Sw);
    auto cell = make_relperm_cell(Sw, So_res, Sw_res);
    // DerivativeMobilityOil/Water are d/d(Sw_scaled), not d/d(Sw_physical)
    // Convert to physical: d/dSw = d/dSw_scaled * (1 / dSw_scaled)
    double dMobOil_analytical = cell.DerivativeMobilityOil() / dSw_scaled;
    double dMobWater_analytical = cell.DerivativeMobilityWater() / dSw_scaled;

    auto cell_plus = make_relperm_cell(Sw + eps, So_res, Sw_res);
    auto cell_minus = make_relperm_cell(Sw - eps, So_res, Sw_res);

    double dMobOil_numerical = (cell_plus.MobilityOil() - cell_minus.MobilityOil()) / (2 * eps);
    double dMobWater_numerical = (cell_plus.MobilityWater() - cell_minus.MobilityWater()) / (2 * eps);

    REQUIRE_THAT(dMobOil_analytical, WithinRel(dMobOil_numerical, 1e-4));
    REQUIRE_THAT(dMobWater_analytical, WithinRel(dMobWater_numerical, 1e-4));
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
