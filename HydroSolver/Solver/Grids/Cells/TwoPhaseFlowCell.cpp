#include "TwoPhaseFlowCell.h"


namespace reservoir_simulator
{
	namespace cell
	{
		 void TwoPhaseFlowCell::ApplyPhysicalConstraints()
		{
			// 0 < S_Water_Scaled < 1
			//VariableFieldProperties[0] = std::min(1.0, std::max(0.0, SWater_Scaled()));
			VariableFieldProperties[0] = std::min(1.0, std::max(std::min(0.0, SWaterScaled_PreviousState()), SWater_Scaled()));
		}
		 void TwoPhaseFlowCell::SetPreviousStateDependentFieldProperties()
		{
			PreviousState_DependentFieldProperties[0] = OilMass();    // previous oil mass in the cell
			PreviousState_DependentFieldProperties[1] = WaterMass();  // previous water mass in the cell
		}
		 const std::vector<double> TwoPhaseFlowCell::PreviousState_Mass() const { return GetPreviousState_DependentFieldProperties(); }
		 const std::vector<double>& TwoPhaseFlowCell::PreviousState_Mass_ref() const { return GetPreviousState_DependentFieldProperties(); }

		// DependentFieldProperties

		 double TwoPhaseFlowCell::PermeabilityOil() const { return Permeability() * RelativePermeabilityOil(); }

		// overall permeability of oil

		 double TwoPhaseFlowCell::PermeabilityWater() const { return Permeability() * RelativePermeabilityWater(); }

		// overall permeability of water

		 double TwoPhaseFlowCell::MobilityOil() const { return Permeability() * RelativePermeabilityOil() / ViscousityOil(); }
		 double TwoPhaseFlowCell::MobilityWater() const { return Permeability() * RelativePermeabilityWater() / ViscousityWater(); }
		 double TwoPhaseFlowCell::DensityOil(double p) const { return DensityOil_Fixed() * (1 + CompressibilityOil() * (p - P_Fixed())); }
		 double TwoPhaseFlowCell::DensityWater(double p) const { return DensityWater_Fixed() * (1 + CompressibilityWater() * (p - P_Fixed())); }
		 double TwoPhaseFlowCell::OilVolume() const { return PoreVolume() * SOil(); }

		// oil volume

		 double TwoPhaseFlowCell::WaterVolume() const { return PoreVolume() * SWater(); }

		// water volume

		 double TwoPhaseFlowCell::DerivativeMassOilBySwater() const { return -MobilePoreVolume() * DensityOil(); }
		 double TwoPhaseFlowCell::DerivativeMassWaterBySwater() const { return MobilePoreVolume() * DensityWater(); }
		 double TwoPhaseFlowCell::DerivativeMassOilByP() const { return OilVolume() * DensityOil_Fixed() * CompressibilityOil(); }
		 double TwoPhaseFlowCell::DerivativeMassWaterByP() const { return WaterVolume() * DensityWater_Fixed() * CompressibilityWater(); }
		 double TwoPhaseFlowCell::ViscousityOil() const { return ViscousityOil_Fixed(); }
		 double TwoPhaseFlowCell::ViscousityWater() const { return ViscousityWater_Fixed(); }
		 double TwoPhaseFlowCell::DensityOil() const { return DependentFieldProperties[0]; }
		 double TwoPhaseFlowCell::DensityWater() const { return DependentFieldProperties[1]; }
		 double TwoPhaseFlowCell::MobilityOverall() const { return DependentFieldProperties[2]; }
		 double TwoPhaseFlowCell::F_Oil() const { return DependentFieldProperties[3]; }

		// d(f_Oil)/d(S_Water)

		 double TwoPhaseFlowCell::F_Water() const { return DependentFieldProperties[4]; }
		 double TwoPhaseFlowCell::OilMass() const { return DependentFieldProperties[5]; }
		 double TwoPhaseFlowCell::WaterMass() const { return DependentFieldProperties[6]; }
		 double TwoPhaseFlowCell::DerivativeMobilityOil() const { return DependentFieldProperties[7]; }
		 double TwoPhaseFlowCell::DerivativeMobilityWater() const { return DependentFieldProperties[8]; }
		 double TwoPhaseFlowCell::Derivative_F_Oil() const { return DependentFieldProperties[9]; }
		 double TwoPhaseFlowCell::Derivative_F_Water() const { return -Derivative_F_Oil(); }
		 double TwoPhaseFlowCell::RelativePermeabilityOil() const { return  pow(std::max(0.0, SOil_Scaled()), permPower); }
		 double TwoPhaseFlowCell::RelativePermeabilityWater() const { return pow(std::max(0.0, SWater_Scaled()), permPower); }
		 double TwoPhaseFlowCell::DerivativeRelativePermeabilityOil() const { return  -pow(std::max(0.0, SOil_Scaled()), permPower - 1) * permPower; }
		 double TwoPhaseFlowCell::DerivativeRelativePermeabilityWater() const { return pow(std::max(0.0, SWater_Scaled()), permPower - 1) * permPower; }

