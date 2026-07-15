#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/NewtonSolver.h"
#include "../../../HydroSolver/Reservoir/ReservoirSImulator.h"

using namespace reservoir_simulator;

namespace {

ReservoirSimulator make_sim(
    size_t Nx, size_t Ny, size_t Nz,
    double Lx, double Ly, double hz,
    double perm_mD, double poro,
    double P_init_atm, double oil_saturation)
{
    auto h = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz,
        perm_mD, poro, P_init_atm, oil_saturation);
    auto np = test_helpers::default_num_params();
    return ReservoirSimulator(np, h, h.oil, h.water, h.other);
}

void compare_newton(ReservoirSimulator& sim_old,
                    ReservoirSimulator& sim_new,
                    double tau)
{
    sim_old.PerformNewtonLoop(tau, tau);
    auto P_old  = sim_old.GetPressureField();
    auto Sw_old = sim_old.GetWaterSaturationField();

    NewtonSolver solver;
    SolverProfile profile{};
    solver.Solve(tau, tau, sim_new.Grid, sim_new.MyProblem,
        sim_new.numPrm, sim_new.RefPressure, sim_new.Wells, profile);
    auto P_new  = sim_new.GetPressureField();
    auto Sw_new = sim_new.GetWaterSaturationField();

    REQUIRE(P_old.size() == P_new.size());
    for (size_t i = 0; i < P_old.size(); ++i) {
        INFO("P[" << i << "]");
        REQUIRE(P_old[i] == P_new[i]);
    }

    REQUIRE(Sw_old.size() == Sw_new.size());
    for (size_t i = 0; i < Sw_old.size(); ++i) {
        INFO("Sw[" << i << "]");
        REQUIRE(Sw_old[i] == Sw_new[i]);
    }

    REQUIRE(sim_old.numPrm.IsSuccessfullNewtonTrial() ==
            sim_new.numPrm.IsSuccessfullNewtonTrial());
    REQUIRE(sim_old.numPrm.CurrentNewtonIterationCount() ==
            sim_new.numPrm.CurrentNewtonIterationCount());
}

} // anonymous namespace

TEST_CASE("NewtonSolver: bitwise match 1D 5x1x1 no wells",
    "[NewtonSolver][bitwise]")
{
    auto sim_old = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto sim_new = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_newton(sim_old, sim_new, 0.1);
}

TEST_CASE("NewtonSolver: bitwise match 2D 5x5x1 with wells",
    "[NewtonSolver][bitwise]")
{
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();

    auto sim_old = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);
    auto sim_new = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);

    test_helpers::add_simple_well(sim_old, horizon, "INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_old, horizon, "PROD", 450.0, 250.0, 5.0, 0.0);
    test_helpers::add_simple_well(sim_new, horizon, "INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_new, horizon, "PROD", 450.0, 250.0, 5.0, 0.0);

    compare_newton(sim_old, sim_new, 0.1);
}

TEST_CASE("NewtonSolver: bitwise match Sw=0 (oil_saturation=1.0)",
    "[NewtonSolver][bitwise]")
{
    auto sim_old = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    auto sim_new = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    compare_newton(sim_old, sim_new, 0.1);
}
