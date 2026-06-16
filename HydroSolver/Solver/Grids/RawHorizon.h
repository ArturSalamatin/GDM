#pragma once
#include <string>
#include <fstream>
#include <cstdio>
#include "GridDescriptors.h"
#include "PropertyDescriptor.h"
//#include "Descriptors.h"
//#include "../defines.h"

namespace reservoir_simulator
{
	class RawHorizon
	{
	public:
		GridSize grid_size;
		GridShift grid_shift;
		BlockSize block_size;
		VolumeData volume;
		XCellCenter x_center;
		YCellCenter y_center;
		ZCellCenter z_center;
		CellThickness cell_thickness;
		GridBounds grid_bounds;

		PorosityData porosity;
		PermeabilityData permeability_x, permeability_y;
		ActiveCellData active_cells;
		InitialSaturationData initial_oil_saturation;
		InitialPressureData initial_pressure;

		OilPhaseProperty oil;
		WaterPhaseProperty water;
		OtherProperties other;



		void PrintModelData(size_t frames) const
		{
			std::wstring fName{ L"ReservoirTestData//grid_descriptor.txt" };
			std::ofstream myfile{ fName, std::ios_base::out };

			char buffer[1000];
			snprintf(buffer, 1000, "%u;%u;%u;%u\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				grid_size.Nx, grid_size.Ny,
				grid_size.Nz, frames,
				grid_bounds.x_min, grid_bounds.y_min,
				grid_bounds.x_max, grid_bounds.y_max,
				block_size.step_x, block_size.step_y,
				0.0, 0.0);
			std::string result{ buffer };
			myfile << result << std::endl;
			myfile.close();
		}


		InitialSaturationData initial_water_saturation() const
		{
			InitialSaturationData water(initial_oil_saturation.size(), 0.0);
			for (size_t l = 0; l < water.size(); ++l)
				water[l] = 1- initial_oil_saturation[l];

			return water;
		}

		size_t GridSize() const
		{
			return GridPlanarSize() * grid_size.Nz;
		}

		size_t GridPlanarSize() const
		{
			return grid_size.Nx * grid_size.Ny;
		}

		size_t nx() const
		{
			return grid_size.Nx;
		}
		size_t ny() const
		{
			return grid_size.Ny;
		}
	};
} // reservoir_simulator