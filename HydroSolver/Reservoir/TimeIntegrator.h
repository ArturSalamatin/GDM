#pragma once
#include <vector>
#include <functional>

namespace reservoir_simulator {

class NumericalParameters;
struct SolverProfile;

class TimeIntegrator {
public:
	struct StepCallbacks {
		std::function<void(double nextRefMoment)> prepare_step;
		std::function<void(double loc_tau, double nextTime)> perform_newton;
		std::function<void(double loc_tau)> on_accept;
		std::function<void()> on_post_update;
	};

	double Integrate(
		const std::vector<double>& timeMoments,
		NumericalParameters& numPrm,
		SolverProfile& profile,
		const StepCallbacks& callbacks);
};

} // namespace reservoir_simulator
