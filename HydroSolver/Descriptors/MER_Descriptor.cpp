#include <fstream>
#include "MER_Descriptor.h"
#include "../Data/ExceptionFactory.h"
#include "../Helpers/DebugDump.h"

namespace reservoir_simulator
{
	namespace mer_descriptor
	{
		void MER_Data::CleanMER_record() const
		{
			if (MER_records.size() == 0)
			{
				reservoir_simulator::MessageFactory::MERnoData();
				return;
			}

			size_t i = 0;
			while (
				(i < MER_records.size()) &&
				((MER_records[i].at("oil_v") + MER_records[i].at("water_v")) == 0.0)
				)
				i++;
			if (i == MER_records.size())
			{
				MER_records.clear();
				reservoir_simulator::WarningFactory::MERDataRemoved(itsName);
				return;
			}
			if (i > 0)
			{
				monthGap = MER_records[i].at("time") - MER_records[i - 1].at("time");
				MER_records.erase(MER_records.begin(), MER_records.begin() + i);
				//	LogFileSpace::LogFile::Well_InitialMER_RecordsRemoved(Name(), i);
			}

			int j = (int)MER_records.size() - 1;
			while ((MER_records[j].at("oil_v") + MER_records[j].at("water_v")) == 0.0)
				j--;
			if (j < MER_records.size() - 1)
			{
				//	LogFileSpace::LogFile::Well_LastMER_RecordsRemoved(Name(), (int)MER_records.size() - j);
				MER_records.erase(MER_records.begin() + j + 1, MER_records.end());
			}
		}

		void MER_Data::identify_cur_MER_record(double timeMoment) const
		{
			constexpr double daysInMonth = 30.74; // should be DOUBLE... it was INT

			// the MER_record has been already identified for this time moment
			if (timeMoment == curTime)
				return; // we already know the record ID
			// query time moment is smaller than the curTime moment
			// so, one has search for the proper MER record from the very beginning
			if (timeMoment < curTime)
				monthID = 0; // start search from the very beginning
			curTime = timeMoment; // update (current time)

			if (recordsSize() == 0)
			{ // if there are no records of MER
				// e.g., they all might be with zero overall debit, and were removed
				monthID = -1;
				curFluidDebit = FluidDebit{ 0.0, 0.0 }; // just a hole connecting several reservoir horizons/layers
			//	size_t m = static_cast<size_t>(curTime / daysInMonth);
				itsBeginOfCurPeriod = 0.0;// m* daysInMonth;
				itsEndOfCurPeriod = std::numeric_limits<double>::max(); // (m + 1.0)* daysInMonth;
				its_lastUsed_MER_record = nullptr;
				return;
			}

			// so, there are some real MER records
			if (curTime < firstRecordDate() - monthGap)
			{ // the well did not exist at that time moment
			//	LogFileSpace::LogFile::Well_RequestTimeIsBeforeFirstMER(Name(), (int)curTime, (int)knownExploitationPeriod().first);

				monthID = 0;
				curFluidDebit = FluidDebit{ 0.0, 0.0 };

				size_t m = static_cast<size_t>(curTime / daysInMonth);
				itsBeginOfCurPeriod = 0.0;
				itsEndOfCurPeriod = firstRecordDate() - monthGap;
				its_lastUsed_MER_record = nullptr;
				return;
			}

			for (; monthID < recordsSize(); ++monthID)
				if (curTime < record_time(monthID))
				{
					if (monthID == 0)
					{ //assume monthGap days in the first month of MER records
						itsBeginOfCurPeriod = firstRecordDate() - monthGap;
						itsEndOfCurPeriod = firstRecordDate();
					}
					else
					{
						itsBeginOfCurPeriod = record_time(monthID - 1);
						itsEndOfCurPeriod = record_time(monthID);
					}
					its_lastUsed_MER_record = const_cast<SingleMERrecord*>(&MER_records[monthID]);
					break;
				}
			if (monthID == recordsSize())
			{
				//	LogFileSpace::LogFile::Well_RequestTimeIsAfterLastMER(Name(), (int)curTime, (int)knownExploitationPeriod().second);
				curFluidDebit = FluidDebit{ 0.0, 0.0 };
				size_t m = static_cast<size_t>(curTime / daysInMonth);
				itsBeginOfCurPeriod = std::max(lastRecordDate(), (m + 0.0) * daysInMonth);
				itsEndOfCurPeriod = (m + 1.0) * daysInMonth;
				its_lastUsed_MER_record = nullptr;
				return;
			}

			curFluidDebit = FluidDebit{ lastUsed_MER_record().at("oil_m"),
				lastUsed_MER_record().at("water_m") } /
				MER_record_time_interval(timeMoment) *
				conversion.convert2SI();
		}

		MER_Data::MER_Data(const WellName& name,
			const SingleWell_MER_Data& itsData) :
			itsName{ name },
			MER_records{ itsData }
		{
			// sort MER records with respect to time
			std::sort(
				MER_records.begin(), MER_records.end(),
				[](const SingleMERrecord& a, const SingleMERrecord& b) -> bool
				{
					return a.at("time") < b.at("time");
				}
			);
			// delete records corresponding to the same date
			for (size_t l = 1; l < MER_records.size(); ++l)
			{
				if (MER_records[l - 1].at("time") == MER_records[l].at("time"))
				{// two records at same date
					WarningFactory::MERrecordsAtSameDate(itsName, MER_records[l - 1], MER_records[l]);
					// delete one record
					if (MER_records[l - 1].at("oil_v") + MER_records[l - 1].at("water_v") == 0.0)
					{
						MER_records.erase(MER_records.begin() + l - 1);
						WarningFactory::DuplicateMERrecordWithZeroDebitDeleted();
						--l;
						continue;
					}
					if (MER_records[l].at("oil_v") + MER_records[l].at("water_v") == 0.0)
					{
						MER_records.erase(MER_records.begin() + l);
						WarningFactory::DuplicateMERrecordWithZeroDebitDeleted();
						--l;
						continue;
					}
					{ // sumup two records
						auto r1 = MER_records[l - 1];
						auto r2 = MER_records[l];
						if (r1.at("type") == r2.at("type") &&
							r1.at("is_work") == r2.at("is_work"))
						{
							r1.at("oil_v") += r2.at("oil_v");
							r1.at("water_v") += r2.at("water_v");
							r1.at("oil_m") += r2.at("oil_m");
							r1.at("water_m") += r2.at("water_m");
							r1.at("pump_water") += r2.at("pump_water");
							r1.at("idle_time") += r2.at("idle_time");
							MER_records[l - 1] = r1;
							MER_records.erase(MER_records.begin() + l);

							WarningFactory::DuplicateMERrecordUnited();
							--l;
 						}
						else
							throw std::exception("Two distinct MER records encountered!");
					}
				}
			}
			//	initialize_MER(itsData);
#ifdef GDM_DUMP_DEBUG
			PrintMER();
#endif
		}

#ifdef GDM_DUMP_DEBUG
		void MER_Data::PrintMER() const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_MER_" + Name() + ".txt"));
			sendMER2Stream(myfile);
			myfile.close();
		}

		void MER_Data::PrintMER(const std::string& str0) const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_MER_" + Name() + str0 + ".txt"));
			sendMER2Stream(myfile);
			myfile.close();
		}
#endif

		std::string MER_Data::Name() const
		{
			return itsName;
		}
	} // mer_descriptor
} // reservoir_simulator