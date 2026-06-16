#include <array>
#include "Trajectory.h"

namespace reservoir_simulator
{
	namespace phasePortrait
	{
		Direction f(const trPoint& p, SomeFlowField& field)
		{
			/*auto vxvy = field(p.t(), p.P());
			double V = sqrt(vxvy.first * vxvy.first + vxvy.second * vxvy.second);
			if (!isfinite(V))
				return { 0.0,0.0,0.0 };
			return { V , vxvy.first , vxvy.second };*/
			return f_const(p, field, p.t());
		}

		Direction f_const(const trPoint& p, SomeFlowField& field, double time)
		{
	//		std::cout << time << ' ' << std::flush;
			auto vxvy = field(time, p.P());
			double V = sqrt(vxvy.first * vxvy.first + vxvy.second * vxvy.second);
			if (!isfinite(V))
				return { 0.0,0.0,0.0 };
			return { V , vxvy.first , vxvy.second };
		}
	} // phasePortrait
} // reservoir_simulator