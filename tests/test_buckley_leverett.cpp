#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "buckley_leverett_analytical.h"
#include <cmath>
#include <fstream>
#include <filesystem>

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

std::filesystem::path project_root() {
    auto p = std::filesystem::path(__FILE__).parent_path().parent_path();
    return std::filesystem::canonical(p);
}

std::filesystem::path validation_dir() {
    auto d = project_root() / "results" / "validation";
    std::filesystem::create_directories(d);
    return d;
}

std::vector<double> run_radial_simulation(
    size_t N, double L, double hz,
    double perm_mD, double poro,
    double P_init_atm, double So_init,
    double water_inject_rate, double t_final,
    double dt_save = 5.0) {

    auto horizon = test_helpers::make_uniform_horizon(
        N, N, 1, L, L, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.numPrm.SetUsePIController(false);

    double h = L / N;
    double r_app = std::max(0.2 * h, 0.2);
    test_helpers::add_simple_well(sim, horizon,
        "INJ", (N / 2 + 0.5) * h, (N / 2 + 0.5) * h,
        0.0, water_inject_rate, r_app);

    std::vector<double> timeMoments = {0.0};
    for (double t = dt_save; t < t_final; t += dt_save)
        timeMoments.push_back(t);
    timeMoments.push_back(t_final);

    sim.Solve(timeMoments);
    return sim.GetWaterSaturationField();
}

struct RadialProfile {
    std::vector<double> r;
    std::vector<double> Sw;
};

RadialProfile azimuthal_average(
    const std::vector<double>& Sw_field, size_t N, double L,
    double r_max) {

    double h = L / N;
    double cx = (N / 2 + 0.5) * h, cy = cx;
    double dr = h;
    size_t n_bins = static_cast<size_t>(r_max / dr) + 1;

    std::vector<double> sum_Sw(n_bins, 0.0);
    std::vector<size_t> count(n_bins, 0);

    for (size_t j = 0; j < N; ++j) {
        for (size_t i = 0; i < N; ++i) {
            double x = (i + 0.5) * h, y = (j + 0.5) * h;
            double r = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            size_t bin = static_cast<size_t>(r / dr);
            if (bin < n_bins) {
                sum_Sw[bin] += Sw_field[j * N + i];
                count[bin]++;
            }
        }
    }

    RadialProfile prof;
    for (size_t b = 0; b < n_bins; ++b) {
        if (count[b] > 0) {
            prof.r.push_back((b + 0.5) * dr);
            prof.Sw.push_back(sum_Sw[b] / count[b]);
        }
    }
    return prof;
}

double radial_L2(const RadialProfile& coarse, const RadialProfile& ref) {
    double sum_sq = 0.0;
    double total_r = 0.0;
    size_t j = 0;
    for (size_t i = 0; i < coarse.r.size(); ++i) {
        while (j + 1 < ref.r.size() && ref.r[j + 1] <= coarse.r[i])
            ++j;
        if (j >= ref.r.size()) break;
        double Sw_ref_interp;
        if (j + 1 < ref.r.size() && ref.r[j] <= coarse.r[i]) {
            double t = (coarse.r[i] - ref.r[j]) / (ref.r[j + 1] - ref.r[j]);
            Sw_ref_interp = ref.Sw[j] * (1.0 - t) + ref.Sw[j + 1] * t;
        } else {
            Sw_ref_interp = ref.Sw[j];
        }
        double diff = coarse.Sw[i] - Sw_ref_interp;
        double dr = (i + 1 < coarse.r.size())
            ? coarse.r[i + 1] - coarse.r[i]
            : coarse.r[i] - coarse.r[i - 1];
        sum_sq += diff * diff * dr;
        total_r += dr;
    }
    return (total_r > 0) ? std::sqrt(sum_sq / total_r) : 0.0;
}

double compute_qt_eff(size_t Nx,
                      double Lx, double hy, double hz,
                      double perm_mD, double poro,
                      double P_init_atm, double So_init,
                      double water_inject_rate, double oil_prod_rate,
                      double t_final) {
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
    sim.numPrm.SetUsePIController(false);

    double hx = Lx / Nx;
    double r_app = std::max(0.2 * hx, 0.2);
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 0.5, 0.0, water_inject_rate, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx - hx * 0.5, hy * 0.5, oil_prod_rate, 0.0, r_app);

    constexpr double rho_w = 1000.0;
    double qt = std::abs(water_inject_rate) / rho_w / A;
    double v = qt / (poro * A);
    double dt_max = 0.5 * hx / v;
    std::vector<double> timeMoments;
    timeMoments.push_back(0.0);
    for (double t = dt_max; t < t_final; t += dt_max)
        timeMoments.push_back(t);
    timeMoments.push_back(t_final);

    sim.Solve(timeMoments);

    auto Sw_gdm = sim.GetWaterSaturationField();
    if (Sw_gdm.size() != Nx) return -1.0;

    double integral_dSw = 0.0;
    for (size_t i = 0; i < Nx; ++i)
        integral_dSw += (Sw_gdm[i] - Sw_init) * hx;
    return integral_dSw * poro / t_final;
}

} // namespace

