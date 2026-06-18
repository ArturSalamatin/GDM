#include "WellDataHandler.h"


void WellDataHandler::IData::Add(pqxx::connection& con, std::mutex& mut)
{
	std::lock_guard<std::mutex> lock(DataAccessMutex);

	mut.lock();
	pqxx::result result;
	pqxx::work w(con);
	try {
		
		result = w.exec(SQLSelectQuery());
	
	}
	catch (std::exception err)
	{
		mut.unlock();
		w.abort();
		throw err;
	}
	w.commit();
	mut.unlock();

	for (const auto& row : result)
	{
		HandleRow(row);
	}
	result.clear();
	Optimize();
}


std::string WellDataHandler::PerfData::SQLSelectQuery()
{
	return "SELECT * FROM input_data.perforations";
}

void WellDataHandler::PerfData::HandleRow(const pqxx::row& row)
{
	Container[std::to_wstring(row["wellid"].as<size_t>(0))].emplace_back(std::initializer_list<std::pair<const std::wstring, float>>{
		{L"time", PathUtils::Utils::ConvertDateToExcelDate<char>(row["date"].c_str())},
		{ L"is_open", IsInclude(row["type"].as<size_t>(0), 10200ull, 10100ull, 10000ull, 10160ull, 10210ull, 10110ull) ? 0 : 1 },
			/*	(
			!wcscmp(line[4].get(), L"������.����������") ||
			!wcscmp(line[4].get(), L"����������") ||
			!wcscmp(line[4].get(), L"��������") ||
			!wcscmp(line[4].get(), L"����.����������") ||
			!wcscmp(line[4].get(), L"�����.��������") ||
			!wcscmp(line[4].get(), L"��������� �������")) ? 0 : 1 },*/
		{ L"is_grp", IsInclude(row["type"].as<size_t>(0), 10130ull,10131ull, 10132ull, 10133ull) ? 1 : 0 },
			/*	(
			!wcscmp(line[4].get(), L"���������� � ���") ||
			!wcscmp(line[4].get(), L"��������� � ���") ||
			!wcscmp(line[4].get(), L"������ � ���")) ? 1 : 0 },*/
		{ L"perf_start", row["start_depth"].as<float>(0) },
		{ L"perf_stop",  row["stop_depth"].as<float>(0) },
		{ L"perf_density" , row["holes_count"].as<float>(0) }
	});
}


std::string WellDataHandler::GISData::SQLSelectQuery()
{
	return "SELECT * FROM input_data.gis";
}

void WellDataHandler::GISData::HandleRow(const pqxx::row& row)
{

	auto name = std::to_wstring(row["wellid"].as<size_t>(0));

	Container[name].emplace_back(std::initializer_list<std::pair<const std::wstring, float>> {
		{L"layer_start", row["h_md"].as<float>(0)},
		{ L"layer_stop",  row["h_l_md"].as<float>(0) },
		{ L"layer_height", row["l_md"].as<float>(0) },
		{ L"porosity", row["porosity"].as<float>(0) },
		{ L"permability", row["permeability"].as<float>(0) },
		{ L"oil_saturation", row["saturation"].as<float>(0) },
		{ L"is_collector", (row["collector"].as<bool>(false)) ? 1 : 0 },
		{ L"saturated_height", row["saturation"].as<float>(0) * row["l_md"].as<float>(0) / 100 },
		{ L"layer", row["layerid"].as<size_t>(0) },
		{ L"is_collector", row["collector"].as<bool>(false) ? 1 : 0 }
	});
	/*StringContainer[name].emplace_back(std::initializer_list<std::pair<const std::wstring, std::wstring>>
	{
		{L"layer_name", std::to_wstring(row["layerid"].as<size_t>(0))}
	});*/
}






void WellDataHandler::MerData::PushManualy(
	std::wstring Name,
	std::map<std::wstring, float>&& Data)
{
	//��������� ����� ��� ������
	std::lock_guard<std::mutex> lock(DataAccessMutex);

	Container[Name].push_back(std::move(Data));
}

