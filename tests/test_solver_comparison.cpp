#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include <chrono>
#include <fstream>
#include <filesystem>

using namespace reservoir_simulator;
namespace fs = std::filesystem;

TEST_CASE("RES-007: per-timestep solver comparison",
          "[research][solver-comparison][.slow]")
{
    fs::create_directories("results");

#ifdef GDM_SOLVER_CPR_BICGSTAB
    const std::string solver_name = "bicgstab";
#else
    const std::string solver_name = "lgmres";
#endif

    std::string csv_path = "results/solver_comparison_" + solver_name + ".csv";
    std::ofstream csv(csv_path);
    csv << "time,dt,newton_iters,amg_solves,total_amg_iters,"
        << "t_step_s,balance_oil_rel,balance_water_rel\n";

    auto h = test_helpers::make_uniform_horizon(
        21, 21, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();

    ReservoirSimulator sim(np, h, h.oil, h.water, h.other);

    test_helpers::add_simple_well(sim, h, "INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim, h, "PROD", 450.0, 250.0, 5.0, 0.0);

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();

    std::vector<double> times;
    for (double t = 0.0; t <= 100.0; t += 5.0) times.push_back(t);

    using clock = std::chrono::high_resolution_clock;

    for (size_t i = 1; i < times.size(); ++i) {
        auto prof_before = sim.solverProfile_;

        auto t0 = clock::now();
        sim.Solve({sim.numPrm.CurrentTimeMoment(), times[i]});
        double dt_s = std::chrono::duration<double>(clock::now() - t0).count();

        size_t d_newton = sim.solverProfile_.n_newton_iters - prof_before.n_newton_iters;
        size_t d_amg = sim.solverProfile_.n_amg_solves - prof_before.n_amg_solves;
        size_t d_amg_iters = sim.solverProfile_.total_amg_iters - prof_before.total_amg_iters;

        auto bal = sim.GetOverallBalance();
        double oil_rel = (oil_mass_0 > 0)
            ? std::abs(bal[1] + bal[2] - bal[3]) / oil_mass_0 : 0.0;
        double water_rel = (water_mass_0 > 0)
            ? std::abs(bal[4] + bal[5] - bal[6]) / water_mass_0 : 0.0;

        csv << times[i] << ","
            << (times[i] - times[i-1]) << ","
            << d_newton << ","
            << d_amg << ","
            << d_amg_iters << ","
            << dt_s << ","
            << oil_rel << ","
            << water_rel << "\n";

        CHECK(oil_rel < 1e-3);
        CHECK(water_rel < 1e-3);
    }

    csv.close();
    INFO("CSV written to: " << csv_path);
    SUCCEED();
}
