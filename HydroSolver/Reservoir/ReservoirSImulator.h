#pragma once
#include "../stdafx.h"

#include "NumericalParameters.h"

#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"
#include "../Solver/Grids/DevelopedHorizon.h"

#include "../Anomaly/FlowField/FlowField.h"

namespace reservoir_simulator
{
	struct SolverProfile
	{
		size_t n_time_steps = 0;
		size_t n_newton_iters = 0;
		size_t n_amg_solves = 0;
		size_t n_wasted_trials = 0;
		size_t total_amg_iters = 0;
	};

	class wells::SomeWell;
//	class RawHorizon;
	class DevelopedHorizon;

	using namespace cell;
	using namespace grid;
	using namespace linear_problem;

	/// <summary>
	/// A tuple of data to add to A-block and RHS-block
	/// </summary>
	using CellNumericalData = 
		std::tuple<std::vector<size_t>, 
		std::vector<std::vector<double>>, 
		std::vector<std::vector<double>>> ; 

	class ReservoirSimulator
	{
	public:
		NumericalParameters numPrm;

		const std::map<WellName, wells::SomeWell*>& GetWells() const;
	public:
//		double numSchemeTau;
		double RefPressure = 100.0;

		OilField Grid;
		size_t ActiveCellsNmbr;
		LinearProblem MyProblem;
		std::map<WellName, wells::SomeWell*> Wells;

		size_t nz() const;

//		const std::wstring folderName = L"MatLab\\";

		bool doSave;
		std::wstring saveWaterSaturation_fName;
		// ios_base::app -- append to the end of file
		void PrintReservoirState(std::ios_base::openmode mode) const;
		void PrintPlanarMesh(const std::wstring fName) const;
		void PrintWellCoords() const;

		double prevOil, curOil, accumOil, accumOilOutFlux, accumDebet;
		double prevWater, curWater, accumWater, accumWaterOutFlux, accumWaterDebet;
		double curTime;

		std::vector<phasePortrait::SomeFlowField> flowFields;

		SolverProfile solverProfile_;
	public:
		phasePortrait::SomeFlowField::SequencePtr
			GetFlowFieldsPtr(double multiplier) const;

		const SolverProfile& GetSolverProfile() const { return solverProfile_; }

		// solves the problem sarting with the initial condition
		double Solve(const std::vector<double>& timeMoments);

		void AddFlowFieldSnapShot();


		void SaveFlowField2File(const std::wstring& configPath, const std::wstring& fileName);


		void SaveFlowField2File_bin(const std::wstring& configPath, const std::wstring& fileName,
			double saturation_date, double startDate, double endDate, int frameCount);


		void SaveSaturationPressure_bin(const std::wstring& configPath, const std::wstring& fileName,
			double saturation_date, double startDate, double endDate, int frameCount);

		void LoadFlowFieldFromFile(const std::wstring& fileName);

		void LoadFlowFieldFromFile_bin(const std::wstring& fileName);

		void MassBalance(double loc_tau);

		void PerformNewtonLoop(double loc_tau, double nextTimeMoment);

		bool UpdateGrid();

		// compute a single iteration for a given microscale time-step
		void SingleIteration(double loc_tau, double nextTimeMoment);

		void AssembleMyProblem(double loc_tau, double nextTimeMoment);

		void fillMatrixBlockRow(size_t l, double loc_tau);

		void CheckForNAN(std::vector<double> someArr)
		{
			for (auto& a : someArr)
				if (!isfinite(a))
					throw std::exception("bad well rhs");
		}

