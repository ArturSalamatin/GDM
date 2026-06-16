#include "create_oilfield.h"
#include "../Solver/Grids/OilField.h"
#include "../Data/HorizonFactory.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_oilfield()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 0 };
			reservoir_simulator::DevelopedHorizon Horizon
			{ HorizonFactory.createSingleLayerHorizon(layer_id)
			};

			reservoir_simulator::grid::OilField OilField(Horizon);
		}
	} // test_methods
} // reservoir_simulator