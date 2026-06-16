#pragma once
#include "../../stdafx.h"

#include "SomeWell.h"
#include "../../Helpers/LogFile.h"

namespace reservoir_simulator
{
	namespace wells 
	{
		class WellFixedProduction :
			public SomeWell
		{
		public:
			WellFixedProduction();
			WellFixedProduction(const WellName& name, const WellName& guid,
				const WellPosition& intersectionCoords,
				std::unique_ptr<const mer_descriptor::MER_Data>&& mer,
				set_of_points::PerforationsOfWell perforationsOfWell,
				const std::vector<const cell::TwoPhaseFlowCell*>& cells_,
				const std::vector<size_t>& itsLocalIDs, double itsAppWellRadius
			);

			CellNumericalData AddWellToMatrix(double nextTimeMoment) override;

			void SetRefWellPressure();
		};
	} // wells
} // reservoir_simulator