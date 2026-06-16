#include "SomeWell.h"

namespace reservoir_simulator
{
	namespace wells
	{
		WellEnvironment::WellEnvironment() noexcept = default;

		WellEnvironment::WellEnvironment(const WellPosition& intersectionCoords,
			const std::vector<const cell::TwoPhaseFlowCell*>& cells_,
			const std::vector<size_t>& itsLocalIDs) noexcept :
			WellTrajectory{ intersectionCoords, cells_ }, ItsLocalIDs{ itsLocalIDs } 
		{ }

		double WellEnvironment::P_Reservoir(size_t l) const
		{
			return cells[ItsCurCellIDs[l]]->P();
		}
		double WellEnvironment::F_Oil(size_t l) const
		{
			return (productions[l] < 0) ? F_OilWellBalance : cells[ItsCurCellIDs[l]]->F_Oil();
		}
		double WellEnvironment::OverallMobility(size_t l) const
		{
			return cells[ItsCurCellIDs[l]]->MobilityOverall();
		}
		double WellEnvironment::DerivativeOverallMobility(size_t l) const
		{
			return cells[ItsCurCellIDs[l]]->DerivativeMobilityOil() + cells[ItsCurCellIDs[l]]->DerivativeMobilityWater();
		}
		double WellEnvironment::Derivative_F_Oil(size_t l) const
		{
			return cells[ItsCurCellIDs[l]]->Derivative_F_Oil();
		}
		void WellEnvironment::SetProductions()
		{
			for (size_t l = 0; l < P_Well.size(); l++)
				productions[l] = factor[l] * OverallMobility(l) *
				(P_Reservoir(l) - P_Well[l]); // length of perforation is included in the factor
		}
		void WellEnvironment::SetWellPressure(double P)
		{
			refWellPressure = P;
			for (size_t l = 0; l < P_Well.size(); ++l)
			{
				P_Well[l] = refWellPressure;
			}
		}

		const std::vector<size_t>& WellEnvironment::CurCellIDs() const
		{
			return ItsCurCellIDs;
		}
		const std::vector<size_t>& WellEnvironment::CurLocalIDs() const
		{
			return ItsCurLocalIDs;
		}


		const WellName& SomeWell::NameWide() const { return itsName; }
		std::string SomeWell::Name() const 
		{
			std::string str;
			size_t size;
			str.resize(NameWide().length());
			wcstombs_s(&size, &str[0], str.size() + 1, NameWide().c_str(), NameWide().size());
			return str;; }
		std::wstring SomeWell::Guid() const { return itsGUID; }
		double SomeWell::WellRadius() const { return itsWellRadius; }

		mer_descriptor::TimeFrame SomeWell::KnownExploitationPeriod() const
		{
			return mer_Data->knownExploitationPeriod();
		}

		void SomeWell::PrintWellMERDebit() const
		{
			std::ofstream myFile{ OutputPath() + "//well_MER_debit.txt"};
			PrintWellDebitLength(myFile);
			myFile.close();
		}


		void SomeWell::PrintWell() const
		{
			namespace fs = std::filesystem;
			fs::create_directories(OutputPath());
			PrintWellMERDebit();
		}

		std::string SomeWell::OutputPath() const
		{
			return "WellTestData//" + Name();
		}



		SomeWell::SomeWell() noexcept = default;
		SomeWell::SomeWell(const WellName& name, const std::wstring& guid,
			const WellPosition& intersectionCoords,
			std::unique_ptr<const mer_descriptor::MER_Data>&& mer,
			const set_of_points::PerforationsOfWell& perforationsOfWell,
			const std::vector<const cell::TwoPhaseFlowCell*>& cells_,
			const std::vector<size_t>& itsLocalIDs, double itsAppWellRadius) noexcept
			: WellEnvironment(intersectionCoords, cells_, itsLocalIDs),
			itsName(name), itsGUID(guid), itsApparentWellRadius(itsAppWellRadius),
			ItsAccumulatedPerforations(perforationsOfWell)
		{
			reservoir_simulator::MessageFactory::WellInitializationStarted(NameWide());
			initialize_MER_data(std::move(mer));

			// for all perfs in a particular layer_id
			for (const auto& [layer_id, perfs] : PerforationConfiguration())
			{
				// save real job dates, that should be moved to relevant MER dates
				const auto& newJobDates = perfs.AllJobMoments();
				// identify time moments of MER recors enclosing the jobDate
				std::vector<std::pair<size_t, std::pair<double, double>>> jobIntervals; 
				// the very first operation is at 0.0 time moment, and we skip it (i > 0)
				// the well exists, but closed since that time
				for (size_t i = 1; i < perfs.AllJobMoments().size(); i++)
				{
					// we only search for modern well operations, i > 0
					// the two closest dates from MER records enclosing the well operation date
					auto currentPeriodStart = mer_Data->beginOfCurPeriod(newJobDates[i]);
					auto nextPeriodStart = mer_Data->endOfCurPeriod(newJobDates[i]);
					if (currentPeriodStart > newJobDates[i] ||
						nextPeriodStart < newJobDates[i])
						// the enclosing dates must enclose the well operation date
						throw std::runtime_error("Wrong determination of well operation enclosing dates.");
					if (currentPeriodStart == 0.0)
						// there are no real records, 
						// thus, there is no reason to move corresponding well operation date.
						// 
						// Earlier implementation assumed that there are always some records with non-zero MER:
						// currentPeriodStart = nextPeriodStart - 30;
						// Current implementation:
						// just skip this operation date
						continue;
					// otherwise, push the enclosing dates
					jobIntervals.push_back(std::make_pair(i, std::make_pair(currentPeriodStart, nextPeriodStart)));
				}
				const_cast<set_of_points::AccumulatedPerforations&>(perfs).AveragePerforationsOut(jobIntervals);
			}
			BringFirstPerforationToFirstMER();
			BringLastPerforationToLastMER();
			reservoir_simulator::MessageFactory::WellInitializationDone(NameWide());

			PrintWell();
		}

