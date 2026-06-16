#pragma once
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>
#include "../UniversalSVParser.h"

typedef std::vector<std::pair<std::wstring, std::wstring>> JSONObject;
typedef std::vector<std::wstring> JSONArray;

namespace JSON {
	class CreateJSON {
	public:
		static std::wstring CreateObject(std::vector<std::pair<std::wstring, std::wstring>>&& params)
		{
			std::wstring out = L"{";
			for (const auto& key_value : params)
			{
				out += (SurroundQuotes(key_value.first) + L" : " + SurroundQuotes(key_value.second) + L",");
			}
			out.erase(out.end() - 1);
			out += L"}";
			return out;
		}

		static std::wstring CreateArray(std::vector<std::wstring>&& values)
		{
			std::wstring out = L"[";
			for (const auto& value : values)
			{
				out += SurroundQuotes(value) + L",";
			}
			out.erase(out.end() - 1);
			out += L"]";
			return out;
		}


		static void unit_test_SurroundQuotes()
		{
			std::wcout << SurroundQuotes(L"{\"object\" : \"test\"}") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"[ \"arr1\", \"arr1\", \"arr1\"]") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"228") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"228.1488") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"-228.1488") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"-228,1488") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"true") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"false") << "\r\n" << std::flush;
			std::wcout << SurroundQuotes(L"testestetste") << "\r\n" << std::flush;
		}


	private:
		static std::wstring SurroundQuotes(std::wstring val)
		{

			std::wregex object_regex(L"\\{.*\\}", std::regex_constants::ECMAScript);
			std::wregex array_regex(L"\\[.*\\]", std::regex_constants::ECMAScript);
			std::wregex numbers_regex(L"-{0,1}\\d+\\.*\\d*", std::regex_constants::ECMAScript);


			std::wregex rnumbers_regex(L"-{0,1}\\d+,{1}\\d+");

			std::wsmatch match;

			if (std::regex_match(val, match, rnumbers_regex))
			{
				val = ReplaceCommaWithDot(val);
			}

			if (std::regex_match(val, match, object_regex)
				|| std::regex_match(val, match, array_regex)
				|| std::regex_match(val, match, numbers_regex)
				|| val == L"true"
				|| val == L"false")
			{
				return val;
			}
			else {
				return L"\"" + val + L"\"";
			}

			/*if (val[0] != L'{' && val[0] != L'[')
				return L"\"" + val + L"\"";
			else
				return val;*/
		}

		static std::wstring ReplaceCommaWithDot(std::wstring val)
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

	class JSONMessage {
	public:
		static /// <summary>
		/// Создает JSON сообщение
		/// </summary>
		/// <param name="Sender">Отправитель</param>
		/// <param name="ChildSender">Участок отправления</param>
		/// <param name="Status">Статус участка отправления</param>
		/// <param name="Options">Произвольный json обьект отправителя</param>
		/// <returns>Строку JSON</returns>
			std::wstring CreateJSONMessage(
				std::wstring&& Sender,
				std::wstring&& ChildSender,
				std::wstring&& Status,
				std::wstring&& Options = L"")
		{
			return CreateJSON::CreateObject(JSONObject
				{
					{ L"sender", Sender},
					{ L"child_sender", ChildSender},
					{ L"status", Status},
					{ L"options" , Options}
				}
			);
		}
	};

	class IJObject {
	public:
		IJObject(std::wstring&& src)
		{
			JSrc = src;
		}
		virtual std::map<std::wstring, std::unique_ptr<IJObject>> Value() = 0;
		std::wstring GetSrc()
		{
			return JSrc;
		}
	protected:
		std::wstring JSrc;
		size_t FindPair(wchar_t open, wchar_t close, size_t whom);
	};

	class JValue : public IJObject
	{
	public:
		JValue(std::wstring&& src) : IJObject(std::move(src)) {}
		virtual std::map<std::wstring, std::unique_ptr<IJObject>> Value();
	};

	class JArray : public IJObject
	{
	public:
		JArray(std::wstring&& src) : IJObject(std::move(src)) {}
		virtual std::map<std::wstring, std::unique_ptr<IJObject>> Value();
	};

	class JObject : public IJObject {
	public:
		JObject(std::wstring&& src) : IJObject(std::move(src)) {}
		virtual std::map<std::wstring, std::unique_ptr<IJObject>> Value();
	};

	class JSONParser {
	public:
		JSONParser(std::wstring _Path)
		{
			Path = _Path;
		}
		//std::unique_ptr<IJObject> Parse();
		std::unique_ptr<IJObject> Parse();
	private: 
		std::wstring Path;
	};
}