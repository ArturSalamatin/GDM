#pragma once
#define NOMINMAX
#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <filesystem>
#include <map>
#include <iostream>
#include <intrin.h>
#include <immintrin.h>
#include <Windows.h>
#include <stringapiset.h>

namespace UniversalSCParser {
	class EncodingFinder {
	public:
		/// <summary>
		/// Определяет кодировку по заголовку
		/// </summary>
		/// <param name="file">ссылка на буфер с файлом</param>
		/// <returns>0 - UTF16LE, 1 - UTF8, 2 - UTF16BE</returns>
		static int FindEncoding(std::vector<char>* file);
	};
	class FileReader {
	public:
		FileReader();
		FileReader(std::wstring path);
		std::unique_ptr<char[]> Read(size_t& size, bool include_null = false);
	private:
		std::wstring Path;
	};

	template<typename _chartype> class IEncodingParser {
	public:
		virtual wchar_t ToWChar(_chartype& ch) {
			if (EncodingMap.find(ch) != EncodingMap.end())
			{
				return EncodingMap.at(ch);
			}
			else {
				wchar_t out = 0;
				std::memcpy(&out, &ch, sizeof(_chartype) < sizeof(wchar_t) ? sizeof(_chartype) : sizeof(wchar_t));
				if (out == 0)
					return 0xffff;
				else
					return out;
			}
		}
		virtual std::wstring ToWString(std::basic_string<_chartype>& string) {
			std::wstring out;
			for (char& ch : string)
			{
				out += ToWChar(ch);
			}
			return out;
		}
		virtual std::wstring ToWString(_chartype* string, size_t size)
		{
			std::wstring out;
			for (int i = 0; i < size; i++)
			{
				out += ToWChar(*(string + i));
			}
			return out;
		}
		virtual std::wstring ToWString(std::unique_ptr<std::vector<_chartype>> string)
		{
			std::wstring out;
			for (char& ch : *string)
			{
				out += ToWChar(ch);
			}
			return out;
		}
		virtual std::vector<wchar_t> ToWArray(_chartype* string, size_t size)
		{
			std::vector<wchar_t> out(size);
			for (int i = 0; i < size; i++)
			{
				out[i] = ToWChar(string[i]);
			}
			return out;
		}
		virtual _chartype ToChar(wchar_t& ch)
		{
			if (DecodingMap.find(ch) != DecodingMap.end())
			{
				return DecodingMap.at(ch);
			}
			else {
				_chartype out = 0;
				std::memcpy(&out, &ch, sizeof(_chartype) < sizeof(wchar_t) ? sizeof(_chartype) : sizeof(wchar_t));
				
				if (out == 0)
					return 0xff;
				else
					return out;

			}
		}
		virtual std::basic_string<_chartype> ToString(std::wstring& string) {
			std::basic_string<_chartype> out;
			for (int i = 0; i < string.size(); i++)
			{
				auto test = ToChar(string[i]);
				out += test;
			}
			return out;
		}
	protected:
		std::map<_chartype, wchar_t> EncodingMap;
		std::map<wchar_t, _chartype> DecodingMap;
	};

	class CP1251Encoder : public IEncodingParser<char> {
	public:
		CP1251Encoder();
	};

	class UTF8Encoder {
	public:
		static std::wstring FromUT8toUTF16(std::unique_ptr<char[]> src, size_t size);
		static std::vector<wchar_t> FromUT8toUTF16A(std::unique_ptr<char[]> src, size_t size);
	private:
		static std::unique_ptr<wchar_t[]> Convert(
			std::unique_ptr<char[]> src,
			size_t size, 
			size_t& actual_size_inwchar);
	};

	class IFileParser {
	public:
		IFileParser() {}
		IFileParser(std::wstring Path);
		virtual std::wstring Read() = 0;
		virtual std::vector<wchar_t> ReadArray() = 0;
		long long size = 0;
	protected:
		FileReader Reader;
	};

	class CP1251FileParser : public IFileParser {
	public:
		CP1251FileParser(std::wstring Path) : IFileParser(Path) {}
		virtual std::wstring Read();
		virtual std::vector<wchar_t> ReadArray();
	};

	/*class UTF16LEFileParser : public IFileParser {
	public:
		UTF16LEFileParser(std::wstring Path) : IFileParser(Path) {}
		virtual std::wstring Read();
		virtual std::vector<wchar_t> ReadArray();
	};*/

	class UTF8FileParser : public IFileParser {
	public:
		UTF8FileParser(std::wstring Path) : IFileParser(Path) {};
		virtual std::wstring Read();
		virtual std::vector<wchar_t> ReadArray();
	};

	class SVParser {
	public:
		SVParser() {}
		SVParser(std::shared_ptr<IFileParser> parser, wchar_t separator = L';');
		std::vector<std::vector<std::wstring>> Read();
		std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> ReadA(size_t& size);
	private:
		std::shared_ptr<IFileParser> Parser = nullptr;
		wchar_t Separator = 0;
		std::vector<std::wstring> ParseLine(std::vector<wchar_t>& lines, long long offset, long long pos);
		std::unique_ptr<std::unique_ptr<wchar_t[]>[]> ParseLineA(
			std::vector<wchar_t>& lines, 
			size_t offset, 
			size_t diff,
			size_t& read_cell);
		/*/std::unique_ptr<std::unique_ptr<wchar_t[]>[]> AVX2_ParseLine(
			std::vector<wchar_t>& line,
			size_t start,
			size_t stop,
			__m256i& buffer,
			__m256i& cmp_result,
			const __m256i& compare);*/
	};

	class CP1251SVParser {
	public:
		CP1251SVParser(std::wstring path, wchar_t separator = L';');
		std::vector<std::vector<std::wstring>> Read();
		std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> ReadA(size_t& size);
	private:
		SVParser parser;
	};

	class UTF8SVParser {
	public:
		UTF8SVParser(std::wstring path, wchar_t separator = L';');
		std::vector<std::vector<std::wstring>> Read();
		std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> ReadA(size_t& size);
	private:
		SVParser parser;
	};


}