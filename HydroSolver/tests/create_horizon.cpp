#include "create_horizon.h"

#include "../Descriptors/Descriptors.h"
#include "../Data/HorizonFactory.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_horizon()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 1 };


			reservoir_simulator::RawHorizon RawHorizon
			{ HorizonFactory.createSingleLayerRawHorizon(layer_id)
			};

			reservoir_simulator::DevelopedHorizon Horizon
				{ HorizonFactory.createSingleLayerHorizon(layer_id)
			};
		}
	} // test_methods
} // reservoir_simulator



