#include "create_perforations.h"
#include "../Data/HorizonFactory.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_perforations()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 1 };
			reservoir_simulator::DevelopedHorizon Horizon
			{ HorizonFactory.createSingleLayerHorizon(layer_id)
			};

		//	set_of_points::AccumulatedPerforations
		//		Perforations{ Horizon.perfs[Horizon.well_names[0]] };
		}
	} // test_methods
} // reservoir_simu