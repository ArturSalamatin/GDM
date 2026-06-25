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
        L"INJ", 10.5 * hx, 10.5 * hy,
        0.0, -Q_inj_vol * rho_water);

    // 4 продюсера в углах: каждый добывает 12.5 м³/день нефти
    test_helpers::add_simple_well(sim, horizon,
        L"P1", 0.5 * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P2", 0.5 * hx, (Ny - 0.5) * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P3", (Nx - 0.5) * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy,
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
        L"INJ", 10.5 * hx, 10.5 * hy,
        0.0, -Q_inj_vol * rho_water);
    test_helpers::add_simple_well(sim, horizon,
        L"P1", 0.5 * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P2", 0.5 * hx, (Ny - 0.5) * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P3", (Nx - 0.5) * hx, 0.5 * hy,
        Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy,
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
