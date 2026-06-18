#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <map>
#include <algorithm>
#include <regex>
#include <mutex>

#include "UniversalSVParser.h"
#include "BinaryFileHandler.h"




namespace WellDataHandler {
	class DataReader {
	public:
		//static 
		struct ValueContainer {
			float Value = 0;
			std::wstring values;
		};
		static std::vector < std::vector<ValueContainer>> Read(std::wstring path);
		static std::wstring ReplaceCommaWithDot(std::wstring&& val)
		{
			size_t pos = val.find(L",");
			if (pos != std::wstring::npos)
			{
				auto nstr = val;
				nstr.replace(nstr.begin() + pos, nstr.begin() + pos + 1, L".");
				return nstr;
			}
			else {
				return val;
			}
		}
	};
	class IData {
	protected:
		std::map<std::wstring, std::vector<std::map<std::wstring, float>>> Container;
		std::map<std::wstring, std::vector<std::map<std::wstring, std::wstring>>> StringContainer;
		std::map<std::wstring, float> WellsName;
		std::mutex DataAccessMutex;
		/// <summary>
		/// Метод должен вызываться из метода, который уже заблокировал мьютекс
		/// </summary>
		void Optimize();
	public:
		/// <summary>
		/// Добавляем скважину в список
		/// </summary>
		/// <param name="path">Путь до файла</param>
		/// <param name="name">Имя файла</param>
		virtual void Push(std::wstring path, std::wstring name) = 0;

