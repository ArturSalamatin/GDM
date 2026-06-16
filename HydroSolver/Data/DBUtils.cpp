#include "DBUtils.h"
#include <iostream>
void grdecl_memory::DBSaver::FlushBuffer()
{
	if (!AddCount)
		return;

	AddCount = 0;

	try {

		if (DBAsyncUnload.valid())
			DBAsyncUnload.wait();

		auto [_buff, size] = buffer.OccupyBuffer();
		DBAsyncUnload = std::async(std::launch::async,
			[](pqxx::blob& lo, std::string object, std::byte* value, size_t size) {

				auto buff = std::basic_string_view<std::byte>(value, size);
				try {
					lo.write(buff);
				}
				catch (const std::exception& err) {
					std::cout << "error" << err.what() << "\r\n";
				}
			},
			std::ref(CurrentLO), CurrentObject, _buff, size);

	}
	catch (std::exception err)
	{
		CurrentTransaction->abort();
		throw err;
	}

}

void grdecl_memory::DBSaver::UnlinkGRD(std::string name)
{
	CurrentLO.close();
	const auto rows = CurrentTransaction->exec_params("SELECT oid FROM input_data.grdecl_i WHERE name = $1", name);
	if (rows.size() > 0) {
		CurrentTransaction->exec_params("SELECT lo_unlink($1)", rows[0]["oid"].as<pqxx::oid>());
		CurrentTransaction->exec_params("delete from input_data.grdecl_i  where name = $1", name);
	}
}

void grdecl_memory::DBSaver::CreateLO(std::string name)
{
	CurrentObject = name;

	const pqxx::oid new_oid = pqxx::blob::create(*CurrentTransaction);
	CurrentLO = pqxx::blob::open_w(*CurrentTransaction, new_oid);

	CurrentTransaction->exec_params0("INSERT into input_data.grdecl_i VALUES ($1, $2) ON CONFLICT (name) DO UPDATE SET oid = $2", name, new_oid);
}

grdecl_memory::DBLoader::ModelParametrs grdecl_memory::DBLoader::GetModelParams()
{
	pqxx::nontransaction w(*Con);
	auto row = w.exec1("select value[1] as nx, value[2] as ny, value[3] as nz from input_data.grdecl where name = 'SPECGRID' ");
	return { row["nx"].as<size_t>(0), row["ny"].as<size_t>(0), row["nz"].as<size_t>(0) };
}

std::vector<float> grdecl_memory::DBLoader::LoadPillars()
{
	return LoadArray("COORD");
}

std::vector<float> grdecl_memory::DBLoader::LoadZCORN()
{
	return LoadArray("ZCORN");
}

std::vector<float> grdecl_memory::DBLoader::LoadValues(std::string value)
{
	return LoadArray(value);
}

std::vector<std::string> grdecl_memory::DBLoader::GetAvailableGRIDS()
{
	std::string query;
	if (CheckLOExist()) {
		query = "SELECT name FROM input_data.grdecl_i";
	}
	else {
		query = "SELECT name FROM input_data.grdecl";
	}
	pqxx::nontransaction w(*Con);
	pqxx::result zone = w.exec(query);
	std::vector<std::string> out;
	for (const auto& row : zone)
	{
		out.emplace_back(row["name"].c_str());
	}
	return out;

}

std::map<size_t, size_t> grdecl_memory::DBLoader::GetIdxLayerTable()
{
	pqxx::nontransaction w(*Con);
	std::map<size_t, size_t> out;
	pqxx::result res = w.exec("SELECT * FROM input_data.grdecl_layers");
	for (const auto& row : res)
	{
		out.emplace(row["layerid"].as<size_t>(0), row["id"].as<size_t>(0));
	}
	return out;
}

std::map<size_t, float> grdecl_memory::DBLoader::GetLayerCoefs(std::string coef_name)
{
	pqxx::nontransaction w(*Con);
	if (coef_name == "pvt") {
		return GetPVT(w);
	}
	if (coef_name == "oil_conversion_coefficient") {
		return GetOilConversionCoef(w);
	}
	if (coef_name == "density") {
		return GetDensity(w);
	}
	throw std::runtime_error("Invalid coef_name");
}

