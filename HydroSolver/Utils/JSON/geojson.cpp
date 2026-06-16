#include "geojson.h"

GeoJsonEngine::GeoJsonEngine(std::wstring path)
{
	Path = path;
}
bool isWrote = false;
void GeoJsonEngine::WriteGeoJson()
{
	std::wstring out = L"{ \"type\": \"FeatureCollection\",\"features\": [";
	int counter = 0;
	for (auto feature : Features)
	{
		out += feature;
		if (counter != Features.size() - 1)
			out += L",";
		counter++;
	}
	out += L"]}";
	/*std::ofstream json(Path);
	if (json.is_open())
	{
		json.write(out.c_str(), sizeof(char)*out.size());
		json.close();
	}
	isWrote = true;*/
	UniversalWriter::UTF8Writer writer(Path);
	writer.Write(out);
	writer.Close();
	isWrote = true;

	this->~GeoJsonEngine(); //чистим за собой
}

bool isDetroyed = false;
GeoJsonEngine::~GeoJsonEngine()
{
	if(!isWrote)
		WriteGeoJson();
	if (!isDetroyed)
	{
		Features.~vector();
		Path.~basic_string();
		isDetroyed = true;
	}
}

std::wstring GeoJsonEngine::ReplaceDecimalSeparator(std::wstring toReplace)
{
	auto pos = toReplace.find(L',');
	if (pos != std::wstring::npos)
	{
		return toReplace.replace(pos, 1, L".");
	}
	else
		return toReplace;
}

void GeoJsonEngine::AddFeature(
	Geom type,
	std::list<std::vector<double>> coordinates, 
	std::vector<Parameters> params)
{
	//«адаем тип
	std::wstring tp;
	switch (type)
	{
	case Geom::Point:
		tp = L"Point";
		break;
	case Geom::LineString:
		tp = L"LineString";
		break;
	case Geom::Polygon:
		tp = L"Polygon";
		break;
	}
	//ѕарсим координаты
	std::wstring coordints = L"[";
	int cntr = 0;
	for (auto xy : coordinates)
	{
		if (type == Geom::Point) //“очка
		{
			coordints += L"[" + ReplaceDecimalSeparator(std::to_wstring(xy[0])) + L"," + ReplaceDecimalSeparator(std::to_wstring(xy[1])) + L"]";
			if (cntr != coordinates.size() - 1) //концевую зап€тую не ставим
				coordints += L",";
		}
		else {
			coordints = L"[" + ReplaceDecimalSeparator(std::to_wstring(xy[0])) + L"," + ReplaceDecimalSeparator(std::to_wstring(xy[1])) + L"]";
		}
		cntr++;
	}
	if (type != Geom::Point) //закрываем если не точка
		coordints += L"]";
	std::wstring props;
	cntr = 0;
	for (auto key : params)
	{
		if (!key.value.empty())
		{
			props += L"\"" + key.Key + L"\" : \"" + key.value + L"\"";
			if (cntr != params.size() - 1)
				props += L",";
		}
		else {
			if (std::isnormal(key.Value)) //проверка на битые значени€
				props += L"\"" + key.Key + L"\" : " + ReplaceDecimalSeparator(std::to_wstring(key.Value));
			else
				props += L"\"" + key.Key + L"\" : 0";
			if (cntr != params.size() - 1)
				props += L",";
		}
		cntr++;
	}
	if (type == Geom::Polygon)
		Features.push_back(L"{ \"type\": \"Feature\",\"geometry\":{\"type\": \"" + tp + L"\",\"coordinates\" : [" + coordints + L"] },\"properties\" : { " + props + L" }}");
	else 
		Features.push_back(L"{ \"type\": \"Feature\",\"geometry\":{\"type\": \"" + tp + L"\",\"coordinates\" : " + coordints + L" },\"properties\" : { " + props + L" }}");
}

void GeoJsonEngine::AddFeature(Geom type, geos::geom::Geometry* g, std::vector<Parameters> params)
{
	//«адаем тип
	std::wstring tp;
	switch (type)
	{
	case Geom::Point:
		tp = L"Point";
		break;
	case Geom::LineString:
		tp = L"LineString";
		break;
	case Geom::Polygon:
		tp = L"Polygon";
		break;
	}
	//ѕарсим координаты
	std::wstring coordints = L"[";
	int cntr = 0;
	//генерируем геометрию
	std::list<std::vector<double>> coordinates;
	geos::geom::CoordinateSequence::Ptr gm = g->getCoordinates();
	for (int i = 0; i < gm->size(); i++)
	{
		coordinates.push_back(std::vector<double> {gm->getAt(i).x, gm->getAt(i).y});
	}
	//сохран€ем
	for (auto xy : coordinates)
	{
		if (type == Geom::Point) //“очка
		{
			coordints += L"[" + ReplaceDecimalSeparator(std::to_wstring(xy[0])) + L"," + ReplaceDecimalSeparator(std::to_wstring(xy[1])) + L"]";
			if (cntr != coordinates.size() - 1) //концевую зап€тую не ставим
				coordints += L",";
		}
		else {
			coordints += L"[" + ReplaceDecimalSeparator(std::to_wstring(xy[0])) + L"," + ReplaceDecimalSeparator(std::to_wstring(xy[1])) + L"],";
		}
		cntr++;
	}
	if (type != Geom::Point) //закрываем если не точка
		coordints += L"]";
	std::wstring props;
	cntr = 0;
	for (auto key : params)
	{
		if (!key.value.empty())
		{
			props += L"\"" + key.Key + L"\" : \"" + key.value + L"\"";
			if (cntr != params.size() - 1)
				props += L",";
		}
		else {
			if (std::isnormal(key.Value)) //проверка на битые значени€
				props += L"\"" + key.Key + L"\" : " + ReplaceDecimalSeparator(std::to_wstring(key.Value));
			else
				props +=  L"\"" + key.Key + L"\" : 0";
			if (cntr != params.size() - 1)
				props += L",";
		}
		cntr++;
	}
	if (type == Geom::Polygon)
		Features.push_back(L"{ \"type\": \"Feature\",\"geometry\":{\"type\": \"" + tp + L"\",\"coordinates\" : [" + coordints + L"] },\"properties\" : { " + props + L" }}");
	else
		Features.push_back(L"{ \"type\": \"Feature\",\"geometry\":{\"type\": \"" + tp + L"\",\"coordinates\" : " + coordints + L" },\"properties\" : { " + props + L" }}");
}


