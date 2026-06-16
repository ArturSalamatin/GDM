#pragma once

#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <exception>

#undef min
#undef max

namespace reservoir_simulator
{
	namespace phasePortrait
	{
		typedef std::array<double, 3> Direction;

		class Point
		{
		public:
			double x() const { return X; }
			double y() const { return Y; }

			std::wstring print_json() const
			{
				return L"[" + std::to_wstring((int)x()) + L"," + std::to_wstring((int)y()) + L"]";
			}

			double Area(const phasePortrait::Point& P) const
			{
				return abs(P.x() - x()) * (P.y() - y());
			}
			static double Area(const Point& P, const Point& Q)
			{
				return abs((P.x() - Q.x()) * (P.y() - Q.y()));
			}
			static std::array<double, 4> Area(const Point& P, const std::array<Point, 4>& Q)
			{
				std::array<double, 4> result;
				for (int i = 0; i < 4; i++)
					result[i] = Point::Area(P, Q[i]);
				return result;
			}

		public:
			Point(double x_, double y_) { X = x_; Y = y_; }
			double distance(const Point& p) const
			{
				return sqrt(pow((p.x() - this->x()), 2) + pow((p.y() - this->y()), 2));
			}

		private:
			double X, Y;
		};

		class trPoint
		{
			Point p;
			double time;
			double distance;
		public:
			trPoint(double t, double s_, const Point& p_) :p{ p_ }, time{ t }, distance{ s_ }{};
			trPoint(double t, double s_, double x, double y) :trPoint{ t, s_, Point(x, y) } {};

			trPoint operator +(const trPoint& P1)
			{
				return trPoint(P1.t() + (*this).t(), P1.s() + (*this).s(), P1.x() + (*this).x(), P1.y() + (*this).y());
			}
			trPoint operator/(double factor)
			{
				return trPoint((*this).t() / factor, (*this).s() / factor, (*this).x() / factor, (*this).y() / factor);
			}

		public:
			double x() const { return p.x(); }
			double y() const { return p.y(); }
			double t() const { return time; }
			double s() const { return distance; }
			const Point& P() const { return p; }

			trPoint Move(Direction& direction, double dt)
			{
				auto& P = (*this);
				return trPoint(P.t() + dt, P.s() + dt * direction[0], P.x() + dt * direction[1], P.y() + dt * direction[2]);
			}
			trPoint Move(Direction direction, double dt) const
			{
				auto& P = (*this);
				return trPoint(P.t() + dt, P.s() + dt * direction[0], P.x() + dt * direction[1], P.y() + dt * direction[2]);
			}

			bool is_normal() const
			{
				return isfinite(x()) && isfinite(y()) && isfinite(t()) && isfinite(s());
			}
		};

		class Contour
		{
		private:
			std::vector<double> X{}, Y{};
		public:
			Contour(const std::vector<double>& x = {}, const std::vector<double>& y = {}) :X{ x }, Y{ y } {};
			std::wstring print_json() const
			{
				if (size() == 0)
					return L"[]";
				std::wstring result = L"[";
				for (size_t i = 0; i < size(); i++)
				{
					if (i > 0) result += L",";
					result += Point(X[i], Y[i]).print_json();
				}
				return result + L"]";
			}

			size_t size() const { return X.size(); }
			const std::vector<double>& x() const { return X; }
			const std::vector<double>& y() const { return Y; }
		};

		struct Coordinate { double x, y; };

		class geos_polygon
		{
			std::vector<Coordinate> coords_;
		public:
			geos_polygon() = default;
			geos_polygon(std::vector<Coordinate>&& init) : coords_(std::move(init)) {}
			const std::vector<Coordinate>& vertecies() const { return coords_; }
			size_t vertecies_nmbr() const { return coords_.size(); }
			bool contains(const geos_polygon&) const { return false; }
			bool intersects(const geos_polygon&) const { return false; }
			geos_polygon Union(const geos_polygon& p) const { return *this; }
			geos_polygon combine_with(const geos_polygon& p) const { return *this; }
			std::wstring print_json() const { return L"[]"; }
		};

	} // phasePortrait
} // reservoir_simulator
