#include "TimeIntegrator.h"
#include "NumericalParameters.h"
#include "ReservoirSImulator.h"

namespace reservoir_simulator {

double TimeIntegrator::Integrate(
	const std::vector<double>& timeMoments,
	NumericalParameters& numPrm,
	SolverProfile& profile,
	const StepCallbacks& callbacks)
{
	for (size_t i = 1; i < timeMoments.size(); i++)
	{
		while (numPrm.CurrentTimeMoment() < timeMoments[i])
		{
			callbacks.prepare_step(timeMoments[i]);

			callbacks.perform_newton(
				numPrm.CurrentIntegrationStep(),
				numPrm.NextTimeMoment());

			if (numPrm.IsSuccessfullNewtonTrial())
			{
				callbacks.on_accept(numPrm.CurrentIntegrationStep());
				numPrm.update_currentMoment();
				callbacks.on_post_update();
				profile.n_time_steps++;
			}
			else
			{
				numPrm.decrease_schemeTau();
				profile.n_wasted_trials++;
			}
		}
	}
	return numPrm.CurrentSchemeTau();
}

} // namespace reservoir_simulator
