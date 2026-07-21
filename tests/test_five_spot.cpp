#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <fstream>
#include <iomanip>

using Catch::Matchers::WithinAbs;

namespace {

constexpr size_t Nx = 21, Ny = 21, Nz = 1;
constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;
constexpr double perm_mD = 100.0, poro = 0.2;
constexpr double P_init_atm = 200.0;
constexpr double rho_water = 1000.0, rho_oil = 800.0;
constexpr double Q_inj_vol = 50.0;    // м³/день закачки воды
constexpr double Q_prod_vol = 12.5;   // м³/день добычи на каждый продюсер
constexpr double T_end = 500.0;

struct WellSpec {
    const wchar_t* name;
    double x, y;
    double oil_mass_rate;
    double water_mass_rate;
};

std::vector<double> run_five_spot(
    size_t N,
    double Lx, double Ly, double hz,
    double perm_mD, double poro,
    double P_init_atm, double So_init,
    double Q_inj_water_mass, double Q_prod_oil_mass,
    double T_end, double dt_save = 50.0) {

    auto horizon = test_helpers::make_uniform_horizon(
        N, N, 1, Lx, Ly, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / N, hy = Ly / N;
    double r_app = std::max(0.2 * hx, 0.2);

    test_helpers::add_simple_well(sim, horizon,
        "INJ", (N / 2 + 0.5) * hx, (N / 2 + 0.5) * hy,
        0.0, Q_inj_water_mass, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P1", 0.5 * hx, 0.5 * hy,
        Q_prod_oil_mass, 0.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P2", 0.5 * hx, (N - 0.5) * hy,
        Q_prod_oil_mass, 0.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P3", (N - 0.5) * hx, 0.5 * hy,
        Q_prod_oil_mass, 0.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P4", (N - 0.5) * hx, (N - 0.5) * hy,
        Q_prod_oil_mass, 0.0, r_app);

    std::vector<double> timeMoments = {0.0};
    for (double t = dt_save; t < T_end; t += dt_save)
        timeMoments.push_back(t);
    timeMoments.push_back(T_end);

    sim.Solve(timeMoments);
    return sim.GetWaterSaturationField();
}

double five_spot_L2(
    const std::vector<double>& Sw_coarse, size_t N_coarse,
    const std::vector<double>& Sw_ref, size_t N_ref,
    double Lx, double Ly) {

    double h_ref_x = Lx / N_ref;
    double h_ref_y = Ly / N_ref;
    double h_coarse_x = Lx / N_coarse;
    double h_coarse_y = Ly / N_coarse;
    double A_total = Lx * Ly;

    double sum_sq = 0.0;
    for (size_t j = 0; j < N_ref; ++j) {
        for (size_t i = 0; i < N_ref; ++i) {
            double x = (i + 0.5) * h_ref_x;
            double y = (j + 0.5) * h_ref_y;

            size_t ic = std::min(static_cast<size_t>(x / h_coarse_x),
                                 N_coarse - 1);
            size_t jc = std::min(static_cast<size_t>(y / h_coarse_y),
                                 N_coarse - 1);

            double Sw_interp = Sw_coarse[jc * N_coarse + ic];
            double diff = Sw_interp - Sw_ref[j * N_ref + i];
            sum_sq += diff * diff * h_ref_x * h_ref_y;
        }
    }
    return std::sqrt(sum_sq / A_total);
}

} // namespace

TEST_CASE("Five-spot: runs without crash", "[five-spot][2d][benchmark]") {
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, perm_mD, poro, P_init_atm, 0.999);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(5.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx, hy = Ly / Ny;

    // Инжектор в центре (ячейка 10,10): закачка 50 м³/день воды
    test_helpers::add_simple_well(sim, horizon,
        "INJ", 10.5 * hx, 10.5 * hy,
        0.0, -Q_inj_vol * rho_water);

    // 4 продюсера в углах: каждый добывает 12.5 м³/день нефти
    test_helpers::add_simple_well(sim, horizon,
        "P1", 0.5 * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P2", 0.5 * hx, (Ny - 0.5) * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P3", (Nx - 0.5) * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy,
        Q_prod_vol * rho_oil, 0.0);

    std::vector<double> timeMoments = {0.0};
    for (double t = 50.0; t <= T_end; t += 50.0)
        timeMoments.push_back(t);

    sim.Solve(timeMoments);

    CHECK(sim.numPrm.WastedTrialsCount() < 50);

    auto Sw = sim.GetWaterSaturationField();
    auto P = sim.GetPressureField();

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-10);
            CHECK(Sw[i] <= 1.0 + 1e-10);
            CHECK(P[i] > 0.0);
        }
    }

    SECTION("Symmetry: quarter-symmetry of saturation") {
        for (size_t i = 0; i < Nx / 2; ++i) {
            for (size_t j = 0; j < Ny / 2; ++j) {
                double s00 = Sw[j * Nx + i];
                double s10 = Sw[j * Nx + (Nx - 1 - i)];
                double s01 = Sw[(Ny - 1 - j) * Nx + i];
                double s11 = Sw[(Ny - 1 - j) * Nx + (Nx - 1 - i)];
                REQUIRE_THAT(s00, WithinAbs(s10, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s01, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s11, 1e-6));
            }
        }
    }
}

