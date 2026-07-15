#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation_cases/VariableDebitCase.h"

#include <fstream>
#include <iomanip>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace {

constexpr size_t Nx = 21, Ny = 21;
constexpr double Lx = 250.0, Ly = 250.0;
constexpr double rho_oil = 800.0, rho_water = 1000.0;

void write_snapshot_csv(const std::string& path,
                        size_t nx, size_t ny,
                        const std::vector<double>& Sw,
                        const std::vector<double>& P)
{
    std::ofstream ofs(path);
    ofs << "i,j,Sw,P_atm\n";
    for (size_t j = 0; j < ny; ++j)
        for (size_t i = 0; i < nx; ++i) {
            size_t idx = j * nx + i;
            ofs << i << "," << j << ","
                << std::setprecision(8) << Sw[idx] << ","
                << P[idx] / 101325.0 << "\n";
        }
}

void write_metadata_json(const std::string& dir,
                         const simulation_cases::SimulationCase& sc,
                         const std::vector<double>& times)
{
    std::ofstream ofs(dir + "/metadata.json");
    ofs << "{\n";
    ofs << "  \"case\": \"" << sc.name() << "\",\n";
    ofs << "  \"Nx\": " << sc.nx() << ", \"Ny\": " << sc.ny() << ",\n";
    ofs << "  \"Lx\": " << sc.lx() << ", \"Ly\": " << sc.ly() << ",\n";
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

RunResult run_case(const simulation_cases::SimulationCase& sc,
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
        write_snapshot_csv(out_dir + "/snapshot_000.csv", sc.nx(), sc.ny(), Sw0, P0);

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

    for (size_t step = 1; step < times.size(); ++step) {
        sim.Solve({times[step - 1], times[step]});

        auto Sw = sim.GetWaterSaturationField();
        auto P = sim.GetPressureField();

        if (export_snapshots) {
            char snap_name[64];
            std::snprintf(snap_name, sizeof(snap_name),
                          "/snapshot_%03zu.csv", step);
            write_snapshot_csv(out_dir + snap_name, sc.nx(), sc.ny(), Sw, P);
        }

        for (size_t i = 0; i < Sw.size(); ++i) {
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


// --- 6.1: Два периода закачки с разным дебитом ---
TEST_CASE("Variable debit: two injection rates",
          "[variable-debit][mass-balance]")
{
    double Q1 = 30.0, Q2 = 60.0;
    double t1 = 90.0, t2 = 180.0;

    double hx = Lx / Nx, hy = Ly / Ny;
    double cx = (Nx / 2 + 0.5) * hx;
    double cy = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_two_rates", Nx, Ny, Lx, Ly,
        t1 + t2, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;
            builders.emplace_back("INJ", cx, cy);
            builders.back()
                .inject_water(Q1).for_days(t1)
                .inject_water(Q2).for_days(t2);
            return builders;
        },
        {{"INJ", "injector", cx, cy}}
    );

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    size_t inj_cell = (Ny / 2) * Nx + Nx / 2;
    constexpr double initial_Sw = 1.0 - 0.8;
    CHECK(result.Sw[inj_cell] > initial_Sw);
}


// --- 6.2: Закачка — остановка — закачка ---
TEST_CASE("Variable debit: injection with shut-in",
          "[variable-debit][shut-in][mass-balance]")
{
    double Q = 50.0;
    double t_work = 60.0, t_shut = 60.0;

    double hx = Lx / Nx, hy = Ly / Ny;
    double cx = (Nx / 2 + 0.5) * hx;
    double cy = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_shut_in", Nx, Ny, Lx, Ly,
        t_work + t_shut + t_work, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;
            builders.emplace_back("INJ", cx, cy);
            builders.back()
                .inject_water(Q).for_days(t_work)
                .shut_in().for_days(t_shut)
                .inject_water(Q).for_days(t_work);
            return builders;
        },
        {{"INJ", "injector", cx, cy}}
    );

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    size_t inj_cell = (Ny / 2) * Nx + Nx / 2;
    constexpr double initial_Sw = 1.0 - 0.8;
    CHECK(result.Sw[inj_cell] > initial_Sw);
}


// --- 6.3: INJ усиливается, PROD постоянный ---
TEST_CASE("Variable debit: increasing injection with constant production",
          "[variable-debit][two-well][mass-balance]")
{
    double Q_prod = 30.0;
    double Q_inj1 = 20.0, Q_inj2 = 40.0;
    double t1 = 120.0, t2 = 120.0;

    double hx = Lx / Nx, hy = Ly / Ny;
    double inj_x = (Nx / 4 + 0.5) * hx;
    double inj_y = (Ny / 2 + 0.5) * hy;
    double prod_x = (3 * Nx / 4 + 0.5) * hx;
    double prod_y = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_increasing_inj", Nx, Ny, Lx, Ly,
        t1 + t2, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            builders.emplace_back("INJ", inj_x, inj_y);
            builders.back()
                .inject_water(Q_inj1).for_days(t1)
                .inject_water(Q_inj2).for_days(t2);

            builders.emplace_back("PROD", prod_x, prod_y);
            builders.back()
                .produce_oil(Q_prod).for_days(t1 + t2);

            return builders;
        },
        {{"INJ", "injector", inj_x, inj_y},
         {"PROD", "producer", prod_x, prod_y}}
    );

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
}


// --- 6.4: Две INJ попеременно ---
TEST_CASE("Variable debit: alternating injectors",
          "[variable-debit][alternating][mass-balance]")
{
    double Q = 40.0;
    double t_phase = 90.0;

    double hx = Lx / Nx, hy = Ly / Ny;
    double x_a = (Nx / 4 + 0.5) * hx;
    double x_b = (3 * Nx / 4 + 0.5) * hx;
    double cy = (Ny / 2 + 0.5) * hy;

    simulation_cases::VariableDebitCase sc(
        "vardebit_alternating", Nx, Ny, Lx, Ly,
        2 * t_phase, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            builders.emplace_back("INJ_A", x_a, cy);
            builders.back()
                .inject_water(Q).for_days(t_phase)
                .shut_in().for_days(t_phase);

            builders.emplace_back("INJ_B", x_b, cy);
            builders.back()
                .shut_in().for_days(t_phase)
                .inject_water(Q).for_days(t_phase);

            return builders;
        },
        {{"INJ_A", "injector", x_a, cy},
         {"INJ_B", "injector", x_b, cy}}
    );

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
}
