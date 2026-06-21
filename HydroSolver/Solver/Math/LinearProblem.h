#pragma once
#include "../../stdafx.h"
#include "MatrixCSR.h"

#undef min
#undef max

//#include <amgcl/adapter/eigen.hpp>
#include <amgcl/adapter/block_matrix.hpp>
#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/value_type/static_matrix.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/solver/gmres.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/relaxation/damped_jacobi.hpp>

#include <amgcl/io/mm.hpp>
#include <amgcl/profiler.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/spai0.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/solver/bicgstabl.hpp>
#include <amgcl/solver/fgmres.hpp>
#include <amgcl/solver/lgmres.hpp>
#include <amgcl/solver/idrs.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/chebyshev.hpp>
#include <amgcl/relaxation/gauss_seidel.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>


#undef min
#undef max

namespace amgcl { __declspec(selectany) profiler<> prof; } // modifier is to avoid multiple redefinitions of the same variable
using amgcl::prof;

namespace reservoir_simulator
{
	namespace linear_problem
	{
		struct SolveResult
		{
			size_t iters;
			double error;
			bool converged;
		};

		template<unsigned char B>
		using value_type = amgcl::static_matrix<double, B, B>;
		template<unsigned char B>
		using rhs_type = amgcl::static_matrix<double, B, 1>;
		template<unsigned char B>
		using BBackend = amgcl::backend::builtin<value_type<B>>;

		template<unsigned char B>
		using Solver_AMG = amgcl::make_solver<
			amgcl::amg< BBackend<B>, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>
			,
			amgcl::solver::lgmres<BBackend<B>>
		>;

		constexpr unsigned char B = 2;
		class LinearProblem
		{
		protected:
			size_t cellNmbr;
			size_t rhsSize;

			std::vector<double> rhs;
			std::vector<double> solutionCorrections;
			std::unique_ptr<MatrixCSR> matrix;

			Solver_AMG<B>::params prm;

		public:
			Solver_AMG<B>::params& SolverParams() { return prm; }

			const MatrixCSR& Matrix() const;
			MatrixCSR& Matrix();
			void Print() const;
			void PrintRHS() const;

			void PrintCorrections() const;

			// nummber of equations per cell
			unsigned char EqNmbr() const;
			const std::vector<double>& Corrections() const;

			size_t NmbrOfNonZerosPerUnitBlock() const;

			LinearProblem() noexcept;

			LinearProblem(double amg_AbsTol, double AMG_RelTol, const std::vector<std::vector<int>>& connectivityGraph,
				const std::vector<bool>& blPattern = std::vector<bool>(B * B, true)) noexcept;

			virtual ~LinearProblem();

			void ResetProblem();

			SolveResult Solve(int maxIter);

			size_t CellCount() const { return cellNmbr; }
			size_t RhsSize() const { return rhsSize; }
			const std::vector<double>& Rhs() const { return rhs; }
			std::vector<double>& SolutionCorrections() { return solutionCorrections; }

			template<typename SolverType>
			SolveResult SolveWith(int maxIter, typename SolverType::params& custom_prm);


			void AddDiagBlock(size_t l, const std::vector<double>& data, const std::vector<double>& dataRHS);
			void AddOffDiagBlock(size_t l, int neibIdx, std::vector<double>& data);
		};
	} // linear_problem
} // reservoir_simulator