#include "GEOSObject.h"
GeometryHandler::GEOSObjectHandler::GEOSObjectHandler()
{
	pm = std::make_unique<geos::geom::PrecisionModel>(1);
	global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);
}

GeometryHandler::GEOSObjectHandler::~GEOSObjectHandler()
{
	/*for (int i = 0; i < GeometryContainer.size(); i++)
	{
		global_factory->destroyGeometry(*GeometryContainer[i]);
	}
	GeometryContainer.clear();*/
	pm.release();
	global_factory.release();
}

GeosShell::GeosPoint GeometryHandler::GEOSObjectHandler::CreatePoint(std::pair<float, float> coord)
{

	std::lock_guard<std::mutex> lock(CreateLock);

	return GeosShell::GeosPoint(coord.first, coord.second);
}

geos::geom::Polygon::Ptr GeometryHandler::GEOSObjectHandler::CreatePolygon(std::list<std::pair<float, float>>& shell)
{
	//������� ����� ���������
	auto cas = std::make_unique<geos::geom::CoordinateArraySequence>();
	for (const auto& coord : shell)
	{
		cas->add(geos::geom::Coordinate(coord.first, coord.second));
	}

	//std::lock_guard<std::mutex> lock(CreateLock);

	auto lr = global_factory->createLinearRing(std::move(cas));
	//GeometryContainer.push_back(lr);
	//createPolygon �������� �� ���� ���������� ����������� ����������� => ���������� ��������� ������� �� ����
	auto poly = global_factory->createPolygon(std::move(lr));
	return poly;
}

std::shared_ptr<std::map<std::wstring, geos::geom::Geometry::Ptr>> GeometryHandler::GEOSObjectHandler::BuildVoronoiDiagram(
	const std::map<std::wstring, GeosShell::GeosPoint>& Wells,
	const Geometry* WorkCountour)
{
	//������ ����� ����� ��������
	geos::geom::CoordinateArraySequence cas;
	for (const auto& well : Wells)
	{
		cas.add(geos::geom::Coordinate{ (well.second.get())->getX(), (well.second.get())->getY() });
		//std::cout << well.second->getX() << "\t" << well.second->getY() << "\r\n" << std::flush;
	}
	//������ ���������
	std::lock_guard<std::mutex> lock(CreateLock);
	geos::triangulate::VoronoiDiagramBuilder vdb;
	vdb.setTolerance(5);
	vdb.setSites(cas);
	std::unique_ptr<geos::geom::GeometryCollection> vd_vector = vdb.getDiagram(*global_factory);
	std::vector<geos::geom::Geometry::Ptr> ptrs;
	std::vector<std::future<void>> promise_ctr;
	std::mutex ptrs_mtx;
	auto lambda_smooth = [this, &ptrs, &ptrs_mtx](auto _ncell)
	{
		auto coord = this->SmoothPolygon(std::move(_ncell));
		std::lock_guard l(ptrs_mtx);
		ptrs.push_back(this->CreatePolygon(coord));

	};
	for (auto& wcell : *vd_vector)
	{
		auto _ncell = wcell->buffer(0);
		promise_ctr.push_back(std::async(lambda_smooth, std::move(_ncell)));
	}
	for (auto& f : promise_ctr)
	{
		f.get();
	}
	//������������� ������
	auto out = std::make_shared<std::map<std::wstring, geos::geom::Geometry::Ptr>>();
	for (const auto& well : Wells)
	{
		auto well_p = global_factory->createPoint(
			geos::geom::Coordinate(well.second.get()->getX(), well.second.get()->getY()));
		for (auto& ncell : ptrs)
		{
			//auto ncell = wcell->buffer(0);
			//auto _nsmooth = SmoothPolygon(std::move(_ncell));
			//auto ncell = CreatePolygon(_nsmooth);
			//global_factory->destroyGeometry(wcell);
			//������� ������ ������
			if (ncell != nullptr && ncell->contains(well_p))
			{
				//��������� ����� �� ��� � ������� ����
				/*if (WorkCountour != nullptr)
				{*/ //���� ���� ���, �� ��������� �����������
				if (WorkCountour->contains(ncell.get()))
				{
					//if (ObjectCountour != nullptr)
					//{
					//	//���� ����� �����, �� ������ ���������
					//	if (ObjectCountour->contains(ncell.get()))
					//	{
					out->insert(std::make_pair(well.first, std::move(ncell)));

					//	}//���� ������ ������������, �� ��������� �����������
					//	else if (ObjectCountour->intersects(ncell.get()))
					//	{
					//		auto intersection = ncell->intersection(ObjectCountour);
					//		out->insert(std::make_pair(well.first, std::move(intersection)));

					//	}
					//}
					//else {
					//	out->insert(std::make_pair(well.first, std::move(ncell) ));
					//}
				}
				else if (WorkCountour->intersects(ncell.get()))
				{
					auto _cell = WorkCountour->intersection(ncell.get());
					//if (ObjectCountour != nullptr)
					//{
					//	//���� ����� �����, �� ������ ���������
					//	if (ObjectCountour->contains(_cell.get()))
					//	{
					out->insert(std::make_pair(well.first, std::move(_cell)));

					//	}//���� ������ ������������, �� ��������� �����������
					//	else if (ObjectCountour->intersects(_cell.get()))
					//	{
					//		auto intersection = _cell->intersection(ObjectCountour);
					//		out->insert(std::make_pair(well.first, std::move(intersection)));

					//	}
					//}
					//else {
					//	out->insert(std::make_pair(well.first, std::move(_cell)));
					//}
				}
				/*}
				else {
					out->insert(std::make_pair(well.first, std::move(ncell)));
				}*/
				//������� �� �����
				break;
			}

		}
	}
	return out;
}