		void AccountForBoundaryConditions()
		{
			size_t nx = Grid.Nx(), ny = Grid.Ny(), nz = Grid.Nz();

			// loop through the boundaries parallel to YZ-plane
			if(nx>1)
//#ifdef	USE_PARALLEL
//#pragma omp parallel for
//#endif
				for (size_t j = 0; j < ny; j++)
				{
					for (size_t i = 0; i < nx; i += nx - 1)
					{
						for (size_t k = 0; k < nz; k++)
						{
							long int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i); // local index of the cell, inactive cells are not counted

							if (l < 0)
							{// skip cells that are not active
								continue;
							}

							size_t blockSize = MyProblem.NmbrOfNonZerosPerUnitBlock();
							const TwoPhaseFlowCell& cell = Grid[l];
							//	const std::vector<TwoPhaseFlowCell*> neighbourCells = Grid.GetNeighboursPointer(l);
							//	const std::vector<double>& commonEdgeArea = Grid.CommonEdgeArea(l);

							std::vector<double> blDiag = std::vector<double>(blockSize, 0);
							std::vector<double> rhsBlock = std::vector<double>(B, 0);

							const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

							double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

							double dp = cell.P() - refPressure;
							double faceArea = hy * hz / (hx / 2); // face area over the cell thickness
							double p_grad = faceArea * dp;
							double f_oil, f_water; // volumetric fraction of the flux per oil and water respectively
							double cur_mobility = cell.MobilityOverall(),
								d_cur_mobility_oil = cell.DerivativeMobilityOil(),
								d_cur_mobility_water = cell.DerivativeMobilityWater();

							if (dp > 0.0)
							{// the flow is out of the cell
								f_oil = cell.F_Oil() * cell.DensityOil();
								f_water = cell.F_Water() * cell.DensityWater();

								/*(derivative of the backwards coefficient) x (harmonic mean = conductivity) x (pressure gradient -- includes faceArea)*/
								// oil section
								blDiag[0] += d_cur_mobility_oil * p_grad * cell.DensityOil();
								// water section
								blDiag[2] += d_cur_mobility_water * p_grad * cell.DensityWater();
							}
							else
							{// the flow is into the cell
								// assume that pure water goes in
								f_oil = 0.0;
								f_water = 1.0 * cell.DensityWater(refPressure);
								/*(derivative of the backwards coefficient) x (harmonic mean = conductivity) x (pressure gradient -- includes faceArea)*/
								// oil section
								//empty since f_oil = 0.0
								// water section
								blDiag[2] = (d_cur_mobility_oil + d_cur_mobility_water) * p_grad * cell.DensityWater(refPressure);
							}
							// derivative of pressure :: coefficient for the current active cell, sits on the diagonal of the block
							blDiag[1] += faceArea * cur_mobility * f_oil;
							blDiag[3] += faceArea * cur_mobility * f_water;

							rhsBlock[0] -= faceArea * cur_mobility * f_oil * dp;
							rhsBlock[1] -= faceArea * cur_mobility * f_water * dp;


#ifdef DEBUG_SALAMATIN
							CheckForNAN(blDiag);
							CheckForNAN(rhsBlock);
#endif // DEBUG_SALAMATIN

							MyProblem.AddDiagBlock(l, blDiag, rhsBlock);
						}
					}
				}
			// loop through the boundaries parallel to XZ-plane
			if(ny>1)
//#ifdef	USE_PARALLEL
//#pragma omp parallel for
//#endif
				for (int i = 0; i < nx; i++)
				{
					for (int j = 0; j < ny; j += ny - 1)
					{
						for (int k = 0; k < nz; k++)
						{
							int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i); // local index of the cell, inactive cells are not counted

							if (l < 0)
							{// skip cells that are not active
								continue;
							}

							int blockSize = MyProblem.NmbrOfNonZerosPerUnitBlock();
							const TwoPhaseFlowCell& cell = Grid[l];
							//	const std::vector<TwoPhaseFlowCell*> neighbourCells = Grid.GetNeighboursPointer(l);
							//	const std::vector<double>& commonEdgeArea = Grid.CommonEdgeArea(l);

							std::vector<double> blDiag = std::vector<double>(blockSize, 0);
							std::vector<double> rhsBlock = std::vector<double>(B, 0);

							const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();


							double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

							double faceArea = hx * hz / (hy / 2); // face area over the cell thickness
							double dp = cell.P() - refPressure;
							double p_grad = faceArea * dp;
							double f_oil, f_water; // volumetric fraction of the flux per oil and water respectively
							double cur_mobility = cell.MobilityOverall(),
								d_cur_mobility_oil = cell.DerivativeMobilityOil(),
								d_cur_mobility_water = cell.DerivativeMobilityWater();

							if (dp > 0.0)
							{// the flow is out of the cell
								f_oil = cell.F_Oil() * cell.DensityOil();
								f_water = cell.F_Water() * cell.DensityWater();

								/*(derivative of the backwards coefficient) x (harmonic mean = conductivity) x (pressure gradient -- includes faceArea)*/
								// oil section
								blDiag[0] += d_cur_mobility_oil * p_grad * cell.DensityOil();
								// water section
								blDiag[2] += d_cur_mobility_water * p_grad * cell.DensityWater();
							}
							else
							{// the flow is into the cell
								// assume that pure water goes in
								f_oil = 0.0;
								f_water = 1.0 * cell.DensityWater(refPressure);
								/*(derivative of the backwards coefficient) x (harmonic mean = conductivity) x (pressure gradient -- includes faceArea)*/
								// oil section
								//empty since f_oil = 0.0
								// water section
								blDiag[2] = (d_cur_mobility_oil + d_cur_mobility_water) * p_grad * cell.DensityWater(refPressure);
							}
							// derivative of pressure :: coefficient for the current active cell, sits on the diagonal of the block
							blDiag[1] += faceArea * cur_mobility * f_oil;
							blDiag[3] += faceArea * cur_mobility * f_water;

							rhsBlock[0] -= faceArea * cur_mobility * f_oil * dp;
							rhsBlock[1] -= faceArea * cur_mobility * f_water * dp;


#ifdef DEBUG_SALAMATIN
							CheckForNAN(blDiag);
							CheckForNAN(rhsBlock);
#endif // DEBUG_SALAMATIN

							MyProblem.AddDiagBlock(l, blDiag, rhsBlock);
						}
					}
				}

