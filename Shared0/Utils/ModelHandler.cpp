#include "ModelHandler.h"

ModelHanlder::ReadModel::ReadModel(std::wstring projectPath)
{
	if (std::filesystem::exists(projectPath))
	{
		ProjectPath = projectPath;
		//получаем папку проекта
		auto project_dir = GetDir(ProjectPath);
		ProjectDirectory = project_dir;
		//читаем путь до модели
		auto model_guid = BinaryFileRead::GetGridcellGuidFromProject(ProjectPath);
		//читаем файлы модели и её параметры
		ModelFilePath = project_dir + model_guid + L"\\";
		if(std::filesystem::exists(ModelFilePath + L"Model.bin"))
			ModelsFile = BinaryFileRead::ReadGRDECLModels((ModelFilePath + L"Model.bin").c_str(), xshift, yshift, nx, ny, nz);
		else
			throw std::exception("Файл модели не существует");
	}
	else
		throw std::exception("Файл проекта не существует");
}

std::vector<std::vector<std::vector<std::vector<float>>>> ModelHanlder::ReadModel::ReadPillars()
{
	if (
		ModelsFile.find(L"COORD") != ModelsFile.end()  //проверяем наличие ключа
		&& std::filesystem::exists(ModelFilePath + ModelsFile[L"COORD"])) //и файла
	{
		std::vector<std::vector<std::vector<std::vector<float>>>> pillars;
		BinaryFileRead filereader;
		filereader.OpenFile( (ModelFilePath + ModelsFile[L"COORD"]).c_str() );
		std::string sign(filereader.Read(0, 16), 16);
		if (sign == "KPFUBOIL    GRID") //проверяем 
		{
			//получаем размеры
			int xc = 0;
			int yc = 0;
			int zc = 0;
			memcpy_s(&xc, 4, filereader.Read(20, 4), 4);
			memcpy_s(&yc, 4, filereader.Read(24, 4), 4);
			memcpy_s(&zc, 4, filereader.Read(28, 4), 4);
			long last_pos = 32;
			for (int j = 0; j < yc + 1; j++)
			{
				pillars.push_back(std::vector<std::vector<std::vector<float>>>());
				for (int i = 0; i < xc + 1; i++)
				{
					float x = 0;
					float y = 0;
					float z = 0;

					float x1 = 0;
					float y1 = 0;
					float z1 = 0;
					//Первая напрявляющая меньшая
					memcpy_s(&x, 4, filereader.Read(last_pos, 4), 4);
					memcpy_s(&y, 4, filereader.Read(last_pos + 4l, 4), 4);
					memcpy_s(&z, 4, filereader.Read(last_pos + 8l, 4), 4);
					//Сторая направлющая большая
					memcpy_s(&x1, 4, filereader.Read(last_pos + 12l, 4), 4);
					memcpy_s(&y1, 4, filereader.Read(last_pos + 16l, 4), 4);
					memcpy_s(&z1, 4, filereader.Read(last_pos + 20l, 4), 4);
					last_pos += 24;
					//Сохраняем направляющую
					pillars[pillars.size() - 1].push_back(std::vector < std::vector<float>>
					{
						std::vector<float> {x + (float)xshift, y + (float)yshift, z},
							std::vector<float> {x1 + (float)xshift, y1 + (float)yshift, z1} });
				}
			}
			filereader.~BinaryFileRead();
			return pillars;
		}
		else
		{
			filereader.~BinaryFileRead();
			throw std::exception("Файл COORD поврежден");
		}
	}
	else {
		throw std::exception("Файл COORD отсуствует");
	}
}

std::vector<std::vector<std::vector<std::vector<float>>>> ModelHanlder::ReadModel::GetDiagonals(std::vector<std::vector<std::vector<std::vector<float>>>>* plrs)
{
	std::vector<std::vector<std::vector<std::vector<float>>>> out;
	for (int j = 0; j < ny; j++)
	{
		out.push_back(std::vector<std::vector<std::vector<float>>>());
		for (int i = 0; i < nx; i++)
		{
			
			float x00 = plrs->at(j)[i][0][0];
			float y00 = plrs->at(j)[i][0][1];
			float z00 = plrs->at(j)[i][0][2];

			float x11 = plrs->at(j + 1)[i + 1][1][0];
			float y11 = plrs->at(j + 1)[i + 1][1][1];
			float z11 = plrs->at(j + 1)[i + 1][1][2];

			out.back().push_back(std::vector < std::vector<float>>
			{
				std::vector<float> {x00, y00, z00},
				std::vector<float> {x11, y11, z11}
			});
		}
	}
	
	return out;
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadPoro()
{
	return ReadModelFile(L"PORO");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadSoil()
{
	return ReadModelFile(L"SOIL");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadVolume()
{
	return ReadModelFile(L"VOLUME");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadSo()
{
	return ReadModelFile(L"SO");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadPermx()
{
	return ReadModelFile(L"PERMX");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadPermz()
{
	return ReadModelFile(L"PERMZ");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadActnum()
{
	return ReadModelFile(L"ACTNUM");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadPressure()
{
	return ReadModelFile(L"PRESSURE");
}

std::vector<std::vector<std::vector<float>>> ModelHanlder::ReadModel::ReadP()
{
	return ReadModelFile(L"P");
}

std::vector<ModelHanlder::ReadModel::WellData> ModelHanlder::ReadModel::GetWellsData(const std::wstring& Path) // путь до папки Original
{
	//WellDataHandler::WellCoordData data;
	WellDataHandler::WellCoordData wellCoords;

	bool _entry = false;
	for (const auto& entry : std::filesystem::directory_iterator(Path))
	{
		if (entry.path().wstring().find(L"Пластопересечение") != std::wstring::npos)
		{
			wellCoords.Push(entry.path().wstring());
			_entry = true;
			break;
		}
	}

	if (!_entry)
		throw std::exception("unable to find Coords File");





	std::vector<ModelHanlder::ReadModel::WellData> out;


	for (const auto& name : wellCoords.GetWellsName())
	{
		auto well = wellCoords.GetDataPerWell(name);
		ModelHanlder::ReadModel::WellData _Well;

		if (well.size() == 0)
			continue;

		_Well.Name = name;
		_Well.IntersectionCoords = { well[0].at(L"X"), well[0].at(L"Y") };

		out.push_back(_Well);
	}
	
	return out;
}



