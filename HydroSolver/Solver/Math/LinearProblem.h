#pragma once
#include "../../stdafx.h"
#include "MatrixCSR.h"
#include "CRSStructure.h"

#undef min
#undef max

#include "SolverConfig.h"
#include <amgcl/profiler.hpp>

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

		constexpr unsigned char B = 2;
		class LinearProblem
		{
		protected:
			size_t cellNmbr;
			size_t rhsSize;

			std::vector<double> rhs;
			std::vector<double> solutionCorrections;
			std::unique_ptr<MatrixCSR> matrix;

			SolverType::params prm;

		public:
			const MatrixCSR& Matrix() const;
			MatrixCSR& Matrix();
			void Print() const;
			void PrintRHS() const;

			void PrintCorrections() const;

			// nummber of equations per cell
			unsigned char EqNmbr() const;
			const std::vector<double>& Corrections() const;

			size_t NmbrOfNonZerosPerUnitBlock() const;

			const CRSStructure& GetCRS() const;
			void UnpackCellCorrections(size_t cell, double* physical) const;

			LinearProblem() noexcept;

			LinearProblem(Layout layout, double amg_AbsTol, double AMG_RelTol,
				const std::vector<std::vector<int>>& connectivityGraph,
				const std::vector<bool>& blPattern = std::vector<bool>(B * B, true)) noexcept;

			virtual ~LinearProblem();

			void ResetProblem();

			SolveResult Solve(int maxIter);

			size_t CellCount() const { return cellNmbr; }
			size_t RhsSize() const { return rhsSize; }
			const std::vector<double>& Rhs() const { return rhs; }
			std::vector<double>& SolutionCorrections() { return solutionCorrections; }

			void AddDiagBlock(size_t l, const std::vector<double>& data, const std::vector<double>& dataRHS);
			void AddDiagBlock(size_t l, const double* data, const double* dataRHS);
			void AddOffDiagBlock(size_t l, int neibIdx, std::vector<double>& data);
			void AddOffDiagBlock(size_t l, int neibIdx, const double* data);
		};
	} // linear_problem
} // reservoir_simulator
