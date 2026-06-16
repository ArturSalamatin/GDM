#pragma once
#include "../Descriptors/Descriptors.h"


namespace reservoir_simulator
{
	namespace factories
	{
	//	template<typename Conversion>
		class PhaseFactory
		{
		public:
			static WaterPhaseProperty CreateDefaultWater()
			{
				return WaterPhaseProperty{2.0, 1000, 0.0 /*0.0001*/, 0.0, 1.0};
			}
			static OilPhaseProperty CreateDefaultOil()
			{
				return OilPhaseProperty{ 4.3, 800, 0.0 /*0.001*/, 0.0, 1.0 };
			}
		};

		//	template<typename Conversion>
		class OtherFactory
		{
		public:
			static OtherProperties CreateDefaultOthers()
			{
				return OtherProperties{ /*9.81, 10132500/20.0*/ };
			}
		};
	}
} // reservoir_simulator
