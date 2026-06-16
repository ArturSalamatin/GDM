#pragma once
#include "Config.h"
#include "../Utils/JSON/JSONCreate.h"

namespace reservoir_simulator
{
	namespace configurators
	{
		class Config_JSON : public Config
		{
			typedef std::map<std::wstring, std::unique_ptr<JSON::IJObject>> JSONtree;

		protected:
			//	INIReader reader;
			JSONtree values;
			const std::wstring folderName = L"MatLab\\";

		public:
			Config_JSON(std::wstring fileName) : Config()
			{
				if (fileName.back() != '\\')
					fileName += '\\';

				try {
					values = JSON::JSONParser(fileName + L"config.json").Parse()->Value();

					anomaly_date_start = PathUtils::Utils::ConvertDateToExcelDate(GetLeave(L"temporal_parameters", L"anomaly_date_start"));
					anomaly_date_end = PathUtils::Utils::ConvertDateToExcelDate(GetLeave(L"temporal_parameters", L"anomaly_date_end"));
					saturation_field_date = PathUtils::Utils::ConvertDateToExcelDate(GetLeave(L"temporal_parameters", L"saturation_field_date"));
					number_of_snapshots = (int)GetValue(L"temporal_parameters", L"number_of_snapshots");
					anomaly_detection_interval = GetValue(L"anomaly", L"anomaly_detection_interval");
					// at least two snapshopts must be saved, thus we can show the progress bar
					if (number_of_snapshots < 2)
					{
						number_of_snapshots = 2;
						LogFileSpace::LogFile::WriteLog(L"class_Config_JSON", L"method_Config_JSON", L"warning", L"number_of_snapshots in config file is set to 2. Revise confg file.");
					}

					std::string msg{ "temporal_parameters section in config file is wrong." };
					bool f = false; // assume config is good
					if (saturation_field_date > anomaly_date_start)
					{
						msg += " saturation_field_date is greater than anomaly_date_start.";
						f = true;
					}
					if (anomaly_date_start > anomaly_date_end)
					{
						f = true;
						msg += " anomaly_date_start is greater than anomaly_date_end.";
					}
					if (f)
						throw std::runtime_error(msg);

					std::string msg0{ "anomaly section in config file is wrong." };
					if (anomaly_date_start - anomaly_detection_interval < saturation_field_date)
					{
						msg0 += " anomaly_detection_interval does not correspond to computation time frame. Consider that the signal takes some time to reach the well.";
						f = true;
					}
					if (f)
						throw std::runtime_error(msg0);
				}
				catch (std::exception& e)
				{
					throw std::runtime_error(std::string{ "Read config error. " + std::string{e.what()} }.data());
				}
			}
			virtual  std::wstring GetWideProjectPath() {
				auto result = GetLeave(L"model", L"path");
				if (result.back() != '\\')
					result += '\\';
				return  result;
			}

			virtual std::vector<std::vector<std::wstring>> LayerAggregation()
			{
				auto val = values[L"model"]->Value()[L"layer_aggregation"]->Value();
				std::vector < std::vector < std::wstring>> layers;
				for (const auto& pohui : val)
				{
					layers.push_back(std::vector<std::wstring>());
					for (const auto& poh : pohui.second->Value())
					{
						auto val = poh.second->GetSrc();
						if (val.size() > 0)
						{
							if (val[0] == '\"')
								val.erase(val.begin());

							layers.back().push_back(val);
						}
					}
				}
				return layers;
			}

