#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>
#include <fstream>
#include <filesystem>

using Catch::Matchers::WithinAbs;
using namespace reservoir_simulator;

namespace {

constexpr size_t Nx_1d = 50;
constexpr double Lx_1d = 500.0, hy_1d = 1.0, hz_1d = 1.0;
constexpr double perm_mD = 100.0, poro = 0.2;
constexpr double P_init_atm = 200.0;
constexpr double So_init = 0.8;
constexpr double Sw_init = 1.0 - So_init;
constexpr double Q_inj = -1000.0;
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

    test_helpers::add_simple_well(sim, horizon,
        "INJ", Lx_1d * 0.5, hy_1d * 0.5, 0.0, Q_inj);

    sim.Solve({0.0, T_breakthrough});

    auto Sw = sim.GetWaterSaturationField();

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-6);
            CHECK(Sw[i] <= 1.0 + 1e-6);
        }
    }

    SECTION("Water front reached boundary — Sw at edge > Sw_init") {
        CHECK(Sw[0] > Sw_init + 0.01);
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

TEST_CASE("PI controller: five-spot symmetry preserved",
          "[pi-controller][integration][five-spot]") {
    constexpr size_t Nx = 21, Ny = 21, Nz = 1;
    constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;
    constexpr double rho_w = 1000.0, rho_o = 800.0;
    constexpr double Q_inj_vol = 50.0;
    constexpr double Q_prod_vol = 12.5;
    constexpr double T_end = 500.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, perm_mD, poro, P_init_atm, 0.999);
    auto numPrm = test_helpers::default_num_params();

    ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(5.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx, hy = Ly / Ny;

    test_helpers::add_simple_well(sim, horizon,
        "INJ", 10.5 * hx, 10.5 * hy, 0.0, -Q_inj_vol * rho_w);
    test_helpers::add_simple_well(sim, horizon,
        "P1", 0.5 * hx, 0.5 * hy, Q_prod_vol * rho_o, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P2", 0.5 * hx, (Ny - 0.5) * hy, Q_prod_vol * rho_o, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P3", (Nx - 0.5) * hx, 0.5 * hy, Q_prod_vol * rho_o, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy, Q_prod_vol * rho_o, 0.0);

    std::vector<double> timeMoments = {0.0};
    for (double t = 50.0; t <= T_end; t += 50.0)
        timeMoments.push_back(t);

    sim.Solve(timeMoments);

    auto Sw = sim.GetWaterSaturationField();

    SECTION("Quarter symmetry") {
        for (size_t i = 0; i < Nx / 2; ++i) {
            for (size_t j = 0; j < Ny / 2; ++j) {
                double s00 = Sw[j * Nx + i];
                double s10 = Sw[j * Nx + (Nx - 1 - i)];
                double s01 = Sw[(Ny - 1 - j) * Nx + i];
                double s11 = Sw[(Ny - 1 - j) * Nx + (Nx - 1 - i)];
                REQUIRE_THAT(s00, WithinAbs(s10, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s01, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s11, 1e-6));
            }
        }
    }

    SECTION("Wasted trials bounded") {
        CHECK(sim.numPrm.WastedTrialsCount() < 50);
    }

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-10);
            CHECK(Sw[i] <= 1.0 + 1e-10);
        }
    }
}

