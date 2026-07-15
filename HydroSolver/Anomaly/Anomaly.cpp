#include "Anomaly.h"


namespace reservoir_simulator
{
	static std::mutex log_write_mutex;

	 WellSignals::WellSignals(const std::string& name, const std::vector<double> coords, const std::vector<double>& numCoords, const AnomaliesPerWellData& anomalies_)
	{
		(*this).name = name;
		(*this).coords = coords;
		(*this).numCoords = numCoords;
		(*this).anomalies = anomalies_;
	}

	 void WellSignals::SetIntervals(const std::pair<double, double>& overallInterval, double spread, float anomalyType)
	{
		TrimAnomalies(overallInterval);

		if (anomalies.empty())
		{
			LogFileSpace::LogFile::WriteLog("class_Anomaly", "method_SetIntervals", "warning",
				"No anomaly records in the time frame of interest for the well " + name + ".");
			return;
		}

		bool isStartDateSet;
		double startInterval, endInterval;

		startInterval = anomalies[0]["time"] - spread; // only a guess fot the first start date
		endInterval = anomalies[0]["time"] + spread; // only a guess for the end date
		isStartDateSet = anomalies[0].at("type") == anomalyType;// extrapolate initial data point to the past
		for (int i = 0; i < anomalies.size(); i++)
		{
			if (!isStartDateSet)
			{
				isStartDateSet = anomalies[i].at("type") == anomalyType;
				if (!isStartDateSet)
				{
					startInterval = anomalies[i]["time"]; // only a guess is isStartdateSet == false at this line
					endInterval = anomalies[i]["time"] + spread; // only a guess for the end date
				}
				else
				{// first anomaly in the continuous frame is encountered
				 // hence startInterval remains from the previous non-anomaly time moment
					endInterval = anomalies[i]["time"] + spread; // only a guess for the end date
				}
			}
			else
			{//we already encountered an anomaly at i-1
				isStartDateSet = anomalies[i].at("type") == anomalyType; // is this anomaly or not?
				if (isStartDateSet)
				{// anomaly continues in time
					endInterval = anomalies[i]["time"] + spread; // only a guess for the end date
				}
				else
				{// anomaly just ended
					endInterval = anomalies[i]["time"]; // only a guess for the end date
					intervals.push_back(std::make_pair(startInterval, endInterval)); // accept the interval
					startInterval = anomalies[i]["time"];
					endInterval = anomalies[i]["time"] + spread;
				}
			}
		}
		if (isStartDateSet)
			intervals.push_back(std::make_pair(startInterval, endInterval)); // accept the interval

		std::reverse(intervals.begin(), intervals.end()); // make last interval - first

#ifdef DEBUG_SALAMATIN
		print_intervals();
#endif
	}

	 void WellSignals::print_intervals()
	{
		for (int i = 0; i < intervals.size(); i++)
		{
			std::cout << intervals[i].first << " - " << intervals[i].second
				<< std::endl;
		}
		if (intervals.size() == 0)
			std::cout << "empty" << std::endl;
		std::cout << std::endl;
	}

	 const std::string& WellSignals::Name() const { return name; }

	 const std::vector<double>& WellSignals::Coords() const { return coords; }

	void WellSignals::set_trajectories_fixed_time(double startSignalRollback, double endSignalRollback,
		FlowField::FlowFieldSequence& fields, double r, const SomeWell* wellData)
	{
		TrajectoryInitializer data{ { "x0", coords[0] },{ "y0",coords[1] },{ "r", r } ,{ "count", 2.0 },
			{ "dist_abs", 5.0 },{ "dist_rel", 5.0 },{ "time_step", 1.0 } };

		// loop through z-sections
		for (int k = 0; k < fields.size(); k++)
		{
			wellDomains.push_back(WellDomainsPerLayer());
			// in every z-section get domains
			for (int i = 0; i < intervals.size(); i++)
			{
				auto l = wellData->PerforationLengthOverall(intervals[i].first);
				if (l[k] > 0.0)
				{
					std::string name = wellData->Name();
					wellDomains.back().push_back(SingleWellDomain{ data,
						TimeFrame{ intervals[i].first, intervals[i].second, startSignalRollback, endSignalRollback },
						fields[k], Features{ name } });
				}
				else
				{
					log_write_mutex.lock();
					LogFileSpace::LogFile::WriteLog("class_WellSignals", "method_set_trajectories", "warning",
						"The layer " + std::to_string(k) + " is not perforated at " + std::to_string((int)intervals[i].first)
						+ " for the well " + wellData->Name() + ".");
					log_write_mutex.unlock();
				}
			}
		}
	}

