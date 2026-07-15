#pragma once
#include <string>
#include <regex>
#include <iostream>

namespace PathUtils {
	class Utils {
	public:
		static std::string GetDir(std::string project_path)
		{
			//вычисляем путь папки с проектом
			std::string prpath = project_path;
			std::reverse(prpath.begin(), prpath.end()); //реверсим путь
			int size = prpath.find("\\"); //находим первое вхождение обратного слеша
			prpath.erase(0, size); //удаляем все до него
			std::reverse(prpath.begin(), prpath.end()); //реверсим путь обратно
			return prpath;
		}
		static std::string RelativePathParser(std::string raw, std::string MainDirPath)
		{
			/*
				* ./ - установить текущий катало
				* ././ - установить на один каталог выше
				* ./././ - на два каталога выше
				* и так далее
				*/

			int _off = 0;
			int _pos = 0;
			int entry = 0;
			do {
				_pos = raw.find(".\\", _off);
				if (_pos != std::string::npos)
				{
					_off = _pos + 2;
					entry++;
				}
			} while (_pos != std::string::npos);
			if (entry)
			{
				std::string add_path = MainDirPath;
				//i == 1 т.к. при entry = 1 добавляем только путь до рабочей папки
				for (int i = 1; i < entry; i++)
				{
					add_path = GetDir(add_path.substr(0, add_path.size() - 1));
				}
				raw.replace(0, entry * 2, add_path);
				return raw;
			}
			else {
				return raw;
			}
		}

		static std::string DelimeterReplacer(std::string raw)
		{
			if (raw.find(".") != std::string::npos)
			{
				int pos = raw.find(".");
				auto nstr = raw;
				nstr.replace(nstr.begin() + pos, nstr.begin() + pos+ 1, ",");
				return nstr;
			}
			else {
				return raw;
			}
		}

		static int ConvertDateToExcelDate(const std::string& date)
		{
			const std::regex date_regex("\\d{2}\\W\\d{2}\\W\\d{4}");
			std::smatch base_match;

			if (std::regex_match(date, base_match, date_regex))//дд.мм.гггг
			{
				int year = stoi(std::string(date.begin() + 6, date.end())) - 1900 + 70;
				int month = stoi(std::string(date.begin() + 3, date.begin() + 5));
				int day = stoi(std::string(date.begin(), date.begin() + 2));



				tm time = { 0 };
				time.tm_year = year;
				time.tm_mon = month - 1;
				time.tm_mday = day;
				auto seconds = mktime(&time);
				return seconds / 86400;
			}
			else {
				//std::wcout << date;
				/*if (!date.empty())
				{*/
					int _date = stoi(date);
					return _date;
				/*}
				else {
					return 0;
				}*/
			}
		}
	};
}