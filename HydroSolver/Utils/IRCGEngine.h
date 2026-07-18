#pragma once
#include "UniversalSVWriter.h"
//#include <geos/geom.h>
#include <map>

namespace IRCGEngine {
	class IGrid {
	public:
		IGrid(std::wstring path)
		{
			Path = path;
		}
//		virtual void Add(geos::geom::Point* p, float value) = 0;
		virtual void Add(std::pair<float, float> p, float value) = 0;
		virtual void Write() = 0;
	protected:
		std::wstring Path;
		std::wstring FloatToWS(float val);
	};

	class IrapClassicGrid : public IGrid {
	public:
		IrapClassicGrid(std::wstring path, 
			float dx,
			float dy,
			std::tuple<float, float, float, float> bounds) : IGrid(path)
		{
			_dx = dx;
			_dy = dy;
			Bounds = bounds;
			_nx = (std::get<1>(bounds) - std::get<0>(bounds)) / dx;
			_ny = (std::get<3>(bounds) - std::get<2>(bounds)) / dy;
		}
//		virtual void Add(geos::geom::Point* p, float value);
		void Add(std::pair<float, float> p, float value) override;
		void Write() override;
	private:
		float _dx = 0;
		float _dy = 0;
		std::tuple<float, float, float, float> Bounds;
		int _nx = 0;
		int _ny = 0;

		std::map<int, float> Buffer;
	};

	class XYZGrid : public IGrid
	{
	public:
		XYZGrid(std::wstring path) : IGrid(path) {
			Buffer += L"X\tY\tZ\n";
		}
//		virtual void Add(geos::geom::Point* p, float value);
		void Add(std::pair<float, float> p, float value) override;
		void Write() override;
	private:
		std::wstring Buffer;
	};
}