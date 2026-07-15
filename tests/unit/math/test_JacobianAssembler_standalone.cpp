#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/JacobianAssembler.h"

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

void compare_assembly(ReservoirSimulator& sim, double loc_tau, double nextTimeMoment)
{
    sim.AssembleMyProblem(loc_tau, nextTimeMoment);
    auto denseOld = sim.MyProblem.Matrix().toDense();
    auto rhsOld   = sim.MyProblem.Rhs();

    JacobianAssembler assembler;
    assembler.Assemble(loc_tau, nextTimeMoment,
        sim.Grid, sim.MyProblem, sim.RefPressure, sim.Wells);
    auto denseNew = sim.MyProblem.Matrix().toDense();
    auto rhsNew   = sim.MyProblem.Rhs();

    REQUIRE(denseOld.size() == denseNew.size());
    for (size_t i = 0; i < denseOld.size(); ++i) {
        REQUIRE(denseOld[i].size() == denseNew[i].size());
        for (size_t j = 0; j < denseOld[i].size(); ++j)
            REQUIRE(denseOld[i][j] == denseNew[i][j]);
    }

    REQUIRE(rhsOld.size() == rhsNew.size());
    for (size_t i = 0; i < rhsOld.size(); ++i)
        REQUIRE(rhsOld[i] == rhsNew[i]);
}

} // anonymous namespace

TEST_CASE("JacobianAssembler: bitwise match 1D 5x1x1",
    "[JacobianAssembler][bitwise]")
{
    auto sim = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_assembly(sim, 0.1, 0.1);
}

TEST_CASE("JacobianAssembler: bitwise match 2D 3x3x1 with BC RefPressure=200",
    "[JacobianAssembler][bitwise]")
{
    auto sim = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 150.0, 0.7);
    sim.RefPressure = 200.0 * 101325.0;
    compare_assembly(sim, 0.05, 0.05);
}

TEST_CASE("JacobianAssembler: bitwise match with wells 5x5x1",
    "[JacobianAssembler][bitwise]")
{
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();
    auto sim = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);

    test_helpers::add_simple_well(sim, horizon, "INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim, horizon, "PROD", 450.0, 250.0, 5.0, 0.0);

    compare_assembly(sim, 0.1, 0.1);
}

TEST_CASE("JacobianAssembler: bitwise match Sw=0 (oil_saturation=1.0)",
    "[JacobianAssembler][bitwise]")
{
    auto sim = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    compare_assembly(sim, 0.1, 0.1);
}
