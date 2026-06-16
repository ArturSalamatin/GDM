#pragma once

#include "../defines.h"
#include "ConnectionFactory.h"
#include "ReservoirFactory.h"
#include "wells/WellDataHandler.h"
#include "wells/WellsBase.h"
#include "../Descriptors/Descriptors.h"
#include "../Descriptors/MER_Descriptor.h"
#include "../../Reservoir/Well/WellJobs.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/// <summary>
		/// Class generating layers data (their amount, inclusion, relation...)
		/// </summary>
		class LayerFactory
		{
		public:
			LayerFactory(
				ConnectionFactory& connection)
				noexcept;
			const LayerAggregationTree&
				LayersDescriptor() const;
		protected:
			LayerAggregationTree layers_descriptor;
		};

		/// <summary>
		/// Class aggregating the data relevant to wells
		/// </summary>
		class RawWellFactory : public LayerFactory
		{
		public:
			RawWellFactory(ConnectionFactory& connection);

			const std::vector<std::wstring>&
				WellName() const noexcept;
			const WellDataHandler::GISData&
				gisd() const noexcept;
			const WellDataHandler::PerfData&
				pfd() const noexcept;
			const WellDataHandler::Database_MerLayeredData&
				mer() const noexcept;
			const WellDataHandler::MerData&
				mer_in_layer(const LayerID& layer_id) const;
			const WellPositions&
				positions() const;
		protected:
			//	ProgramLauncher::WellData_Handler well_data;
			/*имя скважины -- пластопересечение(точка пространства)*/
			std::map<reservoir_simulator::WellName, GeosShell::GeosPoint> well_position;
			/*все имена скважин*/
			std::vector<reservoir_simulator::WellName> well_name;
			/* граница участка, произвольная граница*/
			std::vector<std::vector<double>> workzone_countour;

			std::shared_ptr<const WellDataHandler::GISData> Gis;
			std::shared_ptr<const WellDataHandler::PerfData> Perf;
			//RawMerLayered["layer_id"] --- returns a mer 
			std::shared_ptr<const WellDataHandler::Database_MerLayeredData> RawMerLayered;

			//std::shared_ptr<WellDataHandler::GDISData> GDIS;
			//WellDataHandler::PVTData PVT;
			//std::shared_ptr<WellDataHandler::GTMData> GTM;
			//std::shared_ptr<WellDataHandler::TechModeOil> TechModeOil;
		};

		class RawMERFactory
		{
		protected:
			enum WellType { Producer = 1, Injector = 0 };
		public:
			RawMERFactory(const RawWellFactory& well_factory);

			const MERofAllWells& getMERinLayer(size_t layer_id) const;

		protected:
			/// <summary>
			/// Each vector element is per reservoir layer
			/// </summary>
			std::vector<MERofAllWells> MER_container;

			MERofAllWells createMER(WellDataHandler::MerData& mer) const;



			/*void AggregateMER(
				std::shared_ptr<WellDataHandler::Database_MerLayeredData> mer_data,
				const LayerAggregationTree& layers)
			{
				for (const auto& [layer_id, l_id] : layers)
				{
					const auto& v = *(*mer_data).GetMerDataPerLayer(std::to_wstring(layer_id));

				}

			}*/
		};


		/// <summary>
		/// Describes perforations of wells for absracts layers indexed as (layer_id){ 0,1,2,3,.. }
		/// </summary>
		class PerforationFactory
		{
		public:
			PerforationFactory(
				const RawWellFactory& well_factory);

			void wellPerforationsInLayer();

			/// <summary>
			/// Returns container of accumulated perforations in a single layer for all wells
			/// </summary>
			const AccumulatedPerfsInLayer& accumulatedPerforationsInLayer(size_t layer_id) const;
			/// <summary>
			/// Returns well jobs in al layers for a certain well
			/// </summary>
			/// <returns></returns>
			const WellJobs& jobsOfWell(const WellName& name) const;
			/// <summary>
			/// Returns WellJobs for every well and every layer
			/// </summary>
			const WellJobsContainer& jobsContainer() const;
			/// <summary>
			/// Returns WellJobs in a certain layer for all wells
			/// </summary>
			WellJobsContainer jobsInLayer(size_t layer_id) const;

		protected:
			WellJobsContainer jobs_container;
			std::vector<AccumulatedPerfsInLayer>
				layered_well_perf;
		};

	} // factories
} // reservoir_simulator