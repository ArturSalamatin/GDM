#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation_cases/MultiLayerCase.h"

#include <fstream>
#include <iomanip>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace {

constexpr size_t Nx = 11, Ny = 11;
constexpr double Lx = 500.0, Ly = 500.0;
constexpr double hz = 10.0;
constexpr double rho_oil = 800.0, rho_water = 1000.0;

void write_layer_csv(const std::string& path,
                     size_t nx, size_t ny, size_t layer_k,
                     const std::vector<double>& Sw,
                     const std::vector<double>& P)
{
    std::ofstream ofs(path);
    ofs << "i,j,Sw,P_atm\n";
    size_t offset = nx * ny * layer_k;
    for (size_t j = 0; j < ny; ++j)
        for (size_t i = 0; i < nx; ++i) {
            size_t idx = offset + j * nx + i;
            ofs << i << "," << j << ","
                << std::setprecision(8) << Sw[idx] << ","
                << P[idx] / 101325.0 << "\n";
        }
}

void write_metadata_json(const std::string& dir,
                         const simulation_cases::MultiLayerCase& sc,
                         const std::vector<double>& times)
{
    std::ofstream ofs(dir + "/metadata.json");
    ofs << "{\n";
    ofs << "  \"case\": \"" << sc.name() << "\",\n";
    ofs << "  \"Nx\": " << sc.nx() << ", \"Ny\": " << sc.ny()
        << ", \"Nz\": " << sc.nz() << ",\n";
    ofs << "  \"Lx\": " << sc.lx() << ", \"Ly\": " << sc.ly()
        << ", \"hz\": " << hz << ",\n";
    auto wells = sc.wells_info();
    ofs << "  \"wells\": [\n";
    for (size_t i = 0; i < wells.size(); ++i) {
        ofs << "    {\"name\": \"" << wells[i].name
            << "\", \"type\": \"" << wells[i].type
            << "\", \"x\": " << wells[i].x
            << ", \"y\": " << wells[i].y << "}";
        if (i + 1 < wells.size()) ofs << ",";
        ofs << "\n";
    }
    ofs << "  ],\n";
    ofs << "  \"save_times\": [";
    for (size_t k = 0; k < times.size(); ++k) {
        if (k > 0) ofs << ", ";
        ofs << times[k];
    }
    ofs << "]\n}\n";
}

struct RunResult {
    double oil_mass;
    double water_mass;
    double max_oil_balance_rel;
    double max_water_balance_rel;
    std::vector<double> Sw;
    std::vector<double> P;
};

