#include "../../stdafx.h"
#include "LinearProblem.h"
#include "MatrixCSR.h"

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

		const CRSStructure& LinearProblem::GetCRS() const
		{
			return Matrix().GetCRS();
		}

		void LinearProblem::UnpackCellCorrections(size_t cell, double* physical) const
		{
			GetCRS().UnpackCorrections(cell, solutionCorrections.data(), physical);
		}

		LinearProblem::LinearProblem() noexcept = default;

		LinearProblem::LinearProblem(
			Layout layout,
			double amg_AbsTol, double AMG_RelTol,
			const std::vector<std::vector<int>>& connectivityGraph,
			const std::vector<bool>& blPattern) noexcept :
			cellNmbr {connectivityGraph.size()},
			rhsSize{ cellNmbr * B },
			rhs{ std::vector<double>(rhsSize, 0.0) },
			solutionCorrections{ std::vector<double>(rhsSize, 0.0) },
			matrix{ std::make_unique<MatrixCSR>(layout, B, cellNmbr, connectivityGraph, blPattern) }
		{
#if !defined(GDM_SOLVER_ILU0)
			prm.precond.block_size = B;
#endif
			prm.solver.tol = AMG_RelTol;
			prm.solver.abstol = amg_AbsTol;
			prm.solver.maxiter = 5;
#if !defined(GDM_SOLVER_CPR_BICGSTAB)
			prm.solver.K = 5;
#endif
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
			prm.solver.maxiter = maxIter;

			// Regularize near-zero diagonals for degenerate equation rows.
			// At Sw≈0 the water equation row has zero pressure-dependence;
			// scalar ILU needs nonzero pivots.
			{
				const auto& row = Matrix().Row();
				const auto& col = Matrix().Col();
				auto& val = Matrix().Val();
				for (size_t i = 0; i < rhsSize; ++i) {
					for (size_t k = row[i]; k < row[i + 1]; ++k) {
						if (col[k] == i) {
							if (std::abs(val[k]) < 1e-20)
								val[k] = 1e-6;
							break;
						}
					}
				}
			}

			prof.tic("setup");
			SolverType solve(
				std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()),
				prm);
			prof.toc("setup");

			std::vector<double> F(rhs);
			std::vector<double> X(solutionCorrections);

			prof.tic("solve");
			auto [iters, error] = solve(F, X);
			prof.toc("solve");

			solutionCorrections = std::move(X);

			return { iters, error, std::isfinite(error) };
		}

		void LinearProblem::AddDiagBlock(size_t l, const std::vector<double>& data, const std::vector<double>& dataRHS)
		{
			const auto& crs = GetCRS();
			for (size_t i = 0; i < B; i++)
				rhs[crs.GlobalIndex(l, (unsigned char)i)] += dataRHS[i];
			Matrix().AddDiagBlock(l, data);
		}

		void LinearProblem::AddDiagBlock(size_t l, const double* data, const double* dataRHS)
		{
			const auto& crs = GetCRS();
			for (size_t i = 0; i < B; i++)
				rhs[crs.GlobalIndex(l, (unsigned char)i)] += dataRHS[i];
			Matrix().AddDiagBlock(l, data);
		}

		void LinearProblem::AddOffDiagBlock(size_t l, int neibIdx, std::vector<double>& data)
		{
			Matrix().AddOffDiagBlock(l, neibIdx, data);
		}

		void LinearProblem::AddOffDiagBlock(size_t l, int neibIdx, const double* data)
		{
			Matrix().AddOffDiagBlock(l, neibIdx, data);
		}

	} // linear_problem
} // reservoir_simulator