		// ConstantFieldProperties

		 double TwoPhaseFlowCell::Permeability() const { return ConstantFieldProperties[0]; }
		 double TwoPhaseFlowCell::Porosity() const { return ConstantFieldProperties[1]; }
		 double TwoPhaseFlowCell::SOil_Residual() const { return ConstantFieldProperties[2]; }
		 double TwoPhaseFlowCell::SWater_Residual() const { return ConstantFieldProperties[3]; }
		 double TwoPhaseFlowCell::MobilePorosity() const { return ConstantFieldProperties[6]; }
		 double TwoPhaseFlowCell::PoreVolume() const { return ConstantFieldProperties[7]; }
		 double TwoPhaseFlowCell::MobilePoreVolume() const { return ConstantFieldProperties[8]; }

		// ConstantPointProperties

		 double TwoPhaseFlowCell::ViscousityOil_Fixed() const { return ConstantPointProperties[0]; }
		 double TwoPhaseFlowCell::ViscousityWater_Fixed() const { return ConstantPointProperties[1]; }
		 double TwoPhaseFlowCell::DensityOil_Fixed() const { return ConstantPointProperties[2]; }
		 double TwoPhaseFlowCell::DensityWater_Fixed() const { return ConstantPointProperties[3]; }
		 double TwoPhaseFlowCell::GravityAcceleration() const { return ConstantPointProperties[4]; }
		 double TwoPhaseFlowCell::CompressibilityOil() const { return ConstantPointProperties[5]; }
		 double TwoPhaseFlowCell::CompressibilityWater() const { return ConstantPointProperties[6]; }
		 double TwoPhaseFlowCell::P_Fixed() const { return ConstantPointProperties[7]; }

		// PreviousState_VariableFieldProperties

		 double TwoPhaseFlowCell::SWaterScaled_PreviousState() const { return PreviousState_VariableFieldProperties[0]; }

		// VariableFieldProperties

		 double TwoPhaseFlowCell::SWater_Scaled() const { return VariableFieldProperties[0]; }
		 double TwoPhaseFlowCell::P() const { return VariableFieldProperties[1]; }
		 double TwoPhaseFlowCell::SOil_Scaled() const { return 1 - SWater_Scaled(); }
		 double TwoPhaseFlowCell::SOil() const { return SOil_Scaled() * (1 - SOil_Residual() - SWater_Residual()) + SOil_Residual(); }
		 double TwoPhaseFlowCell::SWater() const { return SWater_Scaled() * (1 - SOil_Residual() - SWater_Residual()) + SWater_Residual(); }
		 TwoPhaseFlowCell::TwoPhaseFlowCell() {}
		 TwoPhaseFlowCell::TwoPhaseFlowCell(
			 const std::vector<double>& center, 
			 const std::vector<double>& size, 
			 const std::vector<double>& constProp, 
			 const std::vector<double>& varProp) :
			SomeProcessCell_TimeDependent<Dim3Cell>(center, size, constProp, varProp)
		{
			ConstantFieldProperties.resize(nmbrOfConstantFieldProperties);
			PreviousState_DependentFieldProperties.resize(2);

			// scale the oil saturation
			PreviousState_VariableFieldProperties[0] = (PreviousState_VariableFieldProperties[0] - SWater_Residual()) / (1 - SOil_Residual() - SWater_Residual());
			VariableFieldProperties[0] = (VariableFieldProperties[0] - SWater_Residual()) / (1 - SOil_Residual() - SWater_Residual());
			// scale porosity
			ConstantFieldProperties[4] = SOil_Residual() / (1 - SOil_Residual() - SWater_Residual());
			ConstantFieldProperties[5] = SWater_Residual() / (1 - SOil_Residual() - SWater_Residual());
			ConstantFieldProperties[6] = Porosity() * (1 - SOil_Residual() - SWater_Residual());
			ConstantFieldProperties[7] = Porosity() * Volume();
			ConstantFieldProperties[8] = MobilePorosity() * Volume();

			UpdateDependentFieldProperties();
			SetPreviousStateDependentFieldProperties();
		}
		}
}