RunResult run_case_3d(const simulation_cases::MultiLayerCase& sc,
                      bool export_snapshots)
{
    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(sc.initial_tau());
    sim.numPrm.set_currentMoment(0.0);

    sc.add_wells(sim, horizon);

    const auto times = sc.save_times();
    std::string out_dir;
    std::ofstream balance_log;

    if (export_snapshots) {
        out_dir = "results/" + sc.name();
        fs::create_directories(out_dir);

        auto Sw0 = sim.GetWaterSaturationField();
        auto P0 = sim.GetPressureField();
        for (size_t k = 0; k < sc.nz(); ++k) {
            char fname[128];
            std::snprintf(fname, sizeof(fname),
                          "/snapshot_000_layer_%zu.csv", k);
            write_layer_csv(out_dir + fname, sc.nx(), sc.ny(), k, Sw0, P0);
        }

        balance_log.open(out_dir + "/mass_balance.csv");
        balance_log << "t,oil_mass,water_mass,"
                       "accumOil,accumOilOutFlux,accumOilDebet,oil_residual,"
                       "accumWater,accumWaterOutFlux,accumWaterDebet,water_residual\n";
        balance_log << std::setprecision(10)
                    << 0.0 << ","
                    << sim.OilTotal() << "," << sim.WaterTotal() << ","
                    << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << ","
                    << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << "\n";
    }

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();
    double max_oil_rel = 0.0, max_water_rel = 0.0;
    size_t total_cells = sc.nx() * sc.ny() * sc.nz();

    for (size_t step = 1; step < times.size(); ++step) {
        sim.Solve({times[step - 1], times[step]});

        auto Sw = sim.GetWaterSaturationField();
        auto P = sim.GetPressureField();

        if (export_snapshots) {
            for (size_t k = 0; k < sc.nz(); ++k) {
                char fname[128];
                std::snprintf(fname, sizeof(fname),
                              "/snapshot_%03zu_layer_%zu.csv", step, k);
                write_layer_csv(out_dir + fname, sc.nx(), sc.ny(), k, Sw, P);
            }
        }

        for (size_t i = 0; i < total_cells; ++i) {
            REQUIRE(Sw[i] >= 0.0);
            REQUIRE(Sw[i] <= 1.0);
            REQUIRE(P[i] > 0.0);
        }

        auto bal = sim.GetOverallBalance();
        double oil_residual   = bal[1] + bal[2] - bal[3];
        double water_residual = bal[4] + bal[5] - bal[6];

        double oil_rel  = (oil_mass_0 > 0)
            ? std::abs(oil_residual) / oil_mass_0 : std::abs(oil_residual);
        double water_rel = (water_mass_0 > 0)
            ? std::abs(water_residual) / water_mass_0 : std::abs(water_residual);

        max_oil_rel = std::max(max_oil_rel, oil_rel);
        max_water_rel = std::max(max_water_rel, water_rel);

        if (export_snapshots) {
            double oil_mass = sim.OilTotal();
            double water_mass = sim.WaterTotal();
            balance_log << std::setprecision(10)
                        << times[step] << ","
                        << oil_mass << "," << water_mass << ","
                        << bal[1] << "," << bal[2] << ","
                        << bal[3] << "," << oil_residual << ","
                        << bal[4] << "," << bal[5] << ","
                        << bal[6] << "," << water_residual << "\n";
        }
    }

    if (export_snapshots) {
        balance_log.close();
        write_metadata_json(out_dir, sc, times);
    }

    return {
        sim.OilTotal(), sim.WaterTotal(),
        max_oil_rel, max_water_rel,
        sim.GetWaterSaturationField(), sim.GetPressureField()
    };
}

} // namespace


// --- 7.1: 3D smoke — 7 скважин, 4 пласта ---
TEST_CASE("3D completions: 7-well 4-layer smoke",
          "[3d][completions][smoke][slow]")
{
    constexpr size_t Nz = 4;
    constexpr double total_time = 600.0;

    simulation_cases::MultiLayerCase sc(
        "3d_smoke", Nx, Ny, Nz, Lx, Ly, hz,
        total_time, 30.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            // INJ-1: пласты 0,1 с t=0, Q=40
            auto c_inj1 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0);
            builders.emplace_back(L"INJ-1", 125.0, 125.0);
            builders.back()
                .set_completions(c_inj1)
                .inject_water(40.0).for_days(total_time);

            // INJ-2: пласт 0 с t=0, пласт 2 добавлен при t=200, Q=30
            auto c_inj2 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(2, 200.0);
            builders.emplace_back(L"INJ-2", 375.0, 375.0);
            builders.back()
                .set_completions(c_inj2)
                .inject_water(30.0).for_days(total_time);

            // PROD-1: все 4 пласта с t=0, Q=25
            auto c_prod1 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .open_layer(2, 0.0).open_layer(3, 0.0);
            builders.emplace_back(L"PROD-1", 375.0, 125.0);
            builders.back()
                .set_completions(c_prod1)
                .produce_oil(25.0).for_days(total_time);

            // PROD-2: только пласт 3, Q=15
            auto c_prod2 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(3, 0.0);
            builders.emplace_back(L"PROD-2", 125.0, 375.0);
            builders.back()
                .set_completions(c_prod2)
                .produce_oil(15.0).for_days(total_time);

            // PROD-3: пласты 1,2 с t=0, закрытие пласта 1 при t=300, Q=20
            auto c_prod3 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(1, 0.0).open_layer(2, 0.0)
                .close_layer(1, 300.0);
            builders.emplace_back(L"PROD-3", 250.0, 250.0);
            builders.back()
                .set_completions(c_prod3)
                .produce_oil(20.0).for_days(total_time);

            // INJ-3: появляется при t=150, пласты 0,1, Q=35
            auto c_inj3 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 150.0).open_layer(1, 150.0);
            builders.emplace_back(L"INJ-3", 250.0, 125.0);
            builders.back()
                .set_completions(c_inj3)
                .shut_in().for_days(150.0)
                .inject_water(35.0).for_days(total_time - 150.0);

            // PROD-4: появляется при t=300, пласты 2,3, Q=15
            auto c_prod4 = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(2, 300.0).open_layer(3, 300.0);
            builders.emplace_back(L"PROD-4", 250.0, 375.0);
            builders.back()
                .set_completions(c_prod4)
                .shut_in().for_days(300.0)
                .produce_oil(15.0).for_days(total_time - 300.0);

            return builders;
        },
        {{"INJ-1",  "injector", 125.0, 125.0},
         {"INJ-2",  "injector", 375.0, 375.0},
         {"PROD-1", "producer", 375.0, 125.0},
         {"PROD-2", "producer", 125.0, 375.0},
         {"PROD-3", "producer", 250.0, 250.0},
         {"INJ-3",  "injector", 250.0, 125.0},
         {"PROD-4", "producer", 250.0, 375.0}}
    );

    auto result = run_case_3d(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
}


