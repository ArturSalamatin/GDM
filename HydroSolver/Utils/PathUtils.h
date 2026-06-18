#pragma once
#include <string>
#include <regex>
#include <iostream>

namespace PathUtils {
	class Utils {
	public:
		static std::wstring GetDir(std::wstring project_path)
		{
			//вычисляем путь папки с проектом
			std::wstring prpath = project_path;
			std::reverse(prpath.begin(), prpath.end()); //реверсим путь
			int size = prpath.find(L"\\"); //находим первое вхождение обратного слеша
			prpath.erase(0, size); //удаляем все до него
			std::reverse(prpath.begin(), prpath.end()); //реверсим путь обратно
			return prpath;
		}
		static std::wstring RelativePathParser(std::wstring raw, std::wstring MainDirPath)
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
				_pos = raw.find(L".\\", _off);
				if (_pos != std::wstring::npos)
				{
					_off = _pos + 2;
					entry++;
				}
			} while (_pos != std::wstring::npos);
			if (entry)
			{
				std::wstring add_path = MainDirPath;
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

		static std::wstring DelimeterReplacer(std::wstring raw)
		{
			if (raw.find(L".") != std::wstring::npos)
			{
				int pos = raw.find(L".");
				auto nstr = raw;
				nstr.replace(nstr.begin() + pos, nstr.begin() + pos+ 1, L",");
				return nstr;
			}
			else {
				return raw;
			}
		}

		static int ConvertDateToExcelDate(const std::wstring& date)
		{
			const std::wregex date_regex(L"\\d{2}\\W\\d{2}\\W\\d{4}");
			std::wsmatch base_match;

			if (std::regex_match(date, base_match, date_regex))//дд.мм.гггг
			{
				int year = stoi(std::wstring(date.begin() + 6, date.end())) - 1900 + 70;
				int month = stoi(std::wstring(date.begin() + 3, date.begin() + 5));
				int day = stoi(std::wstring(date.begin(), date.begin() + 2));



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