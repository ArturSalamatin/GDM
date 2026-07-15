#include "../tests/simulation_cases/MultiLayerCase.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "Solver/Math/SolverConfig.h"
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/relaxation/iluk.hpp>

namespace fs = std::filesystem;
using namespace reservoir_simulator;
using namespace reservoir_simulator::linear_problem;

namespace {

template<typename SolverType>
SolveResult solve_with_scalar(LinearProblem& lp, int maxIter, typename SolverType::params& prm)
{
    prm.solver.maxiter = maxIter;

    prof.tic("setup");
    auto n = lp.RhsSize();
    auto const& row = lp.Matrix().Row();
    auto const& col = lp.Matrix().Col();
    auto const& val = lp.Matrix().Val();
    SolverType solve(std::tie(n, row, col, val), prm);
    prof.toc("setup");

    std::vector<double> F(lp.Rhs().begin(), lp.Rhs().end());
    std::vector<double> X(lp.SolutionCorrections().begin(), lp.SolutionCorrections().end());

    prof.tic("solve");
    auto [iters, error] = solve(F, X);
    prof.toc("solve");

    std::copy(X.begin(), X.end(), lp.SolutionCorrections().begin());

    return { iters, error, true };
}

using SB = amgcl::backend::builtin<double>;

using CPR_AMG_ilu0 = amgcl::preconditioner::cpr<
    amgcl::amg<SB, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>
>;
using CPRSolver_AMG_ilu0 = amgcl::make_solver<CPR_AMG_ilu0, amgcl::solver::lgmres<SB>>;

using CPRDRS_AMG_ilu0 = amgcl::preconditioner::cpr_drs<
    amgcl::amg<SB, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>
>;
using CPRDRSSolver_AMG_ilu0 = amgcl::make_solver<CPRDRS_AMG_ilu0, amgcl::solver::lgmres<SB>>;

using ILU0_scalar = amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>;
using ILU0Solver_scalar = amgcl::make_solver<ILU0_scalar, amgcl::solver::lgmres<SB>>;

using CPR_AMG_iluk = amgcl::preconditioner::cpr<
    amgcl::amg<SB, amgcl::coarsening::aggregation, amgcl::relaxation::iluk>,
    amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>
>;
using CPRSolver_AMG_iluk = amgcl::make_solver<CPR_AMG_iluk, amgcl::solver::lgmres<SB>>;

constexpr size_t BNx = 51, BNy = 51, BNz = 4;
constexpr double BLx = 500.0, BLy = 500.0, Bhz = 10.0;
constexpr double Brho_oil = 800.0, Brho_water = 1000.0;
constexpr double Btotal_time = 730.0;
constexpr double Brate_mult = 1.4;

std::vector<test_helpers::WellScheduleBuilder>
make_benchmark_wells()
{
    std::vector<test_helpers::WellScheduleBuilder> builders;
    constexpr double T = Btotal_time;
    constexpr double rm = Brate_mult;

    auto c_inj1 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 0.0).open_layer(1, 0.0);
    builders.emplace_back("INJ-1", 125.0, 125.0);
    builders.back().set_completions(c_inj1)
        .inject_water(40.0 * rm).for_days(T);

    auto c_inj2 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 0.0);
    builders.emplace_back("INJ-2", 375.0, 375.0);
    builders.back().set_completions(c_inj2)
        .inject_water(30.0 * rm).for_days(T);

    auto c_prod1 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 0.0).open_layer(1, 0.0)
        .open_layer(2, 0.0).open_layer(3, 0.0);
    builders.emplace_back("PROD-1", 375.0, 125.0);
    builders.back().set_completions(c_prod1)
        .produce_oil(25.0 * rm).for_days(T);

    auto c_prod2 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(3, 0.0);
    builders.emplace_back("PROD-2", 125.0, 375.0);
    builders.back().set_completions(c_prod2)
        .produce_oil(15.0 * rm).for_days(T);

    auto c_prod3 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(1, 0.0).open_layer(2, 0.0);
    builders.emplace_back("PROD-3", 250.0, 250.0);
    builders.back().set_completions(c_prod3)
        .produce_oil(20.0 * rm).for_days(T);

    auto c_inj3 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 150.0).open_layer(1, 150.0);
    builders.emplace_back("INJ-3", 250.0, 125.0);
    builders.back().set_completions(c_inj3)
        .shut_in().for_days(150.0)
        .inject_water(35.0 * rm).for_days(T - 150.0);

    return builders;
}

