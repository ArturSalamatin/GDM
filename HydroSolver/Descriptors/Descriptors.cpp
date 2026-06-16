#include "Descriptors.h"

namespace reservoir_simulator
{
	SchemeParamaters::SchemeParamaters(
		double requiredNewtonTol, size_t newtonMaxIterCount,
		double amg_RelTol, double amg_AbsTol,
		double minCellThickness, double minPorosity, double minPermeability) noexcept :
		requiredNewtonTol{ requiredNewtonTol }, newtonMaxIterCount{ newtonMaxIterCount },
		amg_RelTol{ amg_RelTol }, amg_AbsTol{ amg_AbsTol },
		minCellThickness{ minCellThickness }, minPorosity{ minPorosity }, minPermeability{ minPermeability }
	{}
	double SchemeParamaters::RequiredNewtonTol() const { return requiredNewtonTol; };
	size_t SchemeParamaters::NewtonMaxIterCount() const { return newtonMaxIterCount; };
	double SchemeParamaters::AMG_RelTol() const { return amg_RelTol; };
	double SchemeParamaters::AMG_AbsTol() const { return amg_AbsTol; };
	double SchemeParamaters::MinCellThickness() const { return minCellThickness; };
	double SchemeParamaters::MinPorosity() const { return minPorosity; };
	double SchemeParamaters::MinPermeability() const { return minPermeability; };



	AnomalyDetectionProperties::AnomalyDetectionProperties(
		double anomaly_detection_interval, double anomaly_date_start, double anomaly_date_end,
		double saturation_field_date, double veclocity_multiplier,
		size_t number_of_snapshots,
		double initTrajectoryDistance,
		double startSignalRollbackTime, double endSignalRollbackTime) noexcept :
		anomaly_detection_interval{ anomaly_detection_interval }, anomaly_date_start{ anomaly_date_start }, anomaly_date_end{ anomaly_date_end },
		saturation_field_date{ saturation_field_date },
		veclocity_multiplier{ veclocity_multiplier },
		number_of_snapshots{ number_of_snapshots },
		initTrajectoryDistance{ initTrajectoryDistance },
		startSignalRollbackTime{ startSignalRollbackTime },
		endSignalRollbackTime{ endSignalRollbackTime }
	{}

	std::pair<double, double> AnomalyDetectionProperties::PeriodOfInterest() const
	{
		return std::make_pair(anomaly_date_start, anomaly_date_end);
	}
	size_t AnomalyDetectionProperties::TimeDiscretization() const { return  number_of_snapshots; }
	double AnomalyDetectionProperties::CurrentDate() const { return saturation_field_date; }
	double AnomalyDetectionProperties::AnomalyDetectionInterval() const { return anomaly_detection_interval; }
	double AnomalyDetectionProperties::VelocityMultiplier() const { return veclocity_multiplier; };
	double AnomalyDetectionProperties::InitialTrajectoryDistance() const { return initTrajectoryDistance; };
	double AnomalyDetectionProperties::StartSignalRollbackTime() const { return startSignalRollbackTime; };
	double AnomalyDetectionProperties::EndSignalRollbackTime() const { return endSignalRollbackTime; };
} // reservoir_simulator