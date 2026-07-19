#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>
#include <fstream>
#include <filesystem>

using Catch::Matchers::WithinAbs;

namespace {

constexpr size_t Nx_1d = 50;
constexpr double Lx_1d = 500.0, hy_1d = 1.0, hz_1d = 1.0;
constexpr double perm_mD = 100.0, poro = 0.2;
constexpr double P_init_atm = 200.0;
constexpr double So_init = 0.8;
constexpr double Sw_init = 1.0 - So_init;
constexpr double Q_inj = -1000.0;
constexpr double Q_prod = 800.0;
constexpr double T_breakthrough = 600.0;

} // namespace

TEST_CASE("PI controller: BL 1D through breakthrough",
          "[pi-controller][integration][validation]") {
    auto horizon = test_helpers::make_uniform_horizon(
        Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.numPrm.SetUsePIController(true);

    double hx = Lx_1d / Nx_1d;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy_1d * 0.5, 0.0, Q_inj);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx_1d - hx * 0.5, hy_1d * 0.5, Q_prod, 0.0);

    sim.Solve({0.0, T_breakthrough});

    auto Sw = sim.GetWaterSaturationField();

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-6);
            CHECK(Sw[i] <= 1.0 + 1e-6);
        }
    }

    SECTION("Breakthrough occurred — Sw at producer > Sw_init") {
        CHECK(Sw[Nx_1d - 1] > Sw_init + 0.01);
    }

    SECTION("Mass balance — per-phase") {
        auto bal = sim.BalanceTracker().GetBalance();

        double accumOil       = bal[1];
        double accumOilFlux   = bal[2];
        double accumOilDebet  = bal[3];
        double accumWater      = bal[4];
        double accumWaterFlux  = bal[5];
        double accumWaterDebet = bal[6];

        // Невязка: ΔM + outflux - debet_accum ≈ 0
        double oil_residual = accumOil + accumOilFlux - accumOilDebet;
        double oil_scale = std::abs(accumOil) + std::abs(accumOilFlux) + std::abs(accumOilDebet);
        INFO("OIL: dM=" << accumOil << " flux=" << accumOilFlux
             << " debet=" << accumOilDebet << " residual=" << oil_residual);
        CHECK(accumOilDebet < 0.0);
        if (oil_scale > 0.0)
            CHECK(std::abs(oil_residual) / oil_scale < 1e-4);

        double water_residual = accumWater + accumWaterFlux - accumWaterDebet;
        double water_scale = std::abs(accumWater) + std::abs(accumWaterFlux) + std::abs(accumWaterDebet);
        INFO("WATER: dM=" << accumWater << " flux=" << accumWaterFlux
             << " debet=" << accumWaterDebet << " residual=" << water_residual);
        CHECK(accumWaterDebet > 0.0);
        if (water_scale > 0.0)
            CHECK(std::abs(water_residual) / water_scale < 1e-4);
    }

    SECTION("Wasted trials bounded") {
        CHECK(sim.numPrm.WastedTrialsCount() < 100);
    }

    SECTION("dt history shows dip near breakthrough") {
        auto& log = sim.numPrm.TimestepLog();
        REQUIRE(log.size() > 10);

        double dt_min = 1e30;
        double t_at_min = 0.0;
        double dt_max_early = 0.0;
        for (auto& rec : log) {
            if (!rec.accepted) continue;
            if (rec.time < 50.0) continue;
            if (rec.time < T_breakthrough * 0.3 && rec.dt > dt_max_early)
                dt_max_early = rec.dt;
            if (rec.dt < dt_min) {
                dt_min = rec.dt;
                t_at_min = rec.time;
            }
        }
        INFO("dt_min=" << dt_min << " at t=" << t_at_min
             << ", dt_max_early=" << dt_max_early);
        CHECK(dt_min < dt_max_early * 0.5);
    }
}
