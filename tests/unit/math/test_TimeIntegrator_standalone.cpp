#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/TimeIntegrator.h"
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

void compare_solve(ReservoirSimulator& sim_old,
                   ReservoirSimulator& sim_new,
                   const std::vector<double>& timeMoments)
{
	sim_old.Solve(timeMoments);

	TimeIntegrator integrator;
	TimeIntegrator::StepCallbacks callbacks;
	callbacks.prepare_step = [&](double nextRef) {
		sim_new.numPrm.update_maxTauAllowed(nextRef, sim_new.GetWells());
	};
	callbacks.perform_newton = [&](double tau, double nextTime) {
		sim_new.PerformNewtonLoop(tau, nextTime);
	};
	callbacks.on_accept = [&](double tau) {
		sim_new.MassBalance(tau);
		sim_new.Grid.AcceptState();
	};
	callbacks.on_post_update = [&]() {
		sim_new.AddFlowFieldSnapShot();
	};

	integrator.Integrate(timeMoments, sim_new.numPrm,
		sim_new.solverProfile_, callbacks);

	auto P_old = sim_old.GetPressureField();
	auto P_new = sim_new.GetPressureField();
	REQUIRE(P_old.size() == P_new.size());
	for (size_t i = 0; i < P_old.size(); ++i) {
		INFO("P[" << i << "]");
		REQUIRE(P_old[i] == P_new[i]);
	}

	auto Sw_old = sim_old.GetWaterSaturationField();
	auto Sw_new = sim_new.GetWaterSaturationField();
	REQUIRE(Sw_old.size() == Sw_new.size());
	for (size_t i = 0; i < Sw_old.size(); ++i) {
		INFO("Sw[" << i << "]");
		REQUIRE(Sw_old[i] == Sw_new[i]);
	}

	auto bal_old = sim_old.GetOverallBalance();
	auto bal_new = sim_new.GetOverallBalance();
	REQUIRE(bal_old.size() == bal_new.size());
	for (size_t i = 0; i < bal_old.size(); ++i) {
		INFO("balance[" << i << "]");
		REQUIRE(bal_old[i] == bal_new[i]);
	}

	REQUIRE(sim_old.solverProfile_.n_time_steps ==
	        sim_new.solverProfile_.n_time_steps);
	REQUIRE(sim_old.solverProfile_.n_wasted_trials ==
	        sim_new.solverProfile_.n_wasted_trials);
}

} // anonymous namespace

TEST_CASE("TimeIntegrator: bitwise match 1D 5x1x1 no wells",
	"[TimeIntegrator][bitwise]")
{
	auto sim_old = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
	auto sim_new = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
	compare_solve(sim_old, sim_new, {0.0, 10.0});
}

TEST_CASE("TimeIntegrator: bitwise match 2D 5x5x1 with wells",
	"[TimeIntegrator][bitwise]")
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

	compare_solve(sim_old, sim_new, {0.0, 10.0});
}

TEST_CASE("TimeIntegrator: bitwise match multi-step",
	"[TimeIntegrator][bitwise]")
{
	auto sim_old = make_sim(5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
	auto sim_new = make_sim(5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
	compare_solve(sim_old, sim_new, {0.0, 5.0, 10.0, 20.0});
}
