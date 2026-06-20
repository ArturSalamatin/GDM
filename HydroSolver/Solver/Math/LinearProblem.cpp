#include "../../stdafx.h"
#include "LinearProblem.h"
#include "MatrixCSR.h"
#include <chrono>

namespace reservoir_simulator
{
	namespace linear_problem
	{
		/////////////////// LinearProblem
		const MatrixCSR& LinearProblem::Matrix() const
		{
			return *matrix.get();
		}
		MatrixCSR& LinearProblem::Matrix()
		{
			return *matrix.get();
		}

		void LinearProblem::PrintRHS() const
		{
			std::ofstream myfile;
			myfile.open("test_RHS.txt");
			myfile.precision(std::numeric_limits< double >::max_digits10);

			size_t mSize = B * cellNmbr;

			myfile << "RHS entries:" << std::endl;
			for (size_t i = 0; i < mSize; i++)
			{
				myfile << rhs[i] << std::endl;
			}

			myfile.close();
		}

		void LinearProblem::Print() const
		{
			Matrix().PrintCRS();
			Matrix().PrintDiagBlocks();
			PrintRHS();
			PrintCorrections();
		}

		void LinearProblem::PrintCorrections() const
		{
			std::ofstream myfile;
			myfile.open("test_Corrections.txt");

			size_t mSize = B * cellNmbr;

			for (size_t i = 0; i < mSize; i++)
			{
				myfile << solutionCorrections[i] << std::endl;
			}

			myfile.close();
		}

		// nummber of equations per cell
		unsigned char LinearProblem::EqNmbr() const { return B; }

		const std::vector<double>& LinearProblem::Corrections() const {
			return solutionCorrections;
		}

		size_t LinearProblem::NmbrOfNonZerosPerUnitBlock() const { return Matrix().NmbrOfNonZerosPerUnitBlock(); }

		LinearProblem::LinearProblem() noexcept = default;

		LinearProblem::LinearProblem(
			double amg_AbsTol, double AMG_RelTol, 
			const std::vector<std::vector<int>>& connectivityGraph, 
			const std::vector<bool>& blPattern) noexcept :
			cellNmbr {connectivityGraph.size()},
			rhsSize{ cellNmbr * B },
			rhs{ std::vector<double>(rhsSize, 0.0) },
			solutionCorrections{ std::vector<double>(rhsSize, 0.0) },
			matrix{ std::make_unique<MatrixCSR>(B, cellNmbr, connectivityGraph, blPattern) }
		{
			prm.solver.tol = AMG_RelTol;
			prm.solver.abstol = amg_AbsTol;
			prm.solver.maxiter = 5;
			//	prm.precond.coarse_enough = 1000;
			//	prm.precond.pre_cycles = 4;
			//	prm.precond.ncycle = 4;
			//	prm.precond.npost = 4;
			//	prm.precond.npre = 4;
			//	prm.precond.coarsening.estimate_spectral_radius = true;
			//	prm.solver.M = 20;
			//	prm.precond.direct_coarse = true;
			//	prm.precond.relax.damping = 0.5;
			//	prm.precond.coarsening.over_interp = 2.0 / 3.0;
			//	prm.precond.coarsening.aggr.eps_strong = 10;
		}

		LinearProblem::~LinearProblem() = default;

		void LinearProblem::ResetProblem()
		{
			matrix->ResetMatrix();
			std::fill(rhs.begin(), rhs.end(), 0.0);
			std::fill(solutionCorrections.begin(), solutionCorrections.end(), 0.0);
		}

		SolveResult LinearProblem::Solve(int maxIter)
		{
			using clock = std::chrono::steady_clock;
			prm.solver.maxiter = maxIter;

			auto t0 = clock::now();
			auto A = amgcl::adapter::block_matrix<value_type<B>>(
				std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()));
			Solver_AMG<B> solve(A, prm);
			auto t1 = clock::now();

			rhs_type<B> const* fptr = reinterpret_cast<rhs_type<B> const*>(&rhs[0]);
			rhs_type<B>* xptr = reinterpret_cast<rhs_type<B>*>(&solutionCorrections[0]);
			amgcl::backend::numa_vector<rhs_type<B>> F(fptr, fptr + cellNmbr);
			amgcl::backend::numa_vector<rhs_type<B>> X(xptr, xptr + cellNmbr);

			auto [iters, error] = solve(F, X);
			std::copy(X.data(), X.data() + X.size(), xptr);
			auto t2 = clock::now();

			double setup_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
			double solve_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();

			return { iters, error, true, setup_ms, solve_ms };
		}

		void LinearProblem::AddDiagBlock(size_t l, const std::vector<double>& data, const std::vector<double>& dataRHS)
		{
			std::transform(dataRHS.begin(), dataRHS.end(), rhs.begin() + l * B, rhs.begin() + l * B, std::plus<double>());
			Matrix().AddDiagBlock(l, data);
		}

		void LinearProblem::AddOffDiagBlock(size_t l, int neibIdx, std::vector<double>& data)
		{
			Matrix().AddOffDiagBlock(l, neibIdx, data);
		}

	} // linear_problem
} // reservoir_simulator