#include "create_numericalparameters.h"
#include "../Reservoir/NumericalParameters.h"

namespace reservoir_simulator
{
	namespace test_methods
	{
		void create_numericalparameters()
		{
			reservoir_simulator::NumericalParameters NumericalParameters(1E-4, 15, 1E-6, 1E-6);
		}
	} // test_methods
} // reservoir_simulator