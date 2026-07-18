#include "JacobianAssembler.h"
#include "Well/Wells.h"

namespace reservoir_simulator
{
	using namespace cell;
	using namespace grid;
	using namespace linear_problem;

#ifdef DEBUG_SALAMATIN
	static void CheckForNAN(std::vector<double> someArr)
	{
		for (auto& a : someArr)
			if (!isfinite(a))
				throw std::exception("bad well rhs");
	}
#endif

	void JacobianAssembler::Assemble(
		double loc_tau, double nextTimeMoment,
		OilField& grid, LinearProblem& problem,
		double refPressure,
		const std::map<WellName, wells::SomeWell*>& wells)
	{
		prof.tic("reset");
		problem.ResetProblem();
		prof.toc("reset");

		prof.tic("fill_rows");
#ifdef	USE_PARALLEL
#pragma omp parallel for
#endif
		for (int l = 0; l < static_cast<int>(grid.ActiveCellsNmbr()); l++)
		{
			fillMatrixBlockRow(l, loc_tau, grid, problem);
		}
		prof.toc("fill_rows");

		prof.tic("boundary");
		accountForBoundaryConditions(grid, problem, refPressure);
		prof.toc("boundary");

		prof.tic("wells");
		for (auto& [name, well] : wells)
		{
			auto [posLocal, matrixBlockPerPerforation, rhsPerPerforation] =
				well->AddWellToMatrix(nextTimeMoment - loc_tau);
			for (size_t l = 0; l < well->NmbrOfOpenedCells(); ++l)
			{
#ifdef DEBUG_SALAMATIN
				CheckForNAN(matrixBlockPerPerforation[l]);
				CheckForNAN(rhsPerPerforation[l]);
#endif // DEBUG_SALAMATIN
				problem.AddDiagBlock(posLocal[l], matrixBlockPerPerforation[l], rhsPerPerforation[l]);
			}
		}
		prof.toc("wells");
	}

	void JacobianAssembler::fillMatrixBlockRow(size_t l, double loc_tau,
		OilField& grid, LinearProblem& problem)
	{
		constexpr int blockSize = B * B;
		const TwoPhaseFlowCell& cell = grid[l];
		const std::vector<TwoPhaseFlowCell*> neighbourCells = grid.GetNeighboursPointer(l);
		const std::vector<double>& commonEdgeArea = grid.CommonEdgeArea(l);

		double blDiag[blockSize] = {};
		const auto& prevMass = cell.PreviousState_Mass_ref();
		double rhsBlock[B] = {
			(prevMass[0] - cell.OilMass()) / loc_tau,
			(prevMass[1] - cell.WaterMass()) / loc_tau
		};

		blDiag[0] += cell.DerivativeMassOilBySwater() / loc_tau;
		blDiag[2] += cell.DerivativeMassWaterBySwater() / loc_tau;
		blDiag[1] += cell.DerivativeMassOilByP() / loc_tau;
		blDiag[3] += cell.DerivativeMassWaterByP() / loc_tau;

		double
			OilMobilitySum = 0.0,
			WaterMobilitySum = 0.0;

		for (size_t neibCount = 0; neibCount < neighbourCells.size(); neibCount++)
		{
			const TwoPhaseFlowCell& neighbourCell = *neighbourCells[neibCount];

			double blOffDiag[blockSize] = {};

			double c_oil = 0.0, c_water = 0.0;
			double dp = cell.P() - neighbourCell.P();
			double p_grad = commonEdgeArea[neibCount] * dp;

			const TwoPhaseFlowCell* bwCell;
			double* bwBlock;
			if (dp < 0.0) {
				bwCell = &neighbourCell;
				bwBlock = blOffDiag;
			}
			else {
				bwCell = &cell;
				bwBlock = blDiag;
			}

			double OverallMobilityCell = cell.MobilityOverall(),
				OverallMobilityNeighbour = neighbourCell.MobilityOverall();
			double denom = OverallMobilityCell + OverallMobilityNeighbour;
			if (denom == 0.0) continue;

			double MeanOverallMobility = 2 * OverallMobilityNeighbour * OverallMobilityCell / denom;
			double
				c1 = 2 * std::pow(OverallMobilityNeighbour / denom, 2) * (cell.DerivativeMobilityOil() + cell.DerivativeMobilityWater()),
				c3 = 2 * std::pow(OverallMobilityCell / denom, 2) * (neighbourCell.DerivativeMobilityOil() + neighbourCell.DerivativeMobilityWater());
			double f_oil = bwCell->F_Oil(),
				f_water = bwCell->F_Water();

			double temp = p_grad * (bwCell->Derivative_F_Oil()) * MeanOverallMobility;

			bwBlock[0] += bwCell->DensityOil() * temp;
			bwBlock[2] -= bwCell->DensityWater() * temp;

			c_oil = -bwCell->DensityOil() * f_oil * MeanOverallMobility * commonEdgeArea[neibCount];
			rhsBlock[0] += c_oil * dp;
			blOffDiag[1] += c_oil;
			c_water = -bwCell->DensityWater() * f_water * MeanOverallMobility * commonEdgeArea[neibCount];
			rhsBlock[1] += c_water * dp;
			blOffDiag[3] += c_water;
			OilMobilitySum += c_oil;
			WaterMobilitySum += c_water;

			blDiag[0] += bwCell->DensityOil() * f_oil * c1 * p_grad;
			blOffDiag[0] += bwCell->DensityOil() * f_oil * c3 * p_grad;
			blDiag[2] += bwCell->DensityWater() * f_water * c1 * p_grad;
			blOffDiag[2] += bwCell->DensityWater() * f_water * c3 * p_grad;

			problem.AddOffDiagBlock(l, neibCount, blOffDiag);
		}

		blDiag[1] -= OilMobilitySum;
		blDiag[3] -= WaterMobilitySum;

		problem.AddDiagBlock(l, blDiag, rhsBlock);
	}

