#include "../stdafx.h"
#include "create_merdata.h"
#include "../Descriptors/MER_Descriptor.h"
#include "../Data/HorizonFactory.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_merdata()
		{
			// the factory provides access to initial raw data
			std::string CONNECTION_NAME{ "ljihnlsqjfybogpnpddrvrmvwgfyxefj" };
			// create horizon factory
			reservoir_simulator::factories::HorizonFactory HorizonFactory{ CONNECTION_NAME };

			constexpr size_t layer_id{ 1 };
			reservoir_simulator::DevelopedHorizon Horizon
			{ HorizonFactory.createSingleLayerHorizon(layer_id)
			};

			WellName name{ L"273804"};

			reservoir_simulator::mer_descriptor::MER_Data MER_Data(name, Horizon.mer[name]);
		}
	} // test_methods
} // reservoir_simulator