		virtual float GetValue(int id, std::wstring name)
		{
			return 0;
		}
		/// <summary>
		/// Получаем список параметров за период времени (Если он есть)
		/// тэг периода time
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <param name="start_period">Начало периода</param>
		/// <param name="stop_period">Конец периода</param>
		/// <returns>Вектор параметров на период</returns>
		virtual std::vector<std::map<std::wstring, float>> GetDataPerPeriod(std::wstring name, int start_period, int stop_period)
		{
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			std::vector<std::map<std::wstring, float>> out;
			if (Container.find(name) != Container.end())
			{
				if (Container[name][0].find(L"time") != Container[name][0].end())
				{
					for (const auto& val : Container[name])
					{
						if (val.at(L"time") >= start_period && val.at(L"time") <= stop_period)
						{
							out.push_back(val);
						}
					}
				}
			}
			return out;
		}
		virtual std::vector<std::map<std::wstring, std::wstring>> GetStringDataPerPeriod(std::wstring name, int start_period, int stop_period)
		{
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			std::vector<std::map<std::wstring, std::wstring>> out;
			if (Container[name][0].find(L"time") != Container[name][0].end())
			{
				int inContId = 0;
				for (const auto& val : Container[name])
				{
					if (val.at(L"time") >= start_period && val.at(L"time") <= stop_period)
					{
						out.push_back(StringContainer[name][inContId]);
					}
					inContId++;
				}
			}
			return out;
		}
		/// <summary>
		///	Возвращает все данные по указаной скважине
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <returns>См в наследниках</returns>
		virtual std::vector<std::map<std::wstring, float>> GetDataPerWell(std::wstring name)
		{
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			if (Container.find(name) != Container.end())
				return Container.at(name);
			else
				return std::vector<std::map<std::wstring, float>>();
		};
		virtual std::vector<std::map<std::wstring, float>> GetDataPerWellWithSimilarName(std::wstring name)
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
				return std::vector<std::map<std::wstring, float>>();
			}
			else {
				return Container.at(name);
			}
			
		};
		virtual std::vector<std::map<std::wstring, std::wstring>> GetStringDataPerWell(std::wstring name) 
		{ 
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			if (StringContainer.find(name) != StringContainer.end())
				return StringContainer.at(name);
			else
				return std::vector<std::map<std::wstring, std::wstring>>();
		};
		/// <summary>
		/// Возвращает данные за период с текущей даты - период до текущей даты
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <param name="period">Период в днях</param>
		/// <returns>См в наследниках</returns>
		virtual std::vector<std::map<std::wstring, float>> GetDataForLastTime(std::wstring name, int period)
		{
			//получаем текущее время
			__time64_t long_time;
			_time64(&long_time);
			//переводим секунды в минуты
			int current_time = long_time / 86400 + 25569; //+25569 - смешение с  1970
			return GetDataPerPeriod(name, current_time - period, current_time);
		}
		/// <summary>
		/// Возвращает данные ближайщие к текущей даты
		/// работает только если есть ключ time
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <returns>См в наследниках</returns>
		virtual std::map<std::wstring, float> GetDataForClosestForCurrentTime(std::wstring name)
		{
			//получаем текущее время
			__time64_t long_time;
			_time64(&long_time);
			//переводим секунды в минуты
			int current_time = long_time / 86400 + 25569; //+25569 - смешение с  1970
			auto data = GetDataPerWell(name);

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

			return (closest_id != -1) ? data[closest_id] : std::map<std::wstring, float>();
		}
		/// <summary>
		/// Возвращает данные наиболее близкие к указанной дате
		/// </summary>
		/// <param name="name">Имя скважины</param>
		/// <param name="current_time">Дата в днях</param>
		/// <returns>См в наследниках</returns>
		virtual std::map<std::wstring, float> GetDataForClosestTime(std::wstring name, int current_time)
		{
			auto data = GetDataPerWell(name);

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

			return (closest_id != -1) ? data[closest_id] : std::map<std::wstring, float>();
		}

		std::vector<std::wstring> GetWellsName() {
			//блокируем поток для чтения
			std::lock_guard<std::mutex> lock(DataAccessMutex);
			std::vector<std::wstring> names;
			for (const auto& val : WellsName)
			{
				names.push_back(val.first);
			}
			return names;
		};
	};

	class PerfData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_perf);
	};
	class GISData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_gis_file);
	};
	class MerData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_mer);

		void PushManualy(
			std::wstring Name, 
			std::map<std::wstring, float> Data);

		void ManualOptimize()
		{
			Optimize();
		}
		/// <summary>
		/// Возвращает значение по скважине из одного из массивов
		/// </summary>
		/// <param name="id">
		/// 0 - CumOil
		/// 1 - CumWater
		/// 2 - CumPumpWater
		/// 3 - CumDryOil
		/// 4 - SumPumpTime
		/// 5 - SumIdleTime
		/// 6 - SumMixedTime
		/// 7 - SumDryTime
		/// </param>
		/// <param name="name">Имя скважины</param>
		/// <returns>Значение</returns>
		virtual float GetValue(int id, std::wstring name) override;
	
		std::map<std::wstring, float> CumOil;
		std::map<std::wstring, float> CumWater;
		std::map<std::wstring, float> CumPumpWater;
		std::map<std::wstring, float> CumDryOil;

		std::map<std::wstring, float> SumPumpTime;
		std::map<std::wstring, float> SumIdleTime;
		std::map<std::wstring, float> SumMixedTime;
		std::map<std::wstring, float> SumDryTime;

	};
	class GDISData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_gdis);
	};
	class WCData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_wc);
	};
	class GeoChemData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_gc);
	};
	class FECData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_fec);
		std::vector<std::wstring> GetWellsPerPeriod(int start, int stop);
	};
	class AnomData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		virtual void Push(std::wstring path_to_anom);
	};
	class DataHandleUtils
	{
	public:
		static std::map<std::wstring, std::vector<std::tuple<int, int, int, std::pair<float, float>>>> ComparePerfWithGIS(GISData& gisd, PerfData& perfd, std::wstring name);
		static void CompareGeoChemWithWaterCut(WCData& wcdat, GeoChemData& gcd);
		static std::vector<std::wstring> GetEnabledLayers(GISData& gisd, PerfData& perfd, std::wstring name, int date);
		static std::map<std::wstring, std::vector<std::map<std::wstring, float>>> DivideProduction(
			std::wstring name, 
			MerData& mer, 
			GISData& gis, 
			PerfData& perf,
			std::vector<std::wstring> LayersName);
	};


	class IMerLayeredData {
	protected:
		std::map<std::wstring, std::shared_ptr<MerData>> Mers;
		
	public:
		//virtual void Push(MerData& mer, GISData& gis, PerfData& perf, std::vector<std::wstring> LayersName) = 0;
		virtual std::shared_ptr<MerData> GetMerDataPerLayer(std::wstring LayerName)
		{
			return Mers[LayerName];
		}
	};

	class KH_MerLayeredData : public IMerLayeredData
	{
		public:
		void Push(
			MerData& mer, 
			GISData& gis, 
			PerfData& perf, 
			std::vector<std::wstring> LayersName,
			std::vector<std::wstring> WellNames);
	};

	class RaschData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		void Push(std::wstring path_to_rasch);
	};

	class WellCoordData : public IData {
	public:
		virtual void Push(std::wstring path, std::wstring name);
		void Push(std::wstring path_to_rasch);

		std::vector<std::wstring>& GetWellsName()
		{
			return WellsName;
		}

	private:
		std::vector<std::wstring> WellsName;
	};
}