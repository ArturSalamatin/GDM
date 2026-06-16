#pragma once
#include "../stdafx.h"

#include "ConnectionFactory.h"
#include "../Descriptors/Descriptors.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/// <summary>
		/// Generates grid geometry from a DB
		/// </summary>
		class GridGeometryFactory
		{
		public:
			GridGeometryFactory(ConnectionFactory& connection);
			size_t nx() const;
			size_t ny() const;
			size_t nz() const;
			size_t size() const;
			size_t planar_size() const;

			double xstep() const;
			double ystep() const;
			BlockSize planar_step() const { return block_size; }

			double xshift() const;
			double yshift() const;
			constexpr double zshift() const;
			GridShift shift() const { return grid_shift; }

			double xmin() const { return grid_bounds.x_min; }
			double ymin() const { return grid_bounds.y_min; }
			double xmax() const { return grid_bounds.x_max; }
			double ymax() const { return grid_bounds.y_max; }

			const VolumeData& Volume() const;
			const GridSize& gridSize() const;
			const XCellCenter& xcenter() const { return x_center; }
			const YCellCenter& ycenter() const { return y_center; }
			const ZCellCenter& zcenter() const { return z_center; }
			const CellThickness& cellThickness() const { return cell_thickness; }
			const GridBounds& gridBounds() const { return grid_bounds; }
		protected:
			GridSize grid_size;
			GridShift grid_shift;
			GridBounds grid_bounds;
			BlockSize block_size;
			VolumeData volume;
			XCellCenter x_center;
			YCellCenter y_center;
			ZCellCenter z_center;
			CellThickness cell_thickness;
		private:
			GridShift get_grid_shift(ConnectionFactory& connection);

			/// <summary>
			/// Set cell centers from cell volumes and other geometry
			/// </summary>
			void set_cell_centers();

			GridBounds set_grid_bounds(ConnectionFactory& connection)
			{
				auto pillars{ connection.Loader().LoadPillars() };

				return { pillars[0]+grid_shift.shiftX, pillars[1] + grid_shift.shiftY,
					pillars[6 * (nx() + 1) * (ny() + 1) - 3] + grid_shift.shiftX, 
					pillars[6 * (nx() + 1) * (ny() + 1) - 2] + grid_shift.shiftY };

				//auto MeshDiagonals = model.GetDiagonals(&model.ReadPillars());
				//itsXmin = std::min(MeshDiagonals[0][0][0][0], MeshDiagonals.back().back()[0][0]);
				//itsYmin = std::min(MeshDiagonals[0][0][0][1], MeshDiagonals.back().back()[0][1]);
				//itsXmax = std::max(MeshDiagonals[0][0][0][0], MeshDiagonals.back().back()[0][0]);
				//itsYmax = std::max(MeshDiagonals[0][0][0][1], MeshDiagonals.back().back()[0][1]);
				//itsXdim = xmax() - xmin();
				//itsYdim = ymax() - ymin();
			}
		};

		/// <summary>
		/// Class generating descriptors of cubes from DB connection. Simply reads the cubes.
		/// </summary>
		class RawReservoirFactory : public GridGeometryFactory
		{
		public:
			RawReservoirFactory(ConnectionFactory& connection) noexcept;

			InitialSaturationData default_oil_saturation() const;

			InitialPressureData default_pressure() const;

			const PermeabilityData& PermeabilityX() const { return permeability_x; }
			const PermeabilityData& PermeabilityY() const { return permeability_y; }
			const PermeabilityData& PermeabilityZ() const { return permeability_z; }
			const PorosityData& Porosity() const { return porosity; }
			const ActiveCellData& ActiveCells() const { return active_cells; }
			const InitialSaturationData& Saturation() const { return initial_oil_saturation; }
			const InitialPressureData& Pressure() const { return initial_pressure; }

		protected:
			PorosityData porosity;
			PermeabilityData permeability_x, permeability_y, permeability_z;
			ActiveCellData active_cells;
			InitialSaturationData initial_oil_saturation;
			InitialPressureData initial_pressure;
		};

		/// <summary>
		/// The factory imposes constraints on the cubes read by the 
		/// </summary>
		template<typename Conversion>
		class ReservoirFactory : public RawReservoirFactory
		{
		public:
			ReservoirFactory(ConnectionFactory& connection) noexcept :
				RawReservoirFactory{ connection }
			{
				set_active_cells();
				Conversion::convertPermeability(permeability_x);
				Conversion::convertPermeability(permeability_y);
				Conversion::convertPermeability(permeability_z);
				Conversion::convertPressure(initial_pressure);
			}

		protected:
			void set_active_cells()
			{
				double minPoro{ 0.01 }, minPerm{ 1.0 }, minHeight{ 0.1 };
				for (size_t l = 0; l < this->size(); ++l)
					if (porosity[l] < minPoro || permeability_x[l] < minPerm //|| volume[l] < xstep() * ystep() * minHeight
						)
						active_cells[l] = false;
			}
		};

		/// <summary>
		/// Converts permeability data to SI units
		/// </summary>
		class UnitsConversionFactory
		{
		protected:
#pragma region unitsConversionFactors
			// units conversion factors
			static constexpr float permeabilityConversion = 0.9869E-15f; // from mD to m^2 (SI units)
			static constexpr double porosityConversion = 1;
			static constexpr double saturationConversion = 1;
			static constexpr double spatialConversion = 1;
			static constexpr double pressureConversion = 101325; // from atm to Pa
#pragma endregion
		public:
			static void convertPermeability(PermeabilityData& permeability) noexcept
			{
				for (auto& e : permeability)
					e *= permeabilityConversion;
			}
			static void convertPressure(InitialPressureData& pressure) noexcept
			{
				for (auto& e : pressure)
					e *= pressureConversion;
			}
		};

	} // factories
} // reservoir_simulator

