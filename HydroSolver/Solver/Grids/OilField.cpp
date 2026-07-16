#include "OilField.h"

namespace reservoir_simulator
{
	namespace grid
	{
		TwoPhaseFlowCell OilField::MakeCell(const size_t l, const std::vector<double>& permeability, const std::vector<double>& porosity, const double resOilSaturation, const double resWaterSaturation, const std::vector<double>& s_water, const std::vector<double>& p, const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z, double hx, double hy, const std::vector<double>& hz)
		{
			std::vector<double> size{ hx, hy, hz[l] };
			//	size.shrink_to_fit();
			std::vector<double> constProp{ permeability[l], porosity[l] , resOilSaturation, resWaterSaturation };
			//	constProp.shrink_to_fit();
			std::vector<double> center{ X[l], Y[l], Z[l] };
			//	center.shrink_to_fit();
			std::vector<double> varProp{ s_water[l], p[l] };
			//	varProp.shrink_to_fit();

			return TwoPhaseFlowCell(center, size, constProp, varProp);
		}

		std::vector<TwoPhaseFlowCell> OilField::SetActiveCells(
			const std::vector<double>& permeability, 
			const std::vector<double>& porosity, 
			double resOilSaturation, double resWaterSaturation, 
			const std::vector<double>& s_water, const std::vector<double>& p, 
			const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z, 
			double hx, double hy, const std::vector<double>& hz, 
			const std::vector<bool>& active_cells)
		{
			size_t sum = 0; // total number of active cells
			for (bool f : active_cells)
				if (f) sum++;

			totalCellNmbr = active_cells.size();
			std::vector<TwoPhaseFlowCell> myCells(sum);

			for (size_t l = 0, k = 0; l < totalCellNmbr; ++l)
				if (active_cells[l])
				{
					myCells[k] = MakeCell(l, permeability, porosity,
						resOilSaturation, resWaterSaturation, s_water, p, X, Y, Z, hx, hy, hz);
					if (myCells[k].Permeability() <= 0.0)
						throw std::runtime_error("Active cell with zero permeability is found.");
					if (myCells[k].Porosity() <= 0.0)
						throw std::runtime_error("Active cell with zero porosity is found.");
					k++;
				}

			return myCells;
		}

		std::vector<TwoPhaseFlowCell> OilField::SetInActiveCells(
			const std::vector<double>& permeability, 
			const std::vector<double>& porosity, 
			double resOilSaturation, double resWaterSaturation, 
			const std::vector<double>& s_water, const std::vector<double>& p, 
			const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z,
			double hx, double hy, const std::vector<double>& hz, 
			const std::vector<bool>& active_cells)
		{
			size_t sum = 0; // total number of not-active cells
			for (bool f : active_cells)
				if (!f) sum++;

			totalCellNmbr = active_cells.size();
			std::vector<TwoPhaseFlowCell> myInActiveCells(sum);

			for (size_t l = 0, k = 0; l < totalCellNmbr; ++l)
				if (!active_cells[l])
				{
					myInActiveCells[k] = MakeCell(l, permeability, porosity,
						resOilSaturation, resWaterSaturation, s_water, p, X, Y, Z, hx, hy, hz);
					k++;
				}
			return myInActiveCells;
		}

		OilField::OilField() noexcept = default;

		OilField::OilField(const std::vector<double>& permeability, const std::vector<double>& porosity,
			double resOilSaturation, double resWaterSaturation,
			const std::vector<double>& s_water, const std::vector<double>& p,
			const std::vector<double>& X, const std::vector<double>& Y, const std::vector<double>& Z,
			double hx, double hy, const std::vector<double>& hz, const std::vector<bool>& active_cells,
			const GridSize& grid_size,
			const OilPhaseProperty& oil, const WaterPhaseProperty& water,
			const OtherProperties& other) noexcept :
			RawOilField{oil, water, other},
			SomeStructuredGrid3Dim<TwoPhaseFlowCell>(
				SetActiveCells(
					permeability, porosity, 
					resOilSaturation, resWaterSaturation,
					s_water, p,
					X, Y, Z, hx, hy, hz, active_cells),
				SetInActiveCells(
					permeability, porosity, 
					resOilSaturation, resWaterSaturation,
					s_water, p, 
					X, Y, Z, hx, hy, hz, active_cells), 
				active_cells, grid_size)
		{
		}

		OilField::OilField(const RawHorizon& horizon) noexcept :
			OilField{ horizon.permeability_x, horizon.porosity, 0.0, 0.0,
				horizon.initial_water_saturation(), horizon.initial_pressure, horizon.x_center, horizon.y_center, horizon.z_center,
				horizon.block_size.step_x, horizon.block_size.step_y, horizon.cell_thickness, horizon.active_cells,
				horizon.grid_size,
				horizon.oil, horizon.water, horizon.other }
		{ }

	} // grid
} // reservoir_simulator