const std::vector<simulation_cases::WellInfo> bm_wells_info = {
    {"INJ-1",  "injector", 125.0, 125.0},
    {"INJ-2",  "injector", 375.0, 375.0},
    {"PROD-1", "producer", 375.0, 125.0},
    {"PROD-2", "producer", 125.0, 375.0},
    {"PROD-3", "producer", 250.0, 250.0},
    {"INJ-3",  "injector", 250.0, 125.0}
};

struct BenchmarkResult {
    std::string config_name;
    size_t n_time_steps = 0;
    size_t n_newton_iters = 0;
    size_t n_amg_solves = 0;
    size_t n_wasted_trials = 0;
    size_t total_amg_iters = 0;
    double max_oil_balance_rel = 0;
    double max_water_balance_rel = 0;
    bool balance_ok = false;
    bool failed = false;
    double t_total = 0;
    double t_assembly = 0;
    double t_solve = 0;
};

struct SolverProfile {
    size_t n_time_steps = 0;
    size_t n_newton_iters = 0;
    size_t n_amg_solves = 0;
    size_t n_wasted_trials = 0;
    size_t total_amg_iters = 0;
};

BenchmarkResult run_benchmark(const std::string& config_name)
{
    prof.reset();

    simulation_cases::MultiLayerCase sc(
        "benchmark_" + config_name, BNx, BNy, BNz, BLx, BLy, Bhz,
        Btotal_time, 5.0,
        [](double, double) { return make_benchmark_wells(); },
        bm_wells_info
    );

    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    ReservoirSimulator sim{numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(sc.initial_tau());
    sim.numPrm.set_currentMoment(0.0);
    sc.add_wells(sim, horizon);

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();
    double max_oil_rel = 0.0, max_water_rel = 0.0;
    size_t total_cells = BNx * BNy * BNz;

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
                prof.tic("assemble");
                sim.AssembleMyProblem(loc_tau, nextTime);
                prof.toc("assemble");
                chrono_assembly += std::chrono::duration<double>(clock::now() - ta0).count();

                auto ts0 = clock::now();
                auto res = sim.MyProblem.Solve(
                    sim.numPrm.CurrentAMG_maxSolverIterationCount());
                chrono_solve += std::chrono::duration<double>(clock::now() - ts0).count();
                sim.numPrm.update_currentAMGState(
                    {static_cast<int>(res.iters), res.error, res.converged});

                profile.n_amg_solves++;
                profile.total_amg_iters += res.iters;
                profile.n_newton_iters++;

                if (sim.numPrm.IsSuccessfullAMG_Iteration() &&
                    sim.numPrm.IsNewtonIterationContinue()) {
                    prof.tic("update");
                    sim.numPrm.update_isSuccesfullNewtonTrial(sim.UpdateGrid());
                    prof.toc("update");
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
                if (Sw[i] < 0.0 || Sw[i] > 1.0 || P[i] <= 0.0) {
                    solver_failed = true;
                    break;
                }
            }

            auto bal = sim.GetOverallBalance();
            double oil_residual   = bal[1] + bal[2] - bal[3];
            double water_residual = bal[4] + bal[5] - bal[6];
            double oil_rel  = (oil_mass_0 > 0)
                ? std::abs(oil_residual) / oil_mass_0 : std::abs(oil_residual);
            double water_rel = (water_mass_0 > 0)
                ? std::abs(water_residual) / water_mass_0 : std::abs(water_residual);
            max_oil_rel = std::max(max_oil_rel, oil_rel);
            max_water_rel = std::max(max_water_rel, water_rel);
        }
    }

    double t_total_s = std::chrono::duration<double>(clock::now() - t_start).count();

    BenchmarkResult r;
    r.config_name = config_name;
    r.n_time_steps = profile.n_time_steps;
    r.n_newton_iters = profile.n_newton_iters;
    r.n_amg_solves = profile.n_amg_solves;
    r.n_wasted_trials = profile.n_wasted_trials;
    r.total_amg_iters = profile.total_amg_iters;
    r.max_oil_balance_rel = max_oil_rel;
    r.max_water_balance_rel = max_water_rel;
    r.balance_ok = !solver_failed && (max_oil_rel < 1e-3) && (max_water_rel < 1e-3);
    r.failed = solver_failed;
    r.t_total = t_total_s;
    r.t_assembly = chrono_assembly;
    r.t_solve = chrono_solve;
    return r;
}

