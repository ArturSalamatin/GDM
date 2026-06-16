#pragma once

#include "../../Solver/Grids/RawHorizon.h"
#include "../../Solver/Grids/AbstractGrid.h"
#include "Cells/TwoPhaseFlowCell.h"

namespace reservoir_simulator
{
	using namespace cell;

	namespace grid
	{
		class RawOilField
		{
		public:
			RawOilField() noexcept = default;
			RawOilField(
				const OilPhaseProperty& oil, const WaterPhaseProperty& water,
				const OtherProperties& other_properties) noexcept
			{
				// set static properties of cells
				std::array<double, 8> ConstantPointProperties{
					oil.Viscosity(), water.Viscosity(),
					oil.Density(), water.Density(), other_properties.g ,
					oil.Compressibility(),  water.Compressibility(),  water.ReferencePressure() };
				PhysPropCell::set_constantPointProperties(ConstantPointProperties);

			}
		};

		class OilField : 
			public RawOilField, 
			public SomeStructuredGrid3Dim<TwoPhaseFlowCell>
		{
			using base = SomeStructuredGrid3Dim<TwoPhaseFlowCell>::SomeGrid<TwoPhaseFlowCell, Dim3Cell>;
			using base::Cells;
			using base::CellsInactive;
			using base::totalCellNmbr;

		protected:
			TwoPhaseFlowCell MakeCell(const int l, const std::vector<double>& permeability, const std::vector<double>& porosity,
				const double resOilSaturation, const double resWaterSaturation,
				const std::vector<double>& s_water, const std::vector<double>& p,
				const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z,
				double hx, double hy, const std::vector<double>& hz);

			std::vector<TwoPhaseFlowCell> SetActiveCells(const std::vector<double>& permeability, const std::vector<double>& porosity,
				const double resOilSaturation, const double resWaterSaturation,
				const std::vector<double>& s_water, const std::vector<double>& p,
				const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z,
				double hx, double hy, const std::vector<double>& hz, const std::vector<bool>& active_cells);

			std::vector<TwoPhaseFlowCell> SetInActiveCells(const std::vector<double>& permeability, const std::vector<double>& porosity,
				const double resOilSaturation, const double resWaterSaturation,
				const std::vector<double>& s_water, const std::vector<double>& p,
				const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z,
				double hx, double hy, const std::vector<double>& hz, const std::vector<bool>& active_cells);

		public:
			OilField(const RawHorizon& horizon) noexcept;

			OilField() noexcept;
			OilField(const std::vector<double>& permeability, const std::vector<double>& porosity, 
				const double resOilSaturation, const double resWaterSaturation,
				const std::vector<double>& s_water, const std::vector<double>& p,
				const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z,
				double hx, double hy, const std::vector<double>& hz, const std::vector<bool>& active_cells,
				const GridSize& grid_size,
				const OilPhaseProperty& oil, const WaterPhaseProperty& water,
				const OtherProperties& other) noexcept;

		};
	} // grid
} // reservoir_simulator