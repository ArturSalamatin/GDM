#pragma once
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <regex>

typedef std::vector<std::pair<std::string, std::string>> JSONObject;
typedef std::vector<std::string> JSONArray;

namespace JSON {
	class CreateJSON {
	public:
		static std::string CreateObject(std::vector<std::pair<std::string, std::string>>&& params)
		{
			std::string out = "{";
			for (const auto& key_value : params)
			{
				out += (SurroundQuotes(key_value.first) + " : " + SurroundQuotes(key_value.second) + ",");
			}
			out.erase(out.end() - 1);
			out += "}";
			return out;
		}

		static std::string CreateArray(std::vector<std::string>&& values)
		{
			std::string out = "[";
			for (const auto& value : values)
			{
				out += SurroundQuotes(value) + ",";
			}
			out.erase(out.end() - 1);
			out += "]";
			return out;
		}


		static void unit_test_SurroundQuotes()
		{
			std::cout << SurroundQuotes("{\"object\" : \"test\"}") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("[ \"arr1\", \"arr1\", \"arr1\"]") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("228") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("228.1488") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("-228.1488") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("-228,1488") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("true") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("false") << "\r\n" << std::flush;
			std::cout << SurroundQuotes("testestetste") << "\r\n" << std::flush;
		}


	private:
		static std::string SurroundQuotes(std::string val)
		{

			std::regex object_regex("\\{.*\\}", std::regex_constants::ECMAScript);
			std::regex array_regex("\\[.*\\]", std::regex_constants::ECMAScript);
			std::regex numbers_regex("-{0,1}\\d+\\.*\\d*", std::regex_constants::ECMAScript);


			std::regex rnumbers_regex("-{0,1}\\d+,{1}\\d+");

			std::smatch match;

			if (std::regex_match(val, match, rnumbers_regex))
			{
				val = ReplaceCommaWithDot(val);
			}

			if (std::regex_match(val, match, object_regex)
				|| std::regex_match(val, match, array_regex)
				|| std::regex_match(val, match, numbers_regex)
				|| val == "true"
				|| val == "false")
			{
				return val;
			}
			else {
				return "\"" + val + "\"";
			}

			/*if (val[0] != L'{' && val[0] != L'[')
				return "\"" + val + "\"";
			else
				return val;*/
		}

		static std::string ReplaceCommaWithDot(std::string val)
		{
			size_t pos = val.find(",");
			if (pos != std::string::npos)
			{
				auto nstr = val;
				nstr.replace(nstr.begin() + pos, nstr.begin() + pos + 1, ".");
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
			std::string CreateJSONMessage(
				std::string&& Sender,
				std::string&& ChildSender,
				std::string&& Status,
				std::string&& Options = "")
		{
			return CreateJSON::CreateObject(JSONObject
				{
					{ "sender", Sender},
					{ "child_sender", ChildSender},
					{ "status", Status},
					{ "options" , Options}
				}
			);
		}
	};

	class IJObject {
	public:
		IJObject(std::string&& src)
		{
			JSrc = src;
		}
		virtual std::map<std::string, std::unique_ptr<IJObject>> Value() = 0;
		std::string GetSrc()
		{
			return JSrc;
		}
	protected:
		std::string JSrc;
		size_t FindPair(wchar_t open, wchar_t close, size_t whom);
	};

	class JValue : public IJObject
	{
	public:
		JValue(std::string&& src) : IJObject(std::move(src)) {}
		virtual std::map<std::string, std::unique_ptr<IJObject>> Value();
	};

	class JArray : public IJObject
	{
	public:
		JArray(std::string&& src) : IJObject(std::move(src)) {}
		virtual std::map<std::string, std::unique_ptr<IJObject>> Value();
	};

	class JObject : public IJObject {
	public:
		JObject(std::string&& src) : IJObject(std::move(src)) {}
		virtual std::map<std::string, std::unique_ptr<IJObject>> Value();
	};

	class JSONParser {
	public:
		JSONParser(std::string _Path)
		{
			Path = _Path;
		}
		//std::unique_ptr<IJObject> Parse();
		std::unique_ptr<IJObject> Parse();
	private: 
		std::string Path;
	};
}