// --- 7.2: Отложенный запуск скважины ---
TEST_CASE("3D completions: delayed well start",
          "[3d][completions][delayed-start]")
{
    constexpr size_t Nz = 2;

    simulation_cases::MultiLayerCase sc(
        "3d_delayed_start", Nx, Ny, Nz, Lx, Ly, hz,
        400.0, 20.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0);
            builders.emplace_back(L"INJ", 125.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(400.0);

            auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 200.0);
            builders.emplace_back(L"PROD", 375.0, 250.0);
            builders.back()
                .set_completions(c_prod)
                .shut_in().for_days(200.0)
                .produce_oil(20.0).for_days(200.0);

            return builders;
        },
        {{"INJ",  "injector", 125.0, 250.0},
         {"PROD", "producer", 375.0, 250.0}}
    );

    auto result = run_case_3d(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
}


// --- 7.3: Закрытие пласта в процессе работы ---
TEST_CASE("3D completions: layer closure mid-simulation",
          "[3d][completions][layer-closure]")
{
    constexpr size_t Nz = 2;

    simulation_cases::MultiLayerCase sc(
        "3d_layer_closure", Nx, Ny, Nz, Lx, Ly, hz,
        400.0, 20.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(1, 200.0);
            builders.emplace_back(L"INJ", 250.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(400.0);

            return builders;
        },
        {{"INJ", "injector", 250.0, 250.0}}
    );

    auto result = run_case_3d(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
}


// --- 7.4: Частичное вскрытие ---
TEST_CASE("3D completions: partial perforation",
          "[3d][completions][partial]")
{
    constexpr size_t Nz = 1;

    // Полное вскрытие
    simulation_cases::MultiLayerCase sc_full(
        "3d_partial_full", Nx, Ny, Nz, Lx, Ly, hz,
        200.0, 20.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0);
            builders.emplace_back(L"INJ", 250.0, 250.0);
            builders.back()
                .set_completions(c)
                .inject_water(30.0).for_days(200.0);

            return builders;
        },
        {{"INJ", "injector", 250.0, 250.0}}
    );

    // Частичное вскрытие — верхняя половина
    simulation_cases::MultiLayerCase sc_partial(
        "3d_partial_half", Nx, Ny, Nz, Lx, Ly, hz,
        200.0, 20.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_interval(0, 0.0, hz / 2.0, 0.0);
            builders.emplace_back(L"INJ", 250.0, 250.0);
            builders.back()
                .set_completions(c)
                .inject_water(30.0).for_days(200.0);

            return builders;
        },
        {{"INJ", "injector", 250.0, 250.0}}
    );

    auto result_full = run_case_3d(sc_full, true);
    auto result_partial = run_case_3d(sc_partial, true);

    CHECK(result_full.max_oil_balance_rel < 1e-3);
    CHECK(result_partial.max_oil_balance_rel < 1e-3);
    CHECK(result_full.max_water_balance_rel < 1e-3);
    CHECK(result_partial.max_water_balance_rel < 1e-3);
}
