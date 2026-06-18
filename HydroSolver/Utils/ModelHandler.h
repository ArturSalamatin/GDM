#pragma once
#include <filesystem>

#include "BinaryFileHandler.h"
#include "WellDataHandler.h"

namespace ModelHanlder {
	class ReadModel {
	public:
	//	static 
			struct WellData {
			std::wstring Name; //имя скважины
		//	std::wstring Guid; //гуид
			std::vector<double> IntersectionCoords; //координаты пластопересечения
		//	BinaryFileRead::MERResult MerData; //Данные по МЭР
		//	std::vector<std::map<std::wstring, float>> PerfData; //Данные перфорации
		};
		int nx; //кол-во ячеек по х
		int ny; //кол-во ячеек по y
		int nz; //кол-во ячеек по z
		double xshift; //сдвиг начала координат модели
		double yshift; 
		/// <summary>
		/// Инициализирует чтение модели
		/// </summary>
		/// <param name="ProjectPath">Путь до файла проекта</param>
		ReadModel(std::wstring ProjectPath);
		ReadModel() {}
		/// <summary>
		/// Возвращает векторы-направляющие сетки
		/// Приводит их в координаты скважин
		/// </summary>
		/// <returns>Вектор вида [y][x][0-1] где 0-1 нижняя и верхняя координата направляющей</returns>
		std::vector < std::vector < std::vector< std::vector<float> >>> ReadPillars();
		/// <summary>
		/// Возвращает диагонали кубов
		/// </summary>
		/// <param name="plrs">Массив с векторами-направляющими сетки</param>
		/// <returns>Вектор вида [y][x][0-1], где 0 - нижняя ближняя левая точка, 1 - верхняя дальняя правая</returns>
		std::vector < std::vector < std::vector< std::vector<float> >>> GetDiagonals(
			std::vector < std::vector < std::vector< std::vector<float> >>> *plrs);
		/// <summary>
		/// Считывает пористость и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadPoro();
		/// <summary>
		/// Считывает начальную нефтенасыщенность и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadSoil();
		/// <summary>
		/// Считывает объем и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadVolume();
		/// <summary>
		/// Считывает текущую нефтенасыщенность и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadSo();
		/// <summary>
		/// Считывает проницаемость по Х и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadPermx();
		/// <summary>
		/// Считывает проницаемость по У и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadPermz();
		/// <summary>
		/// Считывает активные ячейки и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadActnum();
		/// <summary>
		/// Считывает давление и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadPressure();
		/// <summary>
		/// Считывает P и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadP();
		/// <summary>
		/// Возвращает информацию по скважинам
		/// </summary>
		/// <returns>Вектор с WellData по каждой скважине</returns>
		std::vector<WellData> GetWellsData(const std::wstring& Path);

	//	std::map<std::wstring, std::vector<std::tuple<int, int, std::pair<float, float>>>> ComparePerfWithGIS(WellDataHandler::GISData& gis, std::wstring name);
	private:
		std::wstring ProjectPath; //путь до проекта
		std::wstring ProjectDirectory; //c концевым '\'
		std::wstring ModelFilePath; //с концевым '\'
		std::map<std::wstring, std::wstring> ModelsFile;//карта с название - гуид
	//	WellDataHandler::PerfData Pfd;
		/// <summary>
		/// Считывает указанный массив и возвращает в виде вектора [z][y][x]
		/// </summary>
		/// <param name="Name">Имя считываемого</param>
		/// <returns>Массив вида [z][y][x] - значение</returns>
		std::vector < std::vector < std::vector< float >>> ReadModelFile(std::wstring&& Name) {
			std::wstring path_to_file = ModelFilePath + ModelsFile[Name];
			if (std::filesystem::exists(path_to_file))
			{
				BinaryFileRead Reader;
				Reader.OpenFile(path_to_file.c_str());
				std::vector<std::vector<std::vector<float>>> out;
				std::string sign(Reader.Read(0, 16), 16);
				if (sign == "KPFUBOIL    GRID") //Проверяем заголовок
				{
					long last_pos = 32;
					for (long k = 0; k < nz; k++)
					{
						out.push_back(std::vector<std::vector<float>>());
						for (long j = 0; j < ny; j++)
						{
							out.back().push_back(std::vector<float>());
							for (long i = 0; i < nx; i++)
							{
								float result = 0;
								std::memcpy(&result, Reader.Read(last_pos, 4), 4);
								out.back().back().push_back(result);
								last_pos += 4;
							}
						}
					}
					return out;
				}
				else {
					std::string exception_msg = "Файл  повержден";
					throw std::exception(exception_msg.c_str());
				}
			}
			else {
				std::string exception_msg = "Файл недоступен";
				throw std::exception(exception_msg.c_str());
			}
		}

		static std::wstring GetDir(std::wstring project_path)
		{
			//вычисляем путь папки с проектом
			std::wstring prpath = project_path;
			std::reverse(prpath.begin(), prpath.end()); //реверсим путь
			size_t size = prpath.find(L"\\"); //находим первое вхождение обратного слеша
			prpath.erase(0, size); //удаляем все до него
			std::reverse(prpath.begin(), prpath.end()); //реверсим путь обратно
			return prpath;
		}
	};
}