	void JacobianAssembler::accountForBoundaryConditions(
		OilField& grid, LinearProblem& problem, double refPressure)
	{
		size_t nx = grid.Nx(), ny = grid.Ny(), nz = grid.Nz();

		// loop through the boundaries parallel to YZ-plane
		if(nx>1)
			for (size_t j = 0; j < ny; j++)
			{
				for (size_t i = 0; i < nx; i += nx - 1)
				{
					for (size_t k = 0; k < nz; k++)
					{
						ptrdiff_t l = grid.ConvertGlobal2Local(nx * ny * k + nx * j + i);

						if (l < 0)
						{
							continue;
						}

						size_t blockSize = problem.NmbrOfNonZerosPerUnitBlock();
						const TwoPhaseFlowCell& cell = grid[l];

						std::vector<double> blDiag = std::vector<double>(blockSize, 0);
						std::vector<double> rhsBlock = std::vector<double>(B, 0);

						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

						double dp = cell.P() - refPressure;
						double faceArea = hy * hz / (hx / 2);
						double p_grad = faceArea * dp;
						double f_oil, f_water;
						double cur_mobility = cell.MobilityOverall(),
							d_cur_mobility_oil = cell.DerivativeMobilityOil(),
							d_cur_mobility_water = cell.DerivativeMobilityWater();

						if (dp > 0.0)
						{
							f_oil = cell.F_Oil() * cell.DensityOil();
							f_water = cell.F_Water() * cell.DensityWater();

							blDiag[0] += d_cur_mobility_oil * p_grad * cell.DensityOil();
							blDiag[2] += d_cur_mobility_water * p_grad * cell.DensityWater();
						}
						else
						{
							f_oil = 0.0;
							f_water = 1.0 * cell.DensityWater(refPressure);

							blDiag[2] = (d_cur_mobility_oil + d_cur_mobility_water) * p_grad * cell.DensityWater(refPressure);
						}
						blDiag[1] += faceArea * cur_mobility * f_oil;
						blDiag[3] += faceArea * cur_mobility * f_water;

						rhsBlock[0] -= faceArea * cur_mobility * f_oil * dp;
						rhsBlock[1] -= faceArea * cur_mobility * f_water * dp;

#ifdef DEBUG_SALAMATIN
						CheckForNAN(blDiag);
						CheckForNAN(rhsBlock);
#endif // DEBUG_SALAMATIN

						problem.AddDiagBlock(l, blDiag, rhsBlock);
					}
				}
			}
		// loop through the boundaries parallel to XZ-plane
		if(ny>1)
			for (size_t i = 0; i < nx; i++)
			{
				for (size_t j = 0; j < ny; j += ny - 1)
				{
					for (size_t k = 0; k < nz; k++)
					{
						ptrdiff_t l = grid.ConvertGlobal2Local(nx * ny * k + nx * j + i);

						if (l < 0)
						{
							continue;
						}

						size_t blockSize = problem.NmbrOfNonZerosPerUnitBlock();
						const TwoPhaseFlowCell& cell = grid[l];

						std::vector<double> blDiag = std::vector<double>(blockSize, 0);
						std::vector<double> rhsBlock = std::vector<double>(B, 0);

						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

						double faceArea = hx * hz / (hy / 2);
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_oil, f_water;
						double cur_mobility = cell.MobilityOverall(),
							d_cur_mobility_oil = cell.DerivativeMobilityOil(),
							d_cur_mobility_water = cell.DerivativeMobilityWater();

						if (dp > 0.0)
						{
							f_oil = cell.F_Oil() * cell.DensityOil();
							f_water = cell.F_Water() * cell.DensityWater();

							blDiag[0] += d_cur_mobility_oil * p_grad * cell.DensityOil();
							blDiag[2] += d_cur_mobility_water * p_grad * cell.DensityWater();
						}
						else
						{
							f_oil = 0.0;
							f_water = 1.0 * cell.DensityWater(refPressure);

							blDiag[2] = (d_cur_mobility_oil + d_cur_mobility_water) * p_grad * cell.DensityWater(refPressure);
						}
						blDiag[1] += faceArea * cur_mobility * f_oil;
						blDiag[3] += faceArea * cur_mobility * f_water;

						rhsBlock[0] -= faceArea * cur_mobility * f_oil * dp;
						rhsBlock[1] -= faceArea * cur_mobility * f_water * dp;

#ifdef DEBUG_SALAMATIN
						CheckForNAN(blDiag);
						CheckForNAN(rhsBlock);
#endif // DEBUG_SALAMATIN

						problem.AddDiagBlock(l, blDiag, rhsBlock);
					}
				}
			}

		// loop through the boundaries parallel to XY-plane
		if (false && (nz > 1))
		{
			for (size_t i = 0; i < nx; i++)
			{
				for (size_t j = 0; j < ny; j++)
				{
					for (size_t k = 0; k < nz; k += nz - 1)
					{
						ptrdiff_t l = grid.ConvertGlobal2Local(nx * ny * k + nx * j + i);

						if (l < 0)
						{
							continue;
						}

						size_t blockSize = problem.NmbrOfNonZerosPerUnitBlock();
						const TwoPhaseFlowCell& cell = grid[l];

						std::vector<double> blDiag = std::vector<double>(blockSize, 0);
						std::vector<double> rhsBlock = std::vector<double>(B, 0);

						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

						double faceArea = hx * hy / (hz / 2);
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_oil, f_water;
						double cur_mobility = cell.MobilityOverall(),
							d_cur_mobility_oil = cell.DerivativeMobilityOil(),
							d_cur_mobility_water = cell.DerivativeMobilityWater();

						if (dp > 0.0)
						{
							f_oil = cell.F_Oil();
							f_water = cell.F_Water();

							blDiag[0] += d_cur_mobility_oil * p_grad;
							blDiag[2] += d_cur_mobility_water * p_grad;
						}
						else
						{
							f_oil = 0.0;
							f_water = 1.0;

							blDiag[2] = (d_cur_mobility_oil + d_cur_mobility_water) * p_grad;
						}
						blDiag[1] += faceArea * cur_mobility * f_oil;
						blDiag[3] += faceArea * cur_mobility * f_water;

						rhsBlock[0] -= cur_mobility * f_oil * p_grad;
						rhsBlock[1] -= cur_mobility * f_water * p_grad;

#ifdef DEBUG_SALAMATIN
						CheckForNAN(blDiag);
						CheckForNAN(rhsBlock);
#endif // DEBUG_SALAMATIN

						problem.AddDiagBlock(l, blDiag, rhsBlock);
					}
				}
			}
		}
	}

} // namespace reservoir_simulator