TEST_CASE("Five-spot: export results for MRST comparison",
          "[five-spot][benchmark][.export]") {
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, perm_mD, poro, P_init_atm, 0.999);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(5.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx, hy = Ly / Ny;

    test_helpers::add_simple_well(sim, horizon,
        "INJ", 10.5 * hx, 10.5 * hy,
        0.0, -Q_inj_vol * rho_water);
    test_helpers::add_simple_well(sim, horizon,
        "P1", 0.5 * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P2", 0.5 * hx, (Ny - 0.5) * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P3", (Nx - 0.5) * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy,
        Q_prod_vol * rho_oil, 0.0);

    std::vector<double> save_times = {0.0, 100.0, 300.0, 500.0};

    try {
        sim.Solve(save_times);
    } catch (const std::exception& e) {
        WARN("Solve threw: " << e.what());
        SKIP("Cannot export — solver diverged");
    }

    auto Sw = sim.GetWaterSaturationField();
    auto P = sim.GetPressureField();

    std::ofstream ofs("results/five_spot_Sw_P_t500.csv");
    REQUIRE(ofs.is_open());
    ofs << "i,j,Sw,P\n";
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t idx = j * Nx + i;
            ofs << i << "," << j << ","
                << std::setprecision(8) << Sw[idx] << ","
                << P[idx] << "\n";
        }
}

