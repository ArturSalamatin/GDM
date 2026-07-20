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
    CHECK(np.UsesPIController());
    np.SetUsePIController(false);
    CHECK_FALSE(np.UsesPIController());
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

struct TestableNumericalParameters : NumericalParameters {
    void set_AMG_maxSolverIterCount(size_t val) { AMG_maxSolverIterCount = val; }
    size_t get_AMG_maxSolverIterCount() const { return AMG_maxSolverIterCount; }
    double get_AMG_maxSolverIterAccum() const { return AMG_maxSolverIterAccum; }
};

TEST_CASE("NumericalParameters: AMG iter count adaptation with accumulator",
          "[unit][level2][reservoir][NumericalParameters]") {
    TestableNumericalParameters np;

    CHECK(np.CurrentAMG_maxSolverIterationCount() == 45);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.0));

    np.set_AMG_maxSolverIterCount(20);
    CHECK(np.CurrentAMG_maxSolverIterationCount() == 20);

    np.set_currentAMG_Error(1.0);
    np.update_currentAMGState({10, 0.5, true});
    CHECK(np.get_AMG_maxSolverIterCount() == 20);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.4));

    np.set_currentAMG_Error(0.5);
    np.update_currentAMGState({10, 0.3, true});
    CHECK(np.get_AMG_maxSolverIterCount() == 20);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.8));

    np.set_currentAMG_Error(0.3);
    np.update_currentAMGState({10, 0.2, true});
    CHECK(np.get_AMG_maxSolverIterCount() == 21);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.2));

    np.set_AMG_maxSolverIterCount(45);
    np.set_currentAMG_Error(1.0);
    np.update_currentAMGState({10, 0.5, true});
    CHECK(np.get_AMG_maxSolverIterCount() == 45);

    np.set_AMG_maxSolverIterCount(10);
    np.set_currentAMG_Error(0.0);
    np.update_currentAMGState({10, 0.8, true});
    CHECK(np.get_AMG_maxSolverIterCount() == 15);

    np.set_currentAMG_maxSolverIterationCount();
    CHECK(np.CurrentAMG_maxSolverIterationCount() == 45);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.0));
}

TEST_CASE("NumericalParameters: TimestepLog records accepted steps",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentMoment(0.0);
    np.set_currentAMG_Error(0.5);
    np.set_currentNewtonIterationCount(5);

    np.update_currentMoment();

    auto& log = np.TimestepLog();
    REQUIRE(log.size() == 1);
    CHECK(log[0].time == 0.0);
    CHECK(log[0].dt == 10.0);
    CHECK(log[0].newton_iters == 5);
    CHECK(log[0].accepted == true);
}

TEST_CASE("NumericalParameters: TimestepLog records wasted trials",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentMoment(0.0);
    np.set_currentAMG_Error(0.5);
    np.set_currentNewtonIterationCount(100);

    np.decrease_schemeTau();

    auto& log = np.TimestepLog();
    REQUIRE(log.size() == 1);
    CHECK(log[0].accepted == false);
}
