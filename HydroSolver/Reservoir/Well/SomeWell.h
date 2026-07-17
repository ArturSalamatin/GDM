#pragma once
#include "../../stdafx.h"
#include "../../defines.h"

#include "SetOfPoints.h"
#include "WellTrajectory.h"
#include "../../Descriptors/MER_Descriptor.h"
#include "../../Data/ExceptionFactory.h"

namespace reservoir_simulator
{
	namespace wells
	{
		using SingleMERrecord = std::map<WellName, float>;
		using SingleWell_MER_Data = std::vector<SingleMERrecord>;

		using CellNumericalData =
			std::tuple<
			std::vector<size_t>,
			std::vector<std::vector<double>>,
			std::vector<std::vector<double>>
			>; // a tuple of data to add to A-block and RHS-block

		class WellEnvironment : public WellTrajectory
		{
		public:
			WellEnvironment() noexcept;
			WellEnvironment(const WellPosition& intersectionCoords,
				const std::vector<const cell::TwoPhaseFlowCell*>& cells_,
				const std::vector<size_t>& itsLocalIDs);

		protected:
			std::vector<double> factor; // l*2*pi/log(rApp/rWell)
			std::vector<size_t> ItsCurCellIDs; // index of cells-vector elements where perforation length l > 0 
			std::vector<size_t> ItsLocalIDs; // IDs of these cells in local [l] notation of reservoir mesh
			std::vector<size_t> ItsCurLocalIDs; // elements of ItsLocalIDs where current perforation length l > 0
			const double f_oil_in = 0.0, f_water_in = 1.0; // if production is negative, the water goes into the reservoir
			double refWellPressure; // well pressure
			std::vector<double> productions, P_Well; // an element per perforation

			double F_OilWellBalance; // volume fraction of oil in the well
			double P_Reservoir(size_t l) const;
			double F_Oil(size_t l) const;
			double OverallMobility(size_t l) const;
			double DerivativeOverallMobility(size_t l) const;
			double Derivative_F_Oil(size_t l) const;
			double CellDensityOil(size_t l) const;
			double CellDensityWater(size_t l) const;
			void SetProductions();
			void SetWellPressure(double P);

		public:
			/// <summary>
			/// IDs in the cell std::vector where current perforation length > 0
			/// </summary>
			/// <returns></returns>
			const std::vector<size_t>& CurCellIDs() const;
			/// <summary>
			/// IDs in the cell std::vector where current perforation length > 0
			/// </summary>
			/// <returns></returns>
			const std::vector<size_t>& CurLocalIDs() const;
		};

		class SomeWell :public WellEnvironment
		{
		protected:
			WellName itsName;
			std::string itsGUID;
			double itsWellRadius = 1E-1; // in meters
			double itsApparentWellRadius;
			const int B = 2;
		protected:
			std::unique_ptr<const mer_descriptor::MER_Data> mer_Data;
			double NextWellJobInstance(double timeMoment) const;
			double NextProductionFrameStart(double timeMoment) const 
			{
				return mer_Data->endOfCurPeriod(timeMoment);
			}

		public:
			double TimeToNextMomemnt(double curTime) const;
			const WellName& NameWide() const;
			std::string Name() const;
			std::string Guid() const;
			double WellRadius() const;

			mer_descriptor::TimeFrame KnownExploitationPeriod() const;
			double FirstProductionDate() { return KnownExploitationPeriod().start; }
			double LastProductionDate() { return KnownExploitationPeriod().end; }

			// pair<oil, water> debit, m^3/s, during the month
			// mer_descriptor::FluidDebit DebitPartial(double timeMoment) const { return mer_Data->debitPartial(timeMoment); }
			// overall debit, m^3/s
			double DebitOverall(double timeMoment) const { return mer_Data->debitOverall(timeMoment); }
			double CurOverallDebit() const { return mer_Data->curDebitOverall(); }
			double CurOilDebit_Num() const;
			double CurWaterDebit_Num() const;

			double ApparentWellRadius() const { return itsApparentWellRadius; }

#pragma region UpdateCurrentValues
			const std::vector<double>& CurPerforations() const;

			const set_of_points::PerforationsOfWell&
				PerforationConfiguration() const;

			double FirstPerforationDate() const;

			double LastPerforationDate() const;


			std::pair<std::vector<size_t>, std::vector<double>>
				PerforationLengths(double timeMoment) const;

			std::vector<double>
				PerforationLengthOverall(double timeMoment) const;

			double PerforationLengthTotal(double timeMoment) const;

			/// <summary>
			/// In case the first well perforation is later than the first production date, the first perforation is moved to an earlier time moment
			/// to match the first production date
			/// </summary>
			void BringFirstPerforationToFirstMER();

