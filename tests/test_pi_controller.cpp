#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/PIController.h"

using namespace reservoir_simulator;

TEST_CASE("PIController: fast convergence gives growth > 1", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(3, true);
    CHECK(mult > 1.0);
    CHECK(mult <= 2.0);
}

TEST_CASE("PIController: slow convergence gives shrink < 1", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(40, true);
    CHECK(mult < 1.0);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: at target mult ~= 1", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(12, true);
    CHECK(mult > 0.95);
    CHECK(mult < 1.05);
}

TEST_CASE("PIController: failure gives decrease", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(65, false);
    CHECK(mult < 0.5);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: I-term smooths after fast->slow", "[timestep][pi]") {
    PIController pi;
    pi.ComputeMultiplier(3, true);
    double m_pi = pi.ComputeMultiplier(40, true);

    PIController pi_p_only({.alpha = 0.7, .beta = 0.0});
    pi_p_only.ComputeMultiplier(3, true);
    double m_p = pi_p_only.ComputeMultiplier(40, true);

    CHECK(m_pi <= m_p + 0.01);
}

TEST_CASE("PIController: clamped to max_growth", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(1, true);
    CHECK(mult <= 2.0);
}

TEST_CASE("PIController: clamped to min_shrink", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(65, false);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: reset restores virgin state", "[timestep][pi]") {
    PIController pi;
    pi.ComputeMultiplier(40, true);
    pi.Reset();
    double mult = pi.ComputeMultiplier(3, true);

    PIController pi_fresh;
    double mult_fresh = pi_fresh.ComputeMultiplier(3, true);
    CHECK(mult == Catch::Approx(mult_fresh));
}

TEST_CASE("PIController: custom params", "[timestep][pi]") {
    PIControllerParams p{.alpha = 0.5, .beta = 0.3, .target_iters = 5,
                         .max_iters = 20, .safety = 0.9};
    PIController pi(p);
    double mult = pi.ComputeMultiplier(5, true);
    CHECK(mult > 0.8);
    CHECK(mult < 1.0);
}

TEST_CASE("PIController: steady state no drift", "[timestep][pi]") {
    PIController pi;
    for (int i = 0; i < 20; i++) {
        double mult = pi.ComputeMultiplier(12, true);
        CHECK(mult > 0.95);
        CHECK(mult < 1.05);
    }
}
