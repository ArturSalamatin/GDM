#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/NumericalParameters.h"

using namespace reservoir_simulator;
using Catch::Approx;

TEST_CASE("NumericalParameters: default constructor",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    CHECK(np.CurrentTimeMoment() == Approx(0.0));
    CHECK(np.CurrentNewtonIterationCount() == 0);
    CHECK(np.IsSuccessfullNewtonTrial());
    CHECK(np.WastedTrialsCount() == 0);
}

TEST_CASE("NumericalParameters: parameterized constructor stores tolerances",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np(1e-6, 20, 1e-4, 1e-3);
    CHECK(np.NewtonTol() == Approx(1e-6));
    CHECK(np.MaxNewtonIterationNmbr() == 20);
    CHECK(np.AMG_AbsTol == Approx(1e-4));
    CHECK(np.AMG_RelTol == Approx(1e-3));
}

TEST_CASE("NumericalParameters: set_initial_schemeTau and CurrentSchemeTau",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    CHECK(np.CurrentSchemeTau() == Approx(10.0));
}

TEST_CASE("NumericalParameters: increase_schemeTau grows by factor",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentAMG_Error(0.5);
    double tau_before = np.CurrentSchemeTau();
    np.increase_schemeTau();
    CHECK(np.CurrentSchemeTau() > tau_before);
}

TEST_CASE("NumericalParameters: decrease_schemeTau shrinks by factor",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    double tau_before = np.CurrentSchemeTau();
    np.decrease_schemeTau();
    CHECK(np.CurrentSchemeTau() < tau_before);
    CHECK(np.WastedTrialsCount() == 1);
}

TEST_CASE("NumericalParameters: SetUsePIController toggles PI mode",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    CHECK_FALSE(np.UsesPIController());
    np.SetUsePIController(true);
    CHECK(np.UsesPIController());
}

TEST_CASE("NumericalParameters: update_currentNewtonIterationCount increments",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    CHECK(np.CurrentNewtonIterationCount() == 0);
    np.update_currentNewtonIterationCount();
    CHECK(np.CurrentNewtonIterationCount() == 1);
    np.update_currentNewtonIterationCount();
    CHECK(np.CurrentNewtonIterationCount() == 2);
}

TEST_CASE("NumericalParameters: IsNewtonIterationContinue respects max",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np(1e-4, 3, 1e-2, 1e-2);
    CHECK(np.IsNewtonIterationContinue());
    np.update_currentNewtonIterationCount();
    np.update_currentNewtonIterationCount();
    np.update_currentNewtonIterationCount();
    CHECK_FALSE(np.IsNewtonIterationContinue());
}

TEST_CASE("NumericalParameters: PI controller increase_schemeTau path",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentAMG_Error(0.5);
    np.SetUsePIController(true);
    np.set_currentNewtonIterationCount(3);
    double tau_before = np.CurrentSchemeTau();
    np.increase_schemeTau();
    CHECK(np.CurrentSchemeTau() > tau_before);
}
