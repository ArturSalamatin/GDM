#pragma once
#include <map>
#include <vector>
#include <string>
#include "../defines.h"

namespace reservoir_simulator
{
	namespace mer_descriptor
	{
		class DebitConversion2SI
		{
		protected:
			// units conversion factors
			static constexpr double debitConversion = 1.0; // kg/day 

		public:
			double convert2SI() const { return debitConversion; }

		};

		/// <summary>
		/// This MER_Data class describes a single horizon for a single well. 
		/// It contains a summarized MER among all cell-layers which are part of this horizon
		/// </summary>
		class MER_Data
		{
		public:
			double record_time(const size_t id) const { 
				
				return MERdata()[id].at(L"time"); }
			size_t recordsSize() const { return MERdata().size(); }

		protected:
			// well production is negative when water goes into
			mutable FluidDebit curFluidDebit = FluidDebit{ 0.0, 0.0 }; // current debit of fluids
			mutable SingleMERrecord* its_lastUsed_MER_record = nullptr; // current record
			mutable SingleWell_MER_Data MER_records; // all available records
			WellName itsName; // well name

		protected:
			SingleMERrecord& lastUsed_MER_record() const {
				return (*its_lastUsed_MER_record);
			}
			/// <summary>
			/// Identifies the MER record id corresponding to a certain time momemnt, and sets it internally
			/// </summary>
			/// <param name="timeMoment"></param>
			void identify_cur_MER_record(double timeMoment) const;
		public:
			double MER_record_time_interval(double timeMoment) const { return (endOfCurPeriod(timeMoment) - beginOfCurPeriod(timeMoment)); }
			TimeFrame knownExploitationPeriod() const
			{
				if (recordsSize() > 0)
					return TimeFrame{ firstRecordDate() - monthGap, lastRecordDate() };
				else
					return TimeFrame{ 0.0, 0.0 };
			}
			double firstRecordDate() const { return record_time(0); }
			double lastRecordDate() const { return record_time(recordsSize() - 1); }
			double beginOfCurPeriod(double timeMoment) const { identify_cur_MER_record(timeMoment);  return itsBeginOfCurPeriod; }
			double endOfCurPeriod(double timeMoment) const { identify_cur_MER_record(timeMoment);  return itsEndOfCurPeriod; }

			FluidDebit& debitPartial(double timeMoment) const
			{
				identify_cur_MER_record(timeMoment);
				return curFluidDebit;
			}
			double curDebitOverall() const { return curFluidDebit.oil + curFluidDebit.water; }
			double debitOverall(double timeMoment) const {
				auto debitData = debitPartial(timeMoment);
				return debitData.water + debitData.oil;
			}

		public:
			MER_Data() noexcept = default;
			MER_Data(const WellName& name,
				const SingleWell_MER_Data& itsData) noexcept;
			/*void initialize_MER(const SingleWell_MER_Data& itsData)
			{
				MER_records = itsData;
			}*/
			void CleanMER_record() const;

			const SingleWell_MER_Data& MERdata() const
			{
				return MER_records;
			}

			std::string Name() const;

			void PrintMER() const;
			void PrintMER(const std::string& str) const;

		protected:
			mutable double itsBeginOfCurPeriod = std::numeric_limits<double>::min();
			mutable double itsEndOfCurPeriod = std::numeric_limits<double>::max();
			mutable double monthGap = 30.74; // the number of days in the first MER record
			mutable long int monthID = 0;
			mutable double curTime = std::numeric_limits<double>::max();

			template<typename stream>
			void sendMER2Stream(stream& o) const
			{
				o << "Well " << Name() << " MER data:\n";
				o << std::setw(8) << std::left 
					<< "time"
					<< std::setw(10) << std::left
					<< "oil_v"
					<< std::setw(10) << std::left
					<< "water_v"
					<< std::setw(12) << std::left
					<< "oil_m"
					<< std::setw(12) << std::left
					<< "water_m"
					<< std::setw(12) << std::left
					<< "pump_water"
					<< std::setw(11) << std::left
					<< "idle_time"
					<< std::setw(5) << std::left
					<< "type"
					<< std::setw(8) << std::left
					<< "is_work"
					<< std::endl;

				for (const auto& m : MER_records)
				{
					o << std::setw(8) << std::left
						<< m.at(L"time")
						<< std::setw(10) << std::left
						<< m.at(L"oil_v")
						<< std::setw(10) << std::left
						<< m.at(L"water_v")
						<< std::setw(12) << std::left
						<< m.at(L"oil_m")
						<< std::setw(12) << std::left
						<< m.at(L"water_m")
						<< std::setw(12) << std::left
						<< m.at(L"pump_water")
						<< std::setw(11) << std::left
						<< m.at(L"idle_time")
						<< std::setw(5) << std::left
						<< m.at(L"type")
						<< std::setw(8) << std::left
						<< m.at(L"is_work")
						<< std::endl;
				}
			}

		private:

#pragma region unitsConversionFactors
		protected:
			// units conversion factors
			const DebitConversion2SI conversion; // = 1.0;// / (3600 * 24); // from m^3/day to m^3/s (SI units)
#pragma endregion
		};
	} // mer_descriptor
} // reservoir_simulator
