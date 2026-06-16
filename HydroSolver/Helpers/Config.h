#pragma once
#include <locale> 
#include <tuple>

#include "../Helpers/LogFile.h"
#include "../Utils/PathUtils.h"
#include "../Descriptors/Descriptors.h"

namespace reservoir_simulator
{
	struct UnitsConversionFactors
	{
#pragma region unitsConversionFactors
		// units conversion factors
		
		static constexpr double densityConversion = 1;
		static constexpr double viscosityConversion = (1E-3) / (24 * 3600); // from mPa.s to Pa.day (SI units)
//			const double permeabilityConversion = 0.9869E-15; // from mD to m^2 (SI units)
//			const double porosityConversion = 1;
		static constexpr double saturationConversion = 1;
		//			const double spatialConversion = 1;
		static constexpr double pressureConversion = 101325; // from atm to Pa
		static constexpr double compressibilityConversion = 1E-6; // from MPa^(-1) to Pa^(-1)
		static constexpr double permeabilityConversion = 0.9869E-15; // from mD to m^2 (SI units)
#pragma endregion // unitsConversionFactors
	};

	namespace configurators
	{
		/// <summary>
		/// Interface that sets up the work with phases
		/// </summary>
		struct IPhaseConfigurator : public reservoir_simulator::PhaseProperties
		{

		};

		struct IWaterPhaseConfigurator : public reservoir_simulator::PhaseProperties {};
		struct IOilPhaseConfigurator : public reservoir_simulator::PhaseProperties {};

		class Config : public AnomalyDetectionProperties, public SchemeParamaters
		{
		public:

			virtual  std::wstring GetWideProjectPath() = 0;
			virtual  std::string GetProjectPath()
			{
				std::wstring path = GetWideProjectPath();
				UniversalSCParser::CP1251Encoder enc;
				return enc.ToString(path);
			}
			virtual  std::wstring GetWideProject_FileName_Full(std::wstring projectName = L"Project.bop")
			{
				return GetWideProjectPath() + projectName;
			}
			virtual  std::string GetProject_FileName_Full(std::wstring projectName = L"Project.bop")
			{
				std::wstring fileName = GetWideProject_FileName_Full(projectName);
				UniversalSCParser::CP1251Encoder enc;
				return enc.ToString(fileName);
			}



			virtual double ExtBoundaryPressure() = 0;
			constexpr double g() { return 9.81; };

			PhaseProperties WaterPhaseProperties() { return waterPhaseProperties; };
			PhaseProperties OilPhaseProperties() { return oilPhaseProperties; };




			virtual double StartTimeStep() = 0;




			virtual std::wstring WaterSaturation_fileName() = 0;
			virtual std::wstring OilSaturation_fileName() = 0;
			virtual std::wstring Pressure_fileName() = 0;
			virtual std::wstring OverallBalance_fileName() = 0;



			virtual std::vector<std::vector<std::wstring>> LayerAggregation() = 0;
		protected:
			PhaseProperties waterPhaseProperties, oilPhaseProperties;
		};
	} // configurators

} // reservoir_simulator