std::vector<float> grdecl_memory::DBLoader::LoadArray(std::string name)
{
	if (CheckLOExist()) {
		pqxx::work w(*Con);
		const pqxx::result row_oid = w.exec_params("SELECT oid FROM input_data.grdecl_i WHERE name = $1", name);
		if (row_oid.size() == 0)
			return {};

		auto blob = pqxx::blob::open_r(w, row_oid[0]["oid"].as<pqxx::oid>());

		constexpr size_t chunk_size_byte = 1048576ULL;
		std::vector<float> out;

		std::basic_string<std::byte> buffer;
		size_t act_byte_size = 0;
		while (act_byte_size = blob.read(buffer, chunk_size_byte)) {
			for (size_t i = 0; i < act_byte_size / sizeof(float); i++) {
				out.emplace_back(*(float*)(buffer.data() + i * sizeof(float)));
			}
		}
		return out;
	}
	else {
		pqxx::nontransaction w(*Con);
		pqxx::result zone = w.exec_params("SELECT unnest(value) FROM input_data.grdecl WHERE name = $1", name);
		if (zone.size() == 0) {
			return {};
		}
		//auto arr = zone["value"].as_array();
		//auto elem = arr.get_next();
		auto locale = _create_locale(LC_NUMERIC, "en-us");
		char* end;

		std::vector<float> out;
		for (const auto& val : zone)
		{
			out.push_back(_strtof_l(val[0].c_str(), &end, locale));
		}
		return out;
	}
}

bool grdecl_memory::DBLoader::CheckLOExist() {
	pqxx::nontransaction w(*Con);
	const auto row = w.exec1("SELECT EXISTS(SELECT FROM information_schema.tables WHERE  table_schema = 'input_data' AND table_name   = 'grdecl_i')");
	return row["exists"].as<bool>(false);
}

std::map<size_t, float> grdecl_memory::DBLoader::GetPVT(pqxx::nontransaction& w)
{
	std::map<size_t, float> out;
	pqxx::result res = w.exec("select gl.id, pv.sowcr from input_data.pvt_v pv inner join input_data.grdecl_layers gl on gl.layerid = pv.layerid ");
	for (const auto& row : res)
	{
		out.emplace(row["id"].as<size_t>(0), row["sowcr"].as<float>(0));
	}
	return out;
}

std::map<size_t, float> grdecl_memory::DBLoader::GetOilConversionCoef(pqxx::nontransaction& w)
{
	std::map<size_t, float> out;
	pqxx::result res = w.exec("select gl.id, 1 / pv.oil_conversion_coefficient as oil_conversion_coefficient from input_data.pvt_v pv inner join input_data.grdecl_layers gl on gl.layerid = pv.layerid ");
	for (const auto& row : res)
	{
		out.emplace(row["id"].as<size_t>(0), row["oil_conversion_coefficient"].as<float>(1));
	}
	return out;
}

std::map<size_t, float> grdecl_memory::DBLoader::GetDensity(pqxx::nontransaction& w)
{
	std::map<size_t, float> out;
	pqxx::result res = w.exec("select gl.id, pv.oil_density_sc from input_data.pvt_v pv inner join input_data.grdecl_layers gl on gl.layerid = pv.layerid ");
	for (const auto& row : res)
	{
		out.emplace(row["id"].as<size_t>(0), row["oil_density_sc"].as<float>(1));
	}
	return out;
}

std::vector<float> grdecl_memory::TempGRDLoader::LoadArray(std::string name)
{
	size_t filesize = std::filesystem::file_size("./temp_grdecl/" + name);
	std::vector<float> out(filesize / sizeof(float));
	std::ifstream stream("./temp_grdecl/" + name, std::ios::binary);
	stream.read(reinterpret_cast<char*>(&out[0]), filesize);
	stream.close();
	return out;
}
