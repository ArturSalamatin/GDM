#pragma once

#include <memory>
#include <string>
#include <iostream>

template<size_t cache_size> // set cache size
class CacheFile
{
private:
	std::unique_ptr<char[]> cache; // 
	size_t current_pos, current_end;
	std::unique_ptr<std::istream> file_stream;

public:
	constexpr CacheFile() : current_pos{ 0 }, current_end{ 0 }
	{
		cache = std::make_unique<char[]>(cache_size);
	}

	void load(std::wstring file_name)
	{
		if (file_stream == nullptr)
		{
			file_stream = std::make_unique<std::istream>(file_name, std::ios::binary);
			if (file_stream->good())
			{
				file_stream->read(cache.get(), cache_size);
				current_end = cache_size;
			}
			else
			{
				throw std::exception("Unable to read file.");
			}
		}
		else
		{
			throw std::exception("Stream has already opened.");
		}
	}

	// read file byte by byte. But we need a value rather than a byte. Value takes more then 1 byte in memory
	template<typename T> 
	T& operator()(size_t pos, size_t size)
	{
		if (pos + size-1 < current_end)
		{
			return std::ref(*reinterpret_cast<T*>(cache.get() + pos * sizeof(T)));
		}
		else
		{// pos is outside current_end
			if (pos > current_end)
			{

			}
			// pos < current_end, but pos+size > current_end

		}
	}

};

