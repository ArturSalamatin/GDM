#include "WellsBase.h"

void ProgramLauncher::Database_FileHandler::LoadWells()
{
	pqxx::work w(*Con);
	pqxx::result result = w.exec("SELECT * from get_all_coord_at_top_layer()");
	for (const auto& row : result)
	{
		float x = row["x"].as<float>(std::numeric_limits<size_t>::max());
		float y = row["y"].as<float>(std::numeric_limits<size_t>::max());
		size_t wid = row["wellid"].as<size_t>(0);

		if (x == std::numeric_limits<size_t>::max() || y == std::numeric_limits<size_t>::max())
			continue;

		Wells.emplace(
			std::to_wstring(wid), Geos->CreatePoint({ x, y }));
		WellsName.push_back(std::to_wstring(wid));
	}

	pqxx::row zone = w.exec1("SELECT * FROM input_data.object_contour");
	auto arr = zone["countour"].as_array();
	auto elem = arr.get_next();

	std::vector<double> line;
	do {
		//auto elem2 = elem.second;
		if (*elem.second.c_str() != '\x00')
		{
			char* end;
			line.push_back(_strtod_l(elem.second.c_str(), &end, _create_locale(LC_NUMERIC, "en-us")));
			if (line.size() == 2)
			{
				WorkZoneCountour.push_back(std::move(line));
			}
			/*auto _arr = pqxx::array_parser(elem.second);
			auto elem2 = _arr.get_next();
			if (*elem2.second.c_str() != '\x00')
			{
				std::vector<double> point;
				do {
					point.push_back(std::stod(elem2.second));
				} while (elem2.first != pqxx::array_parser::juncture::done);
				WorkZoneCountour.push_back(std::move(point));
			}*/
		}
		elem = arr.get_next();
	} while (elem.first != pqxx::array_parser::juncture::done);

	WorkZoneCountour.push_back(WorkZoneCountour[0]);
	Zone = Geos->CreateWorkCountour(WorkZoneCountour);

	w.commit();
}

void ProgramLauncher::WellData_Handler::AddData(pqxx::connection& cons)
{

	std::vector<std::future<void>> Tasks;

	std::mutex database_mutex;

	Tasks.push_back(
		std::async(
			[&]()
			{
				MerLayered = std::make_shared<WellDataHandler::Database_MerLayeredData>();
				MerLayered->Push(cons, database_mutex);
			})
	);

	Tasks.push_back(
		std::async([&]()
			{
				GDIS = std::make_shared<WellDataHandler::GDISData>();
				GDIS->Add(cons, database_mutex);
			})
	);
	Tasks.push_back(
		std::async([&]()
			{
				Gis = std::make_shared<WellDataHandler::GISData>();
				Gis->Add(cons, database_mutex);
			})
	);
	Tasks.push_back(
		std::async([&]()
			{
				Perf = std::make_shared<WellDataHandler::PerfData>();
				Perf->Add(cons, database_mutex);
			})
	);

	Tasks.push_back(
		std::async([&]()
			{
				GTM = std::make_shared<WellDataHandler::GTMData>();
				GTM->Add(cons, database_mutex);
			})
	);
	Tasks.push_back(
		std::async([&]()
			{
				TechModeOil = std::make_shared<WellDataHandler::TechModeOil>();
				TechModeOil->Add(cons, database_mutex);
			})
	);

	Tasks.push_back(
		std::async([&]()
			{
				PVT.Load(cons, database_mutex);
			})
	);



	for (int i = 0; i < Tasks.size(); i++)
	{
		try {
			Tasks[i].get();
		}
		catch (std::exception e)
		{
			throw e;
		}
	}
}
