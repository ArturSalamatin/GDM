#include "UniversalSVParser.h"



int UniversalSCParser::EncodingFinder::FindEncoding(std::vector<char>* file)
{
	//определяем по BOM
	if (file->at(0) == '\xff' && file->at(1) == '\xfe')
		return 0;
	if (file->at(1) == '\xff' && file->at(2) == '\xfe')
		return 2;
	if (file->at(0) == '\xEF' && file->at(1) == '\xBB' && file->at(2) == '\xBF')
		return 1;
	return 1;
}

UniversalSCParser::FileReader::FileReader()
{
}

UniversalSCParser::FileReader::FileReader(std::wstring path)
{
	Path = path;
}

std::unique_ptr<char[]> UniversalSCParser::FileReader::Read(size_t& _size, bool include_null)
{
	if (std::filesystem::exists(Path))
	{
		long long size = std::filesystem::file_size(Path);
		if (size)
		{
			if (include_null)
				size++;

			std::ifstream file(Path, std::ios::binary);
			if (file.is_open())
			{
				_size = size;
				auto buffer_ptr = std::make_unique <char[]>(size);
				file.read(&buffer_ptr[0], size - ((include_null) ? 1 : 0));
				file.close();
				return std::move(buffer_ptr);
			}
			else {
				CP1251Encoder enc;
				auto msg = "unable to open file at " + enc.ToString(Path);
				throw std::exception(msg.c_str());
			}
		}
		else {
			CP1251Encoder enc;
			auto msg = "file at " + enc.ToString(Path) + " has 0 size";
			throw std::exception(msg.c_str());
		}

	}
	else {
		CP1251Encoder enc;
		auto msg = "file at " + enc.ToString(Path) + " doesnt exist";
		throw std::exception(msg.c_str());
	}
}


std::wstring UniversalSCParser::CP1251FileParser::Read()
{
	size_t size = 0;
	auto ptr = Reader.Read(size);

	CP1251Encoder encoder;
	auto out = encoder.ToWString(ptr.get(), size);
	ptr.reset();
	return out;

}

std::vector<wchar_t> UniversalSCParser::CP1251FileParser::ReadArray()
{
	size_t size = 0;
	auto ptr = Reader.Read(size);

	CP1251Encoder encoder;
	auto arr = encoder.ToWArray(ptr.get(), size);
	ptr.reset();
	return arr;

}