	 void WellSignals::print_well_signals_json() const
	{
		const std::vector<std::string> layerNames{ { "d0", "d1a", "d1b", "d1vgd" } };

		for (int k = 0; k < wellDomains.size(); k++)
		{// loop through z-sections
			for (int l = 0; l < wellDomains[k].size(); l++)
			{
				/*std::vector<std::pair<std::string, std::string>> params{
				{"name", Name()},
				{"coords", Point(numCoords[0], numCoords[1]).print_json()},
				{"layer", layerNames[k]},
				{"time", wellDomains[k][l].print_time_json()},
				{"contour", wellDomains[k][l].print_json()}
				};*/
				LogFileSpace::LogFile::WriteLog("class_WellSignals", "method_print_well_signals_json",
					"success",
					JSON::CreateJSON::CreateObject( //std::vector<std::pair<std::string, std::string>>
						{
							{ "name", Name() },
						{ "coords", phasePortrait::Point(numCoords[0], numCoords[1]).print_json() },
						{ "layer", layerNames[k] },
						{ "time", wellDomains[k][l].print_time_json() },
						{ "contour", wellDomains[k][l].print_json() }
						}
					)
				);
			}
		}
	}

	 void WellSignals::print_well_signals_geojson(GeoJsonEngine& sraka) const
	{
		for (int k = 0; k < wellDomains.size(); k++)
			for (int l = 0; l < wellDomains[k].size(); l++)
				wellDomains[k][l].print_geo_json(sraka);
	}

	 bool WellSignals::empty() const
	{
		for (int i = 0; i < wellDomains.size(); i++)
			if (!wellDomains[i].empty())
				return false;
		return true;
	}

	 void WellSignals::TrimAnomalies(const std::pair<double, double>& overallInterval)
	{// anomalies of interest must be in the [startDate; endDate] time frame
		for (int j = (int)anomalies.size() - 1; j > -1; j--)
			if (overallInterval.first > anomalies[j]["time"] || anomalies[j]["time"] > overallInterval.second)
				anomalies.erase(anomalies.begin() + j);
	}


