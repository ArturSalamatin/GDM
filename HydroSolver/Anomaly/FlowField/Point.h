#pragma once

#include <array>
#include <algorithm>
#include <exception>
#include <mutex>

#include "../../Helpers/LogFile.h"
#include "../../Utils/JSON/JSONCreate.h"
#include "../../Utils/JSON/geojson.h"

#undef min
#undef max

#include <geos/geom.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/algorithm/ConvexHull.h>
#include <geos/operation/union/CascadedPolygonUnion.h>

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
				return JSON::CreateJSON::CreateArray({ std::to_wstring((int)x()), std::to_wstring((int)y()) });
			}

			// calculates the area of a square based on two points (this; P)
			double Area(const phasePortrait::Point& P) const
			{
				return abs(P.x() - x()) * (P.y() - y());
			}
			static double Area(const reservoir_simulator::phasePortrait::Point& P, const reservoir_simulator::phasePortrait::Point& Q)
			{
				return abs((P.x() - Q.x()) * (P.y() - Q.y()));
			}
			static std::array<double, 4> Area(const reservoir_simulator::phasePortrait::Point& P, const std::array<reservoir_simulator::phasePortrait::Point, 4>& Q)
			{
				std::array<double, 4> result;
				for (int i = 0; i < 4; i++)
					result[i] = reservoir_simulator::phasePortrait::Point::Area(P, Q[i]);
				return result;
			}

		public:
			//	Point() {} // default constructor
			Point(double x_, double y_) { X = x_; Y = y_; }
			double distance(const Point& p) const
			{
				return sqrt(pow((p.x() - this->x()), 2) + pow((p.y() - this->y()), 2));
			}

		private:
			double X, Y; // Cartesian coordinates of a trajectory point
		};

		/// <summary>
		/// A point on the trajectory in the phase portrait
		/// The problem is 3D, (x,y,t) as functions of distance s along the trajectory
		/// </summary>
		class trPoint
		{
			reservoir_simulator::phasePortrait::Point p;
			double time;
			double distance;
		public:
			//	trPoint() {} // default constructor
			trPoint(double t, double s_, const reservoir_simulator::phasePortrait::Point& p_) :p{ p_ }, time{ t }, distance{ s_ }{};
			trPoint(double t, double s_, double x, double y) :trPoint{ t, s_, reservoir_simulator::phasePortrait::Point(x, y) } {};

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
			const reservoir_simulator::phasePortrait::Point& P() const { return p; }

			reservoir_simulator::phasePortrait::trPoint Move(Direction& direction, double dt)
			{
				auto& P = (*this);
				return reservoir_simulator::phasePortrait::trPoint(P.t() + dt, P.s() + dt * direction[0], P.x() + dt * direction[1], P.y() + dt * direction[2]);
			}
			reservoir_simulator::phasePortrait::trPoint Move(Direction direction, double dt) const
			{
				auto& P = (*this);
				return reservoir_simulator::phasePortrait::trPoint(P.t() + dt, P.s() + dt * direction[0], P.x() + dt * direction[1], P.y() + dt * direction[2]);
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

				std::vector<std::wstring> result;
				for (int i = 0; i < size(); i++)
					result.push_back(Point(x()[i], y()[i]).print_json());

				return JSON::CreateJSON::CreateArray(std::move(result));
			}

			size_t size() const
			{
				return X.size();
			}

			const std::vector<double>& x() const { return X; }
			const std::vector<double>& y() const { return Y; }
		};

		class Polygon
		{

		};

		class geos_geometry
		{
		public:
			static std::unique_ptr<geos::geom::PrecisionModel> pm;
			static geos::geom::GeometryFactory::Ptr global_factory;
		};

		//class geos_contour:public geos_geometry
		//{
		//public:
		//	std::vector<geos::geom::Coordinate> its_contour;
		//	inline int size() const { return its_contour.size(); };
		//	void close() { its_contour.push_back(its_contour[0]); }
		//	bool is_closed() { return its_contour[0].equals2D(its_contour.back()); }
		//public:
		//	geos_contour(const std::vector<geos::geom::Coordinate>& init) : its_contour{ init } 
		//	{ 
		//		if (!is_closed()) close(); 
		//		auto its_contour_temp{ its_contour };
		//	};
		//	geos_contour(std::vector<geos::geom::Coordinate>&& init) : its_contour{ std::move(init) } 
		//	{
		//		if (!is_closed()) close();
		//		auto its_contour_temp{ its_contour };
		//	};
		//	geos_contour() :its_contour{} {};
		//	void append(const std::vector<geos::geom::Coordinate>& c)
		//	{
		//		std::copy(c.begin(), c.end(), its_contour.end());
		//		close();
		//	}
		//	
		//	void rappend(const std::vector<geos::geom::Coordinate>& c)
		//	{
		//		int orig_size = its_contour.size();
		//		int input_size = c.size();
		//		its_contour.resize(input_size + orig_size);
		//		for (int i = orig_size, j = input_size - 1; i < orig_size + input_size; i++, j--)
		//			its_contour[i] = c[j];
		//		close();
		//	}
		//	std::wstring print_json() const
		//	{
		//		if (size() == 0)
		//			return L"[]";
		//		std::vector<std::wstring> result;
		//		for (int i = 0; i < size(); i++)
		//			result.push_back(coordinate_print_json(contour()[i]));
		//		return JSON::CreateJSON::CreateArray(std::move(result));
		//	}
		//	bool contains(const geos_contour& p) const
		//	{
		//	//	static auto pm = std::make_unique<geos::geom::PrecisionModel>(geos::geom::PrecisionModel::FLOATING); // creates precision model
		//	//	static auto global_factory = geos::geom::GeometryFactory::create(pm.get());
		//		auto temp1 = (*this).copy();
		//		auto ext_poly = global_factory->createPolygon(std::move(temp1));
		//		auto temp2 = p.copy();
		//		auto int_poly = global_factory->createPolygon(std::move(temp2));
		//		return ext_poly->contains(int_poly.get());
		//	}
		//	bool intersects(const geos_contour& p) const
		//	{
		//		auto temp1 = (*this).contour();
		//		auto ext_poly = global_factory->createPolygon(std::move(temp1));
		//		auto temp2 = p.contour();
		//		auto int_poly = global_factory->createPolygon(std::move(temp2));
		//		return ext_poly->intersects(int_poly.get());
		//	}
		//	geos::geom::Geometry::Ptr Union(const geos_contour& p) const
		//	{
		//		auto temp1 = (*this).contour();
		//		auto ext_poly = global_factory->createPolygon(std::move(temp1));
		//		auto temp2 = p.contour();
		//		auto int_poly = global_factory->createPolygon(std::move(temp2));
		//		std::vector<geos::geom::Polygon*> polys{ int_poly.get(), ext_poly.get() };
		//		geos::operation::geounion::CascadedPolygonUnion unite(&polys);
		//		auto shape = unite.Union();
		//		return shape;
		//	}
		//	std::wstring coordinate_print_json(const geos::geom::Coordinate& p) const
		//	{
		//		return JSON::CreateJSON::CreateArray({ std::to_wstring((int)p.x), std::to_wstring((int)p.y) });
		//	}
		//	inline const std::vector<geos::geom::Coordinate>& contour() const { return its_contour; };
		//	std::vector<geos::geom::Coordinate> copy() const { auto result = its_contour; return result; };
		//};

	//	static std::mutex geojson_writer;

		class geos_polygon :public geos_geometry
		{
		protected: // fields, incapsulated data
			geos::geom::Polygon::Ptr its_polygon;
			std::vector<GeoJsonEngine::Parameters> its_features;
		public: // constructors
			geos_polygon(std::vector<geos::geom::Coordinate>&& init, std::vector<GeoJsonEngine::Parameters>&& p = {}) :
				its_polygon{ create_polygon_from_points(std::move(init)) },
				its_features{ std::move(p) } {};
			geos_polygon() :
				its_polygon{ global_factory->createPolygon() },
				its_features{} {};
			void assign_features(const std::vector<GeoJsonEngine::Parameters>& p);
		public: // access methods
			std::vector<geos::geom::Coordinate> vertecies() const;
			size_t vertecies_nmbr() const;
			const geos::geom::Polygon::Ptr& polygon() const { return its_polygon; };
			const std::vector<GeoJsonEngine::Parameters>& features() const { return its_features; };
		public: // print methods
			void print_geo_json(GeoJsonEngine& filename) const;
			std::wstring print_json() const;
		public: // operations with object
			bool contains(const geos_polygon& p) const { return polygon()->contains(p.polygon().get()); }
			bool intersects(const geos_polygon& p) const { return polygon()->intersects(p.polygon().get()); }
			geos_polygon Union(const geos_polygon& p) const;
			geos_polygon combine_with(const geos_polygon& p) const;
			//	std::vector<geos::geom::Coordinate> copy() const { auto result = its_contour; return result; };
		private: // internal use methods
			geos::geom::Polygon::Ptr create_polygon_from_points(std::vector<geos::geom::Coordinate>&& init);
			bool is_coord_sequence_closed(const std::vector<geos::geom::Coordinate>& init);
			void close_coord_sequence(std::vector<geos::geom::Coordinate>& init);
			std::wstring coordinate_print_json(const geos::geom::Coordinate& p) const
			{
				return JSON::CreateJSON::CreateArray({ std::to_wstring((int)p.x), std::to_wstring((int)p.y) });
			}

			template<typename T>
			void rappend(std::vector<T>& c, const std::vector<T>& other) const;
		};
	} // phasePortrait
} // reservoir_simulator