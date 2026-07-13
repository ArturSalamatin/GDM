#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/MassBalanceTracker.h"
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

void compare_balance(ReservoirSimulator& sim_old,
                     ReservoirSimulator& sim_new,
                     double tau)
{
    MassBalanceTracker tracker;
    tracker.Initialize(sim_new.OilTotal(), sim_new.WaterTotal());

    sim_old.PerformNewtonLoop(tau, tau);
    sim_new.PerformNewtonLoop(tau, tau);

    REQUIRE(sim_old.numPrm.IsSuccessfullNewtonTrial() ==
            sim_new.numPrm.IsSuccessfullNewtonTrial());

    if (!sim_old.numPrm.IsSuccessfullNewtonTrial())
        return;

    sim_old.MassBalance(tau);

    tracker.Update(tau,
        sim_new.OilTotal(), sim_new.WaterTotal(),
        sim_new.OilContourFlux(), sim_new.WaterContourFlux(),
        sim_new.OilDebitTotal(), sim_new.WaterDebitTotal());

    auto bal_old = sim_old.GetOverallBalance();
    auto bal_new = tracker.GetBalance();

    REQUIRE(bal_old.size() == bal_new.size());
    for (size_t i = 0; i < bal_old.size(); ++i) {
        INFO("balance[" << i << "]");
        REQUIRE(bal_old[i] == bal_new[i]);
    }
}

} // anonymous namespace

TEST_CASE("MassBalanceTracker: bitwise match 1D 5x1x1 no wells",
    "[MassBalanceTracker][bitwise]")
{
    auto sim_old = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto sim_new = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_balance(sim_old, sim_new, 0.1);
}

TEST_CASE("MassBalanceTracker: bitwise match 2D 5x5x1 with wells",
    "[MassBalanceTracker][bitwise]")
{
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();

    auto sim_old = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);
    auto sim_new = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);

    test_helpers::add_simple_well(sim_old, horizon, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_old, horizon, L"PROD", 450.0, 250.0, 5.0, 0.0);
    test_helpers::add_simple_well(sim_new, horizon, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_new, horizon, L"PROD", 450.0, 250.0, 5.0, 0.0);

    compare_balance(sim_old, sim_new, 0.1);
}

TEST_CASE("MassBalanceTracker: bitwise match Sw=0",
    "[MassBalanceTracker][bitwise]")
{
    auto sim_old = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    auto sim_new = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    compare_balance(sim_old, sim_new, 0.1);
}
