#include "JSONCreate.h"
#include <iostream>
std::map<std::wstring, std::unique_ptr<JSON::IJObject>> JSON::JObject::Value()
{
	size_t end = FindPair(L'{', L'}', 0);
	if (end != -1)
	{
		std::map<std::wstring, std::unique_ptr<JSON::IJObject>> out;
		//удаляем все ненужные символы
		JSrc.erase(std::remove_if(JSrc.begin(), JSrc.end(), [isQuotes = false](wchar_t x) mutable {
			if (x == L'\"')
			{
				isQuotes = !isQuotes;
			}
			return (isQuotes) ? false : x == L' ' || x == L'\n' || x == L'\r';
		}), JSrc.end());

		do {
			size_t pos = JSrc.find(L"\\\\");
			if (pos != std::wstring::npos)
			{
				JSrc.erase(JSrc.begin() + pos);
			}
		} while (JSrc.find(L"\\\\") != std::wstring::npos);

		size_t fcolon = 0;
		size_t foffset = 1; //указывает на следующий парный символ
		do {
			fcolon = JSrc.find(L':', foffset);
			if (fcolon != std::wstring::npos)
			{
				size_t close = 0;
				int type = 0;

				int shiftNeed = 0;
				
				switch (JSrc[fcolon + 1])
				{
				case L'{':
					close = FindPair(L'{', L'}', fcolon + 1);
					if (close == -1)
						throw std::exception("missing }");
					break;
				case L'[':
					type = 1;
					close = FindPair(L'[', L']', fcolon + 1);
					if (close == -1)
						throw std::exception("missing ]");
					break;
				default:
					type = 2;
					if (JSrc[fcolon + 1] == L'\"')
					{
						shiftNeed = 1;
						close = JSrc.find(L'\"', fcolon + 2); //т.к. на +1 - начинающие кавычки 
						if (close == std::wstring::npos)
							throw std::exception("missing \"");
					}
					else {
						close = JSrc.find(L',', fcolon) - 1; //-1 - т.к. указывать будет ровно на запятую
						if (close + 1 == std::wstring::npos)
						{
							close = end - 1;
							if (fcolon > close)
								throw std::exception("missing data after :");
						}
					}
					break;
				}

				std::wstring src = L"";
				if (type == 2 && shiftNeed)
				{
					src = JSrc.substr(fcolon + 2, close - (fcolon +2)); //+2 т.к. избегаем \"
				}
				else {
					src = JSrc.substr(fcolon + 1, close - fcolon);
				}

				//auto src = JSrc.substr(fcolon + 1, close - fcolon);
				auto key = JSrc.substr(foffset, fcolon - foffset);

				foffset = close + 2; //+2 т.к. } , {

				auto clean = [](wchar_t x)
				{
					return x == L'\"';
				};
				key.erase(std::remove_if(key.begin(), key.end(), clean), key.end());
				switch (type)
				{
				case 0:
				{
					//auto obj = std::make_unique<JObject>(std::move(src));
					out[key] = std::make_unique<JObject>(std::move(src));
				}
				break;
				case 1:
				{
					//auto obj = std::make_unique<JArray>(std::move(src));
					out[key] = std::make_unique<JArray>(std::move(src));
				}
				break;
				case 2:
				{
					//auto obj = std::make_unique<JValue>(std::move(src));
					out[key] = std::make_unique<JValue>(std::move(src));
					//out.emplace(key, std::move(src));
				}
				break;
				}
			}
		} while (fcolon != std::wstring::npos);

		////находим все запятые в строке
		//std::vector<size_t> commas;
		//size_t pos = 0;
		//size_t offset = 0;
		//do {
		//	pos = JSrc.find(L',', offset);
		//	if (pos != std::wstring::npos)
		//	{
		//		commas.push_back(pos);
		//		offset = pos + 1;
		//	}
		//	else {
		//		commas.push_back(JSrc.length() - 2); //-1 -> }, тогда конец строки - конец отдела (может быть и пустым фрагментом если последний символ запятая)
		//	}
		//} while (pos != std::wstring::npos);
		////делим на значения
		//for (int i = 0; i < commas.size(); i++)
		//{
		//	size_t prev = (i) ? commas[i - 1] + 1: 1; //+1 т.к. мы рассматриваем со следующей до текущей позиции
		//	//auto substr = JSrc.substr(prev, commas[i] - prev);
		//	size_t colon = std::find(JSrc.begin() + prev, JSrc.begin() + commas[i], L':') - JSrc.begin();
		//	//size_t colon = JSrc.find(L":", prev, (size_t)(commas[i] - prev));
		//	//получаем ключ и значение
		//	auto key = JSrc.substr(prev, colon - prev);
		//	auto src = JSrc.substr(colon + 1, commas[i - 1] - colon);

		//	auto clean = [](wchar_t x)
		//	{
		//		return x == L' ' || x == L'\n' || x == L'\r' || x == L'\"';
		//	};
		//	//удаляем все лишние символы
		//	key.erase(std::remove_if(key.begin(), key.end(), clean), key.end());
		//	src.erase(std::remove_if(src.begin(), src.end(), clean), src.end());

		//	switch (src[0])
		//	{
		//	case L'{':
		//	{
		//		auto obj = std::make_unique<JObject>(src);
		//		out[key] = std::move(obj);
		//	}
		//		break;
		//	case L'[':
		//	{
		//		auto obj = std::make_unique<JArray>(src);
		//		out[key] = std::move(obj);
		//	}
		//		break;
		//	default:
		//	{
		//		auto obj = std::make_unique<JValue>(src);
		//		out[key] = std::move(obj);
		//	}
		//		break;
		//	}
		//}

		return out;
	}
	else {
		throw std::exception("Invalid json object");
	}
}

