#include "simulate_reservoir.h"
#include "../Data/HorizonFactory.h"
#include "../Data/PhaseFactory.hpp"
#include "../Reservoir/ReservoirSimulator.h"
#include "../Reservoir/CalculationManager.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void simulate_reservoir()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 1 };
			reservoir_simulator::DevelopedHorizon DevelopedHorizon
			{ HorizonFactory.createSingleLayerHorizon(layer_id) };

			reservoir_simulator::NumericalParameters NumericalParameters(1E-4, 15, 1E-6, 1E-6);
			reservoir_simulator::PhaseProperties oil{ 
				reservoir_simulator::factories::PhaseFactory::CreateDefaultOil() };
			reservoir_simulator::PhaseProperties water{ 
				reservoir_simulator::factories::PhaseFactory::CreateDefaultWater() };

			reservoir_simulator::ReservoirSimulator simulator{
				NumericalParameters,
				DevelopedHorizon,
				oil, water,
				reservoir_simulator::factories::OtherFactory::CreateDefaultOthers() };

		//	const auto& well = *simulator.GetWells().at(L"273802");

			double tau = 15.0; // in days
			simulator.numPrm.set_initial_schemeTau(tau); // initial allowed integration time-step
			simulator.numPrm.set_currentMoment(42702); // set current time moment

			simulator.Solve({42702,43098});
		}

		void run_simulations()
		{
			CalculationManager manager{105};
			manager.Run(42702, 43740);
		//	manager.Run(44650, 44740);
		}
	} // test_methods
} // reservoir_simulator