template<typename SolverType>
BenchmarkResult run_benchmark_scalar(const std::string& config_name,
                                     typename SolverType::params& prm,
                                     Layout layout)
{
    prof.reset();

    simulation_cases::MultiLayerCase sc(
        "benchmark_" + config_name, BNx, BNy, BNz, BLx, BLy, Bhz,
        Btotal_time, 5.0,
        [](double, double) { return make_benchmark_wells(); },
        bm_wells_info
    );

    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    ReservoirSimulator sim{numPrm, horizon, horizon.oil, horizon.water, horizon.other, layout};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(sc.initial_tau());
    sim.numPrm.set_currentMoment(0.0);
    sc.add_wells(sim, horizon);

    prm.solver.tol = sim.numPrm.AMG_RelTol;
    prm.solver.abstol = sim.numPrm.AMG_AbsTol;

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();
    double max_oil_rel = 0.0, max_water_rel = 0.0;
    size_t total_cells = BNx * BNy * BNz;

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
                prof.tic("assemble");
                sim.AssembleMyProblem(loc_tau, nextTime);
                prof.toc("assemble");
                chrono_assembly += std::chrono::duration<double>(clock::now() - ta0).count();

                auto ts0 = clock::now();
                auto res = solve_with_scalar<SolverType>(sim.MyProblem,
                    sim.numPrm.CurrentAMG_maxSolverIterationCount(), prm);
                chrono_solve += std::chrono::duration<double>(clock::now() - ts0).count();
                sim.numPrm.update_currentAMGState(
                    {static_cast<int>(res.iters), res.error, res.converged});

                profile.n_amg_solves++;
                profile.total_amg_iters += res.iters;
                profile.n_newton_iters++;

                if (sim.numPrm.IsSuccessfullAMG_Iteration() &&
                    sim.numPrm.IsNewtonIterationContinue()) {
                    prof.tic("update");
                    sim.numPrm.update_isSuccesfullNewtonTrial(sim.UpdateGrid());
                    prof.toc("update");
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
                if (Sw[i] < 0.0 || Sw[i] > 1.0 || P[i] <= 0.0) {
                    solver_failed = true;
                    break;
                }
            }

            auto bal = sim.GetOverallBalance();
            double oil_residual   = bal[1] + bal[2] - bal[3];
            double water_residual = bal[4] + bal[5] - bal[6];
            double oil_rel  = (oil_mass_0 > 0)
                ? std::abs(oil_residual) / oil_mass_0 : std::abs(oil_residual);
            double water_rel = (water_mass_0 > 0)
                ? std::abs(water_residual) / water_mass_0 : std::abs(water_residual);
            max_oil_rel = std::max(max_oil_rel, oil_rel);
            max_water_rel = std::max(max_water_rel, water_rel);
        }
    }

    double t_total_s = std::chrono::duration<double>(clock::now() - t_start).count();

    BenchmarkResult r;
    r.config_name = config_name;
    r.n_time_steps = profile.n_time_steps;
    r.n_newton_iters = profile.n_newton_iters;
    r.n_amg_solves = profile.n_amg_solves;
    r.n_wasted_trials = profile.n_wasted_trials;
    r.total_amg_iters = profile.total_amg_iters;
    r.max_oil_balance_rel = max_oil_rel;
    r.max_water_balance_rel = max_water_rel;
    r.balance_ok = !solver_failed && (max_oil_rel < 1e-3) && (max_water_rel < 1e-3);
    r.failed = solver_failed;
    r.t_total = t_total_s;
    r.t_assembly = chrono_assembly;
    r.t_solve = chrono_solve;
    return r;
}

