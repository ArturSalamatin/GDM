#include "HorizonFactory.h"

namespace reservoir_simulator
{
	namespace factories
	{
		HorizonFactory::HorizonFactory(const std::string& connection_name)
		{
			reservoir_simulator::factories::ConnectionFactory connection{ connection_name };
			// create reservoir with all properties
			reservoir_factory = std::make_shared<const RawReservoirFactory>(
				ReservoirFactory<UnitsConversionFactory>{connection});
			// create well factory	
			well_factory = std::make_shared<const WellFactory>(connection);
			phase_factory = std::make_shared<const PhaseFactory>();
			other_factory = std::make_shared<const OtherFactory>();			
		}


		RawHorizon HorizonFactory::createSingleLayerRawHorizon(
			size_t layer_id)
		{
			return RawHorizon{
				GridSize{reservoir_factory->nx(), reservoir_factory->ny(), 1 },
				reservoir_factory->shift(),
				reservoir_factory->planar_step(),
				VolumeData{
					reservoir_factory->Volume().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Volume().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				XCellCenter{
					reservoir_factory->xcenter().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->xcenter().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				YCellCenter{
					reservoir_factory->ycenter().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->ycenter().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				ZCellCenter{
					reservoir_factory->zcenter().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->zcenter().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				CellThickness{
					reservoir_factory->cellThickness().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->cellThickness().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				GridBounds{
					reservoir_factory->gridBounds()},
				PorosityData{
					reservoir_factory->Porosity().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Porosity().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				PermeabilityData{
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				PermeabilityData{
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				ActiveCellData{
					reservoir_factory->ActiveCells().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->ActiveCells().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				InitialSaturationData{
					reservoir_factory->Saturation().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Saturation().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				InitialPressureData{
					reservoir_factory->Pressure().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Pressure().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				phase_factory->CreateDefaultOil(),
				phase_factory->CreateDefaultWater(),
				other_factory->CreateDefaultOthers()
			};
		};

		DevelopedHorizon HorizonFactory::createSingleLayerHorizon(
			size_t layer_id)
		{
			return DevelopedHorizon{
				GridSize{reservoir_factory->nx(), reservoir_factory->ny(), 1 },
				reservoir_factory->shift(),
				reservoir_factory->planar_step(),
				VolumeData{
					reservoir_factory->Volume().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Volume().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				XCellCenter{
					reservoir_factory->xcenter().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->xcenter().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				YCellCenter{
					reservoir_factory->ycenter().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->ycenter().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				ZCellCenter{
					reservoir_factory->zcenter().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->zcenter().begin() + reservoir_factory->planar_size() * (1 + layer_id) },
				CellThickness{
					reservoir_factory->cellThickness().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->cellThickness().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				GridBounds{
					reservoir_factory->gridBounds()},
				PorosityData{
					reservoir_factory->Porosity().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Porosity().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				PermeabilityData{
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				PermeabilityData{
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->PermeabilityX().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				ActiveCellData{
					reservoir_factory->ActiveCells().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->ActiveCells().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				InitialSaturationData{
					reservoir_factory->Saturation().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Saturation().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				InitialPressureData{
					reservoir_factory->Pressure().begin() + reservoir_factory->planar_size() * layer_id,
					reservoir_factory->Pressure().begin() + reservoir_factory->planar_size() * (1 + layer_id)},
				phase_factory->CreateDefaultOil(),
				phase_factory->CreateDefaultWater(),
				other_factory->CreateDefaultOthers(),
				MERofAllWells{well_factory->MER().getMERinLayer(layer_id)},
			//	AccumulatedPerfsInLayer{well_factory->Perforations().accumulatedPerforationsInLayer(layer_id)},
				WellJobsContainer{well_factory->jobsInLayer(layer_id)},
				WellNames{well_factory->WellName()},
				WellPositions{well_factory->positions()}
			};
		};

	} // factories
} // reservoir_simulator