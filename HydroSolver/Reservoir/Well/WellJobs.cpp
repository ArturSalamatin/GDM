#include "WellJobs.h"

namespace reservoir_simulator
{
	const WellName& WellJobs::Name()  const
	{ 
		return ItsName; 
	}

	set_of_points::PerforationsOfWell
		WellJobs::AccumulatePerforations(const std::vector<bool>& IsActiveCell) const
	{
		RemovePerfsAtInactiveCells(IsActiveCell);
		SortJobs();

		return AccumulatePerforations();
	}


	set_of_points::PerforationsOfWell 
		WellJobs::AccumulatePerforations() const
	{
	//	RemovePerfsAtInactiveCells(IsActiveCell);
	//	SortJobs();

		set_of_points::PerforationsOfWell
			ItsAccumulatedPerforationsContainer;

		// take every layer for the well
		for (size_t j = 0; j < RawWellPerforationData.size(); ++j)
		{
			const auto& jobsInLayer = RawWellPerforationData[j];
			if (!jobsInLayer.empty())
			{
				set_of_points::AccumulatedPerforations
					perforationsInTime{ jobsInLayer[0] }; // initialize with the very first job
				for (size_t i = 1; i < jobsInLayer.size(); i++)
					perforationsInTime.AddNewJob(jobsInLayer[i]); // add other jobs one by one
				ItsAccumulatedPerforationsContainer.emplace(j, perforationsInTime);
				ItsAccumulatedPerforationsContainer.at(j).RemoveRedundantJobs();
				ItsAccumulatedPerforationsContainer.at(j).AssembleJobDates();
			}
		}
		return ItsAccumulatedPerforationsContainer;
	}

	// only aggregates jobs, does not integrate perforations in time
	// to get the final set of open segments use AccumulatePerforations method

	WellJobs::WellJobs(
		const std::string& WellName,
		const WellJobsData& ItsJobsInLayers, 
		const LayerAggregationTree& LayerAggregation,
		size_t NmbrOfLayers) noexcept :
		ItsName{ WellName },
		RawWellPerforationData(NmbrOfLayers, 
			std::vector<set_of_points::WellJobTime>())
	{
		// create a collection of perforations for the well ItsName
		if (!ItsJobsInLayers.empty())
		{// there are jobs---perforations in the well

		 // create empty std::vector of WellJobs for every layer in LayerAggregation
		//	RawWellPerforationData = 
		//		std::vector<std::vector<set_of_points::WellJobTime>>(NmbrOfLayers);

			for (const auto& [layerName, jobs] : ItsJobsInLayers)
			{// loop through every layer with jobs
			//	auto layerName = openLayer.first; // take the layer name
			//									  // take its id in the upscaled model
				size_t aggrIdx = LayerAggregation.at(std::stoi(layerName));
				// current layer belongs to the group aggrIdx of grouped layers
				// thus save corresponding records for every job in this layer
				for (size_t i = 0; i < jobs.size(); ++i)
				{
					const auto& jobRecord = jobs[i];
					RawWellPerforationData[aggrIdx].emplace_back(
						std::get<3>(jobRecord), // std::pair<double, double>
						std::get<1>(jobRecord), //
						std::get<0>(jobRecord));
				}
			}

			SortJobs();
		}
		else
		{// the well does not open anything
		//	LogFileSpace::LogFile::WriteLog(L">>>>>> Well " + Name() + L" does not contain any information regarding perforations!");
		//	LogFileSpace::LogFile::WriteLog(L">>>>>> It is excluded from simulation.", true);
		}
	}

	// we do have some jobs in the well
	bool WellJobs::IsEmpty() const
	{
		for (size_t i = 0; i < RawWellPerforationData.size(); ++i)
			if (!RawWellPerforationData[i].empty())
				return false;
		return true;
	}

	void WellJobs::SortJobs() const
	{
		// take each layer in the well
		for (auto& layer : RawWellPerforationData)
			std::sort(layer.begin(), layer.end(), 
				set_of_points::WellJobTime::sortOperator);
	}

	void WellJobs::RemovePerfsAtInactiveCells(
		const std::vector<bool>& IsActiveCell) const
	{
		for (size_t i = 0; i < IsActiveCell.size(); i++)
		{
			if (!IsActiveCell[i])
				RawWellPerforationData[i] = 
				std::vector<set_of_points::WellJobTime>();
		}
	}

} // reservoir_simulator