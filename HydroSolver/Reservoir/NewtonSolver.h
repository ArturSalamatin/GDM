#pragma once

#include "../stdafx.h"
#include "JacobianAssembler.h"
#include "NumericalParameters.h"
#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"

namespace reservoir_simulator
{
	namespace wells { class SomeWell; }
	struct SolverProfile;

	class NewtonSolver
	{
	public:
		void Solve(
			double loc_tau, double nextTimeMoment,
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			NumericalParameters& numPrm,
			double refPressure,
			const std::map<WellName, wells::SomeWell*>& wells,
			SolverProfile& profile);

		void SingleIteration(
			double loc_tau, double nextTimeMoment,
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			NumericalParameters& numPrm,
			double refPressure,
			const std::map<WellName, wells::SomeWell*>& wells,
			SolverProfile& profile);

		bool UpdateGrid(
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			const NumericalParameters& numPrm);

		JacobianAssembler assembler_;
	};
} // namespace reservoir_simulator
