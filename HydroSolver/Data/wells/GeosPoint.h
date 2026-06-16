#pragma once
#include <geos/geom.h>
namespace GeosShell {
	class GeosPoint {
	public:
		GeosPoint() {}

		GeosPoint(const GeosPoint& p) {
			factory = p.factory;
			point = p.point->clone().release();
		}
		GeosPoint& operator=(const GeosPoint& p) {
			this->factory = p.factory;
			this->point = p.point->clone().release();
			return *this;
		}
		GeosPoint& operator=(GeosPoint&& p) noexcept
		{
			p.SetUndestructable();
			this->factory = p.factory;
			this->point = p.point;
			return *this;
		}

		GeosPoint(GeosPoint&& p) noexcept
		{
			p.SetUndestructable();
			factory = p.factory;
			point = p.point;
		}
		
		GeosPoint(geos::geom::GeometryFactory* _factory, geos::geom::Point* p)
		{
			factory = _factory;
			point = p;
			//std::cout << "call 2 GeosPoint() " << point << "\r\n" << std::flush;
		}

		geos::geom::Point* get() {
			return point;
		}

		geos::geom::Point* get() const {
			return point;
		}

		~GeosPoint() {
			if (Destructable)
			{
				factory->destroyGeometry(point);
				//std::cout << "call destructor on " << point << "\r\n" << std::flush;
			}
		}


	private:
		geos::geom::GeometryFactory* factory = nullptr;
		geos::geom::Point* point = nullptr;

		bool Destructable = true;
		
		void SetUndestructable() 
		{
			Destructable = false;
		}
	};
}