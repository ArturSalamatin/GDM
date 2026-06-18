#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <map>
#include <algorithm>
#include <regex>
#include <mutex>
#include <pqxx/pqxx>

#include "PathUtils.h"

namespace WellDataHandler {	

	class IData {
	private:
		std::vector<std::map<std::wstring, float>> nullReference;
		std::map<std::wstring, float> m_nullReference;
		std::vector<std::map<std::wstring, std::wstring>> snullReference;
		template<typename T> void _IsInclude(size_t& entiries, T searched)
		{}
		template<typename T, typename... TArgs> void _IsInclude(size_t& entiries, T searched, T first, TArgs... rest)
		{
			if (searched == first)
			{
				entiries++;
			}
			_IsInclude(entiries, searched, rest...);
		}		
	protected:
		std::map<std::wstring, std::vector<std::map<std::wstring, float>>> Container;
		std::map<std::wstring, std::vector<std::map<std::wstring, std::wstring>>> StringContainer;
		std::map<std::wstring, float> WellsName;
		std::mutex DataAccessMutex;
		/// <summary>
		/// Метод должен вызываться из метода, который уже заблокировал мьютекс
		/// </summary>
		void Optimize();
		
		template<typename T, typename... TArgs> bool IsInclude(T searched, TArgs... rest)
		{
			size_t entiries = 0;
			_IsInclude(entiries, searched, rest...);
			return entiries > 0;
		}

		virtual std::string SQLSelectQuery() = 0;
		virtual void HandleRow(const pqxx::row& row) = 0;
	public:
		virtual void Add(pqxx::connection& con, std::mutex& mut);

		/// <summary>
		///	Возвращает все данные по указаной скважине
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <returns>См в наследниках</returns>
		const virtual std::vector<std::map<std::wstring, float>>& GetDataPerWell(std::wstring name)
		{
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			if (Container.find(name) != Container.end())
				return Container.at(name);
			else
				return nullReference;
		};
		const virtual std::vector<std::map<std::wstring, float>>& GetDataPerWellWithSimilarName(std::wstring name)
		{
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			if (Container.find(name) == Container.end())
			{
				for (const auto& val : Container)
				{
					if (val.first.find(name) != std::wstring::npos 
						|| 
						name.find(val.first) != std::wstring::npos)
						return Container.at(val.first);
				}
				return nullReference;
			}
			else {
				return Container.at(name);
			}
			
		};
		
		/// <summary>
		/// Возвращает данные ближайщие к текущей даты
		/// работает только если есть ключ time
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <returns>См в наследниках</returns>
		const virtual std::map<std::wstring, float>& GetDataForClosestForCurrentTime(std::wstring name)
		{
			//получаем текущее время
			__time64_t long_time;
			_time64(&long_time);
			//переводим секунды в минуты
			size_t current_time = long_time / 86400 + 25569; //+25569 - смешение с  1970
			auto& data = GetDataPerWell(name);

			float time_diff = 1e36f;
			int closest_id = -1;

			int itr = 0;
			for (const auto& val : data)
			{
				float td = abs(val.at(L"time") - current_time);
				if (td < time_diff)
				{
					time_diff = td;
					closest_id = itr;
				}
				itr++;
			}

			return (closest_id != -1) ? data.at(closest_id) : m_nullReference;
		}
		/// <summary>
		/// Возвращает данные наиболее близкие к указанной дате
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <param name="current_time">Дата в днях</param>
		/// <returns>См в наследниках</returns>
		const virtual std::map<std::wstring, float>& GetDataForClosestTime(std::wstring name, int current_time)
		{
			auto& data = GetDataPerWell(name);

			float time_diff = 1e36f;
			int closest_id = -1;

			int itr = 0;
			for (const auto& val : data)
			{
				float td = abs(val.at(L"time") - current_time);
				if (td < time_diff)
				{
					time_diff = td;
					closest_id = itr;
				}
				itr++;
			}

			return (closest_id != -1) ? data.at(closest_id) : m_nullReference;
		}

		std::vector<std::wstring> GetWellsName() {
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			std::vector<std::wstring> names;
			for (const auto& val : Container)
			{
				names.push_back(val.first);
			}
			return names;
		};
	};

	class PerfData : public IData {
	protected: 
		virtual std::string SQLSelectQuery();
		virtual void HandleRow(const pqxx::row& row);
	};
	class GISData : public IData {
	protected:
		virtual std::string SQLSelectQuery();
		virtual void HandleRow(const pqxx::row& row);
	};
	class MerData : public IData {
	public:
		
		void PushManualy(
			std::wstring Name, 
			std::map<std::wstring, float>&& Data);

		void ManualOptimize()
		{
			Optimize();
		}
	protected:
		virtual std::string SQLSelectQuery();
		virtual void HandleRow(const pqxx::row& row);

	};
	class GDISData : public IData {
	protected:
		virtual std::string SQLSelectQuery();
		virtual void HandleRow(const pqxx::row& row);
	};
	class DataHandleUtils
	{
	public:
		static std::map<std::wstring, std::vector<std::tuple<int, int, int, std::pair<float, float>>>> ComparePerfWithGIS(GISData& gisd, PerfData& perfd, std::wstring name);
		static std::vector<std::wstring> GetEnabledLayers(GISData& gisd, PerfData& perfd, std::wstring name, int date);
		
	};


	class IMerLayeredData {
	protected:
		std::map<std::wstring, std::shared_ptr<MerData>> Mers;
		
	public:
		virtual std::shared_ptr<MerData> GetMerDataPerLayer(std::wstring LayerName)
		{
			return Mers[LayerName];
		}
	};


	class Database_MerLayeredData : public IMerLayeredData
	{
	public:
		void Push(pqxx::connection& con, std::mutex& m);
	};
		

	class GTMData : public IData
	{
	protected:
		virtual std::string SQLSelectQuery();
		virtual void HandleRow(const pqxx::row& row);
	};

	class TechModeOil : public IData
	{
	protected:
		virtual std::string SQLSelectQuery();
		virtual void HandleRow(const pqxx::row& row);
	};
}