geos::geom::Geometry::Ptr GeometryHandler::GEOSObjectHandler::CreateWorkCountour(const std::vector<std::vector<double>>& countour)
{
	std::lock_guard<std::mutex> lock(CreateLock);
	return Maps::CreateZone(countour, global_factory.get());
}

std::shared_ptr<geos::geom::LineString*> GeometryHandler::GEOSObjectHandler::CreateLineString(geos::geom::Point* start, geos::geom::Point* end)
{
	geos::geom::CoordinateArraySequence cas;
	cas.add(Coordinate{ start->getX(), start->getY() });
	cas.add(Coordinate{ end->getX(), end->getY() });

	std::lock_guard<std::mutex> lock(CreateLock);

	auto ptr = global_factory->createLineString(cas);
	auto _ptr = std::shared_ptr<geos::geom::LineString*>(&ptr, [del = global_factory.get()](geos::geom::LineString** p)
	{
		del->destroyGeometry(*p);
	});

	return _ptr;
}

std::list<std::pair<float, float>> GeometryHandler::GEOSObjectHandler::SmoothPolygon(geos::geom::Geometry::Ptr polygon)
{
	/*auto original_coord = polygon->getCoordinates();*/

	auto buffer_reg = polygon->buffer(25);
	auto coordinates = buffer_reg->getCoordinates();
	std::list<std::pair<float, float>> new_poly;
	size_t n = coordinates->getSize();

	/*std::cout << "\r\n" << std::flush;

	for (size_t i = 0; i < original_coord->size(); i++)
	{
		std::cout << std::fixed << original_coord->getAt(i).x << ";" << original_coord->getAt(i).y << "\r\n" << std::flush;
	}*/

	new_poly.emplace_back(coordinates->getAt(0).x, coordinates->getAt(0).y);
	for (size_t step = 0; step < 50; step++)
	{
		double t = 0.02 + 0.02 * step;
		double x = 0;
		double y = 0;
		for (size_t i = 0; i < coordinates->size(); i++)
		{
			double binomal_coef = factorial(n - 1) / (factorial(i) * factorial(n - i - 1));
			double polynom = pow(t, i) * pow(1 - t, n - i - 1);

			x += binomal_coef * polynom * coordinates->getAt(i).x;
			y += binomal_coef * polynom * coordinates->getAt(i).y;


		}
		new_poly.emplace_back(x, y);
	}

	/*std::cout << "\r\n" << std::flush;

	for (const auto& coord : new_poly)
	{
		std::cout << std::fixed << coord.first << ";" << coord.second << "\r\n" << std::flush;
	}*/

	return new_poly;

}
