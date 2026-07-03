#pragma once
#include <fstream>
#include <utility>
#include <string>
#include <filesystem>

#include "../Descriptors/Descriptors.h"
#include "../Reservoir/ReservoirInstantiator.h"

namespace reservoir_simulator
{
	template<typename Factory>
	class CubePrinter
	{
	public:
		CubePrinter(ReservoirInstantiator<Factory>* reservoirIntantiator, const std::wstring& path) noexcept :
			path{path},
			reservoirIntantiator{ reservoirIntantiator } 
		{
			namespace fs = std::filesystem;
			fs::create_directories(path);
		};

		void PrintActNum(std::wstring fName = L"MatLab\\testActiveCells.txt") const
		{
			DataPrinter<bool>(reservoirIntantiator->ActiveCells(), fName);
		}
		void PrintVolume(std::wstring fName = L"MatLab\\testVolumes.txt") const
		{
			DataPrinter<float>(reservoirIntantiator->Volume(), fName);
		}
		void PrintPoro(std::wstring fName = L"MatLab\\testPorosity.txt") const
		{
			DataPrinter<float>(reservoirIntantiator->Porosity(), fName);
		}
		void PrintPermX(const std::wstring& fName = L"MatLab\\testPermX.txt") const
		{
			DataPrinter<float>(reservoirIntantiator->PermeabilityX(), fName);
		}
		void PrintStartOil(std::wstring fName = L"MatLab\\testStartOil.txt") const
		{
		//	DataPrinter<float>(reservoirIntantiator->startSoil(), fName);
		}
		void PrintStartPressure(std::wstring fName = L"MatLab\\testStartPressure.txt") const
		{
		//	DataPrinter<float>(reservoirIntantiator->startPressure(), fName);
		}
		void PrintPlanarMesh(std::wstring fName = L"MatLab\\testPlanarMesh.txt") const
		{
			std::ofstream myfile;
			myfile.open(path+fName, std::ios_base::out);

			char buffer[1000];
			snprintf(buffer, 1000, "%zu;%zu;%zu;%zu\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				reservoirIntantiator->nx(), reservoirIntantiator->ny(),
				reservoirIntantiator->nz(), static_cast<size_t>(3),//reservoirIntantiator->nt(),
				reservoirIntantiator->xmin(), reservoirIntantiator->ymin(),
				reservoirIntantiator->xmax(), reservoirIntantiator->ymax(),
				reservoirIntantiator->xstep(), reservoirIntantiator->ystep(), 0.0, 0.0);
			std::string result{ buffer };
			myfile << result << std::endl;
			myfile.close();
		}

		void PrintDefault() const
		{
			PrintPlanarMesh();
			PrintActNum();
			PrintVolume();
			PrintPoro();
			PrintPermX();
			PrintStartOil();
			PrintStartPressure();
		}

	protected:
		ReservoirInstantiator<Factory>* reservoirIntantiator;
		std::wstring path;

		template<typename T>
		void DataPrinter(const SomeCube<T>& cube, const std::wstring& fName) const
		{
			MakeDirectories(path + fName);

			std::ofstream myfile;
			myfile.open(path + fName, std::ios_base::out);

			for (size_t l = 0; l < cube.size(); l++)
				myfile << cube[l] << ",";

			myfile.close();
		}

		void MakeDirectories(const std::wstring& fName) const
		{
			namespace fs = std::filesystem;

			fs::path path{ fName };
			path.remove_filename();

			fs::create_directories(path);
		}

	};

	//template<typename Factory>
	//class WellDataPrinter
	//{
	//public:

	//	WellDataPrinter(ReservoirInstantiator<Factory>* reservoirIntantiator) noexcept :
	//		reservoirIntantiator{ reservoirIntantiator } {};

	//	void PrintWellDebitLength(std::wstring fName = L"MatLab\\testWellDebits.txt") const
	//	{
	//		std::ofstream myfile;
	//		myfile.open(fName, std::ios_base::out);

	//		UniversalWriter::UTF8Writer writer(L"MatLab\\testWellNames.txt");
	//		UniversalWriter::UTF8Writer coordFile(L"MatLab\\testWellCoords.txt");

