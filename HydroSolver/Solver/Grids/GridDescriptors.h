#pragma once
#include <cmath>
#include <vector>


namespace reservoir_simulator
{
	/// <summary>
	/// Grid size in three directions
	/// </summary>
	using GridSize = struct {
		size_t Nx;
		size_t Ny;
		size_t Nz;
	};

	/// <summary>
	/// Shift of the reservoir grid with respect to wells
	/// </summary>
	struct GridShift
	{
		float shiftX, shiftY;
	};

	class GridBounds
	{
	public:
		GridBounds(double x_min, double y_min, double x_max, double y_max) noexcept :
			x_min{ x_min }, y_min{ y_min }, x_max{ x_max }, y_max{ y_max },
			x_length{ std::abs(x_max - x_min) },
			y_length{ std::abs(y_max - y_min) }
		{}

		double x_min, y_min, x_max, y_max, x_length, y_length;
	};

	using BlockSize = struct { double step_x, step_y; };

	template<typename T>
	using SomeCube = std::vector<T>;
	using PorosityData = SomeCube<double>;
	using PermeabilityData = SomeCube<double>;
	using ActiveCellData = SomeCube<bool>;
	using InitialSaturationData = SomeCube<double>;
	using InitialPressureData = SomeCube<double>;
	using VolumeData = SomeCube<double>;

	using XCellCenter = std::vector<double>;
	using YCellCenter = XCellCenter;
	using ZCellCenter = XCellCenter;
	using CellThickness = XCellCenter;
} // reservoir_simulator