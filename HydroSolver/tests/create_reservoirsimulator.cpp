#include "create_reservoirsimulator.h"
#include "../Data/HorizonFactory.h"
#include "../Reservoir/ReservoirSimulator.h"
#include "../Data/PhaseFactory.hpp"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_reservoirsimulator()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 0 };
			reservoir_simulator::DevelopedHorizon DevelopedHorizon
			{ HorizonFactory.createSingleLayerHorizon(layer_id) };

			reservoir_simulator::NumericalParameters
				NumericalParameters{
				1E-4, 15, 1E-6, 1E-6 };
			reservoir_simulator::PhaseProperties
				oil{
				reservoir_simulator::factories::PhaseFactory::CreateDefaultOil() };
			reservoir_simulator::PhaseProperties
				water{
				reservoir_simulator::factories::PhaseFactory::CreateDefaultWater() };



			reservoir_simulator::ReservoirSimulator simulator{
				NumericalParameters,
				DevelopedHorizon,
				oil, water,
				reservoir_simulator::factories::OtherFactory::CreateDefaultOthers() };


			//		reservoir_simulator::ReservoirInstantiator ReservoirInstantiator{ Horizon };

		}
	} // tests
} // reservoir_simulator