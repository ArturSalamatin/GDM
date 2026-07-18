#include "NewtonSolver.h"
#include "ReservoirSImulator.h"
#include "Well/Wells.h"

namespace reservoir_simulator
{
	using namespace cell;
	using namespace grid;
	using namespace linear_problem;

	void NewtonSolver::Solve(
		double loc_tau, double nextTimeMoment,
		OilField& grid, LinearProblem& problem,
		NumericalParameters& numPrm, double refPressure,
		const std::map<WellName, wells::SomeWell*>& wells,
		SolverProfile& profile)
	{
		numPrm.set_currentNewtonIterationCount(0);
		numPrm.update_isSuccesfullNewtonTrial(false);
		numPrm.set_currentAMG_maxSolverIterationCount();
		while (!numPrm.IsSuccessfullNewtonTrial())
		{
			SingleIteration(loc_tau, nextTimeMoment, grid, problem,
				numPrm, refPressure, wells, profile);
			profile.n_newton_iters++;

			if (numPrm.IsSuccessfullAMG_Iteration() && numPrm.IsNewtonIterationContinue())
			{
				prof.tic("update");
				numPrm.update_isSuccesfullNewtonTrial(
					UpdateGrid(grid, problem, numPrm));
				prof.toc("update");
			}
			else
			{
				grid.ReverseState();
				break;
			}
		}
	}

	void NewtonSolver::SingleIteration(
		double loc_tau, double nextTimeMoment,
		OilField& grid, LinearProblem& problem,
		NumericalParameters& numPrm, double refPressure,
		const std::map<WellName, wells::SomeWell*>& wells,
		SolverProfile& profile)
	{
		prof.tic("assemble");
		assembler_.Assemble(loc_tau, nextTimeMoment, grid, problem, refPressure, wells);
		prof.toc("assemble");

		auto res = problem.Solve(numPrm.CurrentAMG_maxSolverIterationCount());
		numPrm.update_currentAMGState(
			{ res.iters, res.error, res.converged });

		profile.n_amg_solves++;
		profile.total_amg_iters += res.iters;
	}

	bool NewtonSolver::UpdateGrid(
		OilField& grid, LinearProblem& problem,
		const NumericalParameters& numPrm)
	{
		const double tol = 3E-3;
		std::vector<char> f(B * grid.ActiveCellsNmbr(), 1);

#ifdef	USE_PARALLEL
#pragma omp parallel for
#endif
		for (int l = 0; l < grid.ActiveCellsNmbr(); l++)
		{
			double corr[B];
			problem.UnpackCellCorrections(l, corr);
			grid[l].UpdateState(corr);

			const std::vector<double>& stateVaiables = grid[l].GetVariableFieldProperties();

			int i = 0; // saturation
			f[B * l + i] =
				(abs(stateVaiables[i]) < numPrm.NewtonTol() * tol) || (abs(1.0 - stateVaiables[i]) < numPrm.NewtonTol() * tol) ||
				(abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
			i = 1; // pressure
			f[B * l + i] =
				(abs(stateVaiables[i]) < 1E6) ||
				(abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
		}
		return std::all_of(f.begin(), f.end(), [](char x) { return x != 0; });
	}

} // namespace reservoir_simulator
