#pragma once
#include "../Solver/Grids/DevelopedHorizon.h"
#include "ReservoirFactory.h"
#include "WellFactory.h"
#include "PhaseFactory.hpp"

namespace reservoir_simulator
{
	namespace factories
	{
		class HorizonFactory
		{
		public:
		//	HorizonFactory(RawReservoirFactory* const  reservoir_factory,
		//		WellFactory* const well_fatory) noexcept;

			HorizonFactory(const std::string& connection);

			RawHorizon createSingleLayerRawHorizon(
				size_t layer_id);
			DevelopedHorizon createSingleLayerHorizon(
				size_t layer_id);





		protected:
			std::shared_ptr<const RawReservoirFactory> reservoir_factory;
			std::shared_ptr<const WellFactory>  well_factory;
			std::shared_ptr<const PhaseFactory> phase_factory;
			std::shared_ptr<const OtherFactory> other_factory;
		};

	} // factories
} // reservoir_simulator
