#include "../stdafx.h"
#include <iomanip>
#include <sstream>
#include "NumericalParameters.h"
#include "Well/SomeWell.h"

namespace reservoir_simulator
{

	NumericalParameters::NumericalParameters(double nTol, size_t nCount, double amg_AbsTol, double amg_RelTol) noexcept
		: newtonTol(nTol), newtonMaxIterNmbr(nCount), AMG_AbsTol(amg_AbsTol), AMG_RelTol(amg_RelTol) {}
	NumericalParameters::NumericalParameters() noexcept { }

	void NumericalParameters::update_currentAMGState(const std::tuple<int, double, bool>& AMGstate)
	{
		AMG_currentIterationCount = std::get<0>(AMGstate);
		double newAMG_error = std::get<1>(AMGstate);
		update_isAMG_itertationSuccessfull(std::get<2>(AMGstate) && isfinite(newAMG_error));
		update_overallCurIterCount(CurrentAMG_IterationsCount());

		if (newAMG_error < CurrentAMG_Error()) {
			AMG_maxSolverIterAccum += 0.4;
			if (AMG_maxSolverIterAccum >= 1.0) {
				AMG_maxSolverIterCount += 1;
				AMG_maxSolverIterAccum -= 1.0;
			}
		}


		/*if (newAMG_error / CurrentAMG_Error() < 1.1 && CurrentAMG_maxSolverIterationCount() > 5.0)
			AMG_maxSolverIterCount -= 3;*/

		if (newAMG_error > 0.7)
			AMG_maxSolverIterCount = std::max(15, AMG_maxSolverIterCount);

		AMG_curError = newAMG_error;


		AMG_maxSolverIterCount = std::min(AMG_maxSolverIterCount, AMG_MAXSOLVERITERCOUNT);


#ifdef DEBUG_SALAMATIN
		std::cout 
			<< std::setw(5) << CurrentNewtonIterationCount()
			<< std::setw(5) << CurrentAMG_IterationsCount() 
			<< std::setw(15) << CurrentAMG_Error() 
			<< std::setw(5) << overallSolverIterationCount 
			<< std::endl;
#endif // DEBUG_SALAMATIN
	}


	void NumericalParameters::update_currentMoment() 
	{
		currentMoment += CurrentIntegrationStep();

#ifdef DEBUG_SALAMATIN
		std::cout << ">>> Just used cur_tau            = " << CurrentIntegrationStep() << " days" << std::endl;
#endif // DEBUG_SALAMATIN

		// increase the cur_tau value
		if (CurrentIntegrationStep() < CurrentTimeStepTillNextSaveMomemnt())
			increase_schemeTau();
#ifdef DEBUG_SALAMATIN
		std::cout << ">>> Continue at scheme mesh step = " << CurrentSchemeTau() << " days" << std::endl;
		std::cout << ">>> Current time: " << CurrentTimeMoment() << " days" << std::endl << std::endl;
#endif // DEBUG_SALAMATIN
	}

	void NumericalParameters::update_maxTauAllowed(double nextRefMoment, const std::map<WellName, wells::SomeWell*>& wells)
	{

		//	if (abs(nextRefMoment - currentMoment) < 1E-12)
		//		std::cout << "problem here\n";

		timeStepTillNextSaveMomemnt = nextRefMoment - currentMoment;

		//	std::vector<double> times1, times2;

		for (const auto& [name, well] : wells)
		{
			//		times1.push_back(well->NextProductionFrameStart(currentMoment));
			//		times2.push_back(well->NextWellJobInstance(currentMoment));

			timeStepTillNextSaveMomemnt =
				std::min(
					well->TimeToNextMomemnt(currentMoment),
					timeStepTillNextSaveMomemnt);
		}
		/*if (abs(timeStepTillNextSaveMomemnt) < 1E-12)
		{
			std::cout << "problem here: " << nextRefMoment << '\n';
			std::cout << "problem here: " << nextRefMoment << '\n';
		}*/
	}

	void NumericalParameters::increase_schemeTau() {
		if (CurrentAMG_Error() > 0.0) {
			if (use_pi_controller_) {
				double mult = pi_controller_.ComputeMultiplier(
					CurrentNewtonIterationCount(), true);
				schemeTau = CurrentIntegrationStep() * mult;
			} else {
				schemeTau = CurrentIntegrationStep() * (1 + 1 * factor);
			}
		}
	}
	void NumericalParameters::decrease_schemeTau() {
		if (use_pi_controller_) {
			double mult = pi_controller_.ComputeMultiplier(
				CurrentNewtonIterationCount(), false);
			schemeTau = CurrentIntegrationStep() * mult;
		} else {
			schemeTau = CurrentIntegrationStep() * (1 - 2 * factor);
		}
#ifdef DEBUG_SALAMATIN
		std::cout << ">>> RollBack with new cur_tau = " << CurrentSchemeTau() << " days" << std::endl;
#endif // DEBUG_SALAMATIN
		update_wastedTrialsCount();
		set_currentNewtonIterationCount(0);
	}

} // reservoir_simulator