			// loop through the boundaries parallel to XY-plane
			if (false && (nz > 1))
			{
//#ifdef	USE_PARALLEL
//#pragma omp parallel for
//#endif
				for (int i = 0; i < nx; i++)
				{
					for (int j = 0; j < ny; j++)
					{
						for (int k = 0; k < nz; k += nz - 1)
						{
							int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i); // local index of the cell, inactive cells are not counted

							if (l < 0)
							{// skip cells that are not active
								continue;
							}

							int blockSize = MyProblem.NmbrOfNonZerosPerUnitBlock();
							const TwoPhaseFlowCell& cell = Grid[l];
							//	const std::vector<TwoPhaseFlowCell*> neighbourCells = Grid.GetNeighboursPointer(l);
							//	const std::vector<double>& commonEdgeArea = Grid.CommonEdgeArea(l);

							std::vector<double> blDiag = std::vector<double>(blockSize, 0);
							std::vector<double> rhsBlock = std::vector<double>(B, 0);

							const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();


							double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

							double faceArea = hx * hy / (hz / 2); // face area over the cell thickness
							double dp = cell.P() - refPressure;
							double p_grad = faceArea * dp;
							double f_oil, f_water; // volumetric fraction of the flux per oil and water respectively
							double cur_mobility = cell.MobilityOverall(),
								d_cur_mobility_oil = cell.DerivativeMobilityOil(),
								d_cur_mobility_water = cell.DerivativeMobilityWater();

							if (dp > 0.0)
							{// the flow is out of the cell
								f_oil = cell.F_Oil();
								f_water = cell.F_Water();

								/*(derivative of the backwards coefficient) x (harmonic mean = conductivity) x (pressure gradient -- includes faceArea)*/
								// oil section
								blDiag[0] += d_cur_mobility_oil * p_grad;
								// water section
								blDiag[2] += d_cur_mobility_water * p_grad;
							}
							else
							{// the flow is into the cell
								// assume that pure water goes in
								f_oil = 0.0;
								f_water = 1.0;
								/*(derivative of the backwards coefficient) x (harmonic mean = conductivity) x (pressure gradient -- includes faceArea)*/
								// oil section
								//empty since f_oil = 0.0
								// water section
								blDiag[2] = (d_cur_mobility_oil + d_cur_mobility_water) * p_grad;
							}
							// derivative of pressure :: coefficient for the current active cell, sits on the diagonal of the block
							blDiag[1] += faceArea * cur_mobility * f_oil;
							blDiag[3] += faceArea * cur_mobility * f_water;

							rhsBlock[0] -= cur_mobility * f_oil * p_grad;
							rhsBlock[1] -= cur_mobility * f_water * p_grad;


#ifdef DEBUG_SALAMATIN
							CheckForNAN(blDiag);
							CheckForNAN(rhsBlock);
#endif // DEBUG_SALAMATIN

							MyProblem.AddDiagBlock(l, blDiag, rhsBlock);
						}
					}
				}
			}
		}

		// continues solution process once the solver stopped
		void Continue(const std::vector<double>& additionalTimeMoments)
		{

		}

		std::vector<double> GetOilSaturationField() const;

		std::vector<double> GetWaterSaturationField() const;

		std::vector<double> GetPressureField() const;

		std::vector<double> GetOverallBalance() const;
				
		void printPointVariable(const std::wstring fName,
			const std::vector<double>& data,
			std::ios_base::openmode mode = std::ios_base::out) const;

		void printFieldVariable(const std::wstring fName,
			const std::vector<double>& data,
			std::ios_base::openmode mode = std::ios_base::out) const;

		void pressure_json() const;//(const std::vector<double>& s, const std::vector<double>& p)

		ReservoirSimulator() noexcept;
		
		ReservoirSimulator(
			const NumericalParameters& numPrm_,
			const DevelopedHorizon& horizon,
			const OilPhaseProperty& oil, const WaterPhaseProperty& water,
			const OtherProperties& other_properties) noexcept;

		bool is_flow_field_empty() const;

		void AddWell_FixedProduction(const WellName& name,
			const mer_descriptor::SingleWell_MER_Data& well_data,
			const WellJobs& perforationsOfWell,
			const WellPosition& intersecCoords,
			const GridBounds& reservoir_bounds,
			const BlockSize& block_size,
			double appRadiusWell);

		virtual ~ReservoirSimulator();

		double OilTotal() const;
		double WaterTotal() const;
		double OilDebitTotal() const;
		double WaterDebitTotal() const;
		double OilContourFlux() const;
		double WaterContourFlux() const;

		std::tuple<std::vector<std::vector<std::vector<double>>>, std::vector<std::vector<std::vector<double>>>,
			std::vector<std::vector<std::vector<double>>>, std::vector<std::vector<std::vector<double>>>>
			OverallFluxes() const;
	};
}// reservoir_simulator