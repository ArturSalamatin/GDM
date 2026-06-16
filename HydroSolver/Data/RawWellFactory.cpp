#include "RawWellFactory.h"
#include "ExceptionFactory.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/////////////////// LayerFactory		
		LayerFactory::LayerFactory(ConnectionFactory& connection) noexcept :
			layers_descriptor{ connection.Loader().GetIdxLayerTable() }
		{}
		const LayerAggregationTree&
			LayerFactory::LayersDescriptor() const
		{
			return layers_descriptor;
		}


		/////////////////// RawWellFactory
		RawWellFactory::RawWellFactory(ConnectionFactory& connection) :
			LayerFactory{ connection }
		{
			/*для скважин соединение нужно создавать вручную*/
			auto temp_connection{ ProgramLauncher::Utils::GetConnection(connection.uuid()) };
			ProgramLauncher::WellData_Handler well_data;
			well_data.AddData(*temp_connection); // get data from the DB

			auto geosObj{ std::make_shared<GeometryHandler::GEOSObjectHandler>() };
			ProgramLauncher::Database_FileHandler dbWell{ temp_connection, geosObj };
			dbWell.LoadWells(); // взяли данные по скважинам --- имена и координаты

			well_position = dbWell.Wells;
			well_name = dbWell.WellsName;

			/*for (size_t l = well_name.size(); l > 0; --l)
				if (well_name[l - 1] != L"273802")
					well_name.erase(well_name.begin() + l - 1);*/

			workzone_countour = dbWell.WorkZoneCountour;

			Gis = well_data.Gis;
			Perf = well_data.Perf;
			RawMerLayered = well_data.MerLayered;
		}
		const std::vector<WellName>&
			RawWellFactory::WellName() const noexcept { return well_name; }
		const WellDataHandler::GISData&
			RawWellFactory::gisd() const noexcept { return *Gis; };
		const WellDataHandler::PerfData&
			RawWellFactory::pfd() const noexcept { return *Perf; };
		const WellDataHandler::Database_MerLayeredData&
			RawWellFactory::mer() const noexcept { return *RawMerLayered; };
		const WellDataHandler::MerData&
			RawWellFactory::mer_in_layer(const LayerID& layer_id) const
		{
			auto& dd = const_cast<WellDataHandler::Database_MerLayeredData&>(mer());
			std::shared_ptr<WellDataHandler::MerData> xx = dd.GetMerDataPerLayer(layer_id);
			return *xx.get();
		};
		/*const WellPosition&
			RawWellFactory::position(const reservoir_simulator::WellName& well_name) const
		{
			return well_position.at(well_name);
		}*/
		const WellPositions&
			RawWellFactory::positions() const
		{
			return well_position;
		}

		/////////////////// RAWMERFactory
		RawMERFactory::RawMERFactory(const RawWellFactory& well_factory) :
			MER_container(well_factory.LayersDescriptor().size())
		{
			const auto& layers = well_factory.LayersDescriptor();

			for (const auto& [key, value] : layers)
			{
				const auto& mer = well_factory.mer_in_layer(
					std::to_wstring(key));
				MER_container[value] =
					createMER(
						const_cast<WellDataHandler::MerData&>(
							mer));
			}
		}
		const MERofAllWells& RawMERFactory::getMERinLayer(size_t layer_id) const
		{
			return MER_container[layer_id];
		}
		MERofAllWells RawMERFactory::createMER(WellDataHandler::MerData& mer) const
		{
			MERofAllWells out;
			for (const auto& name : mer.GetWellsName()) // each layer may contain different wells --- some wells do not open the layer
			{
				out.emplace(name, mer.GetDataPerWell(name));
			}
			return out;
		}

		/////////////////// PerforationFactory
		PerforationFactory::PerforationFactory(
			const RawWellFactory& well_factory) :
			layered_well_perf(well_factory.LayersDescriptor().size(),
				std::map<WellName, set_of_points::AccumulatedPerforations>{})
		{
			// loop through every well
			for (const auto& name : well_factory.WellName())
			{
				const auto& data = WellDataHandler::DataHandleUtils::
					ComparePerfWithGIS(
						const_cast<WellDataHandler::GISData&>(well_factory.gisd()),
						const_cast<WellDataHandler::PerfData&>(well_factory.pfd()), name);
				//	validate_perforation_data(name, LayerAggregation, data);
					//	ItsLayerContainer.insert({ name, data });

				WellJobs jobs{ name, data,
					well_factory.LayersDescriptor(),
					well_factory.LayersDescriptor().size() };
				if (!jobs.IsEmpty())
				{
					//		jobs.RemovePerfsAtInactiveCells(reservoir_factory.ActiveCells());
					jobs_container.emplace(name, jobs);
				}
				else
					WarningFactory::WellOperationDataIsEmpty(name);
			}
			wellPerforationsInLayer();
		}
		void PerforationFactory::wellPerforationsInLayer()
		{
			// loop through well name and corresponding set of jobs
			for (const auto& [well_name, well_jobs] : jobs_container)
			{
				set_of_points::PerforationsOfWell accumulated_perfs{ well_jobs.AccumulatePerforations() };
				for (const auto& [layer_id, perfs_in_layer] : accumulated_perfs)
					layered_well_perf[layer_id].emplace(well_name, perfs_in_layer);
			}
		}
		const AccumulatedPerfsInLayer&
			PerforationFactory::accumulatedPerforationsInLayer(size_t layer_id) const
		{
			return layered_well_perf[layer_id];
		}
		const WellJobs&
			PerforationFactory::jobsOfWell(const WellName& name) const
		{
			return jobs_container.at(name);
		}
		const WellJobsContainer&
			PerforationFactory::jobsContainer() const
		{
			return jobs_container;
		}
		WellJobsContainer
			PerforationFactory::jobsInLayer(size_t layer_id) const
		{
			WellJobsContainer out;
			for (const auto& [name, jobs] : jobs_container)
			{
				out.emplace(name,
					WellJobs{ name, jobs.jobsInLayer(layer_id) });
			}
			return out;
		}
	} // factories
} // reservoir_simulator