	SingleWellDomain::SingleWellDomain(TrajectoryInitializer data, TimeFrame timeFrame, phasePortrait::FlowField& field, const Features& features)
	{
		double& field_time = extTimeStart;

		// external contour
		extTimeStart = timeFrame.interval_end; // external contour comes second;
		extTimeEnd = extTimeStart - timeFrame.endSignalRollback; // when it started
		data["start_time"] = extTimeStart;
		geos_polygon ext_contour = field.FollowPhasePortrait_fixed_time(data, extTimeStart, extTimeEnd, field_time);
		ext_contour.assign_features({
			{ "ext_start_time", extTimeStart, "" },
			{ "ext_end_time", extTimeEnd, "" } });
		// internal contour
		intTimeStart = timeFrame.interval_start; // internal contour comes first
		intTimeEnd = intTimeStart - timeFrame.startSignalRollback; // when it started
		data["start_time"] = intTimeStart;
		geos_polygon int_contour = field.FollowPhasePortrait_fixed_time(data, intTimeStart, intTimeEnd, field_time);
		int_contour.assign_features({
			{ "int_start_time", intTimeStart, "" },
			{ "int_end_time", intTimeEnd, "" } });

		domain = ext_contour.combine_with(int_contour);
		domain.assign_features({
			{ "layer", 0.0, field.layer_name() },
			{ "well_name",0.0, features.well_name },
			{ "field_time",field_time,"" } });
	}
	 void SingleWellDomain::print_geo_json(GeoJsonEngine& sraka) const
	{
		domain.print_geo_json(sraka);
	}
	 std::string SingleWellDomain::print_json() const
	{
		return JSON::CreateJSON::CreateArray({ domain.print_json() });
	}
	 std::string SingleWellDomain::print_time_json() const
	{
		return JSON::CreateJSON::CreateArray({ std::to_string(extTimeStart), std::to_string(extTimeEnd),
			std::to_string(intTimeStart), std::to_string(intTimeEnd) });
	}
	 Anomalies::Anomalies(const std::map<std::string, SomeWell*>& wells, const std::string& anomalyPath)
	{
		(*this).wells = wells;
		(*this).anomalyData.Push(anomalyPath);
	}
	 void Anomalies::instantiate(const std::vector<ModelHanlder::ReadModel::WellData>& well_data, double r, int count, const std::pair<double, double>& overallInterval, double spread, float anomalyType)
	{
		(*this).count = count;
		(*this).r = r;
		relevantAnomalies.clear();

		// loop through wells
		for (int i = 0; i < well_data.size(); i++) {
			std::string wellName = well_data[i].Name;

			AnomaliesPerWellData wellAnomalies_All = anomalyData.GetDataPerWell(wellName);
			if (!wellAnomalies_All.empty())
			{// there are some records for the well
				auto wellIdx = wells.find(wellName);
				if (wellIdx != wells.end())
				{
					auto& well = wells.at(wellName);
					relevantAnomalies.emplace_back(wellName, well->IntersectionCoords(), well->IntersectionCoordsNum(),
						wellAnomalies_All);
				}
				else
					LogFileSpace::LogFile::WriteLog("class_ReservoirInstantiator", "method_TraceAnomalies",
						"warning", "No hydrodynamic data for well " + wellName);
			}
		}
		instantiate(overallInterval, spread, anomalyType);
	}
	 void Anomalies::instantiate(const std::pair<double, double>& overallInterval, double spread, float anomalyType) {
		(*this).overallInterval = overallInterval;
		(*this).spread = spread;
		(*this).anomalyType = anomalyType;

		for (auto& anom : relevantAnomalies)
			anom.SetIntervals(overallInterval, spread, anomalyType);
	}
	void Anomalies::set_trajectories_fixed_time(double startRollback, double endRollback, FlowField::FlowFieldSequence& flowFields)
	{
		/*std::for_each(
		std::execution::par_unseq,
		relevantAnomalies.begin(),
		relevantAnomalies.end(),
		[startRollback, endRollback, flowFields, this](auto& anomaly)
		{
		anomaly.set_trajectories_fixed_time(startRollback, endRollback, flowFields,
		(*this).r, (*this).count, wells.at(anomaly.Name()));
		});*/

#ifdef	USE_PARALLEL
#pragma omp parallel for
#endif
		for (int i = 0; i < relevantAnomalies.size(); i++)
			relevantAnomalies[i].set_trajectories_fixed_time(startRollback, endRollback,
				flowFields, r, wells.at(relevantAnomalies[i].Name()));
	}
	 void Anomalies::print_json()
	{
		// start result sending
		LogFileSpace::LogFile::WriteLog("main", "anomaly_sender", "success",
			JSON::CreateJSON::CreateArray(
				{ JSON::CreateJSON::CreateObject({ { "name", "domain" } ,{ "dimension", "4" } ,
					{ "names", JSON::CreateJSON::CreateArray({ "d0", "d1a", "d1b", "d1vgd" }) } }) }));

		for (const auto& anom : relevantAnomalies)
		{
			if (anom.empty())
				continue;
			anom.print_well_signals_json();
		}
	}
	 void Anomalies::print_geo_json(const std::string& filename)
	{
		GeoJsonEngine sraka(filename);
		for (const auto& anom : relevantAnomalies)
		{
			if (anom.empty())
				continue;
			anom.print_well_signals_geojson(sraka);
		}
		sraka.WriteGeoJson();
	}
}