UniversalSCParser::CP1251Encoder::CP1251Encoder()
{
	EncodingMap = {
			{0xC0, L'А'},{0xe0, L'а'},
			{0xC1, L'Б'},{0xe1, L'б'},
			{0xC2, L'В'},{0xe2, L'в'},
			{0xC3, L'Г'},{0xe3, L'г'},
			{0xc4, L'Д'},{0xe4, L'д'},
			{0xc5, L'Е'},{0xe5, L'е'},
			{0xc6, L'Ж'},{0xe6, L'ж'},
			{0xc7, L'З'},{0xe7, L'з'},
			{0xc8, L'И'},{0xe8, L'и'},
			{0xc9, L'Й'},{0xe9, L'й'},
			{0xca, L'К'},{0xea, L'к'},
			{0xcb, L'Л'},{0xeb, L'л'},
			{0xcc, L'М'},{0xec, L'м'},
			{0xcd, L'Н'},{0xed, L'н'},
			{0xce, L'О'},{0xee, L'о'},
			{0xcf, L'П'},{0xef, L'п'},
			{0xd0, L'Р'},{0xf0, L'р'},
			{0xd1, L'С'},{0xf1, L'с'},
			{0xd2, L'Т'},{0xf2, L'т'},
			{0xd3, L'У'},{0xf3, L'у'},
			{0xd4, L'Ф'},{0xf4, L'ф'},
			{0xd5, L'Х'},{0xf5, L'х'},
			{0xd6, L'Ц'},{0xf6, L'ц'},
			{0xd7, L'Ч'},{0xf7, L'ч'},
			{0xd8, L'Ш'},{0xf8, L'ш'},
			{0xd9, L'Щ'},{0xf9, L'щ'},
			{0xda, L'Ъ'},{0xfa, L'ъ'},
			{0xdb, L'Ы'},{0xfb, L'ы'},
			{0xdc, L'Ь'},{0xfc, L'ь'},
			{0xdd, L'Э'},{0xfd, L'э'},
			{0xde, L'Ю'},{0xfe, L'ю'},
			{0xdf, L'Я'},{0xff, L'я'},
	};
	DecodingMap = {
		   {L'А', 0xC0},{L'а', 0xe0, },
		   {L'Б', 0xC1},{L'б',0xe1},
		   {L'В', 0xC2, },{L'в',0xe2},
		   {L'Г',0xC3},{L'г', 0xe3},
		   {L'Д', 0xc4},{L'д', 0xe4},
		   {L'Е', 0xc5},{L'е', 0xe5},
		   {L'Ж', 0xc6},{L'ж',0xe6},
		   {L'З',0xc7},{L'з',0xe7 },
		   {L'И', 0xc8 },{L'и', 0xe8},
		   {L'Й',0xc9},{ L'й', 0xe9},
		   { L'К',0xca},{ L'к',0xea},
		   { L'Л',0xcb},{ L'л',0xeb},
		   { L'М',0xcc},{ L'м',0xec},
		   { L'Н',0xcd},{ L'н',0xed},
		   { L'О',0xce},{ L'о',0xee},
		   { L'П',0xcf},{ L'п',0xef},
		   { L'Р',0xd0},{ L'р',0xf0},
		   { L'С',0xd1},{ L'с',0xf1},
		   { L'Т',0xd2},{ L'т',0xf2},
		   { L'У',0xd3},{ L'у',0xf3},
		   { L'Ф',0xd4},{ L'ф',0xf4},
		   { L'Х',0xd5},{ L'х',0xf5},
		   { L'Ц',0xd6},{ L'ц',0xf6},
		   { L'Ч',0xd7},{ L'ч',0xf7},
		   { L'Ш',0xd8},{ L'ш',0xf8},
		   { L'Щ',0xd9},{ L'щ',0xf9},
		   { L'Ъ',0xda},{ L'ъ',0xfa},
		   { L'Ы',0xdb},{ L'ы',0xfb},
		   { L'Ь',0xdc},{ L'ь',0xfc},
		   { L'Э',0xdd},{ L'э',0xfd},
		   { L'Ю',0xde},{ L'ю',0xfe},
		   { L'Я',0xdf},{ L'я',0xff},
	};
}

UniversalSCParser::IFileParser::IFileParser(std::wstring Path)
{
	try {
		Reader = FileReader(Path);
		size = std::filesystem::file_size(Path);
	}
	catch (std::exception e)
	{
		CP1251Encoder enc;
		auto msg = "File not Found " + std::string(e.what()) + " at path: " + enc.ToString(Path);
		throw std::exception(msg.c_str());
	}
}

UniversalSCParser::SVParser::SVParser(std::shared_ptr<IFileParser> parser, wchar_t separator)
{
	Parser = std::shared_ptr<IFileParser>(parser);
	Separator = separator;
}

std::vector<std::vector<std::wstring>> UniversalSCParser::SVParser::Read()
{
	std::vector<wchar_t> lines;

	lines = Parser->ReadArray();
	//добавляем концевое \n для правильного парсинга
	//if (lines.back() != L'\n')
	//	lines.push_back(L'\n');

	//int cpuid[4]; //EAX EBX ECX EDX
	//__cpuidex(&cpuid[0], 7, 0); //eax=7 ecx=0 - Extended Features
	//int isAVX2Suport = (cpuid[1] >> 5) & 1; //5 бит в EBX отвечает за AVX2
	////std::cout << ((isAVX2Suport) ? "Support" : "Unsupport") << "\n";
	//

	//__m256i buffer = _mm256_set1_epi16(0);
	//const __m256i cmp_buffer = _mm256_set1_epi16(L'\n');
	//__m256i cmp_result = _mm256_set1_epi16(0);
	//wchar_t* ptr = &lines[0];
	//wchar_t const* fptr = &lines[0];

	//long long _last_pos = 0;
	//std::vector<std::vector<std::wstring>> _out;

	//for (long long pos = 0; pos < lines.size(); pos+=16)
	//{
	//	buffer = _mm256_loadu_si256((__m256i*)ptr); //загружаем кусок памяти
	//	cmp_result = _mm256_cmpeq_epi16(buffer, cmp_buffer); //сравниваем с массивом \n => ищем конец линии
	//	unsigned int isnl = _mm256_movemask_epi8(cmp_result); //порядок байт инвертированный
	//	if (isnl != 0) //isnl != 0 если нашлось совпадение строк
	//	{
	//		//ищем позицию этоих \n
	//		//поскольку маска из epi8, то мы будем иметь 11 в месте где нашлось \n
	//		for (int i = 0; i < 16; i++)
	//		{
	//			/*
	//			* делаем сдвиг влево и сравниваем с 11
	//			* результат битового сравнения должен быть равен 11 -> 3
	//			*/
	//			if ( ((isnl >> i*2) & 3) == 3) 
	//			{
	//				long long cpos = i + (ptr - fptr);
	//				auto test = ParseLine(lines, _last_pos, cpos);
	//				_out.push_back(std::move(test));
	//				_last_pos = cpos + 1;
	//			}
	//		}
	//	}
	//	ptr += 16;
	//}

	std::vector<std::vector<std::wstring>> out;
	long long offset = 0;

	for (long long pos = 0; pos < lines.size(); pos++)
	{
		if (lines[pos] == L'\n' || pos == lines.size() - 1)
		{
			size_t _pos = (lines[pos] == L'\n') ? pos : pos + 1;
			auto test = ParseLine(lines, offset, _pos);
			out.push_back(std::move(test)); //\n
			offset = pos + 1;
		}
	}
	return out;
}

