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
					//cout << "<<< production is not determined. Well " << NameString() << " Date " << std::to_string(CurrentTime()) << endl;

					rhsPerPerforation[l][0] = -F_Oil(l) * productions[l];
					rhsPerPerforation[l][1] = -(1 - F_Oil(l)) * productions[l];
				}

				// fill in the matrix blocks of the linear problem
				for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
				{
					if (!isfinite(P_Reservoir(l)) || !isfinite(P_Well[l])
						|| !isfinite(OverallMobility(l)) || !isfinite(Derivative_F_Oil(l))
						|| !isfinite(factor[l]) || !isfinite(DerivativeOverallMobility(l)))
						throw std::runtime_error("well production is not determined. Date:" + std::to_string(nextTimeMoment));
					//cout << "<<< production is not determined. Well " << NameString() << " Date " << std::to_string(CurrentTime()) << endl;

					double dP = P_Reservoir(l) - P_Well[l];

					// derivative of OverallMobility
					// with S_Water
					matrixBlockPerPerforation[l][0] += factor[l] * F_Oil(l) * DerivativeOverallMobility(l) * dP;
					matrixBlockPerPerforation[l][2] += factor[l] * (1 - F_Oil(l)) * DerivativeOverallMobility(l) * dP;
					// derivative of P_Reservoir
					matrixBlockPerPerforation[l][1] += factor[l] * F_Oil(l) * OverallMobility(l);
					matrixBlockPerPerforation[l][3] += factor[l] * (1 - F_Oil(l)) * OverallMobility(l);

					// derivative of P_Well
					/////!!!!!!!!!!!////////////

					// derivative of F_Oil
					if (dP > 0)
					{
						double temp = productions[l] * Derivative_F_Oil(l);
						matrixBlockPerPerforation[l][0] += temp;
						matrixBlockPerPerforation[l][2] += -temp;
					}
					else
					{
						// derivative of balanced oil. Let's try to take it from the previous iteration
					}
				}

				
		/*		std::cout
					<< "Well: " << Name() << '\n'
					<< "Overall debit:    " << CurOverallDebit() << '\n';*/


				return { CurLocalIDs(), matrixBlockPerPerforation, rhsPerPerforation };
			}

			void WellFixedProduction::SetRefWellPressure()
			{
				double denom = 0, numer = -CurOverallDebit();
				for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
				{
					double temp = factor[l] * OverallMobility(l);
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
