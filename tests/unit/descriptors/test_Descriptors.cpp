#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Descriptors/Descriptors.h"

using namespace reservoir_simulator;
using Catch::Approx;

// --- SchemeParamaters ---

TEST_CASE("SchemeParamaters: default constructor uses declared defaults",
          "[unit][level1][descriptors][SchemeParamaters]") {
    SchemeParamaters sp;
    CHECK(sp.RequiredNewtonTol() == Approx(1E5));
    CHECK(sp.NewtonMaxIterCount() == 0);
    CHECK(sp.AMG_RelTol() == Approx(3.15));
    CHECK(sp.AMG_AbsTol() == Approx(1000.0));
    CHECK(sp.MinCellThickness() == Approx(0.0));
    CHECK(sp.MinPorosity() == Approx(1000.0));
    CHECK(sp.MinPermeability() == Approx(0.0));
}

TEST_CASE("SchemeParamaters: parameterized constructor stores all values",
          "[unit][level1][descriptors][SchemeParamaters]") {
    SchemeParamaters sp(1e-3, 50, 1e-6, 1e-8, 0.5, 0.01, 1e-15);
    CHECK(sp.RequiredNewtonTol() == Approx(1e-3));
    CHECK(sp.NewtonMaxIterCount() == 50);
    CHECK(sp.AMG_RelTol() == Approx(1e-6));
    CHECK(sp.AMG_AbsTol() == Approx(1e-8));
    CHECK(sp.MinCellThickness() == Approx(0.5));
    CHECK(sp.MinPorosity() == Approx(0.01));
    CHECK(sp.MinPermeability() == Approx(1e-15));
}

// --- AnomalyDetectionProperties ---

TEST_CASE("AnomalyDetectionProperties: parameterized constructor and getters",
          "[unit][level1][descriptors][AnomalyDetectionProperties]") {
    AnomalyDetectionProperties adp(
        30.0,    // anomaly_detection_interval
        100.0,   // anomaly_date_start
        500.0,   // anomaly_date_end
        200.0,   // saturation_field_date
        2.5,     // velocity_multiplier
        10,      // number_of_snapshots
        5.0,     // initTrajectoryDistance
        50.0,    // startSignalRollbackTime
        150.0    // endSignalRollbackTime
    );

    CHECK(adp.AnomalyDetectionInterval() == Approx(30.0));
    auto period = adp.PeriodOfInterest();
    CHECK(period.first == Approx(100.0));
    CHECK(period.second == Approx(500.0));
    CHECK(adp.CurrentDate() == Approx(200.0));
    CHECK(adp.VelocityMultiplier() == Approx(2.5));
    CHECK(adp.TimeDiscretization() == 10);
    CHECK(adp.InitialTrajectoryDistance() == Approx(5.0));
    CHECK(adp.StartSignalRollbackTime() == Approx(50.0));
    CHECK(adp.EndSignalRollbackTime() == Approx(150.0));
}
