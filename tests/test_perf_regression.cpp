#include <catch2/catch_test_macros.hpp>
#include "simulation_cases/MultiLayerCase.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace reservoir_simulator;

namespace {

#ifdef NDEBUG
constexpr size_t PNx = 51, PNy = 51, PNz = 4;
constexpr double Ptotal_time = 100.0;
#else
constexpr size_t PNx = 11, PNy = 11, PNz = 2;
constexpr double Ptotal_time = 10.0;
#endif
constexpr double PLx = 500.0, PLy = 500.0, Phz = 10.0;
constexpr double Psnapshot_dt = 10.0;
constexpr double Pinit_tau = 1.0;

std::vector<test_helpers::WellScheduleBuilder>
make_fivespot_wells()
{
    std::vector<test_helpers::WellScheduleBuilder> builders;
    constexpr double T = Ptotal_time;

    auto c_all = test_helpers::WellCompletionBuilder(PNz, Phz);
    for (size_t i = 0; i < PNz; ++i)
        c_all.open_layer(i, 0.0);

    // INJ center (250, 250)
    builders.emplace_back("INJ-C", 250.0, 250.0);
    builders.back().set_completions(c_all)
        .inject_water(50.0).for_days(T);

    // PROD corners, 12.5 m3/day each
    builders.emplace_back("PROD-1", 25.0, 25.0);
    builders.back().set_completions(c_all)
        .produce_oil(12.5).for_days(T);

    builders.emplace_back("PROD-2", 475.0, 25.0);
    builders.back().set_completions(c_all)
        .produce_oil(12.5).for_days(T);

    builders.emplace_back("PROD-3", 25.0, 475.0);
    builders.back().set_completions(c_all)
        .produce_oil(12.5).for_days(T);

    builders.emplace_back("PROD-4", 475.0, 475.0);
    builders.back().set_completions(c_all)
        .produce_oil(12.5).for_days(T);

    return builders;
}

const std::vector<simulation_cases::WellInfo> fivespot_wells_info = {
    {"INJ-C",  "injector", 250.0, 250.0},
    {"PROD-1", "producer",  25.0,  25.0},
    {"PROD-2", "producer", 475.0,  25.0},
    {"PROD-3", "producer",  25.0, 475.0},
    {"PROD-4", "producer", 475.0, 475.0}
};

struct PerfResult {
    size_t n_time_steps = 0;
    size_t n_newton_iters = 0;
    size_t n_amg_solves = 0;
    size_t n_wasted_trials = 0;
    size_t total_amg_iters = 0;
    double max_balance_rel = 0;
    bool failed = false;
    double t_total = 0;
    double t_assembly = 0;
    double t_solve = 0;
};

PerfResult run_perf_regression()
{
    simulation_cases::MultiLayerCase sc(
        "perf_regression", PNx, PNy, PNz, PLx, PLy, Phz,
        Ptotal_time, Psnapshot_dt,
        [](double, double) { return make_fivespot_wells(); },
        fivespot_wells_info
    );

    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    ReservoirSimulator sim{numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(Pinit_tau);
    sim.numPrm.set_currentMoment(0.0);
    sim.numPrm.SetUsePIController(true);
    sc.add_wells(sim, horizon);

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();
    double max_rel = 0.0;
    size_t total_cells = PNx * PNy * PNz;

    SolverProfile profile{};

    constexpr size_t MAX_CONSECUTIVE_ROLLBACKS = 15;
    size_t consecutive_rollbacks = 0;
    bool solver_failed = false;

    using clock = std::chrono::high_resolution_clock;
    double chrono_assembly = 0, chrono_solve = 0;
    auto t_start = clock::now();

    auto times = sc.save_times();
    for (size_t step = 1; step < times.size() && !solver_failed; ++step) {
        double target = times[step];

        while (sim.numPrm.CurrentTimeMoment() < target && !solver_failed) {
            sim.numPrm.update_maxTauAllowed(target, sim.GetWells());

            double loc_tau = sim.numPrm.CurrentIntegrationStep();
            double nextTime = sim.numPrm.NextTimeMoment();

            sim.numPrm.set_currentNewtonIterationCount(0);
            sim.numPrm.update_isSuccesfullNewtonTrial(false);
            sim.numPrm.set_currentAMG_maxSolverIterationCount();
            while (!sim.numPrm.IsSuccessfullNewtonTrial()) {
                auto ta0 = clock::now();
                sim.AssembleMyProblem(loc_tau, nextTime);
                chrono_assembly += std::chrono::duration<double>(clock::now() - ta0).count();

                auto ts0 = clock::now();
                auto res = sim.MyProblem.Solve(
                    sim.numPrm.CurrentAMG_maxSolverIterationCount());
                chrono_solve += std::chrono::duration<double>(clock::now() - ts0).count();
                sim.numPrm.update_currentAMGState(
                    {res.iters, res.error, res.converged});

                profile.n_amg_solves++;
                profile.total_amg_iters += res.iters;
                profile.n_newton_iters++;

                if (sim.numPrm.IsSuccessfullAMG_Iteration() &&
                    sim.numPrm.IsNewtonIterationContinue()) {
                    sim.numPrm.update_isSuccesfullNewtonTrial(sim.UpdateGrid());
                } else {
                    sim.Grid.ReverseState();
                    break;
                }
            }

            if (sim.numPrm.IsSuccessfullNewtonTrial()) {
                sim.MassBalance(loc_tau);
                sim.Grid.AcceptState();
                sim.numPrm.update_currentMoment();
                sim.AddFlowFieldSnapShot();
                profile.n_time_steps++;
                consecutive_rollbacks = 0;
            } else {
                sim.numPrm.decrease_schemeTau();
                profile.n_wasted_trials++;
                consecutive_rollbacks++;
                if (consecutive_rollbacks >= MAX_CONSECUTIVE_ROLLBACKS) {
                    solver_failed = true;
                }
                continue;
            }
        }

        if (!solver_failed) {
            auto Sw = sim.GetWaterSaturationField();
            auto P = sim.GetPressureField();
            for (size_t i = 0; i < total_cells; ++i) {
                REQUIRE(Sw[i] >= 0.0);
                REQUIRE(Sw[i] <= 1.0);
                REQUIRE(P[i] > 0.0);
            }

            auto bal = sim.GetOverallBalance();
            double oil_residual   = bal[1] + bal[2] - bal[3];
            double water_residual = bal[4] + bal[5] - bal[6];
            double oil_rel  = (oil_mass_0 > 0)
                ? std::abs(oil_residual) / oil_mass_0 : std::abs(oil_residual);
            double water_rel = (water_mass_0 > 0)
                ? std::abs(water_residual) / water_mass_0 : std::abs(water_residual);
            max_rel = std::max(max_rel, std::max(oil_rel, water_rel));
        }
    }

    double t_total_s = std::chrono::duration<double>(clock::now() - t_start).count();

    PerfResult r;
    r.n_time_steps = profile.n_time_steps;
    r.n_newton_iters = profile.n_newton_iters;
    r.n_amg_solves = profile.n_amg_solves;
    r.n_wasted_trials = profile.n_wasted_trials;
    r.total_amg_iters = profile.total_amg_iters;
    r.max_balance_rel = max_rel;
    r.failed = solver_failed;
    r.t_total = t_total_s;
    r.t_assembly = chrono_assembly;
    r.t_solve = chrono_solve;
    return r;
}

void append_perf_csv(const PerfResult& r) {
    fs::create_directories("results");
    const std::string path = "results/perf_regression.csv";
    bool exists = fs::exists(path);
    std::ofstream ofs(path, std::ios::app);
    if (!exists)
        ofs << "time_steps,newton_iters,wasted_trials,total_amg_iters,"
               "max_balance_rel,t_total_s,t_assembly_s,t_solve_s\n";
    ofs << r.n_time_steps << ","
        << r.n_newton_iters << ","
        << r.n_wasted_trials << ","
        << r.total_amg_iters << ","
        << std::scientific << r.max_balance_rel << ","
        << std::fixed << std::setprecision(3)
        << r.t_total << "," << r.t_assembly << "," << r.t_solve << "\n";
}

// Baseline (experimental, 2026-07-19, CPR_BICGSTAB + PI-controller defaults)
#ifdef NDEBUG
constexpr size_t BASELINE_TIMESTEPS = 227;
constexpr size_t BASELINE_NEWTON_ITERS = 2617;
constexpr size_t BASELINE_WASTED_TRIALS = 0;
constexpr double BASELINE_TIME_S = 96.0;
#endif

} // anonymous namespace

TEST_CASE("Performance regression: five-spot 51x51x4", "[perf][.slow]")
{
    auto r = run_perf_regression();
    REQUIRE_FALSE(r.failed);

    std::cout << "\n=== Performance Regression Report ===\n"
              << "  Grid: " << PNx << "x" << PNy << "x" << PNz << "\n"
              << "  Time: " << Ptotal_time << " days\n"
              << "  Timesteps:      " << r.n_time_steps << "\n"
              << "  Newton iters:   " << r.n_newton_iters << "\n"
              << "  AMG solves:     " << r.n_amg_solves << "\n"
              << "  Wasted trials:  " << r.n_wasted_trials << "\n"
              << "  Total AMG iters:" << r.total_amg_iters << "\n"
              << "  Max balance rel:" << std::scientific << r.max_balance_rel << "\n"
              << "  Wall-clock:     " << std::fixed << std::setprecision(2)
              << r.t_total << " s (asm=" << r.t_assembly
              << " s, solve=" << r.t_solve << " s)\n"
              << "=====================================\n";

    append_perf_csv(r);

    REQUIRE(r.max_balance_rel < 1e-5);

#ifdef NDEBUG
    REQUIRE(r.n_time_steps <= static_cast<size_t>(BASELINE_TIMESTEPS * 1.05));
    REQUIRE(r.n_newton_iters <= static_cast<size_t>(BASELINE_NEWTON_ITERS * 1.05));
    REQUIRE(r.n_wasted_trials <= BASELINE_WASTED_TRIALS + 2);
    if (r.t_total > BASELINE_TIME_S * 1.10)
        WARN("Wall-clock regression: " << r.t_total << "s vs baseline " << BASELINE_TIME_S << "s");
#endif
}