std::string WellDataHandler::MerData::SQLSelectQuery()
{
	return std::string();
}

void WellDataHandler::MerData::HandleRow(const pqxx::row& row)
{
}

std::string WellDataHandler::GDISData::SQLSelectQuery()
{
	return "SELECT wellid, layerid,start_date,wellbore_pressure FROM input_data.gdis WHERE measurement_type IN (1,11)";
}
void WellDataHandler::GDISData::HandleRow(const pqxx::row& row)
{
	auto name = std::to_wstring(row["wellid"].as<size_t>(0));
	Container[name].emplace_back(std::initializer_list<std::pair<const std::wstring, float>> {
		{L"time", PathUtils::Utils::ConvertDateToExcelDate<char>(row["start_date"].c_str())},
		{ L"pressure" , row["wellbore_pressure"].as<float>(0) },
		{ L"layer" , row["layerid"].as<size_t>(0) }
	});

}



/// <summary>
/// ���������� ��������������� ��������� � ����� �� ���������� � ���������/�����������
/// </summary>
/// <param name="gisd">���������� ������ �����</param>
/// <param name="perfd">��������� ������ ����������</param>
/// <param name="name">��� ��������</param>
/// <returns>{��� ���������� {���� ��������������, ���������/�����������, ���� �� ���, ������� ����������}}</returns>
std::map<std::wstring, std::vector<std::tuple<int, int, int, std::pair<float, float>>>> WellDataHandler::DataHandleUtils::ComparePerfWithGIS(GISData& gisd, PerfData& perfd, std::wstring name)
{
	std::map<std::wstring, std::vector<std::tuple<int, int, int, std::pair<float, float>>>> out;
	auto& perf_dat = perfd.GetDataPerWell(name);
	auto& gis_data = gisd.GetDataPerWell(name);
	//auto& giss_data = gisd.GetStringDataPerWell(name);

	for (int i = 0; i < perf_dat.size(); i++)
	{
		float pstart = perf_dat[i].at(L"perf_start");
		float pstop = perf_dat[i].at(L"perf_stop");

		for (int j = 0; j < gis_data.size(); j++)
		{
			float lstart = gis_data[j].at(L"layer_start");
			float lstop = gis_data[j].at(L"layer_stop");

			auto _layer = std::to_wstring(static_cast<size_t>(gis_data[j].at(L"layer")));

			if (!(pstop < lstart || pstart > lstop)) //������ ������ ������ ������ ����, ��� ������ ����������, �� ����, ��� � �����
			{
				/*if (giss_data[j].at(L"layer_name") != L"�/�")
				{*/
					//double height = 0;
					//height = std::min<float>(pstop, lstop) - std::max<float>(pstart, lstart);
					out[_layer].emplace_back(
						(int)perf_dat[i].at(L"time"),
						(int)perf_dat[i].at(L"is_open"),
						(int)perf_dat[i].at(L"is_grp"),
						std::pair<float, float>{ std::max<float>(pstart, lstart), std::min<float>(pstop, lstop) });

					//if (std::get<3>(out[_layer].back()).first >
					//	std::get<3>(out[_layer].back()).second)
					//	std::cout << "pizda";
					/*out[giss_data[j][L"layer_name"]].push_back(
						std::make_tuple<int, int, int, std::pair<float, float>>(
							(int)perf_dat[i][L"time"],
							(int)perf_dat[i][L"is_open"],
							(int)perf_dat[i][L"is_grp"],
							{ std::max<float>(pstart, lstart) , std::min<float>(pstop, lstop) }));*/
				/*}*/
			}
		}

	}
	return out;
}


std::vector<std::wstring> WellDataHandler::DataHandleUtils::GetEnabledLayers(GISData& gisd, PerfData& perfd, std::wstring name, int date)
{
	auto layers = ComparePerfWithGIS(gisd, perfd, name);
	std::vector<std::wstring> out;
	for (auto& val : layers)
	{
		int state = 0;
		for (const auto& pf : val.second)
		{
			if (std::get<0>(pf) <= date)
			{
				state = std::get<1>(pf);
			}
		}
		if (state)
			out.push_back(val.first);
	}
	return out;
}

