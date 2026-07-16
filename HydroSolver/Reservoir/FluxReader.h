#pragma once
#include <vector>
#include "../Utils/UniversalSVParser.h"

static class FluxReader
{
public:
	static std::vector<std::vector<std::vector<std::vector<double>>>> readOilFluxX(int nx, int ny, int nz, std::wstring path = L"test_FluxOil_X.txt")
	{
		// includes the bounding edges
		return readXdir(nx, ny, nz, path);
	}
	static std::vector<std::vector<std::vector<std::vector<double>>>> readOilFluxY(int nx, int ny, int nz, std::wstring path = L"test_FluxOil_Y.txt")
	{
		// includes the bounding edges
		return readYdir(nx, ny, nz, path);
	}
	static std::vector<std::vector<std::vector<std::vector<double>>>> readOilFluxZ(int nx, int ny, int nz, std::wstring path = L"test_FluxOil_Z.txt")
	{
		// includes the bounding edges
		return readZdir(nx, ny, nz, path);
	}

	static std::vector<std::vector<std::vector<std::vector<double>>>> readOverallFluxX(int nx, int ny, int nz, std::wstring path = L"test_FluxOverall_X.txt")
	{
		// includes the bounding edges
		return readXdir(nx, ny, nz, path);
	}
	static std::vector<std::vector<std::vector<std::vector<double>>>> readOverallFluxY(int nx, int ny, int nz, std::wstring path = L"test_FluxOverall_Y.txt")
	{
		// includes the bounding edges
		return readYdir(nx, ny, nz, path);
	}
	static std::vector<std::vector<std::vector<std::vector<double>>>> readOverallFluxZ(int nx, int ny, int nz, std::wstring path = L"test_FluxOverall_Z.txt")
	{
		// includes the bounding edges
		return readZdir(nx, ny, nz, path);
	}

private:
	static std::vector<std::vector<std::vector<std::vector<double>>>> readXdir(int nx, int ny, int nz, std::wstring path)
	{
		// includes the bounding edges
		return readCube(nx + 1, ny, nz, path);
	}
	static std::vector<std::vector<std::vector<std::vector<double>>>> readYdir(int nx, int ny, int nz, std::wstring path)
	{
		// includes the bounding edges
		return readCube(nx, ny + 1, nz, path);
	}
	static std::vector<std::vector<std::vector<std::vector<double>>>> readZdir(int nx, int ny, int nz, std::wstring path)
	{
		// includes the bounding edges
		return readCube(nx, ny, nz + 1, path);
	}




	static std::vector<std::vector<std::vector<std::vector<double>>>> readCube(int nx, int ny, int nz, std::wstring path)
	{
		std::vector<std::vector<std::vector<std::vector<double>>>> result; // the cube of some fluxes in some direction

		auto parser = UniversalSCParser::CP1251SVParser(path);
		auto data = parser.Read();

		for (size_t t = 0; t < data.size(); t++)
		{
			int l = 0;
			result.push_back(std::vector<std::vector<std::vector<double>>>{});
			for (int k = 0; k < nz; k++)
			{
				result.back().push_back(std::vector<std::vector<double>>());
				for (int j = 0; j < ny; j++)
				{
					result.back().back().push_back(std::vector<double>());
					for (int i = 0; i < nx; i++)
					{
						result.back().back().back().push_back(stod(data[t][l]));
						l++;
					}
				}
			}

		}
		return result;
	}
};

