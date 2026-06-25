#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/PIController.h"

using namespace reservoir_simulator;
using Catch::Approx;

TEST_CASE("PIController: fast convergence gives growth > 1",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(3, true);
    CHECK(mult > 1.0);
    CHECK(mult <= 2.0);
}

TEST_CASE("PIController: slow convergence gives shrink < 1",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(40, true);
    CHECK(mult < 1.0);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: at target iters multiplier ~= 1",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(12, true);
    CHECK(mult > 0.95);
    CHECK(mult < 1.05);
}

TEST_CASE("PIController: failure gives decrease near min_shrink",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(65, false);
    CHECK(mult < 0.5);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: I-term smooths after fast->slow transition",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    pi.ComputeMultiplier(3, true);
    double m_pi = pi.ComputeMultiplier(40, true);

    PIController pi_p_only({.alpha = 0.7, .beta = 0.0});
    pi_p_only.ComputeMultiplier(3, true);
    double m_p = pi_p_only.ComputeMultiplier(40, true);

    CHECK(m_pi <= m_p + 0.01);
}

TEST_CASE("PIController: clamped to max_growth",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(1, true);
    CHECK(mult <= 2.0);
}

TEST_CASE("PIController: clamped to min_shrink",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(65, false);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: Reset restores virgin state",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    pi.ComputeMultiplier(40, true);
    pi.Reset();
    double mult = pi.ComputeMultiplier(3, true);

    PIController pi_fresh;
    double mult_fresh = pi_fresh.ComputeMultiplier(3, true);
    CHECK(mult == Approx(mult_fresh));
}

TEST_CASE("PIController: custom params stored via Params()",
          "[unit][level0][reservoir][PIController]") {
    PIControllerParams p{.alpha = 0.5, .beta = 0.3, .target_iters = 5,
                         .max_iters = 20, .safety = 0.9};
    PIController pi(p);
    CHECK(pi.Params().alpha == 0.5);
    CHECK(pi.Params().beta == 0.3);
    CHECK(pi.Params().target_iters == 5);
    CHECK(pi.Params().max_iters == 20);
    CHECK(pi.Params().safety == 0.9);

    double mult = pi.ComputeMultiplier(5, true);
    CHECK(mult > 0.8);
    CHECK(mult < 1.0);
}

TEST_CASE("PIController: steady state at target shows no drift",
          "[unit][level0][reservoir][PIController]") {
    PIController pi;
    for (int i = 0; i < 20; i++) {
        double mult = pi.ComputeMultiplier(12, true);
        CHECK(mult > 0.95);
        CHECK(mult < 1.05);
    }
}
