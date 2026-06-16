#pragma once

#include "../../stdafx.h"
#include "../../defines.h"

#include "SetOfPoints.h"
#include "../../Descriptors/Descriptors.h"
// #include "../../Helpers/LogFile.h"

namespace reservoir_simulator
{
//	typedef std::map<std::string, std::map<std::string, std::vector<std::tuple<int, int, std::pair<float, float>>>>> PerforationWellData;

	/// <summary>
	/// tuple[job time_stamp, job type(open/close), is grp, interval of operatiorn(pair[start, end])
	/// </summary>
	using JobDescriptor = std::tuple<int, int, int, std::pair<float, float>>;
	/// <summary>
	/// Container for jobs on a well
	/// </summary>
	using JobsOfWell = std::vector<JobDescriptor>;
	/// <summary>
	/// [layerName, std::vector[tuple[time, open/close, pair[start, end]]]
	/// </summary>
	using LayerName = std::wstring;
	using WellJobsData = std::map<LayerName, JobsOfWell>;

	/// <summary>
	/// The set of operations in well in layer
	/// </summary>
	using JobsInLayer = std::vector<set_of_points::WellJobTime>;
	/// <summary>
	/// Container with element corresponding to jobs in a single layer
	/// </summary>
	using WellJobsPerLayer = std::vector<JobsInLayer>;

	/// <summary>
	/// Class representing set of well jobs in every layer
	/// </summary>
	class WellJobs
	{
	public:
		// well name
		const WellName& Name() const;
		set_of_points::PerforationsOfWell 
			AccumulatePerforations() const;
		set_of_points::PerforationsOfWell
			AccumulatePerforations(const std::vector<bool>& IsActiveCell) const;
		
		// only aggregates jobs, does not integrate perforations in time
		// to get the final set of open segments use AccumulatePerforations method
		WellJobs(
			const WellName& well_name,
			const WellJobsData&  ItsJobsInLayers, 
			const LayerAggregationTree&  LayerAggregation,
			size_t NmbrOfLayers
		) noexcept;

		// we do have some jobs in the well
		bool IsEmpty() const;

		const JobsInLayer& jobsInLayer(size_t layer_id) const
		{
			return RawWellPerforationData[layer_id];
		}

		void RemovePerfsAtInactiveCells(
			const std::vector<bool>& IsActiveCell) const;

		WellJobs(const WellName& well_name, const JobsInLayer& jobs) noexcept :
			ItsName{ well_name }, RawWellPerforationData{jobs}
		{ }

	protected:
		const WellName& ItsName; // = L"The name is not assigned.";
		// create a collection of perforations for the well
		// [layerID][JobId in that layer]
		mutable WellJobsPerLayer
			RawWellPerforationData;

	private:
		void SortJobs() const;
	};
} // reservoir_simulator