			/// <summary>
			/// In case the last well perforation is earlier than the first production date, the last perforation is moved to a later time moment
			/// to match the last production date
			/// </summary>
			void BringLastPerforationToLastMER();

		protected:
			std::vector<double> ItsCurPerforationLengths;
			double itsFirstPerforationDate;
			//			size_t cellID_FirstPerforation;
#pragma endregion

		public:
			double CurrentTime() { return currentTime; }
			void UpdateWellState(double timeMoment);

		protected:
			double currentTime;
			// remain constant in time
			set_of_points::PerforationsOfWell ItsAccumulatedPerforations;
#pragma region NumericalSection
		protected:
			// eq data
			std::vector<std::vector<double>> rhsPerPerforation;
			std::vector<std::vector<double>> matrixBlockPerPerforation;
#pragma endregion

#pragma region WellEnvironment
		protected:
			void BalanceOil();
		public:
			size_t NmbrOfOpenedCells()
			{
				return CurCellIDs().size();
			}
			SomeWell() noexcept;
			SomeWell(const WellName& name, const std::string& guid,
				const WellPosition& intersectionCoords,
				std::unique_ptr<const mer_descriptor::MER_Data>&& mer,
				const set_of_points::PerforationsOfWell& perforationsOfWell,
				const std::vector<const cell::TwoPhaseFlowCell*>& cells_,
				const std::vector<size_t>& itsLocalIDs, double itsAppWellRadius);

			void initialize_MER_data(
				std::unique_ptr<const mer_descriptor::MER_Data>&& mer);

			virtual  CellNumericalData AddWellToMatrix(
				double nextTimeMoment) = 0;


#ifdef GDM_DUMP_DEBUG
			void PrintWell() const;
			std::string OutputPath() const;
			void PrintWellMERDebit() const;
#endif

			template<typename stream>
			void PrintWellDebitLength(stream& myfile) const
			{
				bool f = false;
				auto& o = myfile;
				o	<< "# Well " << Name() << " data:\n";
				o	<< std::setw(8) << std::left
					<< "time, d"
					<< std::setw(12) << std::left
					<< "debit, kg/d"
					<< std::setw(10) << std::left
					<< "Length, m";

				size_t layers_count = ItsLocalIDs.size();
				for (size_t j = 0; j < layers_count; ++j)
					o	<< std::setw(11) << std::left
						<< "length" + std::to_string(j) + ", m";

				o << std::setw(13) << std::left
					<< "proper_state";

				o	<< std::setw(11) << std::left
					<< "oil_v, m^3"
					<< std::setw(13) << std::left
					<< "water_v, m^3"
					<< std::setw(10) << std::left
					<< "oil_m, kg"
					<< std::setw(12) << std::left
					<< "water_m, kg"
					<< std::setw(14) << std::left
					<< "pump_water, t"
					<< std::setw(13) << std::left
					<< "idle_time, h"
					<< std::setw(5) << std::left
					<< "type"
					<< std::setw(8) << std::left
					<< "is_work"
					<< std::endl;

				for (const auto& mer : (*mer_Data).MERdata())
				{
					double time = mer.at("time");
					auto length = PerforationLengthOverall(time);
					double debit = DebitOverall(time);
					// whether well operates, i.e., produces or injects fluid
					bool isOperating = debit != 0.0;
					// overall perforation length
					double l = std::accumulate(length.begin(), length.end(), 0.0);
					// we have perforations when debit is non-zero
					bool isPerforated = !(isOperating && (l == 0.0));

					myfile 
						<< std::setw(8) << time    // print initial moment of the time frame
						<< std::setw(12) << debit   // print well debit
						<< std::setw(10) << l;      // print overall length in all layers
					for (size_t i = 0; i < length.size(); ++i)
						myfile << std::setw(11) << length[i]; // print all partial lengths of perforations

					// whether perforations correspond to debit or not
					myfile << std::setw(13) << std::left 
						<< isPerforated ? "yes" : "no";

					{ // print MER data
						const auto& m = mer;
						o << std::setw(11) << std::left
							<< m.at("oil_v")
							<< std::setw(13) << std::left
							<< m.at("water_v")
							<< std::setw(10) << std::left
							<< m.at("oil_m")
							<< std::setw(12) << std::left
							<< m.at("water_m")
							<< std::setw(14) << std::left
							<< m.at("pump_water")
							<< std::setw(13) << std::left
							<< m.at("idle_time")
							<< std::setw(5) << std::left
							<< m.at("type")
							<< std::setw(8) << std::left
							<< m.at("is_work");
					}

					myfile << std::endl;
					if (!isPerforated)
					{
						std::cout << "<<<<<  " << NameWide() << " at date " << time << std::endl;
						f = true;
					}
				}
			}
		};
	}
}