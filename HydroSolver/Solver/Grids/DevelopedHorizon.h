#pragma once
#include "RawHorizon.h"
#include "../../defines.h"
#include "../../Reservoir/Well/WellJobs.h"

namespace reservoir_simulator
{
	class DevelopedHorizon : public RawHorizon
	{
	public:
		MERofAllWells mer;
		WellJobsContainer well_jobs;

		WellNames well_names;
		WellPositions well_positions;
	};
} // reservoir_simulator