		void SomeWell::initialize_MER_data(std::unique_ptr<const mer_descriptor::MER_Data>&& mer)
		{
			reservoir_simulator::MessageFactory::MERInitializationStarted(NameWide());

			mer_Data = std::move(mer);

			reservoir_simulator::MessageFactory::WellOverallTimeFrame(NameWide(), KnownExploitationPeriod());
			mer_Data->CleanMER_record();
			reservoir_simulator::MessageFactory::WellOverallTimeFrame(NameWide(), KnownExploitationPeriod());
			mer_Data->PrintMER("inside_well");
			reservoir_simulator::MessageFactory::MERInitializationDone(NameWide());
		}

		double SomeWell::CurOilDebit_Num() const
		{
			auto overallDebit = CurOverallDebit();
			if (overallDebit > 0.0)
				return overallDebit * F_OilWellBalance;
			else
				return overallDebit * f_oil_in;;
		}
		double SomeWell::CurWaterDebit_Num() const
		{
			auto overallDebit = CurOverallDebit();
			if (overallDebit > 0.0)
				return overallDebit * (1 - F_OilWellBalance);
			else
				return overallDebit * (1 - f_oil_in);;
		}

		const std::vector<double>& 
			SomeWell::CurPerforations() const
		{
			return ItsCurPerforationLengths;
		}

		const set_of_points::PerforationsOfWell&
			SomeWell::PerforationConfiguration() const
		{
			return ItsAccumulatedPerforations;
		}

		double SomeWell::FirstPerforationDate() const
		{
			double minDate = std::numeric_limits<double>::max();
			for (const auto& [layer, perf] : PerforationConfiguration())
				minDate = std::min(minDate, perf.EarliestJobDate());
			return minDate;
		}

		double SomeWell::LastPerforationDate() const
		{
			double maxDate = std::numeric_limits<double>::min();
			for (const auto& [layer, perf] : PerforationConfiguration())
				maxDate = std::max(maxDate, perf.LatestJobDate());
			return maxDate;
		}

		double SomeWell::NextWellJobInstance(double timeMoment) const
		{
			double nextTime = std::numeric_limits<double>::max();
			for (const auto& [layer, perf] : PerforationConfiguration())
				nextTime = std::min(nextTime, perf.NextJobMoment(timeMoment));
			return nextTime;
		}

		std::pair<std::vector<size_t>, std::vector<double>>
			SomeWell::PerforationLengths(double timeMoment) const
		{
			std::vector<double> lengths;
			std::vector<size_t> ids;

			for (const auto& [layer, perf] : PerforationConfiguration())
			{
				double l = perf.getPerforations(timeMoment).TotalLength();
				if (l > 0)
				{
					lengths.push_back(l);
					ids.push_back(layer);
				}
			}

			// an error occurs if Debit is not zero at zero perforation length
			if (lengths.empty() && (DebitOverall(timeMoment) != 0.0))
			{
			//	throw std::exception("TODO: class_SomeWell::method_PerforationLengths");


				//const set_of_points::SetOfPerforations& future_perforations{ 
				//	PerforationConfiguration()[0].getPerforations_future(timeMoment) };
				//double nearest_time = future_perforations.curTime();
				//

				//for (int i = 1; i < PerforationConfiguration().size(); i++)
				//{
				//	const auto& fp = PerforationConfiguration()[0].second.getPerforations_future(timeMoment);
				//	if (fp.curTime() < nearest_time)
				//	{
				//		nearest_time = fp.curTime();
				//		future_perforations = fp;
				//	}
				//}

				//for (int i = 0; i < PerforationConfiguration().size(); i++)
				//{
				//	double l = PerforationConfiguration()[i].second.getPerforations(nearest_time).TotalLength();
				//	if (l > 0)
				//	{
				//		lengths.push_back(l);
				//		ids.push_back(PerforationConfiguration()[i].first);
				//	}
				//}

				//LogFileSpace::LogFile::WriteLog(L"class_SomeWell", L"method_PerforationLengths", L"warning", L">>>>>Well " + Name() +
				//	L" had zero overall length of perforations at time momemnt "
				//	+ std::to_wstring((int)timeMoment) + L"." + L"It is assumed that the well is perforated according to the first date of known perforation");

			}
			return std::make_pair(ids, lengths);
		}