TEST_CASE("PI controller: PI vs fixed-dt profile comparison",
          "[pi-controller][integration][validation][.]") {
    constexpr double T = 400.0;
    constexpr double dt_fixed = 1.0;

    auto run_sim = [](bool use_pi) {
        auto horizon = test_helpers::make_uniform_horizon(
            Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
        auto numPrm = test_helpers::default_num_params();

        ReservoirSimulator sim{
            numPrm, horizon, horizon.oil, horizon.water, horizon.other};
        sim.RefPressure = P_init_atm * 101325.0;
        sim.numPrm.set_initial_schemeTau(dt_fixed);
        sim.numPrm.set_currentMoment(0.0);
        sim.numPrm.SetUsePIController(use_pi);

        test_helpers::add_simple_well(sim, horizon,
            "INJ", Lx_1d * 0.5, hy_1d * 0.5, 0.0, Q_inj);

        sim.Solve({0.0, T});
        return sim.GetWaterSaturationField();
    };

    auto Sw_pi = run_sim(true);
    auto Sw_fixed = run_sim(false);

    REQUIRE(Sw_pi.size() == Sw_fixed.size());

    SECTION("Pointwise difference bounded") {
        double max_diff = 0.0;
        for (size_t i = 0; i < Sw_pi.size(); ++i) {
            double diff = std::abs(Sw_pi[i] - Sw_fixed[i]);
            if (diff > max_diff) max_diff = diff;
        }
        INFO("max |Sw_PI - Sw_fixed| = " << max_diff);
        CHECK(max_diff < 0.02);
    }

    SECTION("L2 norm of difference") {
        double hx = Lx_1d / Nx_1d;
        double sum_sq = 0.0;
        for (size_t i = 0; i < Sw_pi.size(); ++i) {
            double d = Sw_pi[i] - Sw_fixed[i];
            sum_sq += d * d;
        }
        double L2 = std::sqrt(sum_sq * hx / Lx_1d);
        INFO("L2(Sw_PI - Sw_fixed) = " << L2);
        CHECK(L2 < 0.01);
    }
}

TEST_CASE("PI controller: CSV export for visual verification",
          "[pi-controller][validation][.export]") {
    auto horizon = test_helpers::make_uniform_horizon(
        Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);

    test_helpers::add_simple_well(sim, horizon,
        "INJ", Lx_1d * 0.5, hy_1d * 0.5, 0.0, Q_inj);

    sim.Solve({0.0, T_breakthrough});

    std::filesystem::create_directories("results/validation");

    {
        std::ofstream csv("results/validation/pi_dt_history.csv");
        csv << "t,dt,newton_iters,accepted\n";
        for (auto& rec : sim.numPrm.TimestepLog())
            csv << rec.time << "," << rec.dt << ","
                << rec.newton_iters << "," << rec.accepted << "\n";
    }

    auto Sw_pi = sim.GetWaterSaturationField();
    {
        auto h2 = test_helpers::make_uniform_horizon(
            Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
        auto np2 = test_helpers::default_num_params();
        ReservoirSimulator sim2{
            np2, h2, h2.oil, h2.water, h2.other};
        sim2.RefPressure = P_init_atm * 101325.0;
        sim2.numPrm.set_initial_schemeTau(1.0);
        sim2.numPrm.set_currentMoment(0.0);
        sim2.numPrm.SetUsePIController(false);

        test_helpers::add_simple_well(sim2, h2,
            "INJ", Lx_1d * 0.5, hy_1d * 0.5, 0.0, Q_inj);

        sim2.Solve({0.0, T_breakthrough});
        auto Sw_fixed = sim2.GetWaterSaturationField();

        double hx = Lx_1d / Nx_1d;
        std::ofstream csv("results/validation/pi_vs_fixed_sw_profile.csv");
        csv << "x,Sw_PI,Sw_fixed\n";
        for (size_t i = 0; i < Nx_1d; ++i)
            csv << (i + 0.5) * hx << "," << Sw_pi[i] << "," << Sw_fixed[i] << "\n";

        std::ofstream csv2("results/validation/linear_dt_history.csv");
        csv2 << "t,dt,newton_iters,accepted\n";
        for (auto& rec : sim2.numPrm.TimestepLog())
            csv2 << rec.time << "," << rec.dt << ","
                 << rec.newton_iters << "," << rec.accepted << "\n";
    }

    CHECK(std::filesystem::exists("results/validation/pi_dt_history.csv"));
    CHECK(std::filesystem::exists("results/validation/pi_vs_fixed_sw_profile.csv"));
}

TEST_CASE("PI controller: timestep growth after easy step (VAL-032)",
          "[unit][level2][reservoir][NumericalParameters][pi-controller][VAL-032]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentMoment(0.0);
    np.set_currentAMG_Error(0.5);
    np.set_currentNewtonIterationCount(1);

    double tau_before = np.CurrentSchemeTau();
    np.increase_schemeTau();
    double tau_after = np.CurrentSchemeTau();

    CHECK(tau_after > tau_before);
    CHECK(tau_after > tau_before * 1.5);
}
