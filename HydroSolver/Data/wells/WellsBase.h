#pragma once

#include <memory>
#include <pqxx/pqxx>

#include "GEOSObject.h"
#include "WellDataHandler.h"
#include "LayeredData.h"

namespace ProgramLauncher {

	class Utils {
	public:
		
		static std::string GetCONNSTRNG(
			std::string host,
			std::string port,
			std::string dbname,
			std::string user,
			std::string pass)
		{
			std::stringstream out;
			out << "host=" << host
				<< " port=" << port
				<< " dbname=" << dbname
				<< " user=" << user
				<< " password=" << pass
				<< " client_encoding=utf8 application_name=gdm";
			return out.str();
		}

		static std::shared_ptr<pqxx::connection> GetConnection(std::string project_uuid) {
			return std::make_shared<pqxx::connection>((GetCONNSTRNG("localhost", "5432", project_uuid, "gdm", "")));
		}
	};

	class IProjectInfo {
	public:
		IProjectInfo(std::shared_ptr<GeometryHandler::GEOSObjectHandler> geos)
		{
			Geos = std::shared_ptr<GeometryHandler::GEOSObjectHandler>(geos);
		}

/**
 * имя скважины -- точка пространства ---- пластопересечение
 * */
		std::map<std::wstring, GeosShell::GeosPoint> Wells;
		/*все имена скважин*/
		std::vector<std::wstring> WellsName;
		/* граница участка, произвольная граница*/
		std::vector<std::vector<double>> WorkZoneCountour;
		virtual void LoadWells() = 0;

	protected:
		std::shared_ptr<GeometryHandler::GEOSObjectHandler> Geos;
		geos::geom::Geometry::Ptr Zone;

	};

	class Database_FileHandler : public IProjectInfo
	{
	public:
		Database_FileHandler(std::shared_ptr<pqxx::connection> con, std::shared_ptr<GeometryHandler::GEOSObjectHandler> geos)
			: IProjectInfo(geos)
		{
			Con = std::shared_ptr<pqxx::connection>(con);
		}
		virtual void LoadWells();
	private:
		std::shared_ptr<pqxx::connection> Con;
	};

	class WellData_Handler {
	public:

		std::shared_ptr<WellDataHandler::GISData> Gis;
		std::shared_ptr<WellDataHandler::PerfData> Perf;
		std::shared_ptr<WellDataHandler::GDISData> GDIS;
		WellDataHandler::PVTData PVT;
		std::shared_ptr<WellDataHandler::GTMData> GTM;
		std::shared_ptr<WellDataHandler::TechModeOil> TechModeOil;
		std::shared_ptr<WellDataHandler::Database_MerLayeredData> MerLayered;

		void AddData(pqxx::connection& con);


	};
}