		std::vector<double> SomeWell::PerforationLengthOverall(double timeMoment) const
		{
			auto [layer_id, length] = PerforationLengths(timeMoment);

			std::vector<double> l(ItsLocalIDs.size(), 0.0);
			double sum = 0;

			for (size_t i = 0; i < layer_id.size(); i++)
				l[layer_id[i]] = length[i];

			return l;
		}

		double SomeWell::PerforationLengthTotal(double timeMoment) const
		{
			auto [layer_id, length] = PerforationLengths(timeMoment);
			double sum = 0;

			for (auto n : length)
				sum += n;

			return sum;
		}

		void SomeWell::BringFirstPerforationToFirstMER()
		{
			// loop through all layers to find the earliest perforation date
			double perfTime = std::numeric_limits<double>::max();
			for (const auto& [layer_id, perf] : PerforationConfiguration())
				if (perfTime > perf.EarliestJobDate())
					perfTime = perf.EarliestJobDate();

			double date = FirstProductionDate();
			// date == 0.0 if there are no MER records
			// but the well may still be perforated
			// connecting various cells of the grid
			if (date > 0.0 && perfTime > date)
			{// the first perforation is later than the first MER
				// then shift this earliest perforation date
				for (auto& [layer_id, perf] : PerforationConfiguration())
					if (perf.EarliestJobDate() == perfTime)
					{
						const_cast<set_of_points::SetOfPerforations&>(
							perf.getPerforationsSet().front()).MoveDate(date);
						const_cast<set_of_points::AccumulatedPerforations&>(perf).AssembleJobDates();
					}
				reservoir_simulator::WarningFactory::FirstPerforationMoved(NameWide(), perfTime, date);
			}
		}

		void SomeWell::BringLastPerforationToLastMER()
		{
			double perfTime = std::numeric_limits<double>::min();
			for (const auto& [layer_id, perf] : PerforationConfiguration())
				if (perfTime < perf.LatestJobDate())
					perfTime = perf.LatestJobDate();

			auto length = PerforationLengthTotal(perfTime);
			double date = LastProductionDate();
			if (length == 0.0 && perfTime < date)
			{// last job closes the entire well before the last MER date
				for (auto& [layer_id, perf] : PerforationConfiguration())
					if (perf.LatestJobDate() == perfTime)
					{
						const_cast<set_of_points::SetOfPerforations&>(
							perf.getPerforationsSet().back()).MoveDate(date);
						const_cast<set_of_points::AccumulatedPerforations&>(perf).AssembleJobDates();
					}
				reservoir_simulator::WarningFactory::LastPerforationMoved(NameWide(), perfTime, date);
			}
		}

		void SomeWell::UpdateWellState(double timeMoment)
		{
			currentTime = timeMoment;
			// structured binding cannot be used here
			std::tie(this->ItsCurCellIDs, this->ItsCurPerforationLengths) =
				PerforationLengths(timeMoment);

			//l * 2 * pi / log(rApp / rWell)
			factor.clear();
			double multiplier = 2 * M_PI / std::log(ApparentWellRadius() / WellRadius());
			for (size_t i = 0; i < NmbrOfOpenedCells(); i++)
				factor.push_back(CurPerforations()[i] * multiplier);

			// update numerical parameters
			// [ItsCurLocalIDs; rhsPerPerforation; matrixBlockPerPerforation]
			ItsCurLocalIDs.clear();


			for (size_t i = 0; i < NmbrOfOpenedCells(); i++)
				ItsCurLocalIDs.push_back(ItsLocalIDs[ItsCurCellIDs[i]]);

			rhsPerPerforation.clear();
			rhsPerPerforation.resize(NmbrOfOpenedCells());
			matrixBlockPerPerforation.clear();
			matrixBlockPerPerforation.resize(NmbrOfOpenedCells());
			for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
			{
				rhsPerPerforation[l] = std::vector<double>(B, 0.0);
				rhsPerPerforation[l].reserve(B);
				matrixBlockPerPerforation[l] = std::vector<double>(B * B, 0.0);
				matrixBlockPerPerforation[l].reserve(B * B);
			}

			productions = std::vector<double>(NmbrOfOpenedCells());
			P_Well = std::vector<double>(NmbrOfOpenedCells());
		}

		void SomeWell::BalanceOil()
		{
			double posProduction = 0, negProduction = 0;
			for (int l = 0; l < NmbrOfOpenedCells(); l++)
			{
				auto bufProduction = productions[l];
				if (bufProduction > 0.0)
					posProduction += F_Oil(l) * bufProduction;
				else
					negProduction += bufProduction;
			}

			auto overallDebit = CurOverallDebit();
			if (overallDebit > 0.0)
				negProduction -= overallDebit;
			else
				posProduction -= f_oil_in * overallDebit;

			if (negProduction < 0.0)
				F_OilWellBalance = -posProduction / negProduction;
		}


		double SomeWell::TimeToNextMomemnt(double curTime) const
		{
			return std::min(NextProductionFrameStart(curTime), NextWellJobInstance(curTime)) - curTime;
		}


	} // wells
} // reservoir_simulator