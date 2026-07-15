#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "buckley_leverett_analytical.h"
#include <cmath>
#include <fstream>

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
        "INJ", hx * 0.5, hy_cell * 0.5,
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
        "INJ", hx * 0.5, hy_cell * 0.5,
        0.0, -1000.0);

    sim.Solve({0.0, 10.0});

    auto Sw = sim.GetWaterSaturationField();
    CHECK(Sw[0] > Sw_init);
    for (size_t i = 0; i < Sw.size(); ++i) {
        CHECK(Sw[i] >= -1e-6);
        CHECK(Sw[i] <= 1.0 + 1e-6);
    }
}

namespace {

double compute_BL_L2(size_t Nx,
                     double Lx, double hy, double hz,
                     double perm_mD, double poro,
                     double P_init_atm, double So_init,
                     double water_inject_rate, double oil_prod_rate,
                     double t_final, double M) {
    double Sw_init = 1.0 - So_init;
    double A = hy * hz;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    double r_app = std::max(0.2 * hx, 0.2);
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 0.5, 0.0, water_inject_rate, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx - hx * 0.5, hy * 0.5, oil_prod_rate, 0.0, r_app);

    sim.Solve({0.0, t_final});

    auto Sw_gdm = sim.GetWaterSaturationField();
    if (Sw_gdm.size() != Nx) return -1.0;

    double integral_dSw = 0.0;
    for (size_t i = 0; i < Nx; ++i)
        integral_dSw += (Sw_gdm[i] - Sw_init) * hx;
    double qt_eff = integral_dSw * poro / t_final;
    if (qt_eff <= 0.0) return -1.0;

    double fw_init = buckley_leverett::f_w(Sw_init, M);
    double Swf;
    {
        double lo = Sw_init + 0.01, hi = 0.99;
        for (int iter = 0; iter < 100; ++iter) {
            double mid = 0.5 * (lo + hi);
            double secant = (buckley_leverett::f_w(mid, M) - fw_init) / (mid - Sw_init);
            double tangent = buckley_leverett::df_w(mid, M);
            if (tangent > secant) lo = mid;
            else hi = mid;
        }
        Swf = 0.5 * (lo + hi);
    }
    double slope_front = (buckley_leverett::f_w(Swf, M) - fw_init) / (Swf - Sw_init);
    double v_front = qt_eff * slope_front / (poro * A);
    double x_front = v_front * t_final;

    double sum_sq = 0.0;
    for (size_t i = 1; i + 1 < Nx; ++i) {
        double x = (i + 0.5) * hx;
        double Sw_ana;
        if (x >= x_front) {
            Sw_ana = Sw_init;
        } else {
            double target = x * poro * A / (qt_eff * t_final);
            double lo = Swf, hi = 1.0 - 1e-10;
            for (int iter = 0; iter < 100; ++iter) {
                double mid = 0.5 * (lo + hi);
                if (buckley_leverett::df_w(mid, M) > target) lo = mid;
                else hi = mid;
            }
            Sw_ana = 0.5 * (lo + hi);
        }
        double diff = Sw_gdm[i] - Sw_ana;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq * hx / Lx);
}

double compute_qt_eff(size_t Nx,
                      double Lx, double hy, double hz,
                      double perm_mD, double poro,
                      double P_init_atm, double So_init,
                      double water_inject_rate, double oil_prod_rate,
                      double t_final) {
    double Sw_init = 1.0 - So_init;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    double r_app = std::max(0.2 * hx, 0.2);
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 0.5, 0.0, water_inject_rate, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx - hx * 0.5, hy * 0.5, oil_prod_rate, 0.0, r_app);

    sim.Solve({0.0, t_final});

    auto Sw_gdm = sim.GetWaterSaturationField();
    if (Sw_gdm.size() != Nx) return -1.0;

    double integral_dSw = 0.0;
    for (size_t i = 0; i < Nx; ++i)
        integral_dSw += (Sw_gdm[i] - Sw_init) * hx;
    return integral_dSw * poro / t_final;
}

} // namespace

