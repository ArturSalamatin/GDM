#include "IRCGEngine.h"

//void IRCGEngine::IrapClassicGrid::Add(geos::geom::Point* p, float value)
//{
//	int xid = (p->getX() - std::get<0>(Bounds)) / _dx;
//	int yid = (p->getY() - std::get<2>(Bounds)) / _dy;
//
//	int linear_id = yid * _nx + xid;
//	Buffer.insert({ linear_id , value });
//}

void IRCGEngine::IrapClassicGrid::Add(std::pair<float, float> p, float value)
{
	int xid = (p.first - std::get<0>(Bounds)) / _dx;
	int yid = (p.second - std::get<2>(Bounds)) / _dy;

	int linear_id = yid * _nx + xid;
	Buffer.insert({ linear_id , value });
}

void IRCGEngine::IrapClassicGrid::Write()
{
	UniversalWriter::UTF8Writer writer(Path);
	//заголовок
	auto h1 = L"-996 " + std::to_wstring(_ny) + L" " + FloatToWS(_dx) + L" " + FloatToWS(_dy) + L"\n";
	writer.Write(h1);
	auto h2 =
		FloatToWS(std::get<0>(Bounds)) + L" " +
		FloatToWS(std::get<1>(Bounds)) + L" " +
		FloatToWS(std::get<2>(Bounds)) + L" " +
		FloatToWS(std::get<3>(Bounds)) + L"\n";
	writer.Write(h2);
	auto h3 = std::to_wstring(_nx) + L" 0.0 " + FloatToWS(std::get<0>(Bounds)) + L" " + FloatToWS(std::get<2>(Bounds)) + L"\n";
	writer.Write(h3);
	writer.Write(L"0 0 0 0 0 0 0\n");
	//значения
	//по 6 штук в строке
	int allrow = _nx * _ny;
	int itr = allrow / 6;
	for (int i = 0; i < itr; i++)
	{
		std::wstring line = L"";
		for (int j = 0; j < 6; j++)
		{
			int id = i * 6 + j;
			if (Buffer.find(id) != Buffer.end())
			{
				line += FloatToWS(Buffer.at(id));
			}
			else {
				line += L"9999900.000000";
			}
			line += (j == 5) ? L"\n" : L" ";
		}
		writer.Write(line);
	}
	writer.Close();
}

std::wstring IRCGEngine::IGrid::FloatToWS(float _val)
{
	auto val = std::to_wstring(_val);
	auto pos = val.find(L',');
	if (pos != std::string::npos)
	{
		auto replaced = val.replace(pos, 1, L".");
		return replaced;
	}
	else {
		return val;
	}
}

//void IRCGEngine::XYZGrid::Add(geos::geom::Point* p, float value)
//{
//	std::wstring x = FloatToWS(p->getX());
//	std::wstring y = FloatToWS(p->getY());
//	std::wstring val = FloatToWS(value);
//	Buffer += x + L"\t" + y + L"\t" + val + L"\n";
//}

void IRCGEngine::XYZGrid::Add(std::pair<float, float> p, float value)
{
	std::wstring x = FloatToWS(p.first);
	std::wstring y = FloatToWS(p.second);
	std::wstring val = FloatToWS(value);
	Buffer += x + L"\t" + y + L"\t" + val + L"\n";
}

void IRCGEngine::XYZGrid::Write()
{
	UniversalWriter::UTF8Writer writer(Path);
	writer.Write(Buffer);
	writer.Close();
}
