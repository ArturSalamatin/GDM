#include <mutex>

#include "Point.h"
#include <geos/geom.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/operation/valid/TopologyValidationError.h>

namespace reservoir_simulator
{
	namespace phasePortrait
	{
		std::unique_ptr<geos::geom::PrecisionModel> 
			geos_geometry::pm{ std::make_unique<geos::geom::PrecisionModel>(geos::geom::PrecisionModel::FLOATING) }; // creates precision model
		geos::geom::GeometryFactory::Ptr 
			geos_geometry::global_factory{ geos::geom::GeometryFactory::create(pm.get()) };

		geos::geom::Polygon::Ptr geos_polygon::create_polygon_from_points(std::vector<geos::geom::Coordinate>&& init)
		{
			if (!is_coord_sequence_closed(init)) close_coord_sequence(init);
			return geos_geometry::global_factory->createPolygon(std::move(init));
		}

		bool geos_polygon::is_coord_sequence_closed(const std::vector<geos::geom::Coordinate>& init)
		{
			return init[0].equals2D(init.back());
		}

		void geos_polygon::close_coord_sequence(std::vector<geos::geom::Coordinate>& init)
		{
			init.emplace_back(init[0]);
		}

		void geos_polygon::assign_features(const std::vector<GeoJsonEngine::Parameters>& p)
		{
			if (its_features.empty())
				its_features = p;
			else
				its_features.insert(its_features.end(), p.begin(), p.end());
		}

		std::vector<geos::geom::Coordinate> geos_polygon::vertecies() const
		{
			std::vector<geos::geom::Coordinate> points_set;
			its_polygon->getCoordinates()->toVector(points_set);
			return points_set;
		}
		size_t geos_polygon::vertecies_nmbr() const
		{
			return its_polygon->getCoordinates()->size();
		}
		void geos_polygon::print_geo_json(GeoJsonEngine& sraka) const
		{
			// its_features == {{L"time", 10, L""}, {L"well_name", 0, L""}}
	//		geojson_writer.lock();
			sraka.AddFeature(GeoJsonEngine::Geom::Polygon, polygon().get(), features());
	//		geojson_writer.unlock();
		}
		
		std::wstring geos_polygon::print_json() const
		{
			if (vertecies_nmbr() == 0)
				return L"[]";

			std::vector<std::wstring> result;
			for (int i = 0; i < vertecies_nmbr(); i++)
				result.push_back(coordinate_print_json(vertecies()[i]));

			return JSON::CreateJSON::CreateArray(std::move(result));
		}
		/*geos::geom::Geometry::Ptr geos_polygon::Union(const geos_polygon& p) const
		{
			std::vector<geos::geom::Polygon*> polys{ p.polygon().get(), polygon().get() };
			geos::operation::geounion::CascadedPolygonUnion unite(&polys);
			auto shape = unite.Union();
			return shape;
		}*/

		/*geos_polygon geos_polygon::Union(const geos_polygon& p) const
		{
			std::vector<geos::geom::Polygon*> polys{ 
				static_cast<geos::geom::Polygon*>(p.polygon().get()), 
				static_cast<geos::geom::Polygon*>(polygon().get()) };
			geos::operation::geounion::CascadedPolygonUnion unite(&polys);
			auto shape = unite.Union();

			std::vector<geos::geom::Coordinate> coordinates;
			reinterpret_cast<geos::geom::Polygon*>(shape.get())->getCoordinates()->toVector(coordinates);
			return geos_polygon{ std::move(coordinates) };
		}*/

		geos_polygon geos_polygon::Union(const geos_polygon& p) const
		{
			std::unique_ptr<geos::geom::Geometry> shape;
				shape = this->polygon()->Union(p.polygon().get());



			/*auto result = std::make_unique<geos::geom::Polygon>(static_cast<geos::geom::Polygon*>(united_domain.get()));

			std::vector<geos::geom::Polygon*> polys{
				static_cast<geos::geom::Polygon*>(p.polygon().get()),
				static_cast<geos::geom::Polygon*>(polygon().get()) };
			geos::operation::geounion::CascadedPolygonUnion unite(&polys);
			auto shape = unite.Union();*/

			std::vector<geos::geom::Coordinate> coordinates;
			reinterpret_cast<geos::geom::Polygon*>(shape.get())->getCoordinates()->toVector(coordinates);
			return geos_polygon{ std::move(coordinates) };
		}

		geos_polygon geos_polygon::combine_with(const geos_polygon& p) const
		{
			std::vector<GeoJsonEngine::Parameters> features{ this->features() };
			rappend(features, p.features());

			if (this->contains(p))
			{ // a shell with a hole
				std::vector<geos::geom::Coordinate> coords{ this->vertecies() };
				rappend(coords, p.vertecies());
				return geos_polygon{ std::move(coords), std::move(features) };
			}
			else
			{
				//if (this->intersects(p))
				//{ // union of two polygons
				//	geos_polygon result;
				//	try {
				//		result = this->Union(p);
				//		result.assign_features(features);
				//	}
				//	catch(geos::operation::valid::TopologyValidationError& e)
				//	{
				//		std::cout << "topology validation err.\n";
				//	}
				//	catch (std::exception& e)
				//	{
				//		LogFileSpace::LogFile::WriteLog(L"class_geos_polygon", L"method_combine_with", L"error", 
				//			L"Could not combine polygons. ", e.what());
				//	}
				//	return result;
				//}
				//else
				{ // convex hull of two sets of points
					std::vector<geos::geom::Coordinate> points_set{ this->vertecies() };
					rappend(points_set, p.vertecies());

					auto set = global_factory->createMultiPoint(std::move(points_set));
					points_set.clear(); // very important line!!
					set->convexHull()->getCoordinates()->toVector(points_set);
					return geos_polygon(std::move(points_set), std::move(features));
				}
			}
		}

		template<typename T>
		void geos_polygon::rappend(std::vector<T>& c, const std::vector<T>& other) const
		{
			size_t orig_size = c.size();
			size_t input_size = other.size();
			c.resize(input_size + orig_size);
			for (size_t i = orig_size, j = input_size - 1; i < c.size(); i++, j--)
				c[i] = other[j];
		}
	}
}