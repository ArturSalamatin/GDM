#pragma once
#include "../stdafx.h"

#include "FlowField/FlowField.h"
//#include "FlowField/Point.h"
#include "../Reservoir/Well/SomeWell.h"
#include "../Data/wells/WellDataHandler.h"
//#include "../Utils/WellDataHandler.h"
#include "../Utils/JSON/JSONCreate.h"
#include "../Utils/JSON/geojson.h"

namespace reservoir_simulator
{
	class SingleWellDomain;

	using AnomaliesPerWellData = std::vector<std::map<std::wstring, float>>;
	using WellDomainsPerLayer = std::vector<SingleWellDomain>; // each layer of reservoir may contain a set of domains
	using TrajectoryInitializer = std::map<std::wstring, double>;
	using TimeFrame = struct { double interval_start, interval_end, startSignalRollback, endSignalRollback; };
	using Features = struct { std::wstring well_name; };

	class SingleWellDomain
	{
	protected:
		double extTimeStart, extTimeEnd, intTimeStart, intTimeEnd;
		geos_polygon domain;

	public:
		//SingleWellDomain(const TrajectoryInitializer& data, std::pair<double, double> interval, double startSignalRollback, double endSignalRollback,
		//	phasePortrait::FlowField& field)
		//{
		//	// external contour
		//	extTimeStart = interval.second; // external contour comes second;
		//	extTimeEnd = extTimeStart - endSignalRollback; // when it started
		//	field.FollowPhasePortrait(data, extTimeStart, extTimeEnd, 1);
		//	ext_contour = field.CalculatedPhasePortraits().back().Contour_geos();
		//	// internal contour
		//	intTimeStart = interval.first; // internal contour comes first
		//	intTimeEnd = interval.first - startSignalRollback; // when it started
		//	field.FollowPhasePortrait(data, intTimeStart, intTimeEnd, 1);
		//	int_contour = field.CalculatedPhasePortraits().back().Contour_geos();
		//}

		SingleWellDomain(TrajectoryInitializer data, TimeFrame timeFrame, phasePortrait::FlowField& field, const Features& features);

		//std::wstring print() const
		//{
		//	const int n = 40;
		//	// external contour
		//	wchar_t buffer[n];
		//	swprintf(buffer, n, L"%u;", extContour.size()); // number of points to print
		//	std::wstring result{ buffer };
		//	for (int l = 0; l < extContour.size(); l++)
		//	{
		//		swprintf(buffer, n, L"%+19.11E;", extContour.x()[l]);
		//		result += buffer;
		//		swprintf(buffer, n, L"%+19.11E;", extContour.y()[l]);
		//		result += buffer;
		//	}
		//	// internal contour
		//	swprintf(buffer, n, L"%u;", intContour.size()); // number of points to print
		//	result += buffer;
		//	for (int l = 0; l < intContour.size(); l++)
		//	{
		//		swprintf(buffer, n, L"%+19.11E;", intContour.x()[l]);
		//		result += buffer;
		//		swprintf(buffer, n, L"%+19.11E;", intContour.y()[l]);
		//		result += buffer;
		//	}
		//	return result;
		//}

		__declspec(noinline) void print_geo_json(GeoJsonEngine& sraka) const;

		std::wstring print_json() const;

		std::wstring print_time_json() const;
	};

	class WellSignals
	{
	public:
		const std::wstring& Name() { return name; }
	protected:
		std::wstring name; // well name
		std::vector<std::pair<double, double>> intervals;
		std::vector<double> coords, numCoords;// real coordinates and coordinates of the cell center
		AnomaliesPerWellData anomalies;

		std::vector<WellDomainsPerLayer> wellDomains;

	public:
		WellSignals(const std::wstring& name, const std::vector<double> coords, const std::vector<double>& numCoords, const AnomaliesPerWellData& anomalies_);

		void SetIntervals(const std::pair<double, double>& overallInterval, double spread, float anomalyType = 2.0);


		void print_intervals();

		const std::wstring& Name() const;
		const std::vector<double>& Coords() const;

