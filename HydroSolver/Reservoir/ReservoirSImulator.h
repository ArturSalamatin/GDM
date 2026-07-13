#pragma once
#include "../stdafx.h"

#include "NumericalParameters.h"

#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"
#include "../Solver/Grids/DevelopedHorizon.h"

#include "../Anomaly/FlowField/FlowField.h"
#include "MassBalanceTracker.h"
#include "NewtonSolver.h"

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

		std::vector<phasePortrait::SomeFlowField> flowFields;

		SolverProfile solverProfile_;
		MassBalanceTracker balance_tracker_;
		NewtonSolver newton_solver_;
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
			const OtherProperties& other_properties,
			Layout layout = Layout::InterleavedPSw) noexcept;

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