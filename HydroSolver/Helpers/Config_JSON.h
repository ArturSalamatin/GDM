#pragma once
#include "Config.h"
#include "../Utils/JSON/JSONCreate.h"

namespace reservoir_simulator
{
	namespace configurators
	{
		class Config_JSON : public Config
		{
			typedef std::map<std::string, std::unique_ptr<JSON::IJObject>> JSONtree;

		protected:
			//	INIReader reader;
			JSONtree values;
			const std::string folderName = "MatLab\\";

		public:
			Config_JSON(std::string fileName) : Config()
			{
				if (fileName.back() != '\\')
					fileName += '\\';

				try {
					values = JSON::JSONParser(fileName + "config.json").Parse()->Value();

					anomaly_date_start = PathUtils::Utils::ConvertDateToExcelDate(GetLeave("temporal_parameters", "anomaly_date_start"));
					anomaly_date_end = PathUtils::Utils::ConvertDateToExcelDate(GetLeave("temporal_parameters", "anomaly_date_end"));
					saturation_field_date = PathUtils::Utils::ConvertDateToExcelDate(GetLeave("temporal_parameters", "saturation_field_date"));
					number_of_snapshots = (int)GetValue("temporal_parameters", "number_of_snapshots");
					anomaly_detection_interval = GetValue("anomaly", "anomaly_detection_interval");
					// at least two snapshopts must be saved, thus we can show the progress bar
					if (number_of_snapshots < 2)
					{
						number_of_snapshots = 2;
						LogFileSpace::LogFile::WriteLog("class_Config_JSON", "method_Config_JSON", "warning", "number_of_snapshots in config file is set to 2. Revise confg file.");
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
			virtual  std::string GetWideProjectPath() {
				auto result = GetLeave("model", "path");
				if (result.back() != '\\')
					result += '\\';
				return  result;
			}

			virtual std::vector<std::vector<std::string>> LayerAggregation()
			{
				auto val = values["model"]->Value()["layer_aggregation"]->Value();
				std::vector < std::vector < std::string>> layers;
				for (const auto& pohui : val)
				{
					layers.push_back(std::vector<std::string>());
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

			virtual double ExtBoundaryPressure() { return UnitsConversionFactors::pressureConversion * GetValue("common_properties", "external_bundary_pressure"); }
			virtual PhaseProperties WaterPhaseProperties()
			{
				return PhaseProperties(UnitsConversionFactors::viscosityConversion * GetValue("phase_properties", "viscosity", "0"),
					UnitsConversionFactors::densityConversion * GetValue("phase_properties", "density", "0"),
					UnitsConversionFactors::compressibilityConversion * GetValue("phase_properties", "compressibility", "0"),
					UnitsConversionFactors::saturationConversion * GetValue("phase_properties", "residual", "0"),
					UnitsConversionFactors::pressureConversion * GetValue("phase_properties", "reference_pressure_for_compressibility", "0"));
			}
			virtual PhaseProperties OilPhaseProperties()
			{
				return PhaseProperties(UnitsConversionFactors::viscosityConversion * GetValue("phase_properties", "viscosity", "1"),
					UnitsConversionFactors::densityConversion * GetValue("phase_properties", "density", "1"),
					UnitsConversionFactors::compressibilityConversion * GetValue("phase_properties", "compressibility", "1"),
					UnitsConversionFactors::saturationConversion * GetValue("phase_properties", "residual", "1"),
					UnitsConversionFactors::pressureConversion * GetValue("phase_properties", "reference_pressure_for_compressibility", "1"));
			}
			virtual double RequiredNewtonTol() { return GetValue("scheme_parameters", "required_Newton_Tolerance"); }
			virtual double g() { return GetValue("common_properties", "freeFall_acceleration"); }
			virtual double StartTimeStep() { return GetValue("scheme_parameters", "initial_time_step"); }


			virtual double VelocityMultiplier() { return GetValue("trajectories", "velocity_multiplier"); }
			virtual double InitialTrajectoryDistance()
			{
				auto val = GetValue("trajectories", "start_distance");
				val = (val < 0.0) ? 0.5 : val;
				return val;
			}
			//		virtual double TrajectoryCount() { return GetValue("trajectories", "number_of_trajectories_from_well"); }
			virtual double StartSignalRollbackTime() { return GetValue("trajectories", "start_signal_rollback_time"); }
			virtual double EndSignalRollbackTime() { return GetValue("trajectories", "end_signal_rollback_time"); }

			virtual int NewtonMaxIterCount() { return (int)GetValue("scheme_parameters", "newton_max_iteration_count"); }
			virtual double AMG_RelTol() { return GetValue("scheme_parameters", "AMG_RelTol"); }
			virtual double AMG_AbsTol() { return GetValue("scheme_parameters", "AMG_AbsTol"); }
			virtual std::string WaterSaturation_fileName() { return folderName + GetLeave("save_files", "water_saturation"); }
			virtual std::string OilSaturation_fileName() { return folderName + GetLeave("save_files", "oil_saturation"); }
			virtual std::string Pressure_fileName() { return folderName + GetLeave("save_files", "pressure"); }
			virtual std::string OverallBalance_fileName() { return folderName + GetLeave("save_files", "overall_oil_balance"); }


			virtual double MinCellThickness() { return GetValue("common_properties", "min_cell_thickness"); }
			virtual double MinPorosity() { return GetValue("common_properties", "min_cell_porosity"); }
			virtual double MinPermeability() { return UnitsConversionFactors::permeabilityConversion * GetValue("common_properties", "min_cell_permeability"); }

			std::string GetLeave(std::string section, std::string name)
			{
				auto val = values[section]->Value()[name]->GetSrc();
				return val;
			}
			std::string GetLeave(std::string section, std::string name, std::string id)
			{
				auto val = values[section]->Value()[id]->Value()[name]->GetSrc();
				return val;
			}

			double GetValue(std::string section, std::string name)
			{
				//		auto result = GetLeave(section, name);
				auto result0 = GetLeave(section, name);

				//auto xx = result.find(L'.');
				//if (xx != std::string::npos)
				//{
				//	result[xx] = L'.';
				//	auto res = stod(result);
				//	cout << res << endl;
				//	res = stod(result);
				//}

				auto res = stod(result0);


				return stod(result0);
			}
			double GetValue(std::string section, std::string name, std::string id)
			{

				auto result = GetLeave(section, name, id);





				return stod(result);
			}
		};
	} // configurators
} // reservoir_simulator

