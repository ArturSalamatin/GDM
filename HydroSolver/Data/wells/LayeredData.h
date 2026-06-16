#pragma once
#include <map>
#include <string>
#include <pqxx/pqxx>
#include <mutex>
namespace WellDataHandler {
	class ILayeredData {
	public:
		const std::map<std::wstring, float>& GetData(size_t layerid) {
			return Data.at(layerid);
		}
		virtual void Load(pqxx::connection& con, std::mutex& m) = 0;
	protected:
		std::map<size_t, std::map<std::wstring, float>> Data;
	};

	class PVTData : public ILayeredData {
	public:
		virtual void Load(pqxx::connection& con, std::mutex& m) {
			m.lock();

			pqxx::work work(con);
			const auto& results = work.exec("SELECT * FROM input_data.pvt_v");
			work.commit();

			m.unlock();

			for (const auto& row : results) {
				const size_t layer = row["layerid"].as<size_t>(0);
				Data.emplace(
					layer,
					std::map<std::wstring, float>
					{
						{L"oil_conversion_coefficient", row["oil_conversion_coefficient"].as<float>(0)},
						{ L"oil_density", row["oil_density_sc"].as<float>(0) },
						{ L"sowcr", row["sowcr"].as<float>(0) }
					}
				);
			}

			
		}
	};
}