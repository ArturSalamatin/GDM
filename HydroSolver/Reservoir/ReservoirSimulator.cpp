#include "ReservoirSimulator.h"

#include "Well/Wells.h"
#include "../Data/ExceptionFactory.h"
#include "../Utils/JSON/JSONCreate.h"

namespace reservoir_simulator
{
	std::wstring OutputPath()
	{
		return L"ReservoirTestData//";
	}

	void ReservoirSimulator::PrintWellCoords() const
	{
		std::wstring fName{ OutputPath() + L"well_coordinates.txt" };
		std::ofstream myfile{ fName, std::ios_base::out };

		for (const auto& [name, p] :
			GetWells())
		{
			char buffer[200];
			snprintf(buffer, 200, "%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				(*p).PosX(), (*p).PosY(), (*p).PosXnum(), (*p).PosYnum());
			std::string result{ buffer };
			myfile << result << '\n';
		}
		myfile << std::flush;
		myfile.close();
	}

	//////////// ctor, dtor
	ReservoirSimulator::ReservoirSimulator() noexcept = default;
	ReservoirSimulator::ReservoirSimulator(
		const NumericalParameters& numPrm_,
		const DevelopedHorizon& horizon,
		const OilPhaseProperty& oil, const WaterPhaseProperty& water,
		const OtherProperties& other_properties,
		Layout layout) noexcept :
		numPrm{ numPrm_ }, RefPressure{ other_properties.extPressure },
		Grid{ OilField{horizon} },
		ActiveCellsNmbr{ Grid.ActiveCellsNmbr() },
		MyProblem{ LinearProblem{layout, numPrm.AMG_AbsTol, numPrm.AMG_RelTol, Grid.GetConnectivityGraph()} }
	{

		flowFields = std::vector<phasePortrait::SomeFlowField>();
		flowFields.reserve(horizon.grid_size.Nz);

		for (size_t k = 0, l = 0; k < nz(); ++k)
		{
			std::vector<std::vector<double>> poro;
			poro.reserve(horizon.GridPlanarSize());
			for (size_t j = 0; j < horizon.ny(); ++j)
			{
				poro.push_back(std::vector<double>());
				for (size_t i = 0; i < horizon.nx(); ++i, ++l)
					poro.back().push_back(horizon.porosity[l]);
			}

			flowFields.emplace_back(
				horizon.grid_bounds.x_min, horizon.nx(), horizon.block_size.step_x,
				horizon.grid_bounds.y_min, horizon.ny(), horizon.block_size.step_y,
				std::move(poro));

		}
		curOil = OilTotal();
		accumDebet = 0.0;
		accumOilOutFlux = 0.0;
		accumOil = 0.0;
		curWater = WaterTotal();
		accumWaterDebet = 0.0;
		accumWaterOutFlux = 0.0;
		accumWater = 0.0;
		curTime = 0.0;

		auto appRadiusWell{ 0.2 * horizon.block_size.step_x };
		for (const auto& name : horizon.well_names)
		{
			try 
			{
				if (horizon.mer.find(name) == horizon.mer.end())
				{
					WarningFactory::WellMERnotFound(name);
					continue;
				}
				if (horizon.well_jobs.find(name) == horizon.well_jobs.end())
				{
					WarningFactory::WellOperationDataNotFound(name);
					continue;
				}
				if (horizon.well_jobs.at(name).IsEmpty())
				{
					WarningFactory::WellOperationDataIsEmpty(name);
					continue;
				}
				AddWell_FixedProduction(
					name, 
					horizon.mer.at(name), 
					horizon.well_jobs.at(name),
					horizon.well_positions.at(name),
					horizon.grid_bounds, 
					horizon.block_size,
					appRadiusWell);
			}
			catch (std::exception& e)
			{
				WarningFactory::WellinitializationFailure(name, e);
			}
		}

		PrintWellCoords();
	}

	void ReservoirSimulator::AddWell_FixedProduction(const WellName& name,
		const mer_descriptor::SingleWell_MER_Data& well_data,
		const WellJobs& perforationsOfWell,
		const WellPosition& intersecCoords,
		const GridBounds& reservoir_bounds,
		const BlockSize& block_size,
		double appRadiusWell)
	{
		// cell trajectory of the well
			// �����������������, ������� ������
		size_t ItsCellId_X = (size_t)std::floor((intersecCoords.get()->getX() - reservoir_bounds.x_min) / block_size.step_x);
		size_t ItsCellId_Y = (size_t)std::floor((intersecCoords.get()->getY() - reservoir_bounds.y_min) / block_size.step_y);

		// global index triples [i,j,k] for each cell
		std::vector<std::vector<size_t>> ItsGlobalIDs;
		for (size_t i = 0; i < nz(); ++i)
			ItsGlobalIDs.push_back(std::vector<size_t>{ItsCellId_X, ItsCellId_Y, i}); // the well is vertical

		std::vector<size_t> well_local_position(ItsGlobalIDs.size());
		std::vector<const TwoPhaseFlowCell*> cells_(ItsGlobalIDs.size());

		std::vector<bool> ActiveCells;

		for (size_t l = 0; l < well_local_position.size(); ++l)
		{
			long int cellIdx = Grid.ConvertTriple2Local(ItsGlobalIDs[l]);
			bool isActive = cellIdx >= 0;
			ActiveCells.push_back(isActive);
			if (isActive) {
				well_local_position[l] = static_cast<size_t>(cellIdx);
				cells_[l] = &(Grid[static_cast<int>(cellIdx)]);
			} else {
				well_local_position[l] = 0;
				cells_[l] = &(Grid[0]);
			}
		}

		Wells.insert({ name,
			new wells::WellFixedProduction(name, name, intersecCoords,
			std::make_unique<const mer_descriptor::MER_Data>(name, well_data),
			perforationsOfWell.AccumulatePerforations(ActiveCells),
			cells_, well_local_position, appRadiusWell) });
	}

	ReservoirSimulator::~ReservoirSimulator()
	{
		for (auto [name, p] : Wells)
			delete p;
	}
	//////////// GETTERS
	const std::map<WellName, wells::SomeWell*>& 
		ReservoirSimulator::GetWells() const
	{
		return Wells;
	}
	size_t 
		ReservoirSimulator::nz() const 
	{ return Grid.Nz(); }
	phasePortrait::SomeFlowField::SequencePtr 
		ReservoirSimulator::GetFlowFieldsPtr(double multiplier) const
	{
		phasePortrait::SomeFlowField::SequencePtr vec;
		for (int k = 0; k < nz(); k++)
		{
			vec.push_back(std::make_shared<phasePortrait::SomeFlowField>(flowFields[k]));
			vec[k]->SetMutiplier(multiplier);
		}
		return vec;
	}

	std::vector<double> ReservoirSimulator::GetOilSaturationField() const
	{
		std::vector<double> OilSaturation;
		OilSaturation.reserve(Grid.TotalCellsNmbr());
		for (size_t l = 0; l < Grid.TotalCellsNmbr(); ++l)
		{
			OilSaturation.push_back(Grid[Grid.ConvertGlobal2Local(l)].SOil());
	//		if (OilSaturation.back() < 0.79)
	//			std::cout << l;
		}
		return OilSaturation;
	}

	std::vector<double> ReservoirSimulator::GetWaterSaturationField() const
	{
		std::vector<double> WaterSaturation;
		WaterSaturation.reserve(Grid.TotalCellsNmbr());
		for (int l = 0; l < Grid.TotalCellsNmbr(); l++)
			WaterSaturation.push_back(Grid[Grid.ConvertGlobal2Local(l)].SWater());
		return WaterSaturation;
	}

	std::vector<double> ReservoirSimulator::GetPressureField() const
	{
		std::vector<double> p;
		p.reserve(Grid.TotalCellsNmbr());
		for (int l = 0; l < Grid.TotalCellsNmbr(); l++)
			p.push_back(Grid[Grid.ConvertGlobal2Local(l)].P());
		return p;
	}

	std::vector<double> ReservoirSimulator::GetOverallBalance() const
	{
		return std::vector<double>{curTime, accumOil, accumOilOutFlux, accumDebet,
			accumWater, accumWaterOutFlux, accumWaterDebet};
	}

	void ReservoirSimulator::PrintReservoirState(std::ios_base::openmode mode) const
	{
		namespace fs = std::filesystem;
		fs::create_directories(OutputPath());

		std::wstring 
			saveOilSaturation_fName = OutputPath() + L"oil_saturation.txt",
			savePressure_fName = OutputPath() + L"pressure.txt",
			saveOverallBalance_fName = OutputPath() + L"overall_balance.txt";

		printPointVariable(saveOverallBalance_fName,
			GetOverallBalance(), mode);

		printFieldVariable(saveOilSaturation_fName,
			GetOilSaturationField(), mode);
		/*		printFieldVariable(saveWaterSaturation_fName,
					GetWaterSaturationField(), mode);*/
		printFieldVariable(savePressure_fName,
			GetPressureField(), mode);

		/*	std::vector<double> jOil_X, jOil_Y, jOil_Z, j_X, j_Y, j_Z;
			tie(jOil_X, jOil_Y, jOil_Z, j_X, j_Y, j_Z) = OverallFluxes();

			printFieldVariable(saveOilFlux_X_fName,
				jOil_X, mode);
			printFieldVariable(saveOilFlux_Y_fName,
				jOil_Y, mode);
			printFieldVariable(saveOilFlux_Z_fName,
				jOil_Z, mode);
			printFieldVariable(saveOverallFlux_X_fName,
				j_X, mode);
			printFieldVariable(saveOverallFlux_Y_fName,
				j_Y, mode);
			printFieldVariable(saveOverallFlux_Z_fName,
				j_Z, mode);*/
	}

	void ReservoirSimulator::printPointVariable(const std::wstring fName,
		const std::vector<double>& data,
		std::ios_base::openmode mode) const
	{
		std::ofstream myfile;
		myfile.open(fName, mode);
		myfile 
			<< std::setw(5)			
			<< data[0];
		for (size_t l = 1; l < data.size(); l++)
			myfile 
			<< ";" 
			<< std::setw(8) 
			<< data[l];
		myfile << std::endl;
		myfile.close();
	}

	void ReservoirSimulator::printFieldVariable(const std::wstring fName,
		const std::vector<double>& data,
		std::ios_base::openmode mode) const
	{
		std::ofstream myfile;
		myfile.open(fName, mode);
		for (size_t l = 0; l < data.size(); l++)
			myfile << (float)data[l] << ";";
		//	myfile << endl;
		myfile.close();
	}

	void ReservoirSimulator::pressure_json() const//(const std::vector<double>& s, const std::vector<double>& p)
	{
		//	const std::vector<double> s = GetOilSaturationField();
		const std::vector<double> p = GetPressureField();

		/////////////////////////////////////////////////
		// start result sending
		JSONArray result;
		//	JSONObject nodes{ { L"name", L"s" } , { L"dimension", L"4" } };
		//	nodes.push_back({ L"names", JSON::CreateJSON::CreateArray(JSONArray{ L"d0", L"d1a", L"d1b", L"d1vgd" }) });
		//	result.push_back(JSON::CreateJSON::CreateObject(nodes));
		JSONObject nodes{ { L"name", L"p" } , { L"dimension", L"4" },
				{ L"names", JSON::CreateJSON::CreateArray(JSONArray{ L"d0", L"d1a", L"d1b", L"d1vgd" }) } };
		result.push_back(JSON::CreateJSON::CreateObject(std::move(nodes)));
//		LogFileSpace::LogFile::WriteLog(L"main", L"pressure_sender", L"success", JSON::CreateJSON::CreateArray(std::move(result)), "");
		/////////////////////////////////////////////////




	//		std::wstring fName = itsPathToConfigFile;
	//		std::vector<IRCGEngine::IrapClassicGrid> irap_output_p;
	//		std::vector<IRCGEngine::IrapClassicGrid> irap_output_s;
	//		for (int k = 0; k < nz(); k++)
	//		{
	//			Grid.Nx()

	//			irap_output_p.emplace_back(fName + L"pressure_" + std::to_wstring(k) + L".irap", xstep(), ystep(), reservoir_bonds());
	//			irap_output_s.emplace_back(fName + L"saturation_" + std::to_wstring(k) + L".irap", xstep(), ystep(), reservoir_bonds());

	////			irap_output_p[k].Add({x, y}, p);

	//		}

	//		irap_output_p[0].Write();


		result.clear();
		for (size_t j = 0; j < Grid.Ny(); ++j)
		{
			double y = Grid(0, j, 0).Y();
			for (size_t i = 0; i < Grid.Nx(); ++i)
			{
				double x = Grid(i, 0, 0).X();
				JSONArray sVals, pVals;
				JSONObject resultXY;
				resultXY.push_back({ L"x", std::to_wstring(x) });
				resultXY.push_back({ L"y", std::to_wstring(y) });
				for (size_t k = 0; k < Grid.Nz(); ++k)
				{
					auto l = Grid.ConvertTriple2Global(std::vector<size_t>{ i, j, k });
					//	sVals.push_back(std::to_wstring(s[l]));
					pVals.push_back(std::to_wstring((p[l] - 101325 * 167) * 1E-5));

					//	irap_output_p[k].Add({ x, y }, p[l]);
				}
				//	resultXY.push_back({ L"s", JSON::CreateJSON::CreateArray(sVals) });
				resultXY.push_back({ L"p", JSON::CreateJSON::CreateArray(std::move(pVals)) });

				result.push_back(JSON::CreateJSON::CreateObject(std::move(resultXY)));
			}
		}
	//	LogFileSpace::LogFile::WriteLog(L"class_ReservoirSimulator", L"method_pressure_json", L"success",
	//		JSON::CreateJSON::CreateArray(std::move(result)));

		//		UniversalWriter::ASCIWriter huinya(path + L"\\dam\\gdm\\x_y_s_p.xyz");
		//		huinya.Write(L"X\tY\tS0\tS1\tS2\tS3");

	}

	double ReservoirSimulator::OilTotal() const
	{
		double result = 0.0;
		for (size_t l = 0; l < ActiveCellsNmbr; ++l)
			result += Grid[l].OilMass();
		return result;
	}
	double ReservoirSimulator::WaterTotal() const
	{
		double result = 0.0;
		for (size_t l = 0; l < ActiveCellsNmbr; ++l)
			result += Grid[l].WaterMass();
		return result;
	}
	double ReservoirSimulator::OilDebitTotal() const
	{
		double result = 0.0;

		for (const auto& [name, well] : Wells)
		{
			result += well->CurOilDebit_Num();
		}
		return result;
	}
	double ReservoirSimulator::WaterDebitTotal() const
	{
		double result = 0.0;
		for (const auto& pair : Wells)
		{
			auto& well = pair.second;
			result += well->CurWaterDebit_Num();
		}
		return result;
	}


	//////////// SOLVER SECTION
	double ReservoirSimulator::Solve(const std::vector<double>& timeMoments)
	{
		prof.tic("total");

#ifdef PRINT_DEBUG_INFO
		std::ofstream myfile;
		myfile.open("test_tau.txt", std::ios_base::out);
#endif // PRINT_DEBUG_INFO

		for (size_t i = 1; i < timeMoments.size(); i++)
		{
			while (numPrm.CurrentTimeMoment() < timeMoments[i])
			{
				numPrm.update_maxTauAllowed(timeMoments[i], GetWells());
#ifdef PRINT_DEBUG_INFO
				myfile << numPrm.CurrentIntegrationStep() << "  " << numPrm.CurrentTimeMoment() << std::endl;
#endif // PRINT_DEBUG_INFO

				PerformNewtonLoop(numPrm.CurrentIntegrationStep(),
					numPrm.NextTimeMoment());

				if (numPrm.IsSuccessfullNewtonTrial())
				{
					MassBalance(numPrm.CurrentIntegrationStep());
					Grid.AcceptState();
					numPrm.update_currentMoment();
					AddFlowFieldSnapShot();
					solverProfile_.n_time_steps++;
				}
				else
				{
					numPrm.decrease_schemeTau();
					solverProfile_.n_wasted_trials++;
					continue;
				}
			}
		}
#ifdef PRINT_DEBUG_INFO
		myfile.close();
#endif // PRINT_DEBUG_INFO

		prof.toc("total");
		return numPrm.CurrentSchemeTau();
	}
	void ReservoirSimulator::PerformNewtonLoop(double loc_tau, double nextTimeMoment)
	{
		numPrm.set_currentNewtonIterationCount(0);
		numPrm.update_isSuccesfullNewtonTrial(false);
		numPrm.set_currentAMG_maxSolverIterationCount();
		while (!numPrm.IsSuccessfullNewtonTrial())
		{
			SingleIteration(loc_tau, nextTimeMoment);
			solverProfile_.n_newton_iters++;

			if (numPrm.IsSuccessfullAMG_Iteration() && numPrm.IsNewtonIterationContinue())
			{
				prof.tic("update");
				numPrm.update_isSuccesfullNewtonTrial(UpdateGrid());
				prof.toc("update");
			}
			else
			{
				Grid.ReverseState();
				break;
			}
		}
	}
	bool ReservoirSimulator::UpdateGrid()
	{
		const double tol = 3E-3;
		std::vector<char> f(B * Grid.ActiveCellsNmbr(), 1);

#ifdef	USE_PARALLEL
#pragma omp parallel for
#endif
		for (int l = 0; l < Grid.ActiveCellsNmbr(); l++)
		{
			double corr[B];
			MyProblem.UnpackCellCorrections(l, corr);
			Grid[l].UpdateState(corr);

			const std::vector<double>& stateVaiables = Grid[l].GetVariableFieldProperties();

			int i = 0; // saturation
			f[B * l + i] =
				(abs(stateVaiables[i]) < numPrm.NewtonTol() * tol) || (abs(1.0 - stateVaiables[i]) < numPrm.NewtonTol() * tol) ||
				(abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
			i = 1; // pressure
			f[B * l + i] =
				(abs(stateVaiables[i]) < 1E6) ||
				(abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
		}
		return std::all_of(f.begin(), f.end(), [](char x) { return x != 0; });
	}

	void ReservoirSimulator::SingleIteration(double loc_tau, double nextTimeMoment)
	{
		prof.tic("assemble");
		AssembleMyProblem(loc_tau, nextTimeMoment);
		prof.toc("assemble");

		auto res = MyProblem.Solve(numPrm.CurrentAMG_maxSolverIterationCount());
		numPrm.update_currentAMGState({ static_cast<int>(res.iters), res.error, res.converged });

		solverProfile_.n_amg_solves++;
		solverProfile_.total_amg_iters += res.iters;
	}

	void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
	{
		assembler_.Assemble(loc_tau, nextTimeMoment, Grid, MyProblem, RefPressure, Wells);
	}

	void ReservoirSimulator::AddFlowFieldSnapShot()
	{
		// oil flux through grid cell boundaries
		std::vector<std::vector<std::vector<double>>> jOil_X, jOil_Y;
		// overall flux through grid cell boundaries
		std::vector<std::vector<std::vector<double>>> j_X, j_Y;
		std::tie(jOil_X, jOil_Y, j_X, j_Y) = OverallFluxes();

		for (int k = 0, l = 0; k < Grid.Nz(); k++)
		{
			flowFields[k].add_snapshot(numPrm.CurrentTimeMoment(), std::move(j_X[k]), std::move(j_Y[k])); // time is in days
		}
	}
	void ReservoirSimulator::MassBalance(double loc_tau)
	{
		prevOil = curOil;
		curOil = OilTotal();
		accumOil += curOil - prevOil;

		accumOilOutFlux += OilContourFlux() * loc_tau;
		accumDebet -= OilDebitTotal() * loc_tau;

		prevWater = curWater;
		curWater = WaterTotal();
		accumWater += curWater - prevWater;

		accumWaterOutFlux += WaterContourFlux() * loc_tau;
		accumWaterDebet -= WaterDebitTotal() * loc_tau;

		curTime += loc_tau;
	}

	//////////// FILE IN/OUT
	void ReservoirSimulator::SaveFlowField2File(const std::wstring& /*configPath*/, const std::wstring& /*fileName*/)
	{
		// TODO: re-implement without legacy UniversalSVWriter (encoding issues)
	}

	void ReservoirSimulator::SaveFlowField2File_bin(const std::wstring& configPath, const std::wstring& fileName,
		double saturation_date, double startDate, double endDate, int frameCount)
	{
		const std::wstring ext = L".bin";

		namespace fs = std::filesystem;
		try {
			// make directory 
			fs::path targetFile = fileName + ext;
			fs::create_directories(targetFile.parent_path());
		}
		catch (std::exception& e)
		{
			reservoir_simulator::WarningFactory::NoFolderCreated();
			/*LogFileSpace::LogFile::WriteLog(L"class_ReservoirSimulator", L"method_SaveFlowField2File",
				L"warning", L"Could not create folder dam//gdm", e.what());*/
		}

		std::ofstream wstream(fileName + ext, std::ios::binary);

		// write time frame and discretization
		wstream.write(reinterpret_cast<const char*>(&saturation_date), sizeof(double));
		wstream.write(reinterpret_cast<const char*>(&startDate), sizeof(double));
		wstream.write(reinterpret_cast<const char*>(&endDate), sizeof(double));
		wstream.write(reinterpret_cast<const char*>(&frameCount), sizeof(int));
		// write grid dimensions
		const std::vector<size_t> grid_dim{ Grid.Nx(), Grid.Ny(), Grid.Nz() };
		wstream.write(reinterpret_cast<const char*>(&grid_dim[0]), grid_dim.size() * sizeof(size_t));
		// write every z-cross-section one-by-one
		for (int k = 0; k < flowFields.size(); k++)
			flowFields[k].write(wstream);
		wstream.close();
	}

	void ReservoirSimulator::SaveSaturationPressure_bin(const std::wstring& configPath, const std::wstring& fileName,
		double saturation_date, double startDate, double endDate, int frameCount)
	{
		const std::wstring ext = L".bin";

		namespace fs = std::filesystem;
		try {
			// make directory 
			fs::path targetFile = fileName + ext;
			fs::create_directories(targetFile.parent_path());
		}
		catch (std::exception&)
		{
			reservoir_simulator::WarningFactory::NoFolderCreated();
			/*LogFileSpace::LogFile::WriteLog(L"class_ReservoirSimulator", L"method_SaveFlowField2File",
				L"warning", L"Could not create folder dam//gdm.");*/
		}

		std::ofstream wstream(fileName + ext, std::ios::binary);

		// write time frame and discretization
		wstream.write(reinterpret_cast<const char*>(&saturation_date), sizeof(double));
		wstream.write(reinterpret_cast<const char*>(&startDate), sizeof(double));
		wstream.write(reinterpret_cast<const char*>(&endDate), sizeof(double));
		wstream.write(reinterpret_cast<const char*>(&frameCount), sizeof(int));
		// write grid dimensions
		const std::vector<size_t> grid_dim{ Grid.Nx(), Grid.Ny(), Grid.Nz() };
		wstream.write(reinterpret_cast<const char*>(&grid_dim[0]), grid_dim.size() * sizeof(size_t));
		// write every z-cross-section one-by-one
		for (int k = 0; k < flowFields.size(); k++)
			flowFields[k].write(wstream);
		wstream.close();

	}

	void ReservoirSimulator::LoadFlowFieldFromFile(const std::wstring& /*fileName*/)
	{
		// TODO: re-implement without legacy UniversalSVParser (encoding issues)
	}


	void ReservoirSimulator::LoadFlowFieldFromFile_bin(const std::wstring& fileName)
	{
		std::ifstream wstream(fileName, std::ios::binary);

		double saturation_date;
		double startDate;
		double endDate;
		int frameCount;

		// skip-read time frame and discretization
		wstream.read(reinterpret_cast<char*>(&saturation_date), sizeof(double));
		wstream.read(reinterpret_cast<char*>(&startDate), sizeof(double));
		wstream.read(reinterpret_cast<char*>(&endDate), sizeof(double));
		wstream.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

		// grid dimensions
		std::array<size_t, 3> grid_dim;
		wstream.read(reinterpret_cast<char*>(grid_dim.data()), 3 * sizeof(size_t));
		size_t nx = grid_dim[0], ny = grid_dim[1], nz = grid_dim[2];

		for (size_t k = 0; k < nz && k < flowFields.size(); k++)
		{
			flowFields[k].clear();
			int nt; // number of time frames saved
			wstream.read(reinterpret_cast<char*>(&nt), sizeof(int));

			for (int t = 0; t < nt; t++)
			{// loop through time frames in current z-section
				double time;
				wstream.read(reinterpret_cast<char*>(&time), sizeof(double));

				// read vxField
				std::vector < std::vector<double>> vxField;
				vxField.reserve(ny);
				for (int j = 0; j < ny; j++)
				{
					vxField.emplace_back(std::vector<double>(nx + 1, 0.0));
					vxField.back().reserve(nx + 1);
					wstream.read(reinterpret_cast<char*>(&vxField.back()[0]), sizeof(double) * (nx + 1));
				}
				// read vyField
				std::vector < std::vector<double>> vyField;
				vyField.reserve(ny + 1);
				for (int j = 0; j < ny + 1; j++)
				{
					vyField.emplace_back(std::vector<double>(nx, 0.0));
					vyField.back().reserve(nx);
					wstream.read(reinterpret_cast<char*>(&vyField.back()[0]), sizeof(double) * (nx));
				}
				flowFields[k].add_snapshot(time, std::move(vxField), std::move(vyField));
			}
		}
		wstream.close();
	}

	bool ReservoirSimulator::is_flow_field_empty() const
	{
		for (const auto& field : flowFields)
			if (field.is_empty())
				return true;
		return false;
	}



	std::tuple<std::vector<std::vector<std::vector<double>>>, std::vector<std::vector<std::vector<double>>>,
		std::vector<std::vector<std::vector<double>>>, std::vector<std::vector<std::vector<double>>>>
		ReservoirSimulator::OverallFluxes() const
	{
		size_t nx = Grid.Nx(), ny = Grid.Ny(), nz = Grid.Nz();
		// oil flux through grid cell boundaries
		std::vector<std::vector<std::vector<double>>> jOil_X, jOil_Y;
		// overall flux through grid cell boundaries
		std::vector<std::vector<std::vector<double>>> j_X, j_Y;

		if (ny > 1)
		{
			for (int k = 0; k < nz; k++)
			{
				jOil_Y.push_back(std::vector<std::vector<double>>());
				j_Y.push_back(std::vector<std::vector<double>>());

				for (int j = 0; j < ny + 1; j++)
				{
					jOil_Y.back().push_back(std::vector<double>());
					j_Y.back().push_back(std::vector<double>());
				}

				for (int i = 0; i < nx; i++)
				{
					{
						// lowest boundary
						int j = 0; // lowest cell
						int l_Global = nx * ny * k + nx * j + i;
						int l_Local = Grid.ConvertGlobal2Local(l_Global); // local index of the cell, inactive cells are not counted
						if (l_Local > -1)
						{ // the cell is active
							const TwoPhaseFlowCell& cell = Grid[l_Local];
							const double hy = cell.StepY();
							const double refPressure = RefPressure;

							double p_grad = (cell.P() - refPressure) / hy;
							double f_oil, f_water;
							if (p_grad > 0)
							{
								f_oil = cell.F_Oil();
								f_water = cell.F_Water();
							}
							else
							{
								f_oil = 0.0;
								f_water = 1.0;
							}
							jOil_Y.back()[0].push_back(-f_oil * cell.MobilityOverall() * p_grad);
							j_Y.back()[0].push_back(-cell.MobilityOverall() * p_grad);
						}
						else
						{
							// the cell is not active
							jOil_Y.back()[0].push_back(0);
							j_Y.back()[0].push_back(0);
						}
					}
					{
						// internal boundaries
						// to the right from the current cell
						for (int j = 0; j < ny - 1; j++)
						{
							int l_Global = nx * ny * k + nx * j + i;
							int l_Local = Grid.ConvertGlobal2Local(l_Global); // local index of the cell, inactive cells are not counted
							int l_Local_Neighbour = Grid.ConvertGlobal2Local(l_Global + nx);// local index of the cell, adjucent to the current one

							if ((l_Local > -1) && (l_Local_Neighbour > -1))
							{   // both cells are active
								const TwoPhaseFlowCell& cell = Grid[l_Local];
								const TwoPhaseFlowCell& cellNeighbour = Grid[l_Local_Neighbour]; // upper cell
								const double hy = cell.StepY();
								//	const double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

								double p_grad = (cellNeighbour.P() - cell.P()) / hy;
								double f_oil, f_water;
								double mob_sum = cellNeighbour.MobilityOverall() + cell.MobilityOverall();
								double mobility = (mob_sum > 0.0) ? 2.0 * cellNeighbour.MobilityOverall() * cell.MobilityOverall() / mob_sum : 0.0;
								if (p_grad > 0)
								{
									f_oil = cell.F_Oil();
									f_water = cell.F_Water();
								}
								else
								{
									f_oil = cellNeighbour.F_Oil();
									f_water = cellNeighbour.F_Water();
								}
								jOil_Y.back()[j + 1].push_back(-f_oil * mobility * p_grad);
								j_Y.back()[j + 1].push_back(-mobility * p_grad);
							}
							else
							{// either cell is not active
								jOil_Y.back()[j + 1].push_back(0);
								j_Y.back()[j + 1].push_back(0);

							}
						}
					}
					{
						// uppertmost boundary
						int j = ny - 1; // uppermost cell
						int l_Global = nx * ny * k + nx * j + i;
						int l_Local = Grid.ConvertGlobal2Local(l_Global); // local index of the cell, inactive cells are not counted
						if (l_Local > -1)
						{ // the cell is active
							const TwoPhaseFlowCell& cell = Grid[l_Local];
							const double  hy = cell.StepY();
							const double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

							double p_grad = (refPressure - cell.P()) / hy;
							double f_oil, f_water;
							if (p_grad > 0)
							{
								f_oil = f_oil = cell.F_Oil();
								f_water = cell.F_Water();
							}
							else
							{
								f_oil = 0.0;
								f_water = 1.0;
							}
							jOil_Y.back()[j + 1].push_back(-f_oil * cell.MobilityOverall() * p_grad);
							j_Y.back()[j + 1].push_back(-cell.MobilityOverall() * p_grad);
						}
						else
						{// either cell is not active
							jOil_Y.back()[j + 1].push_back(0);
							j_Y.back()[j + 1].push_back(0);
						}
					}

				}
			}
		}
		else
		{
			for (size_t k = 0; k < nz; k++)
			{
				jOil_Y.push_back(std::vector<std::vector<double>>());
				j_Y.push_back(std::vector<std::vector<double>>());
			}
		}

		// loop through the boundaries parallel to YZ-plane
		if (nx > 1)
		{
			for (int k = 0; k < nz; k++)
			{
				jOil_X.push_back(std::vector<std::vector<double>>());
				j_X.push_back(std::vector<std::vector<double>>());

				for (int j = 0; j < ny; j++)
				{
					jOil_X.back().push_back(std::vector<double>());
					j_X.back().push_back(std::vector<double>());
					{
						// leftmost boundary
						int i = 0; // leftmost cell
						int l_Global = nx * ny * k + nx * j + i;
						int l_Local = Grid.ConvertGlobal2Local(l_Global); // local index of the cell, inactive cells are not counted
						if (l_Local > -1)
						{ // the cell is active
							const TwoPhaseFlowCell& cell = Grid[l_Local];
							const double hx = cell.StepX();
							const double refPressure = RefPressure;

							double p_grad = (cell.P() - refPressure) / hx;
							double f_oil, f_water;
							if (p_grad > 0)
							{
								f_oil = cell.F_Oil();
								f_water = cell.F_Water();
							}
							else
							{
								f_oil = 0.0;
								f_water = 1.0;
							}
							jOil_X.back().back().push_back(-f_oil * cell.MobilityOverall() * p_grad);
							j_X.back().back().push_back(-cell.MobilityOverall() * p_grad);
						}
						else
						{// the cell is not active
							jOil_X.back().back().push_back(0.0);
							j_X.back().back().push_back(0.0);

						}
					}
					{
						// internal boundaries
						// to the right from the current cell
						for (int i = 0; i < nx - 1; i++)
						{
							int l_Global = nx * ny * k + nx * j + i;
							int l_Local = Grid.ConvertGlobal2Local(l_Global); // local index of the cell, inactive cells are not counted
							int l_Local_Neighbour = Grid.ConvertGlobal2Local(l_Global + 1);// local index of the cell, adjucent to the current one

							if ((l_Local > -1) && (l_Local_Neighbour > -1))
							{   // both cells are active
								const TwoPhaseFlowCell& cell = Grid[l_Local];
								const TwoPhaseFlowCell& cellNeighbour = Grid[l_Local_Neighbour];

								double p_grad = (cellNeighbour.P() - cell.P()) / cell.StepX();
								double f_oil, f_water;
								double mob_sum_x = cellNeighbour.MobilityOverall() + cell.MobilityOverall();
								double mobility = (mob_sum_x > 0.0) ? 2.0 * cellNeighbour.MobilityOverall() * cell.MobilityOverall() / mob_sum_x : 0.0;
								if (p_grad > 0)
								{
									f_oil = cell.F_Oil();
									f_water = cell.F_Water();
								}
								else
								{
									f_oil = cellNeighbour.F_Oil();
									f_water = cellNeighbour.F_Water();
								}
								jOil_X.back().back().push_back(-f_oil * mobility * p_grad);
								j_X.back().back().push_back(-mobility * p_grad);
							}
							else
							{// the cell is not active
								jOil_X.back().back().push_back(0.0);
								j_X.back().back().push_back(0.0);

							}
						}
					}
					{
						// rightmost boundary
						int i = nx - 1; // rightmost cell
						int l_Global = nx * ny * k + nx * j + i;
						int l_Local = Grid.ConvertGlobal2Local(l_Global); // local index of the cell, inactive cells are not counted
						if (l_Local > -1)
						{ // the cell is active
							const TwoPhaseFlowCell& cell = Grid[l_Local];
							const double hx = cell.StepX();
							const double refPressure = RefPressure;

							double p_grad = (refPressure - cell.P()) / hx;
							double f_oil, f_water;
							if (p_grad > 0)
							{
								f_oil = f_oil = cell.F_Oil();
								f_water = cell.F_Water();
							}
							else
							{
								f_oil = 0.0;
								f_water = 1.0;
							}
							jOil_X.back().back().push_back(-f_oil * cell.MobilityOverall() * p_grad);
							j_X.back().back().push_back(-cell.MobilityOverall() * p_grad);
						}
						else
						{// the cell is not active
							jOil_X.back().back().push_back(0.0);
							j_X.back().back().push_back(0.0);

						}
					}
				}
			}
		}
		else
		{
			for (size_t k = 0; k < nz; k++)
			{
				jOil_X.push_back(std::vector<std::vector<double>>());
				j_X.push_back(std::vector<std::vector<double>>());
			}
		}
		return  { jOil_X, jOil_Y, j_X, j_Y };
	}


	double ReservoirSimulator::OilContourFlux() const
	{
		double result = 0.0;
		size_t nx = Grid.Nx(), ny = Grid.Ny(), nz = Grid.Nz();
		if (nx > 1)
			for (int j = 0; j < ny; j++)
			{
				for (size_t i = 0; i < nx; i += nx - 1)
				{
					for (size_t k = 0; k < nz; k++)
					{
						long int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i); // local index of the cell, inactive cells are not counted

						if (l < 0)
						{// skip cells that are not active
							continue;
						}

						const TwoPhaseFlowCell& cell = Grid[l];

						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

						double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

						double faceArea = hy * hz / (hx / 2); // face area over the cell thickness
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_oil; // volumetric fraction of the flux per oil 
						double cur_mobility = cell.MobilityOverall();

						if (dp > 0.0)
						{// the flow is out of the cell
							f_oil = cell.F_Oil() * cell.DensityOil();
						}
						else
						{// the flow is into the cell
							// assume that pure water goes in
							f_oil = 0.0;
						}

						result += cur_mobility * f_oil * p_grad;
					}
				}
			}


		if (ny > 1)
			for (size_t i = 0; i < nx; i++)
			{
				for (size_t j = 0; j < ny; j += ny - 1)
				{
					for (size_t k = 0; k < nz; k++)
					{
						long int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i); // local index of the cell, inactive cells are not counted

						if (l < 0)
						{// skip cells that are not active
							continue;
						}

						const TwoPhaseFlowCell& cell = Grid[l];

						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

						double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

						double faceArea = hx * hz / (hy / 2); // face area over the cell thickness
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_oil; // volumetric fraction of the flux per oil and water respectively
						double cur_mobility = cell.MobilityOverall();

						if (dp > 0.0)
						{// the flow is out of the cell
							f_oil = cell.F_Oil() * cell.DensityOil();
						}
						else
						{// the flow is into the cell
							// assume that pure water goes in
							f_oil = 0.0;
						}

						result += cur_mobility * f_oil * p_grad;
					}
				}
			}

		if (false && (nz > 1))
			for (size_t i = 0; i < nx; i++)
			{
				for (size_t j = 0; j < ny; j++)
				{
					for (size_t k = 0; k < nz; k += nz - 1)
					{
						long int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i); // local index of the cell, inactive cells are not counted

						if (l < 0)
						{// skip cells that are not active
							continue;
						}

						const TwoPhaseFlowCell& cell = Grid[l];

						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();

						double refPressure = RefPressure;// -presKoef * 40.0 * l / (ny - 1.0);

						double faceArea = hx * hy / (hz / 2); // face area over the cell thickness
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_oil; // volumetric fraction of the flux per oil and water respectively
						double cur_mobility = cell.MobilityOverall();

						if (dp > 0.0)
						{// the flow is out of the cell
							f_oil = cell.F_Oil() * cell.DensityOil();
						}
						else
						{// the flow is into the cell
							// assume that pure water goes in
							f_oil = 0.0;
						}

						result += cur_mobility * f_oil * p_grad;
					}
				}
			}
		return result;
	}