	//		int i = 0;

	//		const auto& wellContainer = reservoirIntantiator->Simulator().GetWells();
	//		auto [start, end] = reservoirIntantiator->Config().PeriodOfInterest();

	//		for (const auto& pair : wellContainer)
	//		{
	//			auto& name = pair.first;
	//			auto& well = pair.second;
	//			myfile << i;

	//			auto interval = well->KnownExploitationPeriod();

	//			bool fff = false;

	//			bool f = false;
	//			for (double time = start; time < end; time += 1)
	//			{
	//				auto debit = well->DebitOverall(time);
	//				if (debit != 0.0)
	//					fff = true;
	//				auto length = well->PerforationLengthOverall(time);
	//				double l = 0;
	//				for (size_t i = 0; i < length.size(); i++)
	//					l += length[i];
	//				bool isGood = !((debit != 0.0) && (l == 0.0)); // we have perforations when debit is non-zero
	//				myfile << "," << time << "," << debit << ",";
	//				for (int i = 0; i < length.size(); i++)
	//					myfile << round(length[i]) << ",";

	//				myfile << isGood;
	//				if (!isGood)
	//				{
	//					std::wcout << L"<<<<<  " << name << std::endl;
	//					f = true;
	//				}
	//			}
	//			myfile << std::endl;
	//			i++;

	//			if (fff)
	//			{
	//				writer.Write(name + L'\n');
	//				coordFile.Write(std::to_wstring(well->PosX()) + L";" +
	//					std::to_wstring(well->PosY()) + L";" +
	//					std::to_wstring(well->PosXnum()) + L";" +
	//					std::to_wstring(well->PosYnum()) + L";\n");
	//			}
	//		}
	//		myfile.close();
	//		writer.Close();
	//	}
	//	void PrintWellIntersection(std::wstring fName = L"MatLab\\testWellCoords.txt") const
	//	{
	//		std::ofstream myfile;
	//		myfile.open(fName, std::ios_base::out);

	//		for (const auto& pair : reservoirIntantiator->Simulator().GetWells())
	//		{
	//			auto& well = pair.second;
	//			char buffer[200];
	//			snprintf(buffer, 200, "%+19.11E;%+19.11E;%+19.11E;%+19.11E;", well->PosX(), well->PosY(), well->PosXnum(), well->PosYnum());
	//			std::string result{ buffer };
	//			myfile << result << std::endl;
	//		}
	//		myfile.close();
	//	}

	//	void PrintWellDebit_selected(std::wstring fName = L"MatLab\\testWellDebits_selected.txt") const
	//	{
	//		UniversalWriter::UTF8Writer writer(fName);
	//		std::wstring result;

	//		UniversalSCParser::UTF8SVParser parser(L"sample_wells.txt");
	//		auto res = parser.Read();

	//		const auto& www = reservoirIntantiator->Simulator().GetWells();

	//		auto [start, end] = reservoirIntantiator->Config().PeriodOfInterest();

	//		for (const auto& pair : www)
	//		{
	//			bool f = false;
	//			auto& name = pair.first;
	//			auto& well = pair.second;


	//			std::wcout << name << res[0][0] << std::endl;


	//			for (int i = 0; i < res.size(); i++)
	//			{
	//				if (res[i][0] == name)
	//				{
	//					f = true;
	//					break;
	//				}
	//			}

	//			if (!f) // only print wells with anomalies
	//				continue;

	//			for (double time = start; time < end; time += 1)
	//			{
	//				auto debit = well->DebitOverall(time);
	//				result += std::to_wstring(time) + L';' + std::to_wstring(debit) + L';';
	//			}
	//			result += L'\n';
	//		}
	//		writer.Write(result);
	//		writer.Close();
	//	}

	//	void PrintDefault() const
	//	{
	//		PrintWellDebitLength();
	//		PrintWellIntersection();
	//	}

	//protected:
	//	const ReservoirInstantiator<Factory>* reservoirIntantiator;
	//};

} // reservoir_simulator