std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> UniversalSCParser::SVParser::ReadA(size_t& size)
{
	std::vector<wchar_t> lines;

	lines = Parser->ReadArray();

	/*auto ts1 = std::chrono::high_resolution_clock::now();

	__m256i buffer = _mm256_set1_epi16(0);
	const __m256i cmp_buffer_divider = _mm256_set1_epi16(Separator);
	const __m256i cmp_buffer_nl = _mm256_set1_epi16(L'\n');
	__m256i cmp_result = _mm256_set1_epi16(0);
	__m256i cmp_result_nl = _mm256_set1_epi16(0);

	wchar_t* ptr = &lines[0];
	std::vector<std::vector<std::wstring>> _out(1, std::vector<std::wstring>(1));
	auto iterator = _out[0].begin();
	auto container = _out.begin();

	for (int i = 0; i < lines.size(); i += 16)
	{
		buffer = _mm256_loadu_si256((__m256i*)ptr); //загружаем
		cmp_result = _mm256_cmpeq_epi16(buffer, cmp_buffer_divider); //проверяем наличие разделителя
		cmp_result_nl = _mm256_cmpeq_epi16(buffer, cmp_buffer_nl);
		unsigned int mask = _mm256_movemask_epi8(cmp_result);
		unsigned int mask_nl = _mm256_movemask_epi8(cmp_result_nl);
		/*if (mask != 0) //если он есть то рекурсивно создаем новые ячейки
		{
		int last_pos = 0;
		for (int j = 0; j < 16; j++)
		{
			if ((mask >> j*2) & 3) { //находим разделитель
				(*iterator).append(ptr + last_pos, j - last_pos); //копируем нужную часть в текущую ячейку
				//проверяем на концевое \n

				last_pos = j + 1; //+1 т.к. i указывает на разделитель
				//двигаем итератор
				//++iterator;
				//создаем под него контейнер
				iterator = (*container).emplace(iterator + 1, L"");
			}
			if ((mask_nl >> j * 2) & 3)
			{
				//int shift = (*(ptr + j - 1) == L'\r') ? 1 : 0;
				(*iterator).append(ptr + last_pos, j - last_pos);
				last_pos = j + 1;

				container = _out.emplace(container + 1, 1);
				iterator = (*container).begin();
			}
		}
		(*iterator).append(ptr + last_pos, 16 - last_pos); //копируем остаток

		ptr += 16;

		/* }
		else { //если его нет то прибавляем к текущей ячейки
			(*iterator).append(ptr, 16);
		}
	}

	/*if (lines.back() != L'\n')
		lines.push_back(L'\n');

	int cpuid[4]; //EAX EBX ECX EDX
	__cpuidex(&cpuid[0], 7, 0); //eax=7 ecx=0 - Extended Features
	int isAVX2Suport = (cpuid[1] >> 5) & 1; //5 бит в EBX отвечает за AVX2
	//std::cout << ((isAVX2Suport) ? "Support" : "Unsupport") << "\n";



	auto ts1 = std::chrono::high_resolution_clock::now();

	__m256i buffer = _mm256_set1_epi16(0);
	const __m256i cmp_buffer = _mm256_set1_epi16(L'\n');
	__m256i cmp_result = _mm256_set1_epi16(0);
	wchar_t* ptr = &lines[0];
	wchar_t const* fptr = &lines[0];

	__m256i line_buffer = _mm256_set1_epi16(0);
	__m256i line_cmp_buffer = _mm256_set1_epi16(0);
	const __m256i line_compare = _mm256_set1_epi16(Separator);

	size_t _last_pos = 0;
	std::vector<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>> _out;

	for (size_t pos = 0; pos < lines.size(); pos += 16)
	{
		buffer = _mm256_loadu_si256((__m256i*)ptr); //загружаем кусок памяти
		cmp_result = _mm256_cmpeq_epi16(buffer, cmp_buffer); //сравниваем с массивом \n => ищем конец линии
		unsigned int isnl = _mm256_movemask_epi8(cmp_result); //порядок байт инвертированный
		if (isnl != 0) //isnl != 0 если нашлось совпадение строк
		{
			//ищем позицию этоих \n
			//поскольку маска из epi8, то мы будем иметь 11 в месте где нашлось \n
			for (int i = 0; i < 16; i++)
			{
				/*
				* делаем сдвиг влево и сравниваем с 11
				* результат битового сравнения должен быть равен 11 -> 3

				if (((isnl >> i * 2) & 3) == 3)
				{
					long long cpos = i + (ptr - fptr);
					//auto test = ParseLine(lines, _last_pos, cpos);

					auto test = AVX2_ParseLine(lines,
						_last_pos,
						cpos,
						line_buffer,
						line_cmp_buffer,
						line_compare);

					_out.push_back(std::move(test));
					_last_pos = cpos + 1;
				}
			}
		}
		ptr += 16;
	}

	auto ts2 = std::chrono::high_resolution_clock::now();*/

	std::vector<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>> out1;
	auto pos = lines.begin();
	size_t offset1 = 0;
	do {
		pos = std::find(lines.begin() + offset1, lines.end(), L'\n');
		if (pos != lines.end())
		{
			size_t diff = pos - (lines.begin() + offset1);
			if (diff > 0)
			{
				size_t readed = 0;
				auto data = ParseLineA(lines, offset1, diff, readed);
				if (readed > 0)
					out1.push_back(std::move(data));
			}
			offset1 = pos - lines.begin() + 1; //+1 - перемешение за каретку
		}
		else {
			size_t diff = lines.size() - offset1;
			if (diff > 0)
			{
				size_t readed = 0;
				auto data = ParseLineA(lines, offset1, diff, readed);
				if (readed > 0)
					out1.push_back(std::move(data));
			}
		}
	} while (pos != lines.end());

	/*auto ts3 = std::chrono::high_resolution_clock::now();

	std::cout << "Classic took " << std::chrono::duration<double>(ts3 - ts2).count()
		<< "s Modern Took " << std::chrono::duration<double>(ts2 - ts1).count() << "s\r\n" << std::flush;*/

	lines.~vector();

	std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> out = std::make_unique<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]>(out1.size());
	size = out1.size();
	for (int i = 0; i < out1.size(); i++)
	{
		out[i] = std::move(out1[i]);
	}
	out1.~vector();

	return out;
}

