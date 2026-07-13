#pragma once

#include "../stdafx.h"
#include "../defines.h"
#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"

namespace reservoir_simulator
{
	namespace wells { class SomeWell; }

	class JacobianAssembler
	{
	public:
		void Assemble(
			double loc_tau, double nextTimeMoment,
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			double refPressure,
			const std::map<WellName, wells::SomeWell*>& wells);

	private:
		void fillMatrixBlockRow(size_t l, double loc_tau,
			grid::OilField& grid,
			linear_problem::LinearProblem& problem);

		void accountForBoundaryConditions(
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			double refPressure);
	};
} // namespace reservoir_simulator
