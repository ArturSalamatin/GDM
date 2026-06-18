#pragma once

#include <vector>
#include <algorithm>
#include <fstream>
#include <thread>
#include <mutex>
#include <cmath>
#include <filesystem>
#include <Eigen/Eigen>

#include <geos/geom.h>
#include <geos\triangulate\VoronoiDiagramBuilder.h>

#include "GeosPoint.h"


class Maps
{
public:
	
	
	/// <summary>
	/// Формирует геометрию зоны из файла проекта
	/// </summary>
	/// <param name="ZonePoint">Точки зоны</param>
	/// <param name="global_factory">Указатель на основную фабрику геоса</param>
	/// <param name="xshift">Смещение по x</param>
	/// <param name="yshift">Смещение по y</param>
	/// <returns>Геометрия зоны или nullptr в случае пустой зоны</returns>
	static geos::geom::Geometry::Ptr CreateZone(const std::vector<std::vector<double>>& ZonePoint,
		GeometryFactory* global_factory);
		
	static std::map<std::wstring, GeosShell::GeosPoint> FindNearWell(const std::map<std::wstring, GeosShell::GeosPoint>& Wells, geos::geom::Point* WellCoord, float distance = 500);
	
};