std::vector<std::wstring> UniversalSCParser::SVParser::ParseLine(std::vector<wchar_t>& lines, long long offset, long long pos)
{
	std::vector<std::wstring> out;
	long long _offset = offset;
	for (long long _pos = offset; _pos < pos; _pos++)
	{
		if (lines[_pos] == Separator || _pos == pos - 1)
		{
			size_t diff = (lines[_pos] == Separator) ? _pos - _offset : pos - _offset;
			if (lines[_pos] == L'\r')
				diff--;
			//auto susbt = lines.substr(_offset, diff);
			out.push_back(std::wstring(&lines[_offset], diff));
			_offset = _pos + 1;
		}
	}
	return out;
}

std::unique_ptr<std::unique_ptr<wchar_t[]>[]> UniversalSCParser::SVParser::ParseLineA(
	std::vector<wchar_t>& lines,
	size_t offset,
	size_t diff,
	size_t& read_cell)
{
	std::vector<std::unique_ptr<wchar_t[]>> out;

	auto pos = lines.begin() + offset;

	size_t ioffset = offset;

	size_t last_pos = offset;

	do {
		pos = std::find(lines.begin() + ioffset, lines.begin() + diff + offset, Separator);

		//вычисляем абсолютную позицию разделителя в строке
		size_t cpos = pos - lines.begin();
		//если предыдущая позиция - нулевая, то размер - текущая позиция +1 (нуль терминатор), иначе их разница (нуль терминатор включен)
		size_t size = cpos - last_pos + ((!out.size()) ? 1 : 0);
		if (lines[cpos - 1] == L'\r')
			size--;
		//копируем байты

		auto buffer = std::make_unique<wchar_t[]>(size);
		std::memcpy(&buffer[0], &lines[last_pos + ((last_pos == offset) ? 0 : 1)], (size - 1) * sizeof(wchar_t)); //+1 т.к. last_pos указывает на предыдущий разделитель
		//перемешаем указатель
		out.push_back(std::move(buffer));
		ioffset = cpos + 1;
		last_pos = cpos;


	} while (pos != lines.begin() + diff + offset);

	read_cell = 0;

	std::unique_ptr<std::unique_ptr<wchar_t[]>[]> _out = std::make_unique<std::unique_ptr<wchar_t[]>[]>(out.size());
	for (int i = 0; i < out.size(); i++)
	{

		_out[i] = std::move(out[i]);
		read_cell++;


	}
	out.~vector();
	//for (int i = 0; i < position.size(); i++)
	//{
	//	size_t bsize = (i) ? position[i] - position[i - 1] : position[i] + 1; //+1 - нуль терминатор
	//	size_t _offset = (i) ? position[i - 1] + 1 : 0; //+1 чтобы перескачить разделитель

	//	auto buffer = std::make_unique<wchar_t[]>(bsize);
	//	std::memcpy(&buffer[0], &lines[_offset + offset], (bsize - 1)*sizeof(wchar_t));

	//	out.push_back(std::move(buffer));
	//}
	//lines.erase(lines.begin(), lines.begin() + diff + 1); //+1 - удалить \n
	if (read_cell == 1)
	{
		if (!wcscmp(_out[0].get(), L""))
		{
			read_cell = 0;
		}
	}
	return _out;
}

