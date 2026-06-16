#pragma once


namespace reservoir_simulator
{
	/// <summary>
	/// Properties that are not classified
	/// </summary>
	struct OtherProperties
	{
		double g = 9.81; // m/s^2
		double extPressure = /*100/100 atm*/ 10132500/100; // Pa
	};


	class CustomUnitConverter
	{
		static constexpr double from_mPas_2_PaDay = 1.0 / 86400.0 / 1000;
	public:
		static double viscosityConverter()
		{ 
			return from_mPas_2_PaDay;
		}
	};


	/// <summary>
	/// Descriptor of phase properties
	/// </summary>
	template< typename unitConverter>
	struct PhaseProperties
	{
	public:

		PhaseProperties(
			double visc, double density,
			double compress, double resFrac, double refPressure) noexcept :
			viscosity{ visc* unitConverter::viscosityConverter()}, density{density},
			compressibility{ compress }, refPressure{ refPressure },
			residualFraction{ resFrac }
		{}
		double ReferencePressure() const { return refPressure; }
		double Compressibility() const { return compressibility; }
		double Viscosity() const { return viscosity; }
		double Density() const { return density; }
		double ResidualFraction() const { return residualFraction; }

		PhaseProperties() = default;
	protected:
		double refPressure = 1E5; // Pa
		double compressibility = 0; // Pa^{-1}
		double viscosity = 3.15; // mPa.s
		double density = 1000; // kg/m^3
		double residualFraction = 0.0; // 
	};

	using WaterPhaseProperty = PhaseProperties<CustomUnitConverter>;
	using OilPhaseProperty = PhaseProperties<CustomUnitConverter>;
} //reservoir_simulator