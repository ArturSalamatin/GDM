#pragma once

namespace reservoir_simulator
{
	class ReservoirSimulator;
	class DevelopedHorizon;

	class CalculationManager
	{
		size_t frames;

		std::unique_ptr<reservoir_simulator::ReservoirSimulator> simulator;
	public:
		CalculationManager();
		CalculationManager(size_t frames);
		void Run(double start, double end);
	};
} // reservoir_simulator