TEST_CASE("BL validation: grid convergence of Sw profile",
          "[buckley-leverett][validation]") {
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 0.8;
    constexpr double t_final = 400.0;
    constexpr double M = 4.3 / 2.0;

    constexpr size_t grids[] = {25, 50, 100, 200};
    constexpr size_t N = sizeof(grids) / sizeof(grids[0]);
    double L2[N];

    for (size_t g = 0; g < N; ++g) {
        L2[g] = compute_BL_L2(grids[g], Lx, hy, hz,
                               perm_mD, poro, P_init_atm, So_init,
                               -1000.0, 800.0, t_final, M);
        double hx = Lx / grids[g];
        WARN("Nx=" << grids[g] << " hx=" << hx << " L2=" << L2[g]);
        REQUIRE(L2[g] > 0.0);
    }

    for (size_t g = 1; g < N; ++g) {
        double p = std::log2(L2[g - 1] / L2[g]);
        WARN("Nx=" << grids[g-1] << "->" << grids[g]
             << ": L2 " << L2[g-1] << " -> " << L2[g] << ", order p=" << p);
        CHECK(L2[g] < L2[g - 1]);
        CHECK(p > 0.5);
    }
}

// BUG-020: MER задаёт кг/день, формула Писмана оперирует м³/день.
// qt_eff зависит от сетки (PI Писмана ∝ 1/ln(r_app/r_well), r_app ∝ hx).
// После фикса BUG-020: qt_eff ≈ qt_nominal, не зависит от сетки.
// [!mayfail] — тесты документируют баг, не блокируют сборку.
TEST_CASE("BL validation: qt_eff consistent across meshes",
          "[buckley-leverett][validation][!mayfail]") {
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 0.8;
    constexpr double t_final = 400.0;

    constexpr size_t grids[] = {25, 50, 100, 200};
    constexpr size_t N = sizeof(grids) / sizeof(grids[0]);
    double qt[N];

    for (size_t g = 0; g < N; ++g) {
        qt[g] = compute_qt_eff(grids[g], Lx, hy, hz,
                                perm_mD, poro, P_init_atm, So_init,
                                -1000.0, 800.0, t_final);
        WARN("Nx=" << grids[g] << " qt_eff=" << qt[g]);
        REQUIRE(qt[g] > 0.0);
    }

    for (size_t g = 1; g < N; ++g) {
        double rel_diff = std::abs(qt[g] - qt[0]) / qt[0];
        WARN("qt[" << grids[g] << "] vs qt[" << grids[0]
             << "]: rel_diff=" << rel_diff);
        CHECK(rel_diff < 0.15);
    }
}

TEST_CASE("BL validation: absolute volume balance",
          "[buckley-leverett][validation][!mayfail]") {
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 0.8;
    constexpr double t_final = 400.0;
    constexpr double A = hy * hz;
    constexpr double rho_w = 1000.0;
    constexpr double qt_nominal = 1000.0 / rho_w / A;

    constexpr size_t Nx = 100;
    double qt_eff = compute_qt_eff(Nx, Lx, hy, hz,
                                    perm_mD, poro, P_init_atm, So_init,
                                    -1000.0, 800.0, t_final);

    WARN("qt_eff=" << qt_eff << " qt_nominal=" << qt_nominal
         << " ratio=" << qt_eff / qt_nominal);
    REQUIRE(qt_eff > 0.0);
    CHECK(qt_eff == Catch::Approx(qt_nominal).epsilon(0.1));
}

TEST_CASE("BL validation: front position and monotonicity",
          "[buckley-leverett][validation]") {
    constexpr size_t Nx = 100;
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 0.8;
    constexpr double Sw_init = 1.0 - So_init;
    constexpr double t_final = 400.0;
    constexpr double M = 4.3 / 2.0;
    constexpr double A = hy * hz;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 0.5, 0.0, -1000.0);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx - hx * 0.5, hy * 0.5, 800.0, 0.0);

    sim.Solve({0.0, t_final});

    auto Sw = sim.GetWaterSaturationField();
    REQUIRE(Sw.size() == Nx);

    // Effective qt from mass balance
    double integral_dSw = 0.0;
    for (size_t i = 0; i < Nx; ++i)
        integral_dSw += (Sw[i] - Sw_init) * hx;
    double qt_eff = integral_dSw * poro / t_final;

    // Front from BL with Sw_init != 0
    double fw_init = buckley_leverett::f_w(Sw_init, M);
    double Swf = Sw_init;
    {
        double lo = Sw_init + 0.01, hi = 0.99;
        for (int iter = 0; iter < 100; ++iter) {
            double mid = 0.5 * (lo + hi);
            double secant = (buckley_leverett::f_w(mid, M) - fw_init) / (mid - Sw_init);
            double tangent = buckley_leverett::df_w(mid, M);
            if (tangent > secant) lo = mid;
            else hi = mid;
        }
        Swf = 0.5 * (lo + hi);
    }
    double slope_front = (buckley_leverett::f_w(Swf, M) - fw_init) / (Swf - Sw_init);
    double v_front = qt_eff * slope_front / (poro * A);
    double x_front_analytical = v_front * t_final;

    // GDM front: cell with max |Sw[i] - Sw[i+1]|
    double max_grad = 0.0;
    size_t front_cell = 0;
    for (size_t i = 1; i + 2 < Nx; ++i) {
        double grad = std::abs(Sw[i] - Sw[i + 1]);
        if (grad > max_grad) {
            max_grad = grad;
            front_cell = i;
        }
    }
    double x_front_gdm = (front_cell + 0.5) * hx;

    INFO("GDM front at x = " << x_front_gdm);
    INFO("Analytical front at x = " << x_front_analytical);
    CHECK(std::abs(x_front_gdm - x_front_analytical) < 3.0 * hx);

    SECTION("monotonicity and bounds") {
        for (size_t i = 0; i < Nx; ++i) {
            CHECK(Sw[i] >= -1e-6);
            CHECK(Sw[i] <= 1.0 + 1e-6);
        }
        for (size_t i = 2; i + 2 < Nx; ++i) {
            CHECK(Sw[i] <= Sw[i - 1] + 1e-6);
        }
    }
}