		//void set_trajectories(double startSignalRollback, double endSignalRollback, 
		//	const std::vector<phasePortrait::SomeFlowField>& flowFields, double r, int count, const SomeWell* wellData)
		//{
		//	TrajectoryInitializer data = { {L"x0", numCoords[0]},{L"y0",numCoords[1]},{L"r", r},  //{L"count", count},
		//		{L"dist_abs", 1}, {L"dist_rel", 25} };

		//	std::vector<FlowField> fields;
		//	// loop through z-sections
		//	for (int k = 0; k < flowFields.size(); k++)
		//	{
		//		fields.push_back(FlowField(flowFields[k]));
		//		wellDomains.push_back(WellDomainsPerLayer());
		//		// in every z-section get domains
		//		for (int i = 0; i < intervals.size(); i++)
		//		{
		//			auto l = wellData->PerforationLengthOverall(intervals[i].first);
		//			if (l[k] > 0.0)
		//				wellDomains.back().push_back(SingleWellDomain(data, intervals[i], startSignalRollback, endSignalRollback,
		//					fields.back()));
		//			else
		//			{
		//				LogFileSpace::LogFile::WriteLog(L"class_WellSignals", L"method_set_trajectories", L"warning", 
		//					L"The layer " + std::to_wstring(k) + L" is not perforated at " + std::to_wstring((int)(intervals[i].first)) + L" for the well " +  wellData->Name() + L".");						
		//			}
		//		}
		//	}
		//}

		void set_trajectories_fixed_time(double startSignalRollback, double endSignalRollback,
			FlowField::FlowFieldSequence& fields, double r, const wells::SomeWell* wellData);

		//const std::wstring print_well_signals() const
		//{
		//	wchar_t buffer[20];
		//	swprintf(buffer, 20, L"%u;", wellDomains.size());// number of z-sections
		//	std::wstring result{ buffer };
		//	for (int k = 0; k < wellDomains.size(); k++)
		//	{
		//		swprintf(buffer, 20, L"%u;", wellDomains[k].size());// number of domains in the z-section
		//		result += buffer; 
		//		for (int l = 0; l < wellDomains[k].size(); l++)
		//		{
		//			result += wellDomains[k][l].print();
		//		}
		//	}
		//	return result;
		//}

		void print_well_signals_json() const;

		__declspec(noinline) void print_well_signals_geojson(GeoJsonEngine& sraka) const;

		bool empty() const;

	private:
		void TrimAnomalies(const std::pair<double, double>& overallInterval);
	};

	class Anomalies
	{
	protected:
		std::vector<WellSignals> relevantAnomalies;
		std::map<std::wstring, wells::SomeWell*> wells;
		WellDataHandler::AnomData anomalyData;

		std::pair<double, double> overallInterval;
		double spread;
		float anomalyType;

		double r;
		int count;

	public:

		Anomalies(const std::map<std::wstring, wells::SomeWell*>& wells, const std::wstring& anomalyPath);

		void instantiate(const std::vector<ModelHanlder::ReadModel::WellData>& well_data, double r, int count,
			const std::pair<double, double>& overallInterval, double spread, float anomalyType = 2.0);

		void instantiate(const std::pair<double, double>& overallInterval, double spread, float anomalyType);

		/*void SetTrajectories(double startRollback, double endRollback, 
			const std::vector<phasePortrait::SomeFlowField>& flowFields)
		{
			for (auto& anomaly : relevantAnomalies)
				anomaly.set_trajectories(startRollback, endRollback, flowFields, r, count, wells.at(anomaly.Name()));
		}*/

		void set_trajectories_fixed_time(double startRollback, double endRollback,
			FlowField::FlowFieldSequence& flowFields);

		//std::wstring print()
		//{
		//	wchar_t buffer[20];
		//	swprintf(buffer, 20, L"%u;", relevantAnomalies.size());// number of wells with anomalies
		//	std::wstring result{ buffer };

		//	for (const auto& anom : relevantAnomalies)
		//		result += anom.print_well_signals();
		//	return result;
		//}

		void print_json();

		void print_geo_json(const std::wstring& filename);
	};
}