std::map<std::wstring, std::unique_ptr<JSON::IJObject>> JSON::JArray::Value()
{
	size_t end = FindPair(L'[', L']', 0);
	if (end != -1)
	{
		std::map<std::wstring, std::unique_ptr<JSON::IJObject>> out;
		//удаляем все ненужные символы
		JSrc.erase(std::remove_if(JSrc.begin(), JSrc.end(), [isQuotes = false](wchar_t x) mutable {
			if (x == L'\"')
			{
				isQuotes = !isQuotes;
			}
			return (isQuotes) ? false : x == L' ' || x == L'\n' || x == L'\r';
		}), JSrc.end());

		
		size_t fblock_start = 1;
		size_t fblock_stop = 0;
		size_t foffset = 1; //указывает на следующий парный символ

		int kcount = 0;

		do {
			switch (JSrc[fblock_start])
			{
			case L'{':
			{
				fblock_stop = FindPair(L'{', L'}', fblock_start);
				if (fblock_stop != -1)
				{
					//auto src = JSrc.substr(fblock_start, fblock_stop - fblock_start + 1);
					out[std::to_wstring(kcount)] = std::make_unique<JObject>(
						JSrc.substr(fblock_start, fblock_stop - fblock_start + 1));
					fblock_start = fblock_stop + 2;
				}
			}
			break;
			case L'[':
			{
				fblock_stop = FindPair(L'[', L']', fblock_start);
				if (fblock_stop != -1)
				{
					//auto src = JSrc.substr(fblock_start, fblock_stop - fblock_start + 1);
					out[std::to_wstring(kcount)] = std::make_unique<JArray>(
						JSrc.substr(fblock_start, fblock_stop - fblock_start + 1));
					fblock_start = fblock_stop + 2;
				}
			}
			break;
			default:
			{
				if (JSrc[fblock_start] == L'\"')
				{
					fblock_start++;//+1 символ т.к. кавычки нам не нужны

					fblock_stop = JSrc.find(L'\"', fblock_start + 1);
				}
				else {
					fblock_stop = JSrc.find(L',', fblock_start);
					if (fblock_stop == std::wstring::npos) //если точек нет то читаем до конца
					{
						fblock_stop = end;
						if (fblock_stop <= fblock_start)
							fblock_stop = std::wstring::npos;
					}
				}
				if (fblock_stop != std::wstring::npos)
				{
					if (fblock_stop != fblock_start) //Если они равны, то скорее всего неверный символ
					{
						//auto src = JSrc.substr(fblock_start, fblock_stop - fblock_start);
						out[std::to_wstring(kcount)] = std::make_unique<JValue>(
							JSrc.substr(fblock_start, fblock_stop - fblock_start));
					}
					fblock_start = fblock_stop + 1;
				}

			}
			break;
			}

			kcount++;

		} while (fblock_stop != std::wstring::npos && fblock_stop != -1 && fblock_start < JSrc.length());

		return out;
	}

	else {
		throw std::exception("Invalid json array");
	}
}

std::map<std::wstring, std::unique_ptr<JSON::IJObject>> JSON::JValue::Value()
{
	return std::map<std::wstring, std::unique_ptr<JSON::IJObject>>();
}

/*std::unique_ptr<JSON::IJObject> JSON::JSONParser::Parse()
{
	UniversalSCParser::CP1251FileParser Parser(Path);
	auto content = Parser.Read();

	for (size_t i = 0; i < content.size(); i++)
	{
		switch (content[i])
		{
		case L'{':
			return std::make_unique<JSON::JObject>(content);
			break;
		case L'[':
			return std::make_unique<JSON::JArray>(content);
			break;
		}
	}

	throw std::exception("invalid json object");
}*/

std::unique_ptr<JSON::IJObject> JSON::JSONParser::Parse()
{
	UniversalSCParser::UTF8FileParser Parser(Path);
	auto content = Parser.Read();

	for (size_t i = 0; i < content.size(); i++)
	{
		switch (content[i])
		{
		case L'{':
			return std::make_unique<JSON::JObject>(std::move(content));
			break;
		case L'[':
			return std::make_unique<JSON::JArray>(std::move(content));
			break;
		default:
			return std::make_unique<JSON::JValue>(std::move(content));
			break;
		}
	}

	throw std::exception("invalid json object");
}

size_t JSON::IJObject::FindPair(wchar_t open, wchar_t close, size_t whom)
{
	int oc = 0;
	for (size_t i = whom; i < JSrc.size(); i++)
	{
		if (JSrc[i] == open)
		{
			oc++;
		}
		if (JSrc[i] == close)
		{
			oc--;
			if (!oc)
			{
				return i;
			}
		}
	}
	return -1;
}
