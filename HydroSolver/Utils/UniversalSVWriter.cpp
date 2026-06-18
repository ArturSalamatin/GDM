#include "UniversalSVWriter.h"

UniversalWriter::ISVWriter::ISVWriter(std::wstring path)
{
	Stream = std::ofstream(path, std::ios::binary);
}

void UniversalWriter::ISVWriter::Close()
{
	Stream.flush();
	Stream.close();
}

void UniversalWriter::UTF8Writer::Write(std::wstring content)
{
	if (Stream.is_open())
	{
		size_t reqired_byte_size = WideCharToMultiByte(
			CP_UTF8,
			0,
			content.c_str(),
			-1,
			nullptr,
			0,
			NULL,
			NULL
		);

		auto ptr = std::make_unique<char[]>(reqired_byte_size);
		auto res = WideCharToMultiByte(
			CP_UTF8,
			0,
			content.c_str(),
			-1,
			ptr.get(),
			reqired_byte_size,
			NULL,
			NULL
		);
		if (res)
		{
			Stream.write(ptr.get(), reqired_byte_size-1); //-1 т.к. последний символ - \0, который не нужно записывать в файл
		}
		else {
			throw std::exception("unable to convert utf16 to utf8. Error code: " + res);
		}
		/*for (const wchar_t ch : content)
		{
			//H
			Stream.write(reinterpret_cast<const char*>(&ch), sizeof(char));
			//L
			Stream.write(reinterpret_cast<const char*>(&ch) + 1, sizeof(char));
		}*/
	}
}

/*void UniversalWriter::ASCIWriter::Write(std::wstring content)
{
	if (Stream.is_open())
	{
		for (const wchar_t ch : content)
		{
			Stream.write(reinterpret_cast<const char*>(&ch), sizeof(char));
		}
	}
}*/
