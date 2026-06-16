#pragma once
#include <string>
#include <regex>
#include <iostream>
#include <map>
#include <cwctype>

namespace PathUtils {
	class Utils {
	public:
		

		

		

		 template<typename T> static int ConvertDateToExcelDate(std::basic_string<T> date)
		{
			 if (date.size() == 0)
			 {
				 return 0;
			 }
			std::basic_regex<T> date_regex;
			/*std::basic_regex<T> date_excel_regex;*/
			std::basic_regex<T> yyyymmdd_regex;
			if constexpr (std::is_same<T, char>::value)
			{
				date_regex = std::basic_regex<T>("\\d{2}\\W\\d{2}\\W\\d{4}");
				/*date_excel_regex = std::basic_regex<T>(".{4}\\d{4}");*/
				yyyymmdd_regex = std::basic_regex<T>("\\d{4}\\W\\d{2}\\W\\d{2}");

			}
			if constexpr (std::is_same<T, wchar_t>::value)
			{
				date_regex = std::basic_regex<T>(L"\\d{2}\\W\\d{2}\\W\\d{4}");
				/*date_excel_regex = std::basic_regex<T>(L".{4}\\d{4}");*/
				yyyymmdd_regex = std::basic_regex<T>(L"\\d{4}\\W\\d{2}\\W\\d{2}");
			}

			std::match_results<std::basic_string<T>::const_iterator> base_match;

			//const std::basic_regex<T> date_excel_regex(L".{4}\\d{4}");

			//const std::basic_regex<T> yyyymmdd_regex(L"\\d{4}\\W\\d{2}\\W\\d{2}");

			
			if (std::regex_match(date, base_match, date_regex))//הה.לל.דדדד
			{
				int year = stoi(std::basic_string<T>(date.begin() + 6, date.end())) - 1900 + 70;
				int month = stoi(std::basic_string<T>(date.begin() + 3, date.begin() + 5));
				int day = stoi(std::basic_string<T>(date.begin(), date.begin() + 2));



				tm time = { 0 };
				time.tm_year = year;
				time.tm_mon = month - 1;
				time.tm_mday = day;
				auto seconds = mktime(&time);
				return seconds / 86400;
			}
			if (std::regex_match(date, base_match, yyyymmdd_regex))//דדדד.לל.הה
			{
				int year = stoi(std::basic_string<T>(date.begin(), date.begin() + 4)) - 1900 + 70;
				int month = stoi(std::basic_string<T>(date.begin() + 5, date.begin() + 7));
				int day = stoi(std::basic_string<T>(date.begin() + 8, date.end()));



				tm time = { 0 };
				time.tm_year = year;
				time.tm_mon = month - 1;
				time.tm_mday = day;
				auto seconds = mktime(&time);
				return seconds / 86400;
			}
			//if (std::regex_match(date, base_match, date_excel_regex))//הה.לל.דדדד
			//{
			//	static const std::map<std::basic_string<T>, int> Month
			//	{
			//		{L"ÿםג", 0},
			//		{L"פוג", 1},
			//		{L"לאנ", 2},
			//		{L"אןנ", 3},
			//		{L"לאי", 4},
			//		{L"ט‏ם", 5},
			//		{L"ט‏כ", 6},
			//		{L"אגד", 7},
			//		{L"סום", 8},
			//		{L"מךע", 9},
			//		{L"םמÿ", 10},
			//		{L"הוך", 11}
			//	};

			//	auto moth = date.substr(0, 3);
			//	auto year = date.substr(4);

			//	auto _month = Month.at(moth);
			//	auto _year = stoi(year) - 1900 + 70;

			//	tm time = { 0 };
			//	time.tm_year = _year;
			//	time.tm_mon = _month;
			//	time.tm_mday = 1;
			//	auto seconds = mktime(&time);
			//	return seconds / 86400;
			//}

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

		/*static int GetExcelCurrentTime()
		{
			__time64_t long_time;
			_time64(&long_time);
			return long_time / 86400 + 25569;
		}*/
	};
}