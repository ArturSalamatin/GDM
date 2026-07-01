#pragma once
#include <cmath>

#include "AbstractCells.h"

#undef min
#undef max

//using namespace reservoir_simulator::cell;

namespace reservoir_simulator
{
	namespace cell
	{
		class TwoPhaseFlowCell : public SomeProcessCell_TimeDependent<Dim3Cell>
		{
		protected:
			static const int permPower = 3;
			static const int nmbrOfConstantFieldProperties = 9;

			virtual void ApplyPhysicalConstraints();

			virtual void UpdateDependentFieldProperties()
			{
				ApplyPhysicalConstraints();

				DependentFieldProperties[0] = DensityOil(P());   //  DensityOil_Fixed()* (1 + CompressibilityOil() * (P() - P_Fixed())); // density of oil
				DependentFieldProperties[1] = DensityWater(P()); //  DensityWater_Fixed()* (1 + CompressibilityWater() * (P() - P_Fixed())); // density of water

				double oil_mobility = MobilityOil(); // mobility of oil, k*k/mu
													 //	double water_mobility = MobilityWater(); // mobility of water, k*k/mu
				DependentFieldProperties[2] = oil_mobility + MobilityWater(); // overall mobility, k*k/mu + k*k/mu

				if (MobilityOverall() > 0.0) {
					DependentFieldProperties[3] = oil_mobility / MobilityOverall();
					DependentFieldProperties[4] = 1 - F_Oil();
				} else {
					DependentFieldProperties[3] = 0.0;
					DependentFieldProperties[4] = 0.0;
				}

				double oil_volume = OilVolume(); // oil volume
				double water_volume = WaterVolume(); // water volume

				DependentFieldProperties[5] = oil_volume * DensityOil(); // oil mass
				DependentFieldProperties[6] = water_volume * DensityWater(); // water mass

				DependentFieldProperties[7] = Permeability() * DerivativeRelativePermeabilityOil() / ViscousityOil(); // derivative of oil mobility by SWaterScaled, d(k*k/mu)
				DependentFieldProperties[8] = Permeability() * DerivativeRelativePermeabilityWater() / ViscousityWater(); // derivative of water mobility, d(k*k/mu)

				if (MobilityOverall() > 0.0) {
					DependentFieldProperties[9] = (F_Water() * DerivativeMobilityOil() - F_Oil() * DerivativeMobilityWater()) / MobilityOverall();
				} else {
					DependentFieldProperties[9] = 0.0;
				}
			}
			
			virtual void SetPreviousStateDependentFieldProperties();
		public:

			const std::vector<double> PreviousState_Mass() const;
			const std::vector<double>& PreviousState_Mass_ref() const;

			// DependentFieldProperties
			double PermeabilityOil() const; // overall permeability of oil
			double PermeabilityWater() const; // overall permeability of water
			double MobilityOil() const;
			double MobilityWater() const;
			double DensityOil(double p) const;
			double DensityWater(double p) const;
			double OilVolume() const; // oil volume
			double WaterVolume() const; // water volume
			double DerivativeMassOilBySwater() const;
			double DerivativeMassWaterBySwater() const;
			double DerivativeMassOilByP() const;
			double DerivativeMassWaterByP() const;

			double ViscousityOil() const;
			double ViscousityWater() const;

			double DensityOil() const;
			double DensityWater() const;
			double MobilityOverall() const;
			double F_Oil() const; // d(f_Oil)/d(S_Water)
			double F_Water() const;

			double OilMass() const;
			double WaterMass() const;

			double DerivativeMobilityOil() const;
			double DerivativeMobilityWater() const;
			double Derivative_F_Oil() const;
			double Derivative_F_Water() const;


			double RelativePermeabilityOil() const;
			double RelativePermeabilityWater() const;
			double DerivativeRelativePermeabilityOil() const;
			double DerivativeRelativePermeabilityWater() const;

			// ConstantFieldProperties
			double Permeability() const;
			double Porosity() const;
			double SOil_Residual() const;
			double SWater_Residual() const;
			double MobilePorosity() const;
			double PoreVolume() const;
			double MobilePoreVolume() const;

			// ConstantPointProperties
			double ViscousityOil_Fixed() const;
			double ViscousityWater_Fixed() const;
			double DensityOil_Fixed() const;
			double DensityWater_Fixed() const;
			double GravityAcceleration() const;
			double CompressibilityOil() const;
			double CompressibilityWater() const;
			double P_Fixed() const;

			// PreviousState_VariableFieldProperties
			double SWaterScaled_PreviousState() const;

			// VariableFieldProperties
			double SWater_Scaled() const;
			double P() const;
			double SOil_Scaled() const;

			double SOil() const;
			double SWater() const;

			TwoPhaseFlowCell();
			TwoPhaseFlowCell(const std::vector<double>& center, const std::vector<double>& size,
				const std::vector<double>& constProp, const std::vector<double>& varProp);
		};
	}// cell
}// reservoir_simulator