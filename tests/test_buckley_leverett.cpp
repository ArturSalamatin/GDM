#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "buckley_leverett_analytical.h"
#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

TEST_CASE("BL analytical: fractional flow properties",
          "[buckley-leverett][analytical]") {
    constexpr double M = 4.3 / 2.0;

    REQUIRE_THAT(buckley_leverett::f_w(0.0, M), WithinAbs(0.0, 1e-15));
    REQUIRE_THAT(buckley_leverett::f_w(1.0, M), WithinAbs(1.0, 1e-15));

    double prev = 0.0;
    for (int i = 1; i <= 100; ++i) {
        double s = i / 100.0;
        double f = buckley_leverett::f_w(s, M);
        CHECK(f >= prev);
        prev = f;
    }

    double Swf = buckley_leverett::find_Swf(M);
    INFO("Swf = " << Swf);
    CHECK(Swf > 0.2);
    CHECK(Swf < 0.8);

    double lhs = buckley_leverett::df_w(Swf, M);
    double rhs = buckley_leverett::f_w(Swf, M) / Swf;
    REQUIRE_THAT(lhs, WithinRel(rhs, 1e-8));
}

TEST_CASE("BL analytical: derivative vs finite difference",
          "[buckley-leverett][analytical]") {
    constexpr double M = 4.3 / 2.0;
    constexpr double eps = 1e-7;

    for (double Sw = 0.1; Sw <= 0.9; Sw += 0.1) {
        double df_numerical = (buckley_leverett::f_w(Sw + eps, M)
                             - buckley_leverett::f_w(Sw - eps, M)) / (2 * eps);
        double df_analytical = buckley_leverett::df_w(Sw, M);
        REQUIRE_THAT(df_analytical, WithinRel(df_numerical, 1e-5));
    }
}

TEST_CASE("BL analytical: shock front Welge construction",
          "[buckley-leverett][analytical]") {
    constexpr double M = 4.3 / 2.0;
    double Swf = buckley_leverett::find_Swf(M);

    // Проверка: f(Swf)/Swf = f'(Swf) (касательная из начала координат)
    double slope_secant = buckley_leverett::f_w(Swf, M) / Swf;
    double slope_tangent = buckley_leverett::df_w(Swf, M);
    REQUIRE_THAT(slope_secant, WithinRel(slope_tangent, 1e-8));

    // Профиль: за фронтом S_w = 0, перед фронтом — монотонно
    // Выберем параметры, чтобы фронт оказался в середине домена
    double qt_test = 1.0, phi_test = 0.2, A_test = 1.0;
    double v_f = qt_test * slope_tangent / (phi_test * A_test);
    double t_test = 0.5 / v_f;  // фронт при x = 0.5

    std::vector<double> x_centers(100);
    for (size_t i = 0; i < 100; ++i) x_centers[i] = (i + 0.5) * 0.01;
    auto Sw = buckley_leverett::analytical_profile(
        x_centers, qt_test, phi_test, A_test, t_test, M);

    // До фронта (x < 0.5): S_w > Swf
    CHECK(Sw[0] >= Swf - 0.01);
    // После фронта (x > 0.5): S_w = 0
    CHECK(Sw[99] < 1e-10);
    // Монотонное убывание в зоне разрежения
    for (size_t i = 1; i < 100; ++i) {
        CHECK(Sw[i] <= Sw[i-1] + 1e-10);
    }
}

TEST_CASE("Well injection: basic well works without crash",
          "[buckley-leverett][wells]") {
    constexpr size_t Nx = 10;
    constexpr double L = 500.0, hy_cell = 50.0, hz = 10.0;
    constexpr double P_init_atm = 200.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, L, hy_cell, hz, 100.0, 0.2, P_init_atm, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = L / Nx;
    // Небольшая закачка воды в первую ячейку
    test_helpers::add_simple_well(sim, horizon,
        L"INJ", hx * 0.5, hy_cell * 0.5,
        0.0, -1000.0);  // -1000 кг/день закачки

    sim.Solve({0.0, 1.0});

    auto Sw = sim.GetWaterSaturationField();
    CHECK(Sw[0] >= 0.2);
    for (size_t i = 0; i < Sw.size(); ++i) {
        CHECK(Sw[i] >= -1e-6);
        CHECK(Sw[i] <= 1.0 + 1e-6);
    }
}

TEST_CASE("Well injection: Sw increases with water injection",
          "[buckley-leverett][wells]") {
    constexpr size_t Nx = 10;
    constexpr double L = 500.0, hy_cell = 50.0, hz = 10.0;
    constexpr double P_init_atm = 200.0;
    constexpr double Sw_init = 0.2;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, L, hy_cell, hz, 100.0, 0.2, P_init_atm, 1.0 - Sw_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = L / Nx;
    test_helpers::add_simple_well(sim, horizon,
        L"INJ", hx * 0.5, hy_cell * 0.5,
        0.0, -1000.0);

    sim.Solve({0.0, 10.0});

    auto Sw = sim.GetWaterSaturationField();
    CHECK(Sw[0] > Sw_init);
    for (size_t i = 0; i < Sw.size(); ++i) {
        CHECK(Sw[i] >= -1e-6);
        CHECK(Sw[i] <= 1.0 + 1e-6);
    }
}