TEST_CASE("Radial BL: grid convergence of Sw(r) profile",
          "[buckley-leverett][radial][validation][.]") {
    constexpr double L = 200.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.8;
    constexpr double Q_water_mass = -50000.0;
    constexpr double t_final = 100.0;
    constexpr double r_max = 90.0;

    constexpr size_t N_ref = 321;
    constexpr size_t grids[] = {21, 41, 81, 161};
    constexpr size_t NG = sizeof(grids) / sizeof(grids[0]);

    auto Sw_ref_field = run_radial_simulation(
        N_ref, L, hz, perm_mD, poro, P_init_atm, So_init,
        Q_water_mass, t_final);
    REQUIRE(Sw_ref_field.size() == N_ref * N_ref);
    auto prof_ref = azimuthal_average(Sw_ref_field, N_ref, L, r_max);
    REQUIRE(prof_ref.r.size() > 10);

    double L2[NG];
    for (size_t g = 0; g < NG; ++g) {
        auto Sw_field = run_radial_simulation(
            grids[g], L, hz, perm_mD, poro, P_init_atm, So_init,
            Q_water_mass, t_final);
        REQUIRE(Sw_field.size() == grids[g] * grids[g]);
        auto prof = azimuthal_average(Sw_field, grids[g], L, r_max);
        L2[g] = radial_L2(prof, prof_ref);

        double h = L / grids[g];
        WARN("N=" << grids[g] << " h=" << h << " L2=" << L2[g]);
        REQUIRE(L2[g] >= 0.0);
    }

    for (size_t g = 1; g < NG; ++g) {
        double p = std::log2(L2[g - 1] / L2[g]);
        WARN("N=" << grids[g-1] << "->" << grids[g]
             << ": L2 " << L2[g-1] << " -> " << L2[g] << ", order p=" << p);
        CHECK(L2[g] < L2[g - 1]);
        CHECK(p > 0.3);
    }
}

TEST_CASE("Radial BL: convergence CSV export",
          "[buckley-leverett][radial][validation][convergence][.]") {
    constexpr double L = 200.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.8;
    constexpr double Q_water_mass = -50000.0;
    constexpr double t_final = 100.0;
    constexpr double r_max = 90.0;

    constexpr size_t N_ref = 321;
    constexpr size_t grids[] = {21, 41, 81, 161};
    constexpr size_t NG = sizeof(grids) / sizeof(grids[0]);

    auto Sw_ref_field = run_radial_simulation(
        N_ref, L, hz, perm_mD, poro, P_init_atm, So_init,
        Q_water_mass, t_final);
    auto prof_ref = azimuthal_average(Sw_ref_field, N_ref, L, r_max);

    double L2[NG];
    for (size_t g = 0; g < NG; ++g) {
        auto Sw_field = run_radial_simulation(
            grids[g], L, hz, perm_mD, poro, P_init_atm, So_init,
            Q_water_mass, t_final);
        auto prof = azimuthal_average(Sw_field, grids[g], L, r_max);
        L2[g] = radial_L2(prof, prof_ref);
    }

    auto vdir = validation_dir();
    std::ofstream csv((vdir / "radial_bl_convergence.csv").string());
    csv << "N,h,L2,p\n";
    csv << grids[0] << "," << L / grids[0] << "," << L2[0] << ",\n";
    for (size_t g = 1; g < NG; ++g) {
        double p = std::log2(L2[g - 1] / L2[g]);
        csv << grids[g] << "," << L / grids[g] << "," << L2[g] << "," << p << "\n";
    }
    csv.close();
    INFO("Written: " << (vdir / "radial_bl_convergence.csv").string());
    CHECK(true);
}

