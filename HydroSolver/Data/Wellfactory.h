#pragma once
#include "RawWellFactory.h"

namespace reservoir_simulator
{
	namespace factories
	{
		/// <summary>
		/// MER factory that accounts for phases 
		/// </summary>
		class MERFactory : public RawMERFactory
		{
		public:
			MERFactory(const RawWellFactory& well_factory);

		protected:
			OilPhaseProperty oil;
			WaterPhaseProperty water;
			
			void setOilVolumeInLayer(const OilPhaseProperty& oil,
				size_t layer_id);

			void setWaterVolumeInLayer(const WaterPhaseProperty& water,
				size_t layer_id);

		};

		/// <summary>
		/// WellFactory relates LayerAggregationTree with abstract well data from RawWellFactory
		/// </summary>
		class WellFactory : public RawWellFactory
		{
		public:
			WellFactory(ConnectionFactory& connection);

			const PerforationFactory& Perforations() const;
			const RawMERFactory& MER() const;

			WellJobsContainer jobsInLayer(size_t layer_id) const;

		protected:

			PerforationFactory perforations;
			MERFactory mer;
		};
	} // factories
} // reservoir_simulator