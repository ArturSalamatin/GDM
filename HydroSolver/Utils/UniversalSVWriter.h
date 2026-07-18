#pragma once
#define NOMINMAX

#include <fstream>
#include <string>
#include <Windows.h>
#include <stringapiset.h>
namespace UniversalWriter {
	class ISVWriter {
	public:
		ISVWriter(std::wstring path);
		virtual void Write(std::wstring content) = 0;
		virtual void Close();
	protected:
		std::ofstream Stream;
	};

	class UTF8Writer : public ISVWriter {
	public:
		UTF8Writer(std::wstring path) : ISVWriter(path)
		{
			//пишем заголовок UTF16LE
			/*if (BOM)
			{
				std::wstring header = L"\xfeff";
				Write(header);
			}*/
		}
		void Write(std::wstring content) override;
	};

	/*class ASCIWriter : public ISVWriter
	{
	public:
		ASCIWriter(std::wstring path) : ISVWriter(path){}
		virtual void Write(std::wstring content);
	};*/

}