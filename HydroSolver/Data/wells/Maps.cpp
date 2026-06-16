#include "Maps.h"

geos::geom::Geometry::Ptr Maps::CreateZone(const std::vector<std::vector<double>>& ZonePoint,
	GeometryFactory* global_factory
)
{
	if (ZonePoint.size() > 0)
	{
		geos::geom::CoordinateArraySequence zn;
		for (int i = 0; i < ZonePoint.size(); i++)
		{
			zn.add(geos::geom::Coordinate(ZonePoint.at(i)[0], ZonePoint.at(i)[1]));
		}
		geos::geom::LinearRing* zn_lr = global_factory->createLinearRing(zn); //создаем замкнутый контур из brect_cas
		geos::geom::Polygon* zn_pol = global_factory->createPolygon(zn_lr, NULL); //создаем полигон который лежит внутри контура
		auto zn_g = zn_pol->buffer(500); //получаем точки этого полигона ??
		global_factory->destroyGeometry(zn_pol);
		return zn_g;
	}
	else
		return nullptr;
}


std::map<std::wstring, GeosShell::GeosPoint> Maps::FindNearWell(
	const std::map<std::wstring, GeosShell::GeosPoint>& Wells,
	Point* WellCoord,
	float distance)
{
	auto well = WellCoord;
	auto well_region = well->buffer(distance);
	std::map<std::wstring, GeosShell::GeosPoint> out;
	for (const auto& well_p : Wells)
	{
		auto ws = well_p.second.get();

		if (ws != nullptr)
		{
			if (well_region->contains(ws))
			{
				//сами себя не включаем
				if (ws->getX() != well->getX() && ws->getY() != well->getY())
					out.insert(well_p);
			}
		}
	}
	return out;
}






