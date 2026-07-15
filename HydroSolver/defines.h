#pragma once
#include <string>
#include <vector>
#include <map>

#include "Data/wells/GeosPoint.h"

#include "Reservoir/Well/SetOfPoints.h"

namespace reservoir_simulator
{
	using WellName = std::string;
	using WellNames = std::vector<WellName>;

	using WellPosition = GeosShell::GeosPoint;
	using WellPositions = std::map<WellName, WellPosition>;

	using LayerID = std::string;

	namespace mer_descriptor
	{
		/// <summary>
		/// Essentially a single MER record described as a map-container of pairs [field name; value] with 
		/// field name: {time, oil, water, pump_water, worked_time, idle_time, type, is_work}
		/// </summary>
		using SingleMERrecord = std::map<std::string, float>;

		/// <summary>
		/// Container of single MER records for a well. Suck container should be introduced for every layer for layered_mer
		/// </summary>
		using SingleWell_MER_Data = std::vector<SingleMERrecord>;


		struct TimeFrame
		{
			double start, end;
			operator std::pair<double, double>() const { return std::make_pair(start, end); }
		}; // std::pair<double, double>;

		// fluid debit in kg for every phase
		struct FluidDebit
		{
			static FluidDebit SetFromVolume(double o_v, double w_v)
			{
				return FluidDebit{ o_v * 800.0, w_v * 1000.0 };
			}

			double oil, water;
			FluidDebit operator*(double val) {
				return FluidDebit{ this->oil * val, this->water * val };
			}
			FluidDebit operator/(double val) {
				return (*this) * (1.0 / val);
			}
			FluidDebit operator+(const FluidDebit& val) {
				return FluidDebit{ this->oil + val.oil, this->water + val.water };
			}
		}; // std::pair<double, double> fluidDebit;

	} // mer_descriptor

	/// <summary>
	/// Container for mers indexed by well_name
	/// </summary>
	using MERofAllWells =
		std::map<WellName,
		reservoir_simulator::mer_descriptor::SingleWell_MER_Data
		>;

	/// <summary>
	/// A map of well name to accumulated (in time) perforations of well in a certain layer
	/// </summary>
	using AccumulatedPerfsInLayer = std::map<WellName, set_of_points::AccumulatedPerforations>;


	class WellJobs;
	/// <summary>
	/// Operations(WellJobs), performed on each well(std::string = well_name)
	/// </summary>
	using WellJobsContainer = std::map<WellName, WellJobs>;

} // reservoir_simulator