	double ReservoirSimulator::WaterContourFlux() const
	{
		double result = 0.0;
		size_t nx = Grid.Nx(), ny = Grid.Ny(), nz = Grid.Nz();
		if (nx > 1)
			for (int j = 0; j < ny; j++)
			{
				for (size_t i = 0; i < nx; i += nx - 1)
				{
					for (size_t k = 0; k < nz; k++)
					{
						long int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i);
						if (l < 0) continue;

						const TwoPhaseFlowCell& cell = Grid[l];
						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();
						double refPressure = RefPressure;
						double faceArea = hy * hz / (hx / 2);
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_water;
						double cur_mobility = cell.MobilityOverall();

						if (dp > 0.0)
							f_water = cell.F_Water() * cell.DensityWater();
						else
							f_water = 1.0 * cell.DensityWater(refPressure);

						result += cur_mobility * f_water * p_grad;
					}
				}
			}

		if (ny > 1)
			for (size_t i = 0; i < nx; i++)
			{
				for (size_t j = 0; j < ny; j += ny - 1)
				{
					for (size_t k = 0; k < nz; k++)
					{
						long int l = Grid.ConvertGlobal2Local(nx * ny * k + nx * j + i);
						if (l < 0) continue;

						const TwoPhaseFlowCell& cell = Grid[l];
						const double hx = cell.StepX(), hy = cell.StepY(), hz = cell.StepZ();
						double refPressure = RefPressure;
						double faceArea = hx * hz / (hy / 2);
						double dp = cell.P() - refPressure;
						double p_grad = faceArea * dp;
						double f_water;
						double cur_mobility = cell.MobilityOverall();

						if (dp > 0.0)
							f_water = cell.F_Water() * cell.DensityWater();
						else
							f_water = 1.0 * cell.DensityWater(refPressure);

						result += cur_mobility * f_water * p_grad;
					}
				}
			}