void append_csv(const std::string& path, const BenchmarkResult& r) {
    bool exists = fs::exists(path);
    std::ofstream ofs(path, std::ios::app);
    if (!exists) {
        ofs << "config,time_steps,newton_iters,amg_solves,wasted_trials,"
               "total_amg_iters,avg_iters_per_solve,"
               "max_oil_bal_rel,max_water_bal_rel,balance_ok,failed\n";
    }
    double avg_iters = r.n_amg_solves > 0
        ? static_cast<double>(r.total_amg_iters) / r.n_amg_solves : 0.0;
    ofs << std::setprecision(6)
        << r.config_name << ","
        << r.n_time_steps << ","
        << r.n_newton_iters << ","
        << r.n_amg_solves << ","
        << r.n_wasted_trials << ","
        << r.total_amg_iters << ","
        << avg_iters << ","
        << r.max_oil_balance_rel << ","
        << r.max_water_balance_rel << ","
        << (r.balance_ok ? "true" : "false") << ","
        << (r.failed ? "true" : "false") << "\n";
}

void report(const BenchmarkResult& r) {
    std::cout << "\n=== " << r.config_name << " ===\n"
              << "  time_steps=" << r.n_time_steps
              << "  newton=" << r.n_newton_iters
              << "  amg_solves=" << r.n_amg_solves
              << "  wasted=" << r.n_wasted_trials << "\n"
              << "  total_amg_iters=" << r.total_amg_iters
              << "  avg_iters/solve=" << std::setprecision(1) << std::fixed
              << (r.n_amg_solves > 0
                  ? static_cast<double>(r.total_amg_iters) / r.n_amg_solves : 0.0)
              << "\n"
              << "  balance_ok=" << (r.balance_ok ? "YES" : "NO")
              << "  failed=" << (r.failed ? "YES" : "NO")
              << "  oil_rel=" << std::scientific << r.max_oil_balance_rel
              << "  water_rel=" << r.max_water_balance_rel << "\n"
              << std::fixed << std::setprecision(3)
              << "  t_total=" << r.t_total << "s"
              << "  t_assembly=" << r.t_assembly << "s (" << std::setprecision(1) << (r.t_assembly/r.t_total*100) << "%)"
              << "  t_solve=" << std::setprecision(3) << r.t_solve << "s (" << std::setprecision(1) << (r.t_solve/r.t_total*100) << "%)\n";
}

} // namespace

int main() {
    fs::create_directories("results");
    std::string csv_path = "results/benchmark_cpr.csv";

    std::cout << "=== Series CPR: full benchmark (51x51x4, 730 days) ===\n";

    // CPR1: production CPR (baseline)
    {
        auto r = run_benchmark("CPR1_production_cpr");
        report(r);
        append_csv(csv_path, r);
    }

    // CPR2: cpr<amg_iluk, ilu0> + lgmres, PswLayout
    {
        using S = CPRSolver_AMG_iluk;
        S::params prm;
        prm.precond.block_size = 2;
        prm.precond.pprecond.relax.k = 1;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR2_cpr_iluk_PswLayout", prm, Layout::InterleavedPSw);
        report(r);
        append_csv(csv_path, r);
    }

    // CPR3: cpr_drs<amg_ilu0, ilu0> + lgmres, PswLayout
    {
        using S = CPRDRSSolver_AMG_ilu0;
        S::params prm;
        prm.precond.block_size = 2;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR3_cprdrs_ilu0_PswLayout", prm, Layout::InterleavedPSw);
        report(r);
        append_csv(csv_path, r);
    }

    // CPR4: scalar ilu0 + lgmres, SwPLayout (lower bound)
    {
        using S = ILU0Solver_scalar;
        S::params prm;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR4_ilu0_scalar_SwPLayout", prm, Layout::InterleavedSwP);
        report(r);
        append_csv(csv_path, r);
    }

    std::cout << "\nAll CPR benchmarks complete. Results: " << csv_path << "\n";
    return 0;
}
