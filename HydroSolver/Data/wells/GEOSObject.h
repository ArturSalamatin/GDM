#pragma once
#include <geos/geom.h>
#include <geos/triangulate/VoronoiDiagramBuilder.h>
#include <map>
#include <list>
#include "Maps.h"
#include "GeosPoint.h"
#include <future>

namespace GeometryHandler {
	class GEOSObjectHandler {
	public:
		GEOSObjectHandler();
		~GEOSObjectHandler();
		GeosShell::GeosPoint CreatePoint(std::pair<float, float> coord);
		geos::geom::Polygon::Ptr CreatePolygon(std::list<std::pair<float, float>>& shell);
		std::shared_ptr<std::map<std::wstring, geos::geom::Geometry::Ptr>> BuildVoronoiDiagram(
			const std::map<std::wstring, GeosShell::GeosPoint>& Wells,
			const Geometry* WorkCountour);
		geos::geom::Geometry::Ptr CreateWorkCountour(const std::vector < std::vector<double>>& countour);
		std::shared_ptr < geos::geom::LineString*> CreateLineString(geos::geom::Point* start, geos::geom::Point* end);

	private:
		geos::geom::GeometryFactory::Ptr global_factory;
		std::unique_ptr<geos::geom::PrecisionModel> pm;

		//std::vector<std::shared_ptr<geos::geom::Geometry*>> GeometryContainer;

		std::mutex CreateLock;

		std::list<std::pair<float, float>> SmoothPolygon(geos::geom::Geometry::Ptr polygon);
		double factorial(double i)
		{
			if (i == 0) return 1;
			else return i * factorial(i - 1);
		}
	};
}