/*std::unique_ptr<std::unique_ptr<wchar_t[]>[]> UniversalSCParser::SVParser::AVX2_ParseLine(
	std::vector<wchar_t>& line,
	size_t start,
	size_t stop,
	__m256i& buffer,
	__m256i& cmp_result,
	const __m256i& compare)
{
	wchar_t* ptr = &line[start];
	const wchar_t* fptr = &line[start];
	size_t last_pos = 0;

	std::vector<std::unique_ptr<wchar_t[]>> out;

	for (size_t i = start; i < stop; i += 16)
	{
		buffer = _mm256_loadu_si256((__m256i*)ptr); //грузим буффер
		cmp_result = _mm256_cmpeq_epi16(buffer, compare); //сравниваем со строкой
		unsigned int issep = _mm256_movemask_epi8(cmp_result); //получаем маску
		if (issep != 0) //если != 0 значит сепаратор найден
		{
			int ccize = (stop - i < 16) ? stop - i : 16;
			for (int i = 0; i < ccize; i++) //ищем все сепараторы
			{
				if (((issep >> i * 2) & 3) == 3) //битовый сдвиг дает ...11 => & даст 11 => 3
				{
					size_t cpos = i + (ptr - fptr);

					auto data = std::make_unique<wchar_t[]>(cpos - last_pos);
					std::memcpy(data.get(), fptr + last_pos, sizeof(wchar_t) * (cpos - last_pos));

					last_pos = cpos + 1;
					out.push_back(std::move(data));
				}
			}
		}
		ptr += 16;
	}
	auto out1 = std::make_unique<std::unique_ptr<wchar_t[]>[]>(out.size());
	for (int i = 0; i < out.size(); i++)
		out1[i] = std::move(out[i]);

	return out1;
}*/

UniversalSCParser::CP1251SVParser::CP1251SVParser(std::wstring path, wchar_t separator)
{
	auto ptr = std::make_shared<CP1251FileParser>(path);
	parser = SVParser(ptr, separator);
}

std::vector<std::vector<std::wstring>> UniversalSCParser::CP1251SVParser::Read()
{

	return parser.Read();

}

std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> UniversalSCParser::CP1251SVParser::ReadA(size_t& size)
{

	return parser.ReadA(size);

}

