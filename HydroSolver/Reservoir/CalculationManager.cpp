#include "../stdafx.h"
#include "CalculationManager.h"
#include "../Data/HorizonFactory.h"
#include "../Data/PhaseFactory.hpp"
#include "../Reservoir/ReservoirSimulator.h"
#include "../Reservoir/NumericalParameters.h"
#include "../Data/ExceptionFactory.h"

namespace reservoir_simulator
{
	CalculationManager::CalculationManager() = default;

	CalculationManager::CalculationManager(size_t frames) :
		frames{ frames }
	{
		// the factory provides access to initial raw data
		std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
		// create horizon factory
		reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

		constexpr size_t layer_id{ 1 };
		reservoir_simulator::DevelopedHorizon DevelopedHorizon
		{ HorizonFactory.createSingleLayerHorizon(layer_id) };

		DevelopedHorizon.PrintModelData(frames);

		reservoir_simulator::NumericalParameters NumericalParameters(1E-6, 65, 1E-5, 1E-5);
		reservoir_simulator::PhaseProperties oil{
			reservoir_simulator::factories::PhaseFactory::CreateDefaultOil() };
		reservoir_simulator::PhaseProperties water{
			reservoir_simulator::factories::PhaseFactory::CreateDefaultWater() };

		simulator = std::move(
			std::make_unique<reservoir_simulator::ReservoirSimulator>(
				NumericalParameters,
				DevelopedHorizon,
				oil, water,
				reservoir_simulator::factories::OtherFactory::CreateDefaultOthers()));

		//	const auto& well = *simulator.GetWells().at(L"273802");
	}

	std::vector<double> generate_temporal_grid(double start, double end, size_t frames)
	{
		std::vector<double> out(frames, 0.0);

		double step = (end - start) / (frames - 1);
		for (size_t l = 0; l < frames; ++l)
			out[l] = step * l + start;

		return out;
	}

	void CalculationManager::Run(double start, double end)
	{
		double tau = 15.0; // in days
		simulator->numPrm.set_initial_schemeTau(tau); // initial allowed integration time-step
		simulator->numPrm.set_currentMoment(start); // set current time moment

		std::vector<double> grid{ generate_temporal_grid(start, end, frames) };

#ifdef PRINT_DATA_SALAMATIN
		simulator->PrintReservoirState(std::ios_base::out); // print the initial state
#endif // PRINT_DATA_SALAMATIN

		MessageFactory::CalcProgress(start, 0, grid.size()-1);
		for (size_t l = 0; l < grid.size() - 1; ++l)
		{
			simulator->Solve({ grid[l], grid[l + 1] });
			MessageFactory::CalcProgress(grid[l + 1], (size_t)(l+1), grid.size()-1);
#ifdef PRINT_DATA_SALAMATIN
			simulator->PrintReservoirState(std::ios_base::app); // print the current state
#endif // PRINT_DATA_SALAMATIN

		}
	}
} // reservoir_simulator