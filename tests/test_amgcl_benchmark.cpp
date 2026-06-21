#include <catch2/catch_test_macros.hpp>
#include "simulation_cases/MultiLayerCase.h"

#include <fstream>
#include <iomanip>
#include <filesystem>
#include <string>

#include <amgcl/adapter/block_matrix.hpp>
#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/value_type/static_matrix.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/solver/gmres.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/solver/bicgstabl.hpp>
#include <amgcl/solver/fgmres.hpp>
#include <amgcl/solver/lgmres.hpp>
#include <amgcl/solver/idrs.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/relaxation/damped_jacobi.hpp>
#include <amgcl/relaxation/spai0.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/gauss_seidel.hpp>
#include <amgcl/relaxation/chebyshev.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>

namespace fs = std::filesystem;
using namespace reservoir_simulator;
using namespace reservoir_simulator::linear_problem;

namespace {

template<typename SolverType>
SolveResult solve_with(LinearProblem& lp, int maxIter, typename SolverType::params& prm)
{
    prm.solver.maxiter = maxIter;

    prof.tic("setup");
    auto A = amgcl::adapter::block_matrix<value_type<B>>(
        std::tie(lp.RhsSize(), lp.Matrix().Row(), lp.Matrix().Col(), lp.Matrix().Val()));
    SolverType solve(A, prm);
    prof.toc("setup");

    rhs_type<B> const* fptr = reinterpret_cast<rhs_type<B> const*>(lp.Rhs().data());
    rhs_type<B>* xptr = reinterpret_cast<rhs_type<B>*>(lp.SolutionCorrections().data());
    amgcl::backend::numa_vector<rhs_type<B>> F(fptr, fptr + lp.CellCount());
    amgcl::backend::numa_vector<rhs_type<B>> X(xptr, xptr + lp.CellCount());

    prof.tic("solve");
    auto [iters, error] = solve(F, X);
    prof.toc("solve");

    std::copy(X.data(), X.data() + X.size(), xptr);

    return { iters, error, true };
}

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
    builders.emplace_back(L"INJ-1", 125.0, 125.0);
    builders.back().set_completions(c_inj1)
        .inject_water(40.0 * rm).for_days(T);

    auto c_inj2 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 0.0);
    builders.emplace_back(L"INJ-2", 375.0, 375.0);
    builders.back().set_completions(c_inj2)
        .inject_water(30.0 * rm).for_days(T);

    auto c_prod1 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 0.0).open_layer(1, 0.0)
        .open_layer(2, 0.0).open_layer(3, 0.0);
    builders.emplace_back(L"PROD-1", 375.0, 125.0);
    builders.back().set_completions(c_prod1)
        .produce_oil(25.0 * rm).for_days(T);

    auto c_prod2 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(3, 0.0);
    builders.emplace_back(L"PROD-2", 125.0, 375.0);
    builders.back().set_completions(c_prod2)
        .produce_oil(15.0 * rm).for_days(T);

    auto c_prod3 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(1, 0.0).open_layer(2, 0.0);
    builders.emplace_back(L"PROD-3", 250.0, 250.0);
    builders.back().set_completions(c_prod3)
        .produce_oil(20.0 * rm).for_days(T);

    auto c_inj3 = test_helpers::WellCompletionBuilder(BNz, Bhz)
        .open_layer(0, 150.0).open_layer(1, 150.0);
    builders.emplace_back(L"INJ-3", 250.0, 125.0);
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
};

template<typename SolverType>
BenchmarkResult run_benchmark(const std::string& config_name,
                               typename SolverType::params& prm)
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
                prof.tic("assemble");
                sim.AssembleMyProblem(loc_tau, nextTime);
                prof.toc("assemble");

                auto res = solve_with<SolverType>(sim.MyProblem,
                    sim.numPrm.CurrentAMG_maxSolverIterationCount(), prm);
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
            max_oil_rel = std::max(max_oil_rel, oil_rel);
            max_water_rel = std::max(max_water_rel, water_rel);
        }
    }

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
              << prof << "\n";
}

const std::string csv_path = "results/amgcl_benchmark.csv";

} // namespace


// ======================== Series A: Krylov solvers ========================
// Fixed preconditioner: amg<aggregation, damped_jacobi>, block 2x2

using BB = BBackend<B>;

TEST_CASE("AMGCL benchmark: Series A — Krylov solvers",
          "[benchmark][amgcl][seriesA][.slow]")
{
    fs::create_directories("results");

    SECTION("A1: gmres M=5 (baseline)") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::gmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("A1_gmres_M5", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A2: gmres M=15") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::gmres<BB>>;
        S::params prm;
        prm.solver.M = 15;
        auto r = run_benchmark<S>("A2_gmres_M15", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A3: gmres M=30") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::gmres<BB>>;
        S::params prm;
        prm.solver.M = 30;
        auto r = run_benchmark<S>("A3_gmres_M30", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A4: bicgstab") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("A4_bicgstab", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A5: bicgstabl L=2") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::bicgstabl<BB>>;
        S::params prm;
        prm.solver.L = 2;
        auto r = run_benchmark<S>("A5_bicgstabl_L2", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A6: lgmres") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        prm.solver.M = 15;
        auto r = run_benchmark<S>("A6_lgmres_M15", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A7: fgmres M=15") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::fgmres<BB>>;
        S::params prm;
        prm.solver.M = 15;
        auto r = run_benchmark<S>("A7_fgmres_M15", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("A8: idrs S=4") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::idrs<BB>>;
        S::params prm;
        prm.solver.s = 4;
        auto r = run_benchmark<S>("A8_idrs_s4", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }
}


// ======================== Series B: Relaxation ========================
// Fixed solver: best from A (will be updated). Starting with bicgstab.
// Fixed coarsening: aggregation

TEST_CASE("AMGCL benchmark: Series B — Relaxation",
          "[benchmark][amgcl][seriesB][.slow]")
{
    fs::create_directories("results");

    SECTION("B1: damped_jacobi (baseline)") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::damped_jacobi>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("B1_damped_jacobi", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("B2: spai0") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::spai0>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("B2_spai0", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // B3: spai1 — не компилируется с блочным бэкендом (QR::solve)

    SECTION("B4: ilu0") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("B4_ilu0", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("B5: gauss_seidel") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::gauss_seidel>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("B5_gauss_seidel", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("B6: chebyshev") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::chebyshev>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("B6_chebyshev", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }
}


// ======================== Series C: Coarsening ========================

TEST_CASE("AMGCL benchmark: Series C — Coarsening",
          "[benchmark][amgcl][seriesC][.slow]")
{
    fs::create_directories("results");

    SECTION("C1: aggregation (baseline)") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("C1_aggregation", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("C2: smoothed_aggregation") {
        using S = amgcl::make_solver<
            amgcl::amg<BB, amgcl::coarsening::smoothed_aggregation, amgcl::relaxation::ilu0>,
            amgcl::solver::lgmres<BB>>;
        S::params prm;
        auto r = run_benchmark<S>("C2_smoothed_aggregation", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // C3: ruge_stuben — не работает с блочным бэкендом (static_matrix<2,2>)
    // Нужна обёртка as_scalar, отложено.
}


