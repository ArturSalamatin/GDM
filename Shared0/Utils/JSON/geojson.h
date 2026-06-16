#pragma once

#define USE_GEOS

#include <string>
#include <vector>
#include <list>
#include <map>
#include <fstream>

#include "../UniversalSVWriter.h"

#undef min
#undef max

#ifdef USE_GEOS
	#include "geos/geom.h"
#endif

class GeoJsonEngine
{
public:
	enum class Geom {
		Point, LineString, Polygon
	};

	struct Parameters {
		std::wstring Key; //ключ к параметру
		double Value = 0; //числовое значение
		std::wstring value; //текстовое значение
	};
	/// <summary>
	/// Создает обьект GeoJsonEngine
	/// </summary>
	/// <param name="path">Путь куда записать geojson</param>
	GeoJsonEngine(std::wstring path);
	/// <summary>
	/// Создает обьект геометрии
	/// </summary>
	/// <param name="type">Тип геометрии. Смотреть в дефайнах</param>
	/// <param name="coordinates">Вектор с координатами геометрии. Должен быть замкнут для полигона!</param>
	/// <param name="params">Параметры</param>
	void AddFeature(Geom type, std::list<std::vector<double>> coordinates, std::vector<Parameters> params);
#ifdef USE_GEOS
	void AddFeature(Geom type,  geos::geom::Geometry* g, std::vector<Parameters> params);
#endif
	
	/// <summary>
	/// Записывает geojson файл
	/// </summary>
	void WriteGeoJson();
	virtual ~GeoJsonEngine();
private:
	std::vector<std::wstring> Features;
	std::wstring Path;
	std::wstring ReplaceDecimalSeparator(std::wstring toReplace);
};

