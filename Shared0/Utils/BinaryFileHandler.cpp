#include "BinaryFileHandler.h"


//#define SHOWVOLUME



std::vector<char> BinaryFileRead::Read(std::wstring path)
{
	std::ifstream input(path, std::ios::binary);
	if (input.is_open())
	{
		std::vector<char> bytes(
			(std::istreambuf_iterator<char>(input)),
			(std::istreambuf_iterator<char>()));
		input.close();
		return bytes;
	}
	else {
		return std::vector<char>();
	}
}



std::wstring BinaryFileRead::GetGridcellGuidFromProject(std::wstring path)
{
	wchar_t str[127];
	int itr = 0;
	while (itr < 127)
	{
		GetPrivateProfileStringW(L"Elements", std::to_wstring(itr + 1).c_str(), L"", str, (DWORD)127, path.c_str()); //ищем элемент Wells
		std::wstring names(str);
		if (names == L"Grdecl")
		{
			break;
		}
		itr++;
	}
	std::wstring names(str);
	if (names != L"") //проверка на пустую строку
	{
		wchar_t str1[127];
		GetPrivateProfileStringW(L"Guids", std::to_wstring(itr + 1).c_str(), L"", str1, (DWORD)127, path.c_str()); //Находим соответсвующий guid
		return std::wstring(str1); //Возвращаем guid папки с Model.bin
	}
	return L"";
}

std::map<std::wstring, std::wstring> BinaryFileRead::ReadGRDECLModels(
	std::wstring path, 
	double& xshift, 
	double& yshifth,
	int& nx, 
	int& ny, 
	int& nz)
{
	auto file = Read(path); //читаем файл
	std::map<std::wstring, std::wstring> out;
	if (file.size() > 0)
	{
		std::string sign(&file[0], 16);
		if (sign == "KPFUBOIL  GRDECL") //проверяем сигнатуру
		{
			//загружаем смешение и кол-ва
			memcpy_s(&nx, 4, &file[20], 4);
			memcpy_s(&ny, 4, &file[24], 4);
			memcpy_s(&nz, 4, &file[28], 4);
			memcpy_s(&xshift, 8, &file[32], 8);
			memcpy_s(&yshifth, 8, &file[40], 8);
			//загружаем guid
			std::vector<char> Buffer;
			std::vector<std::string> sBuffer;
			for (int i = 48; i < file.size(); i++)
			{
				if (file[i] != '|')
				{
					Buffer.push_back(file[i]); //все символы собираем в один массив
				}
				else {
					sBuffer.push_back(std::string(Buffer.begin(), Buffer.end())); //на разделителе парсим строку
					Buffer.clear();
					if (sBuffer.size() == 2) //если строка собралась, то добавляем её в выходной массив
					{
						out.insert({ CP1251ToUTF16LE(std::move(sBuffer[0])), CP1251ToUTF16LE(std::move(sBuffer[1])) });

						sBuffer.clear();
					}
				}
			}
			sBuffer.push_back(std::string(Buffer.begin(), Buffer.end())); //для последней строки
			out.insert({ CP1251ToUTF16LE(std::move(sBuffer[0])), CP1251ToUTF16LE(std::move(sBuffer[1])) });
			return out;
		}
	}
	return out;
}




std::vector<char> BinaryFileRead::Read(std::wstring path, long long pos, long long len)
{
	auto size = std::filesystem::file_size(path); //получаем размер файла
	if (size)
	{
		if (pos + len - 1 > size) //проверка на размер
			return std::vector<char>();
		std::ifstream file(path, std::ios::binary);
		std::vector<char> buffer(len);
		if (file.is_open())
		{
			file.seekg(pos); //устанавливает каретка 
			file.read(&buffer[0], len); //читаем
			file.close();
			return buffer;
		}
		else
			return std::vector<char>();
	}
	else {
		throw std::exception("Файл не найден");
	}
}

void BinaryFileRead::OpenFile(std::wstring path)
{
	file_size = std::filesystem::file_size(path);
	if (file_size != -1)
	{
		file_path = path;
		if (file_size > READ_FILE_BUFFER_SIZE) //проверяем размер файла, если он больше 10 мб то грузим по частям
			segment_heap_size = READ_FILE_BUFFER_SIZE;
		else
			segment_heap_size = file_size; //иначе весь
		//нулевая позиция
		current_position = 0;
		file_buffer = std::vector<char>(segment_heap_size);
		//заполняем буфер
		std::ifstream file(path, std::ios::binary);
		if (file.is_open())
		{
			file.read(&file_buffer[0], segment_heap_size);
			file.close();
		}
	}
	else {
		throw std::exception("Файл не найден");
	}
}


char* BinaryFileRead::Read(long long pos, long long len)
{
	if (pos + len <= current_position + segment_heap_size
		&& pos >= current_position) //если запрашиваемый сегмент уже считан то просто его возвращаем
	{
		return &file_buffer[pos - current_position];
	}
	else { //читаем сегмент
		std::ifstream file(file_path, std::ios::binary);
		if (file.is_open())
		{
			auto cp = file.tellg();
			file.seekg(cp + pos);//двигаем на нужную позицию
			current_position = pos;
			if (file_size > current_position + READ_FILE_BUFFER_SIZE - 1)
			{
				file.read(&file_buffer[0], READ_FILE_BUFFER_SIZE); //читаем полный блок
				segment_heap_size = READ_FILE_BUFFER_SIZE;
			}
			else
			{
				file.read(&file_buffer[0], file_size - current_position); //читаем остаток
				segment_heap_size = file_size - current_position;
			}
			file.close(); //хватит
		}
		return &file_buffer[0];
	}
}

BinaryFileRead::~BinaryFileRead()
{
	file_buffer.~vector(); //вызываем деструктор
	file_path.~basic_string();
}

std::wstring BinaryFileRead::CP1251ToUTF16LE(std::string&& src)
{
	std::vector<wchar_t> buffer(src.length() + 1);
	auto size = MultiByteToWideChar(1251, 0, src.c_str(), -1, &buffer[0], buffer.size());
	if (size == buffer.size())
	{
		return std::wstring(&buffer[0]);
	}
	else {
		throw std::exception("convert failed");
	}
}




