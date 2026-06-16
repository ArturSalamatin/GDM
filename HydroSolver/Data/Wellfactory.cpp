#include "WellFactory.h"
#include "PhaseFactory.hpp"
#include "ExceptionFactory.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/////////////////// MERFactory
		MERFactory::MERFactory(const RawWellFactory& well_factory) :
			RawMERFactory(well_factory),
			oil {PhaseFactory::CreateDefaultOil()},
			water{PhaseFactory::CreateDefaultWater()}
		{
			// set volume and mass of oil and water in every MER record
			for (size_t layer_id = 0; layer_id < MER_container.size(); ++layer_id)
			{
				setOilVolumeInLayer(oil, layer_id);
				setWaterVolumeInLayer(water, layer_id);
			}
		}
		void MERFactory::setOilVolumeInLayer(const OilPhaseProperty& oil, size_t layer_id)
		{
			for (auto& [name, mer] : MER_container[layer_id])
			{
				for (auto& record : mer)
				{
					if (record.at(L"type") == WellType::Injector)
					{
						if (record.at(L"oil") != 0.0)
							throw std::runtime_error("Non-zero oil mass.");

						double v = 0.0, m = 0.0;
						record.emplace(L"oil_v", v);
						record.emplace(L"oil_m", m);
						record.erase(L"oil");
						continue;
					}
					if (record.at(L"type") == WellType::Producer)
					{
						double m = record.at(L"oil") * 1000; // convert tone to kg
						double v = m / oil.Density();
						record.emplace(L"oil_v", v);
						record.emplace(L"oil_m", m);
						record.erase(L"oil");
						continue;
					}
					throw std::runtime_error("Unknown MER type in setOilVolumeInLayer()");
				}
			}
		}
		void MERFactory::setWaterVolumeInLayer(const WaterPhaseProperty& water, size_t layer_id)
		{
			for (auto& [name, mer] : MER_container[layer_id])
			{
				for (auto& record : mer)
				{
					double m = record.at(L"water") * 1000; // convert tone to kg
					double v = m / water.Density();
					if (record.at(L"type") == WellType::Injector)
					{
						record.emplace(L"water_m", -m);
						record.emplace(L"water_v", -v);
						record.erase(L"water");
						continue;
					}
					if (record.at(L"type") == WellType::Producer)
					{
						record.emplace(L"water_m", m);
						record.emplace(L"water_v", v);
						record.erase(L"water");
						continue;
					}
					throw std::runtime_error("Unknown MER type in setWaterVolumeInLayer()");
				}
			}
		}

		/////////////////// WellFactory
		WellFactory::WellFactory(ConnectionFactory& connection) :
			RawWellFactory{ connection },
			perforations{ *this },
			mer{ *this }
		{ }
		const PerforationFactory& WellFactory::Perforations() const
		{
			return perforations;
		}
		const RawMERFactory& WellFactory::MER() const
		{
			return mer;
		}

		WellJobsContainer
			WellFactory::jobsInLayer(size_t layer_id) const
		{
			return Perforations().jobsInLayer(layer_id);
		}
	} // factories
} // reservoir_simulator