TEST_CASE("BL validation: CSV export for visual check",
          "[buckley-leverett][validation][.]") {
    constexpr size_t Nx = 200;
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 0.8;
    constexpr double Sw_init = 1.0 - So_init;
    constexpr double t_final = 400.0;
    constexpr double M = 4.3 / 2.0;
    constexpr double A = hy * hz;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    double r_app = std::max(0.2 * hx, 0.2);
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 0.5, 0.0, -1000.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx - hx * 0.5, hy * 0.5, 800.0, 0.0, r_app);

    sim.Solve({0.0, t_final});

    auto Sw_gdm = sim.GetWaterSaturationField();

    // Effective qt from mass balance
    double integral_dSw = 0.0;
    for (size_t i = 0; i < Nx; ++i)
        integral_dSw += (Sw_gdm[i] - Sw_init) * hx;
    double qt_eff = integral_dSw * poro / t_final;

    // Analytical BL with Sw_init != 0
    double fw_init = buckley_leverett::f_w(Sw_init, M);
    double Swf = Sw_init;
    {
        double lo = Sw_init + 0.01, hi = 0.99;
        for (int iter = 0; iter < 100; ++iter) {
            double mid = 0.5 * (lo + hi);
            double secant = (buckley_leverett::f_w(mid, M) - fw_init) / (mid - Sw_init);
            double tangent = buckley_leverett::df_w(mid, M);
            if (tangent > secant) lo = mid;
            else hi = mid;
        }
        Swf = 0.5 * (lo + hi);
    }
    double slope_front = (buckley_leverett::f_w(Swf, M) - fw_init) / (Swf - Sw_init);
    double v_front = qt_eff * slope_front / (poro * A);
    double x_front = v_front * t_final;

    std::vector<double> x_centers(Nx);
    for (size_t i = 0; i < Nx; ++i) x_centers[i] = (i + 0.5) * hx;

    std::vector<double> Sw_analytical(Nx);
    for (size_t i = 0; i < Nx; ++i) {
        double xi = x_centers[i];
        if (xi >= x_front)
            Sw_analytical[i] = Sw_init;
        else {
            double target = xi * poro * A / (qt_eff * t_final);
            double lo = Swf, hi = 1.0 - 1e-10;
            for (int iter = 0; iter < 100; ++iter) {
                double mid = 0.5 * (lo + hi);
                if (buckley_leverett::df_w(mid, M) > target) lo = mid;
                else hi = mid;
            }
            Sw_analytical[i] = 0.5 * (lo + hi);
        }
    }

    std::ofstream csv("bl_validation_profile.csv");
    csv << "x,Sw_GDM,Sw_analytical\n";
    for (size_t i = 0; i < Nx; ++i)
        csv << x_centers[i] << "," << Sw_gdm[i] << "," << Sw_analytical[i] << "\n";
    csv.close();

    double sum_sq = 0.0;
    for (size_t i = 1; i + 1 < Nx; ++i) {
        double diff = Sw_gdm[i] - Sw_analytical[i];
        sum_sq += diff * diff;
    }
    double L2 = std::sqrt(sum_sq * hx / Lx);
    INFO("CSV written to bl_validation_profile.csv, L2 = " << L2);
    CHECK(true);
}
