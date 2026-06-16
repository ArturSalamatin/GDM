#include "ReservoirFactory.h"
#include "ExceptionFactory.h"

namespace reservoir_simulator
{
	std::vector<double> float2doubleVector(const std::vector<float>& v)
	{
		std::vector<double> out(v.size(), 0.0);
		for (size_t l = 0; l < v.size(); ++l)
			out[l] = (double)v[l];
		return out;
	}
	std::vector<bool> float2boolVector(const std::vector<float>& v)
	{
		std::vector<bool> out(v.size(), 0.0);
		for (size_t l = 0; l < v.size(); ++l)
			out[l] = (v[l] > 0.0) ? true : false;
		return out;
	}

	namespace factories
	{
		GridSize modelParams2GridSize(const grdecl_memory::DBLoader::ModelParametrs& mp)
		{
			return { mp.Nx, mp.Ny, mp.Nz };
		}


/////////////////// GridGeometryFactory
		GridGeometryFactory::GridGeometryFactory(ConnectionFactory& connection) :
			grid_size{ modelParams2GridSize(connection.Loader().GetModelParams()) },
			grid_shift{ get_grid_shift(connection) },
			grid_bounds{ set_grid_bounds(connection) },
			volume( size(), 250 )  /*{float2doubleVector(connection.Loader().LoadValues("PORO_VOLUME"))}*/,
			x_center(size(),0.0),
			y_center(size(),0.0),
			z_center(size(),0.0),
			cell_thickness(size(), 0.0)
		{
			set_cell_centers();
		}

		size_t GridGeometryFactory::nx() const { return grid_size.Nx; }
		size_t GridGeometryFactory::ny() const { return grid_size.Ny; }
		size_t GridGeometryFactory::nz() const { return grid_size.Nz; }
		size_t GridGeometryFactory::size() const { return planar_size() * nz(); }
		size_t GridGeometryFactory::planar_size() const { return nx() * ny(); }
		double GridGeometryFactory::xstep() const { return block_size.step_x; }
		double GridGeometryFactory::ystep() const { return block_size.step_y; }
		double GridGeometryFactory::xshift() const { return grid_shift.shiftX; };
		double GridGeometryFactory::yshift() const { return grid_shift.shiftY; };
		constexpr double GridGeometryFactory::zshift() const { return 0.0; };
		const VolumeData& GridGeometryFactory::Volume() const { return volume; }
		const GridSize& GridGeometryFactory::gridSize() const { return grid_size; }
		GridShift GridGeometryFactory::get_grid_shift(ConnectionFactory& connection)
		{
			std::vector<float> vecVal{ connection.Loader().LoadValues("SHIFT") };
			return { vecVal[0], vecVal[1] };
		}
		void GridGeometryFactory::set_cell_centers()
		{
			double hx{ (xmax() - xmin()) / nx() };
			double hy{ (ymax() - ymin()) / ny() };

			block_size.step_x = std::abs(hx);
			block_size.step_y = std::abs(hy);

			double area{ std::abs(hx * hy) };

			double cornerX{ xmin() };
			double cornerY{ ymin() };
			double cornerZ{ 0.0 };

			// vertical depth downwards
			std::vector<double> accum(planar_size(), 0.0);
			for (size_t k = 0; k < nz(); k++)
				for (size_t j = 0; j < ny(); j++)
					for (size_t i = 0; i < nx(); i++)
					{
						size_t l{ planar_size() * k + nx() * j + i };
						double hz{ volume[l] / area };
						size_t lh{ nx() * j + i };

						x_center[l] = ((i + 0.5)* hx + cornerX);
						y_center[l] = ((j + 0.5)* hy + cornerY);
						z_center[l] = (accum[lh] + hz / 2.0 + cornerZ);
						cell_thickness[l] = hz;

						accum[lh] += hz;
					}
		}


/////////////////// RawReservoirFactory
		RawReservoirFactory::RawReservoirFactory(ConnectionFactory& connection) noexcept :
			GridGeometryFactory{ connection },
			porosity{ float2doubleVector(connection.Loader().LoadValues("PORO")) },
			permeability_x{ float2doubleVector(connection.Loader().LoadValues("PERMX")) },
			permeability_y{ float2doubleVector(connection.Loader().LoadValues("PERMY")) },
			permeability_z{ float2doubleVector(connection.Loader().LoadValues("PERMZ")) },
			active_cells{float2boolVector(connection.Loader().LoadValues("ACTNUM"))},
			initial_oil_saturation{default_oil_saturation()},
			initial_pressure{ default_pressure()}
		{

			if (permeability_y.size() == 0)
			{
				WarningFactory::NoPermeabilityYData();
				permeability_y = permeability_x;
			}
			if (permeability_z.size() == 0)
			{
				WarningFactory::NoPermeabilityZData();
				permeability_z = permeability_x;
			}
			if (porosity.size() == 0)
			{
				double val = 0.15;
				WarningFactory::NoPorosityData(val);
				porosity = std::vector<double>(permeability_x.size(), val);
			}
			active_cells = std::vector<bool>(active_cells.size(), true);
			permeability_x = std::vector<double>(permeability_x.size(), 400);
			permeability_y = std::vector<double>(permeability_y.size(), 400);
			permeability_z = std::vector<double>(permeability_z.size(), 400);
			porosity = std::vector<double>(porosity.size(), 0.15);
		}
		InitialSaturationData RawReservoirFactory::default_oil_saturation() const
		{
			return InitialSaturationData(size(), 1.0);
		}
		InitialPressureData RawReservoirFactory::default_pressure() const
		{
			return InitialPressureData(size(), 1);
		}

	} // factories
} // reservoir_simulator