		return result;
	}


} // reservoir_simulator


//ReservoirSimulator::



// examples
		/*void NeuralNetworkCL::NeuralNetwork::SaveWeights()
		{
			for (int i = 0; i < Layers.size(); i++)
			{
				if (i < _Weights.size() && i < _Bias.size())
				{
					std::ofstream wstream(Path + L"weights_layer" + std::to_wstring(i) + L".bin", std::ios::binary);
					wstream.write(reinterpret_cast<const char*>(&_Weights[i][0]), _Weights[i].size());
					wstream.close();
					std::ofstream bstream(Path + L"bias_layer" + std::to_wstring(i) + L".bin", std::ios::binary);
					bstream.write(reinterpret_cast<const char*>(&_Bias[i][0]), _Bias[i].size());
					bstream.close();
				}
			}
		}*/

		//bool NeuralNetworkCL::NeuralNetwork::LoadWeights()
			//{
			//	_Weights.clear();
			//	_Bias.clear();
			//	for (int i = 0; i < Layers.size(); i++)
			//	{
			//		std::filesystem::path wpath = Path + L"weights_layer" + std::to_wstring(i) + L".bin";
			//		std::filesystem::path bpath = Path + L"bias_layer" + std::to_wstring(i) + L".bin";
			//		//������ ����
			//		if (std::filesystem::exists(wpath))
			//		{
			//			auto wfile_size = std::filesystem::file_size(wpath);
			//			_Weights.push_back(std::vector<unsigned char>(wfile_size));
			//			//_Weights[i] = std::vector<unsigned char>(wfile_size);
			//			std::ifstream wstream(wpath, std::ios::binary);
			//			wstream.read(reinterpret_cast<char*>(&(_Weights[i][0])), wfile_size);
			//			wstream.close();
			//		}
			//		else {
			//			return false;
			//		}
			//		//������ �����
			//		if (std::filesystem::exists(bpath))
			//		{
			//			auto wfile_size = std::filesystem::file_size(bpath);
			//			_Bias.push_back(std::vector<unsigned char>(wfile_size));
			//			//_Bias[i] = std::vector<unsigned char>(wfile_size);
			//			std::ifstream wstream(bpath, std::ios::binary);
			//			wstream.read(reinterpret_cast<char*>(&(_Bias[i][0])), wfile_size);
			//			wstream.close();
			//		}
			//		else {
			//			return false;
			//		}
			//	}
			//	return true;
			//}