			virtual double ExtBoundaryPressure() { return UnitsConversionFactors::pressureConversion * GetValue(L"common_properties", L"external_bundary_pressure"); }
			virtual PhaseProperties WaterPhaseProperties()
			{
				return PhaseProperties(UnitsConversionFactors::viscosityConversion * GetValue(L"phase_properties", L"viscosity", L"0"),
					UnitsConversionFactors::densityConversion * GetValue(L"phase_properties", L"density", L"0"),
					UnitsConversionFactors::compressibilityConversion * GetValue(L"phase_properties", L"compressibility", L"0"),
					UnitsConversionFactors::saturationConversion * GetValue(L"phase_properties", L"residual", L"0"),
					UnitsConversionFactors::pressureConversion * GetValue(L"phase_properties", L"reference_pressure_for_compressibility", L"0"));
			}
			virtual PhaseProperties OilPhaseProperties()
			{
				return PhaseProperties(UnitsConversionFactors::viscosityConversion * GetValue(L"phase_properties", L"viscosity", L"1"),
					UnitsConversionFactors::densityConversion * GetValue(L"phase_properties", L"density", L"1"),
					UnitsConversionFactors::compressibilityConversion * GetValue(L"phase_properties", L"compressibility", L"1"),
					UnitsConversionFactors::saturationConversion * GetValue(L"phase_properties", L"residual", L"1"),
					UnitsConversionFactors::pressureConversion * GetValue(L"phase_properties", L"reference_pressure_for_compressibility", L"1"));
			}
			virtual double RequiredNewtonTol() { return GetValue(L"scheme_parameters", L"required_Newton_Tolerance"); }
			virtual double g() { return GetValue(L"common_properties", L"freeFall_acceleration"); }
			virtual double StartTimeStep() { return GetValue(L"scheme_parameters", L"initial_time_step"); }


			virtual double VelocityMultiplier() { return GetValue(L"trajectories", L"velocity_multiplier"); }
			virtual double InitialTrajectoryDistance()
			{
				auto val = GetValue(L"trajectories", L"start_distance");
				val = (val < 0.0) ? 0.5 : val;
				return val;
			}
			//		virtual double TrajectoryCount() { return GetValue(L"trajectories", L"number_of_trajectories_from_well"); }
			virtual double StartSignalRollbackTime() { return GetValue(L"trajectories", L"start_signal_rollback_time"); }
			virtual double EndSignalRollbackTime() { return GetValue(L"trajectories", L"end_signal_rollback_time"); }

			virtual int NewtonMaxIterCount() { return (int)GetValue(L"scheme_parameters", L"newton_max_iteration_count"); }
			virtual double AMG_RelTol() { return GetValue(L"scheme_parameters", L"AMG_RelTol"); }
			virtual double AMG_AbsTol() { return GetValue(L"scheme_parameters", L"AMG_AbsTol"); }
			virtual std::wstring WaterSaturation_fileName() { return folderName + GetLeave(L"save_files", L"water_saturation"); }
			virtual std::wstring OilSaturation_fileName() { return folderName + GetLeave(L"save_files", L"oil_saturation"); }
			virtual std::wstring Pressure_fileName() { return folderName + GetLeave(L"save_files", L"pressure"); }
			virtual std::wstring OverallBalance_fileName() { return folderName + GetLeave(L"save_files", L"overall_oil_balance"); }


			virtual double MinCellThickness() { return GetValue(L"common_properties", L"min_cell_thickness"); }
			virtual double MinPorosity() { return GetValue(L"common_properties", L"min_cell_porosity"); }
			virtual double MinPermeability() { return UnitsConversionFactors::permeabilityConversion * GetValue(L"common_properties", L"min_cell_permeability"); }

			std::wstring GetLeave(std::wstring section, std::wstring name)
			{
				auto val = values[section]->Value()[name]->GetSrc();
				return val;
			}
			std::wstring GetLeave(std::wstring section, std::wstring name, std::wstring id)
			{
				auto val = values[section]->Value()[id]->Value()[name]->GetSrc();
				return val;
			}

			double GetValue(std::wstring section, std::wstring name)
			{
				//		auto result = GetLeave(section, name);
				auto result0 = GetLeave(section, name);

				//auto xx = result.find(L'.');
				//if (xx != std::wstring::npos)
				//{
				//	result[xx] = L'.';
				//	auto res = stod(result);
				//	cout << res << endl;
				//	res = stod(result);
				//}

				auto res = stod(result0);


				return stod(result0);
			}
			double GetValue(std::wstring section, std::wstring name, std::wstring id)
			{

				auto result = GetLeave(section, name, id);





				return stod(result);
			}
		};
	} // configurators
} // reservoir_simulator

