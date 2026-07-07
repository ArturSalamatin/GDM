#include "Wells.h"
#include <iostream>

namespace reservoir_simulator
{
	namespace wells
	{
			WellFixedProduction::WellFixedProduction() = default;
			WellFixedProduction::WellFixedProduction(const WellName& name, const WellName& guid,
				const WellPosition& intersectionCoords,
				std::unique_ptr<const mer_descriptor::MER_Data>&& mer,
				set_of_points::PerforationsOfWell perforationsOfWell,
				const std::vector<const cell::TwoPhaseFlowCell*>& cells_,
				const std::vector<size_t>& itsLocalIDs, double itsAppWellRadius
			) :
				SomeWell(name, guid, intersectionCoords, std::move(mer), perforationsOfWell, cells_, itsLocalIDs, itsAppWellRadius)
			{ }

			CellNumericalData WellFixedProduction::AddWellToMatrix(double nextTimeMoment)
			{
				UpdateWellState(nextTimeMoment);
				SetRefWellPressure();
				SetProductions();
				BalanceOil();
				// fill in the RHS of the linear problem
				for (size_t l = 0; l < NmbrOfOpenedCells(); ++l)
				{
					if (!isfinite(productions[l]) || !isfinite(F_Oil(l)))
						throw std::exception(std::string("well production is not determined. Date:" + std::to_string(nextTimeMoment)).c_str());

					double rhoO = CellDensityOil(l);
					double rhoW = CellDensityWater(l);

					rhsPerPerforation[l][0] = -rhoO * F_Oil(l) * productions[l];
					rhsPerPerforation[l][1] = -rhoW * (1 - F_Oil(l)) * productions[l];
				}

				// denominator for dP_well/dP_res: D = Σ ρ_mix(k)·PI(k)
				double D_pw = 0.0;
				for (size_t k = 0; k < NmbrOfOpenedCells(); k++)
				{
					double f = F_Oil(k);
					double rhoMix = CellDensityOil(k) * f + CellDensityWater(k) * (1 - f);
					D_pw += rhoMix * factor[k] * OverallMobility(k);
				}

				// fill in the matrix blocks of the linear problem
				for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
				{
					if (!isfinite(P_Reservoir(l)) || !isfinite(P_Well[l])
						|| !isfinite(OverallMobility(l)) || !isfinite(Derivative_F_Oil(l))
						|| !isfinite(factor[l]) || !isfinite(DerivativeOverallMobility(l)))
						throw std::runtime_error("well production is not determined. Date:" + std::to_string(nextTimeMoment));

					double dP = P_Reservoir(l) - P_Well[l];
					double rhoO = CellDensityOil(l);
					double rhoW = CellDensityWater(l);

					// derivative of OverallMobility with S_Water
					matrixBlockPerPerforation[l][0] += rhoO * factor[l] * F_Oil(l) * DerivativeOverallMobility(l) * dP;
					matrixBlockPerPerforation[l][2] += rhoW * factor[l] * (1 - F_Oil(l)) * DerivativeOverallMobility(l) * dP;
					// derivative of P_Reservoir (direct contribution)
					matrixBlockPerPerforation[l][1] += rhoO * factor[l] * F_Oil(l) * OverallMobility(l);
					matrixBlockPerPerforation[l][3] += rhoW * factor[l] * (1 - F_Oil(l)) * OverallMobility(l);

					// derivative of P_well w.r.t. P_res(l): dPw/dP = ρ_mix(l)·PI(l) / D
					if (D_pw > 0.0)
					{
						double f = F_Oil(l);
						double rhoMix = rhoO * f + rhoW * (1 - f);
						double dPw_dP = rhoMix * factor[l] * OverallMobility(l) / D_pw;
						matrixBlockPerPerforation[l][1] -= rhoO * factor[l] * F_Oil(l) * OverallMobility(l) * dPw_dP;
						matrixBlockPerPerforation[l][3] -= rhoW * factor[l] * (1 - F_Oil(l)) * OverallMobility(l) * dPw_dP;
					}

					// derivative of F_Oil
					if (dP > 0)
					{
						double temp = productions[l] * Derivative_F_Oil(l);
						matrixBlockPerPerforation[l][0] += rhoO * temp;
						matrixBlockPerPerforation[l][2] += -rhoW * temp;
					}
					else
					{
						// derivative of balanced oil. Let's try to take it from the previous iteration
					}
				}

				return { CurLocalIDs(), matrixBlockPerPerforation, rhsPerPerforation };
			}

			void WellFixedProduction::SetRefWellPressure()
			{
				double denom = 0, numer = -CurOverallDebit();
				for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
				{
					double f = F_Oil(l);
					double rhoMix = CellDensityOil(l) * f + CellDensityWater(l) * (1 - f);
					double temp = rhoMix * factor[l] * OverallMobility(l);
					denom += temp;
					numer += temp * P_Reservoir(l);
				}
				if (denom == 0.0) {
					double avg_P = 0.0;
					for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
						avg_P += P_Reservoir(l);
					if (NmbrOfOpenedCells() > 0) avg_P /= NmbrOfOpenedCells();
					SetWellPressure(avg_P);
					return;
				}
				double P = numer / denom;

				SetWellPressure(P);
			}
	} // wells
} // reservoir_simulator