TEST_CASE("Radial BL: multi-grid profiles CSV export",
          "[buckley-leverett][radial][validation][convergence][.]") {
    constexpr double L = 200.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.8;
    constexpr double Q_water_mass = -50000.0;
    constexpr double t_final = 100.0;
    constexpr double r_max = 90.0;

    constexpr size_t N_ref = 321;
    constexpr size_t grids[] = {21, 41, 81, 161};

    auto Sw_ref_field = run_radial_simulation(
        N_ref, L, hz, perm_mD, poro, P_init_atm, So_init,
        Q_water_mass, t_final);
    auto prof_ref = azimuthal_average(Sw_ref_field, N_ref, L, r_max);

    auto vdir = validation_dir();

    {
        auto path = (vdir / "radial_bl_profile_N321_ref.csv").string();
        std::ofstream csv(path);
        csv << "r,Sw\n";
        for (size_t i = 0; i < prof_ref.r.size(); ++i)
            csv << prof_ref.r[i] << "," << prof_ref.Sw[i] << "\n";
        csv.close();
        WARN("Reference profile: " << path);
    }

    for (size_t g = 0; g < 4; ++g) {
        auto Sw_field = run_radial_simulation(
            grids[g], L, hz, perm_mD, poro, P_init_atm, So_init,
            Q_water_mass, t_final);
        auto prof = azimuthal_average(Sw_field, grids[g], L, r_max);

        auto path = (vdir / ("radial_bl_profile_N"
                    + std::to_string(grids[g]) + ".csv")).string();
        std::ofstream csv(path);
        csv << "r,Sw_GDM,Sw_reference\n";
        for (size_t i = 0; i < prof.r.size(); ++i) {
            size_t j = 0;
            while (j + 1 < prof_ref.r.size() && prof_ref.r[j + 1] <= prof.r[i])
                ++j;
            double Sw_ref_interp = prof_ref.Sw[j];
            if (j + 1 < prof_ref.r.size() && prof_ref.r[j] <= prof.r[i]) {
                double t = (prof.r[i] - prof_ref.r[j])
                         / (prof_ref.r[j + 1] - prof_ref.r[j]);
                Sw_ref_interp = prof_ref.Sw[j] * (1.0 - t)
                              + prof_ref.Sw[j + 1] * t;
            }
            csv << prof.r[i] << "," << prof.Sw[i] << "," << Sw_ref_interp << "\n";
        }
        csv.close();
        WARN("Profile CSV: " << path);
    }
    CHECK(true);
}

TEST_CASE("Radial BL: front position and radial monotonicity",
          "[buckley-leverett][radial][validation]") {
    constexpr double L = 200.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.8;
    constexpr double Q_water_mass = -50000.0;
    constexpr double t_final = 100.0;
    constexpr double r_max = 90.0;

    auto Sw_field = run_radial_simulation(
        41, L, hz, perm_mD, poro, P_init_atm, So_init,
        Q_water_mass, t_final);
    REQUIRE(Sw_field.size() == 41 * 41);
    auto prof = azimuthal_average(Sw_field, 41, L, r_max);
    REQUIRE(prof.r.size() > 10);

    double Sw_init = 1.0 - So_init;
    size_t front_bin = 0;
    double max_grad = 0.0;
    for (size_t i = 1; i < prof.r.size(); ++i) {
        double grad = std::abs(prof.Sw[i - 1] - prof.Sw[i]);
        if (grad > max_grad) {
            max_grad = grad;
            front_bin = i;
        }
    }
    double r_front = prof.r[front_bin];
    WARN("Front at r=" << r_front << " (bin " << front_bin << ")");
    CHECK(r_front > 20.0);
    CHECK(r_front < 80.0);

    SECTION("bounds") {
        for (size_t i = 0; i < prof.r.size(); ++i) {
            CHECK(prof.Sw[i] >= Sw_init - 1e-6);
            CHECK(prof.Sw[i] <= 1.0 + 1e-6);
        }
    }

    SECTION("radial monotonicity: Sw decreases with r") {
        for (size_t i = 2; i < prof.r.size(); ++i) {
            CHECK(prof.Sw[i] <= prof.Sw[i - 1] + 0.02);
        }
    }

    SECTION("azimuthal symmetry: E-N profiles match") {
        double h = L / 41;
        size_t ic = 41 / 2, jc = 41 / 2;
        for (size_t d = 1; d <= 8 && ic + d < 41; ++d) {
            double Sw_east = Sw_field[jc * 41 + ic + d];
            double Sw_north = Sw_field[(jc + d) * 41 + ic];
            CHECK(std::abs(Sw_east - Sw_north) < 0.01);
        }
    }
}

TEST_CASE("Radial BL: CSV export for visual check",
          "[buckley-leverett][radial][validation][.]") {
    constexpr double L = 200.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.8;
    constexpr double Q_water_mass = -50000.0;
    constexpr double t_final = 100.0;

    auto Sw_field = run_radial_simulation(
        41, L, hz, perm_mD, poro, P_init_atm, So_init,
        Q_water_mass, t_final);
    REQUIRE(Sw_field.size() == 41 * 41);

    double h = L / 41;
    double cx = (41 / 2 + 0.5) * h, cy = cx;

    auto vdir = validation_dir();
    std::ofstream csv((vdir / "radial_bl_field.csv").string());
    csv << "x,y,r,Sw\n";
    for (size_t j = 0; j < 41; ++j) {
        for (size_t i = 0; i < 41; ++i) {
            double x = (i + 0.5) * h, y = (j + 0.5) * h;
            double r = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            csv << x << "," << y << "," << r << "," << Sw_field[j * 41 + i] << "\n";
        }
    }
    csv.close();
    INFO("Written: " << (vdir / "radial_bl_field.csv").string());
    CHECK(true);
}
