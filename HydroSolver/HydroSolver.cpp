#include "stdafx.h"

//#include "Reservoir/ReservoirInstantiator.h"
//#include "Helpers/DataPrinter.h"

#include "tests/create_horizon.h"
#include "tests/create_merdata.h"
#include "tests/create_perforations.h"
#include "tests/create_numericalparameters.h"
#include "tests/create_oilfield.h"
#include "tests/create_reservoirsimulator.h"
#include "tests/simulate_reservoir.h"

int main()
{
	//reservoir_simulator::test_methods::create_merdata();
	//reservoir_simulator::test_methods::create_perforations();
	//reservoir_simulator::test_methods::create_horizon();
	//reservoir_simulator::test_methods::create_numericalparameters();
	//reservoir_simulator::test_methods::create_oilfield();
	//reservoir_simulator::test_methods::create_reservoirsimulator();

	//reservoir_simulator::test_methods::simulate_reservoir();
	reservoir_simulator::test_methods::run_simulations();
}
