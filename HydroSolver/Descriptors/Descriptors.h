#pragma once
#include "../stdafx.h"
#include "../Solver/Grids/GridDescriptors.h"
#include "../Solver/Grids/PropertyDescriptor.h"


namespace reservoir_simulator
{
	/// <summary>
	/// Descriptor of numerical scheme parameters for Newton iterative solver, AMG-solver, and individual cell condctivity parameters
	/// </summary>
	struct SchemeParamaters
	{
	public:
		SchemeParamaters() = default;
		SchemeParamaters(
			double requiredNewtonTol, size_t newtonMaxIterCount,
			double amg_RelTol, double amg_AbsTol,
			double minCellThickness, double minPorosity, double minPermeability) noexcept;

		double RequiredNewtonTol() const;
		size_t NewtonMaxIterCount() const;
		double AMG_RelTol() const;
		double AMG_AbsTol() const;
		double MinCellThickness() const;
		double MinPorosity() const;
		double MinPermeability() const;

	protected:
		double requiredNewtonTol = 1E5; // Pa
		size_t newtonMaxIterCount = 0; // Pa^{-1}

		double amg_RelTol = 3.15; // mPa.s
		double amg_AbsTol = 1000; // kg/m^3

		double minCellThickness = 0.0; // 
		double minPorosity = 1000; // kg/m^3
		double minPermeability = 0.0; // 
	};

	/// <summary>
	/// Descriptor of anomaly detection engine
	/// </summary>
	struct AnomalyDetectionProperties
	{
		AnomalyDetectionProperties() = default;
		AnomalyDetectionProperties(
			double anomaly_detection_interval,
			double anomaly_date_start,
			double anomaly_date_end,
			double saturation_field_date,
			double veclocity_multiplier,
			size_t number_of_snapshots,
			double initTrajectoryDistance,
			double startSignalRollbackTime,
			double endSignalRollbackTime) noexcept;

		std::pair<double, double> PeriodOfInterest() const;
		size_t TimeDiscretization() const;
		double CurrentDate() const;
		double AnomalyDetectionInterval() const;
		double VelocityMultiplier() const;
		double InitialTrajectoryDistance() const;
		//		virtual double TrajectoryCount() = 0;
		double StartSignalRollbackTime() const;
		double EndSignalRollbackTime() const;

	protected:
		double anomaly_detection_interval;
		double anomaly_date_start;
		double anomaly_date_end;
		// the date for the saturation field ????
		double saturation_field_date;
		double veclocity_multiplier;

		size_t number_of_snapshots;

		double initTrajectoryDistance;
		double startSignalRollbackTime;
		double endSignalRollbackTime;
	};



	
	/// <summary>
	/// Maps DB layersID to 0,1,2,...
	/// </summary>
	using LayerAggregationTree = std::map<size_t, size_t>;

	
} // reservoir_simulator