void WellDataHandler::IData::Optimize()
{
	//std::lock_guard<std::mutex> lock(DataAccessMutex);
	for (auto& vector : Container)
	{
		vector.second.shrink_to_fit();
	}
	for (auto& vector : StringContainer)
	{
		vector.second.shrink_to_fit();
	}
}

std::string WellDataHandler::GTMData::SQLSelectQuery()
{
	return "SELECT wellid,start_date,gtm_type FROM input_data.gtm";
}

void WellDataHandler::GTMData::HandleRow(const pqxx::row& row)
{
	auto name = std::to_wstring(row["wellid"].as<size_t>(0));
	Container[name].emplace_back(std::initializer_list<std::pair<const std::wstring, float>> {
		{L"time", PathUtils::Utils::ConvertDateToExcelDate<char>(row["start_date"].c_str())},
		{ L"type", row["gtm_type"].as<size_t>(0) }
	});
}


std::string WellDataHandler::TechModeOil::SQLSelectQuery()
{
	return "SELECT wellid, date, pump_depth, nominal_pump_power, wellbore_pressure FROM input_data.trd_dob";
}

void WellDataHandler::TechModeOil::HandleRow(const pqxx::row& row)
{
	auto name = std::to_wstring(row["wellid"].as<size_t>(0));
	Container[name].emplace_back(std::initializer_list<std::pair<const std::wstring, float>> {
		{L"time", PathUtils::Utils::ConvertDateToExcelDate<char>(row["date"].c_str())},
		{ L"height", row["pump_depth"].as<float>(0) },
		{ L"nominal_power", row["nominal_pump_power"].as<float>(0) },
		{ L"actual_power", 0 },
		{ L"layer_pressure", row["wellbore_pressure"].as<float>(0) }
	});
}

void WellDataHandler::Database_MerLayeredData::Push(pqxx::connection& con, std::mutex& m)
{
	m.lock();
	pqxx::work w(con);
	try {
		
		pqxx::result layers = w.exec("SELECT layerid from input_data.layers");
		for (const auto& row : layers)
		{
			Mers.insert({ std::to_wstring(row["layerid"].as<size_t>(0)), std::make_shared<MerData>() });
			//Mers.emplace(ln, );
		}
		
	}
	catch (std::exception err)
	{
		m.unlock();
		w.abort();
		throw err;
	}
	w.commit();
	m.unlock();


	

	m.lock();
	pqxx::result mer;
	pqxx::work w1(con);
	try {/*
		const auto& poorc_exists
			= w1.exec1("select exists (select 1 from poorc.output_mer_full omf)");

		if (poorc_exists["exists"].as<bool>(false)) {
			mer = w1.exec("SELECT * from poorc.output_mer_full");
		}
		else*/
		{
			mer = w1.exec("SELECT * from input_data.mer");
		}		
	}
	catch (std::exception err)
	{
		m.unlock();
		w1.abort();
		throw err;
	}
	w1.commit();
	m.unlock();

	for (const auto& row : mer)
	{
		auto name = std::to_wstring(row["wellid"].as<size_t>(0));
		auto layer = std::to_wstring(row["layerid"].as<size_t>(0));

		Mers.at(layer)->PushManualy(name, std::initializer_list<std::pair<const std::wstring, float>> {
			{L"time", PathUtils::Utils::ConvertDateToExcelDate<char>(row["date"].c_str())},
			{ L"oil",  row["oil"].as<float>(0) },
			{ L"water",  row["water"].as<float>(0) },
			{ L"pump_water", row["pump"].as<float>(0) },
			{ L"worked_time" , row["work_time"].as<float>(0) },
			//{ L"colection_time" , wcstof(line[8].get(), &stopstring) },
			{ L"idle_time", row["hold_time"].as<float>(0) },
			{ L"type", row["work_character"].as<size_t>(0) == 11 ? 1 : 0 },
			{ L"is_work" , row["state"].as<size_t>(0) == 1 ? 1 : 0 }
		});

	}
	
}
