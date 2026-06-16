#include "create_well.h"
#include "../Data/HorizonFactory.h"
#include "../Reservoir/Well/Wells.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_well()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 0 };
			reservoir_simulator::DevelopedHorizon Horizon
			{ HorizonFactory.createSingleLayerHorizon(layer_id)
			};

			WellName name{ Horizon.well_names[0] };

		/*	reservoir_simulator::wells::WellFixedProduction well(name, name,
				Horizon.well_positions[name],
				std::make_unique<const mer_descriptor::MER_Data>(Horizon.mer[name]),
				Horizon.perfs[name],
				const std::vector<const cell::TwoPhaseFlowCell*>&cells_,
				const std::vector<size_t>&itsLocalIDs,
				0.5);*/
		}
	} // test_methods
} // reservoir_simulator

