#pragma once
#include <limits>
#include <algorithm>
#include <map>

#include "../defines.h"
#include "PIController.h"

namespace reservoir_simulator
{
	namespace wells
	{
		class SomeWell;
	};


	class NumericalParameters
	{
	protected:
		static constexpr double factor = 0.15;
		static constexpr int AMG_MAXSOLVERITERCOUNT = 45;
		bool isSuccessfulTrial = true;

		double newtonTol = 1E-4;
		size_t currentNewtonIterationCount = 0;
		size_t overallSolverIterationCount = 0;
		size_t newtonMaxIterNmbr = 12; // maximum number of iterations in the Newton loop
		size_t curNmbrOfSolverIterations = 0;

		int AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT; // initial maximum number of iterations allowed in the AMG::Solver
		double AMG_maxSolverIterAccum = 0.0;
		double AMG_curError = 0.0;
		size_t AMG_currentIterationCount = 0;
		bool AMG_isIterationSuccessfull = true;

		double schemeTau = std::numeric_limits<double>::max();
		double timeStepTillNextSaveMomemnt = std::numeric_limits<double>::max();

		double currentMoment = 0.0;

		size_t wastedTrialsCount = 0;

		PIController pi_controller_{PIControllerParams{.max_iters = 65}};
		bool use_pi_controller_ = false;

	public:
		double AMG_RelTol = 1E-2;
		double AMG_AbsTol = 1E-2;

		double NewtonTol() const { return newtonTol; }
		double NextTimeMoment() const { return CurrentTimeMoment() + CurrentIntegrationStep(); }
		bool IsSuccessfullNewtonTrial() const { return isSuccessfulTrial; }
		bool IsNewtonIterationContinue() const
		{
			bool f = CurrentNewtonIterationCount() < MaxNewtonIterationNmbr();
			/*if (!f)
			{
				set_currentNewtonIterationCount(0);
				decrease_schemeTau();
			}*/

			return f;
		}
		bool IsSuccessfullAMG_Iteration() const { return AMG_isIterationSuccessfull; }
		size_t WastedTrialsCount() const { return wastedTrialsCount; }
		size_t MaxNewtonIterationNmbr() const { return newtonMaxIterNmbr; }
		double CurrentAMG_Error() const { return AMG_curError; }
		size_t CurrentAMG_maxSolverIterationCount() const { return static_cast<size_t>(AMG_maxSolverIterCount); }
		size_t CurrentAMG_IterationsCount() const { return AMG_currentIterationCount; }
		bool CurrentANG_IsAccuracyReached() const
		{
			return CurrentAMG_Error() <= AMG_RelTol
				|| CurrentAMG_IterationsCount() == 0;
		}
		double CurrentTimeMoment() const { return currentMoment; }
		/// <summary>
		/// in days
		/// </summary>
		/// <returns></returns>
		double CurrentIntegrationStep() const { return std::min(CurrentSchemeTau(), timeStepTillNextSaveMomemnt); }
		/// <summary>
		/// in days
		/// </summary>
		/// <returns></returns>
		double CurrentTimeStepTillNextSaveMomemnt() const { return timeStepTillNextSaveMomemnt; }
		/// <summary>
		/// in days
		/// </summary>
		/// <returns></returns>
		double CurrentSchemeTau() const { return schemeTau; }
		size_t CurrentNewtonIterationCount() const { return currentNewtonIterationCount; }

		void set_currentMoment(double curMom) { currentMoment = curMom; }
		void set_initial_schemeTau(double tau_) { schemeTau = tau_; }
		void set_currentAMG_Error(double err) { AMG_curError = err; }
		void set_currentNewtonIterationCount(size_t curNewtonIter) { currentNewtonIterationCount = curNewtonIter; }
		void set_currentAMG_maxSolverIterationCount() { AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT; AMG_maxSolverIterAccum = 0.0; }
		void increase_schemeTau();
		void decrease_schemeTau();
		void update_wastedTrialsCount() { wastedTrialsCount++; }
		void update_overallCurIterCount(size_t increment) { overallSolverIterationCount += increment; }
		void update_maxTauAllowed(double nextRefMoment, const std::map<WellName, wells::SomeWell*>& wells);
		void update_currentMoment();
		void update_currentIntegrationTau(double tau) {}
		void update_currentNewtonIterationCount() { ++currentNewtonIterationCount; }
		void update_currentAMGState(const std::tuple<int, double, bool>& AMGstate);
		void update_isSuccesfullNewtonTrial(bool f) { isSuccessfulTrial = f && CurrentANG_IsAccuracyReached(); update_currentNewtonIterationCount(); }
		void update_isAMG_itertationSuccessfull(bool f) { AMG_isIterationSuccessfull = f; }

		void SetPIControllerParams(PIControllerParams p) {
			p.max_iters = newtonMaxIterNmbr;
			pi_controller_ = PIController(p);
		}
		void SetUsePIController(bool f) { use_pi_controller_ = f; }
		bool UsesPIController() const { return use_pi_controller_; }

	public:

		NumericalParameters(double nTol, size_t nCount, double amg_AbsTol, double amg_RelTol) noexcept;
		NumericalParameters() noexcept;
	};

} // reservoir_simulator