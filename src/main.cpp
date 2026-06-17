#include "../HydroSolver/stdafx.h"
#include "../HydroSolver/Solver/Grids/DevelopedHorizon.h"
#include "../HydroSolver/Reservoir/ReservoirSimulator.h"
#include "../HydroSolver/Reservoir/NumericalParameters.h"
#include "../HydroSolver/Data/PhaseFactory.hpp"
#include "../HydroSolver/Data/ExceptionFactory.h"

static reservoir_simulator::DevelopedHorizon make_synthetic_horizon()
{
	constexpr size_t Nx = 20, Ny = 20, Nz = 1;
	constexpr size_t N = Nx * Ny * Nz;
	constexpr double Lx = 1000.0, Ly = 1000.0;
	constexpr double hx = Lx / Nx, hy = Ly / Ny, hz = 10.0;

	constexpr double poro = 0.2;
	constexpr double perm_mD = 100.0;
	constexpr double perm_SI = perm_mD * 0.9869e-15;
	constexpr double P_init_atm = 200.0;
	constexpr double P_init_Pa = P_init_atm * 101325.0;

	reservoir_simulator::DevelopedHorizon h;

	h.grid_size = {Nx, Ny, Nz};
	h.grid_shift = {0.0f, 0.0f};
	h.block_size = {hx, hy};
	h.grid_bounds = reservoir_simulator::GridBounds{0.0, 0.0, Lx, Ly};

	h.volume.assign(N, hx * hy * hz);
	h.x_center.resize(N);
	h.y_center.resize(N);
	h.z_center.resize(N);
	h.cell_thickness.assign(N, hz);

	for (size_t k = 0; k < Nz; ++k)
		for (size_t j = 0; j < Ny; ++j)
			for (size_t i = 0; i < Nx; ++i)
			{
				size_t l = Nx * Ny * k + Nx * j + i;
				h.x_center[l] = (i + 0.5) * hx;
				h.y_center[l] = (j + 0.5) * hy;
				h.z_center[l] = (k + 0.5) * hz;
			}

	h.porosity.assign(N, poro);
	h.permeability_x.assign(N, perm_SI);
	h.permeability_y.assign(N, perm_SI);
	h.active_cells.assign(N, true);
	h.initial_oil_saturation.assign(N, 0.8);
	h.initial_pressure.assign(N, P_init_Pa);

	h.oil = reservoir_simulator::factories::PhaseFactory::CreateDefaultOil();
	h.water = reservoir_simulator::factories::PhaseFactory::CreateDefaultWater();
	h.other = reservoir_simulator::factories::OtherFactory::CreateDefaultOthers();
	h.other.extPressure = P_init_Pa;

	return h;
}

int main()
{
	std::cout << "GDM Reservoir Simulator — synthetic test case\n";

	auto horizon = make_synthetic_horizon();

	reservoir_simulator::NumericalParameters numPrm(1e-6, 65, 1e-5, 1e-5);

	reservoir_simulator::ReservoirSimulator simulator{
		numPrm, horizon, horizon.oil, horizon.water, horizon.other};

	simulator.numPrm.set_initial_schemeTau(10.0);
	simulator.numPrm.set_currentMoment(0.0);

	std::vector<double> timeMoments;
	for (double t = 10.0; t <= 100.0; t += 10.0)
		timeMoments.push_back(t);

	std::cout << "Solving from t=0 to t=100 days...\n";
	simulator.Solve(timeMoments);

	std::cout << "Done. Oil total = " << simulator.OilTotal() << " kg\n";
	return 0;
}