/*std::wstring UniversalSCParser::UTF16LEFileParser::Read()
{
	size_t size = 0;
	auto ptr = Reader.Read(size);

	std::wstring out;
	//i = 2 т.к. первым идем BOM
	int start = 0;
	if (ptr[0] == '\xFF' && ptr[1] == '\xFE')
		start = 2;

	for (int i = start; i < size; i += sizeof(wchar_t))
	{
		wchar_t ch = 0;
		std::memcpy(&ch, ptr.get() + i, sizeof(wchar_t));
		out += ch;
	}
	//убираем экранирование внутри кавычек
	size_t slash = 0;
	size_t offset = 0;
	do {
		slash = out.find(L"\\\\", offset);
		if (slash != std::wstring::npos)
		{
			out.erase(out.begin() + slash);
			offset = slash + 1; //+1 т.к. slash указывает на '\'
		}
	} while (slash != std::wstring::npos);

	return out;

}

std::vector<wchar_t> UniversalSCParser::UTF16LEFileParser::ReadArray()
{
	size_t size = 0;
	auto ptr = Reader.Read(size);

	//i = 2 т.к. первым идем BOM
	int start = 0;
	if (ptr[0] == '\xFF' && ptr[1] == '\xFE')
		start = 2;

	std::vector<wchar_t> out;

	for (int i = start; i < size; i += sizeof(wchar_t))
	{
		wchar_t ch = 0;
		std::memcpy(&ch, ptr.get() + i, sizeof(wchar_t));
		out.push_back(ch);
	}
	return out;

}*/

UniversalSCParser::UTF8SVParser::UTF8SVParser(std::wstring path, wchar_t separator)
{
	auto ptr = std::make_shared<UTF8FileParser>(path);
	parser = SVParser(ptr, separator);
}

std::vector<std::vector<std::wstring>> UniversalSCParser::UTF8SVParser::Read()
{

	return parser.Read();


}

std::unique_ptr<std::unique_ptr<std::unique_ptr<wchar_t[]>[]>[]> UniversalSCParser::UTF8SVParser::ReadA(size_t& size)
{

	return parser.ReadA(size);

}

std::wstring UniversalSCParser::UTF8Encoder::FromUT8toUTF16(std::unique_ptr<char[]> src, size_t size)
{
	size_t actual_size = 0;
	return std::wstring(Convert(std::move(src), size, actual_size).get());
}

std::vector<wchar_t> UniversalSCParser::UTF8Encoder::FromUT8toUTF16A(std::unique_ptr<char[]> src, size_t size)
{
	size_t actual_size = 0; //размер в wchar
	auto ptr = Convert(std::move(src), size, actual_size);
	std::vector<wchar_t> buffer(actual_size);
	std::memcpy(&buffer[0], ptr.get(), actual_size * sizeof(wchar_t));
	return buffer;
}

std::unique_ptr<wchar_t[]> UniversalSCParser::UTF8Encoder::Convert(
	std::unique_ptr<char[]> src,
	size_t size,
	size_t& actual_size_inwchar) //
{
	//получаем необходимый размер буфера в чарах
	actual_size_inwchar = MultiByteToWideChar(
		CP_UTF8, //кодировка utf8
		0, //совмешаем сложные символы
		src.get(), //указатель на исодник
		-1, //исходник нуль-терминаторный
		nullptr, //указатель на буфер
		0 //0 чтобы получить размер буфера
	);
	//создаем буффер правильного размера
	auto buffer = std::make_unique<wchar_t[]>(actual_size_inwchar);
	auto res = MultiByteToWideChar(
		CP_UTF8, //кодировка utf8
		0, //совмешаем сложные символы
		src.get(), //указатель на исодник
		-1, //исходник нуль-терминаторный
		buffer.get(), //указатель на буфер
		actual_size_inwchar //памяти с запасом
	);
	if (res)
	{
		return buffer;
	}
	else {
		throw std::exception("unable to convert utf8 to utf16. Error code: " + res);
	}
}

std::wstring UniversalSCParser::UTF8FileParser::Read()
{
	size_t size = 0;
	auto src = Reader.Read(size, true);
	return UniversalSCParser::UTF8Encoder::FromUT8toUTF16(std::move(src), size);
}

std::vector<wchar_t> UniversalSCParser::UTF8FileParser::ReadArray()
{
	size_t size = 0;
	auto src = Reader.Read(size, true);
	return UniversalSCParser::UTF8Encoder::FromUT8toUTF16A(std::move(src), size);
}
