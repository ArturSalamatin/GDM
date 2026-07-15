#pragma once
#include "../../defines.h"
#include "../../Solver/Grids/Cells/TwoPhaseFlowCell.h"

namespace reservoir_simulator
{
	namespace wells
	{
		class WellTrajectory
		{
		protected:
			std::vector<const cell::TwoPhaseFlowCell*> cells; // pointers to the cells where the well is
			WellPosition itsIntersectionCoords; // real coordinates

		public:
			WellTrajectory(const WellPosition& intersectionCoords,
				const std::vector<const cell::TwoPhaseFlowCell*>& cells_)
				: itsIntersectionCoords(intersectionCoords), cells(cells_) {};
			WellTrajectory() {};

			double PosX() const { return  itsIntersectionCoords.get()->getX(); }
			double PosY() const { return itsIntersectionCoords.get()->getY(); }
			std::vector<double> IntersectionCoords() const { return { PosX(), PosY() }; }
			std::string IntersectionCoords_json() const { std::string result = "[" + std::to_string(PosX()) + "," + std::to_string(PosY()) + "]";    return result; }
			double PosXnum() const { return Cells()[0]->X(); }
			double PosYnum() const { return Cells()[0]->Y(); }
			std::vector<double> IntersectionCoordsNum() const { return { PosXnum(), PosYnum() }; }

			const std::vector<const cell::TwoPhaseFlowCell*>& Cells() const { return cells; }
		};

	} // wells
} // reservoir_simulator