TEST_CASE("Five-spot: grid convergence of Sw field",
          "[five-spot][2d][validation][convergence][.]") {
    constexpr double fs_Lx = 500.0, fs_Ly = 500.0, fs_hz = 10.0;
    constexpr double fs_perm = 100.0, fs_poro = 0.2;
    constexpr double fs_P_init = 200.0, fs_So_init = 0.999;
    constexpr double fs_Q_inj_water = -50000.0;
    constexpr double fs_Q_prod_oil = 10000.0;
    constexpr double fs_T_end = 500.0;

    constexpr size_t N_ref = 161;
    constexpr size_t grids[] = {21, 41, 81};
    constexpr size_t NG = sizeof(grids) / sizeof(grids[0]);

    auto Sw_ref = run_five_spot(
        N_ref, fs_Lx, fs_Ly, fs_hz, fs_perm, fs_poro, fs_P_init, fs_So_init,
        fs_Q_inj_water, fs_Q_prod_oil, fs_T_end);
    REQUIRE(Sw_ref.size() == N_ref * N_ref);

    double L2[NG];
    for (size_t g = 0; g < NG; ++g) {
        auto Sw_coarse = run_five_spot(
            grids[g], fs_Lx, fs_Ly, fs_hz, fs_perm, fs_poro, fs_P_init, fs_So_init,
            fs_Q_inj_water, fs_Q_prod_oil, fs_T_end);
        REQUIRE(Sw_coarse.size() == grids[g] * grids[g]);
        L2[g] = five_spot_L2(Sw_coarse, grids[g], Sw_ref, N_ref, fs_Lx, fs_Ly);

        double h = fs_Lx / grids[g];
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

    for (size_t i = 0; i < N_ref / 2; ++i) {
        for (size_t j = 0; j < N_ref / 2; ++j) {
            double s00 = Sw_ref[j * N_ref + i];
            double s10 = Sw_ref[j * N_ref + (N_ref - 1 - i)];
            double s01 = Sw_ref[(N_ref - 1 - j) * N_ref + i];
            double s11 = Sw_ref[(N_ref - 1 - j) * N_ref + (N_ref - 1 - i)];
            CHECK(std::abs(s00 - s10) < 1e-6);
            CHECK(std::abs(s00 - s01) < 1e-6);
            CHECK(std::abs(s00 - s11) < 1e-6);
        }
    }
}

TEST_CASE("Five-spot: convergence CSV export",
          "[five-spot][2d][validation][convergence][.export]") {
    constexpr double fs_Lx = 500.0, fs_Ly = 500.0, fs_hz = 10.0;
    constexpr double fs_perm = 100.0, fs_poro = 0.2;
    constexpr double fs_P_init = 200.0, fs_So_init = 0.999;
    constexpr double fs_Q_inj_water = -50000.0;
    constexpr double fs_Q_prod_oil = 10000.0;
    constexpr double fs_T_end = 500.0;

    constexpr size_t N_ref = 161;
    constexpr size_t all_grids[] = {21, 41, 81, 161};
    constexpr size_t N_all = sizeof(all_grids) / sizeof(all_grids[0]);
    constexpr size_t NG = N_all - 1;

    auto vdir = std::filesystem::path(__FILE__).parent_path().parent_path()
              / "results" / "validation";
    std::filesystem::create_directories(vdir);

    std::vector<double> Sw_fields[N_all];
    for (size_t g = 0; g < N_all; ++g) {
        Sw_fields[g] = run_five_spot(
            all_grids[g], fs_Lx, fs_Ly, fs_hz, fs_perm, fs_poro, fs_P_init, fs_So_init,
            fs_Q_inj_water, fs_Q_prod_oil, fs_T_end);
        REQUIRE(Sw_fields[g].size() == all_grids[g] * all_grids[g]);

        auto path = vdir / ("five_spot_Sw_N"
                   + std::to_string(all_grids[g]) + ".csv");
        std::ofstream sw_ofs(path.string());
        sw_ofs << "i,j,Sw\n";
        for (size_t j = 0; j < all_grids[g]; ++j)
            for (size_t i = 0; i < all_grids[g]; ++i)
                sw_ofs << i << "," << j << ","
                    << std::setprecision(8)
                    << Sw_fields[g][j * all_grids[g] + i] << "\n";
        sw_ofs.close();
        WARN("Sw field: " << path.string());
    }

    double L2[NG];
    for (size_t g = 0; g < NG; ++g) {
        L2[g] = five_spot_L2(Sw_fields[g], all_grids[g],
                              Sw_fields[N_all - 1], N_ref, fs_Lx, fs_Ly);
    }

    auto csv_path = vdir / "five_spot_convergence.csv";
    std::ofstream csv(csv_path.string());
    csv << "N,h,L2,p\n";
    csv << all_grids[0] << "," << fs_Lx / all_grids[0] << ","
        << L2[0] << ",\n";
    for (size_t g = 1; g < NG; ++g) {
        double p = std::log2(L2[g - 1] / L2[g]);
        csv << all_grids[g] << "," << fs_Lx / all_grids[g] << ","
            << L2[g] << "," << p << "\n";
    }
    csv.close();
    WARN("Convergence CSV: " << csv_path.string());
    CHECK(true);
}
