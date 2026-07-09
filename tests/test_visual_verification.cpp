#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation_cases/SingleInjectorCase.h"
#include "simulation_cases/TwoWellCase.h"

#include <fstream>
#include <iomanip>
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace {

void write_snapshot_csv(const std::string& path,
                        size_t Nx, size_t Ny,
                        const std::vector<double>& Sw,
                        const std::vector<double>& P)
{
    std::ofstream ofs(path);
    ofs << "i,j,Sw,P_atm\n";
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t idx = j * Nx + i;
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


TEST_CASE("Visual verification: single injector with mass balance",
          "[visual][single-injector][mass-balance]")
{
    simulation_cases::SingleInjectorCase sc;
    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
    CHECK(result.water_mass > sc.poro * sc.Lx * sc.Ly * sc.hz * sc.rho_water
                              * (1.0 - sc.oil_saturation));
}


TEST_CASE("Grid convergence: single injector",
          "[convergence][single-injector]")
{
    struct GridLevel {
        size_t N;
        double Sw_probe;
        double P_probe_atm;
        double oil_mass;
    };

    constexpr size_t grids[] = {11, 21, 41};
    std::vector<GridLevel> levels;

    std::string out_dir = "results/convergence";
    fs::create_directories(out_dir);
    std::ofstream log(out_dir + "/convergence.csv");
    log << "N,hx,oil_mass,water_mass,Sw_probe,P_probe_atm,"
           "max_oil_balance_rel,max_water_balance_rel\n";

    for (size_t N : grids) {
        INFO("Grid " << N << "x" << N);

        simulation_cases::SingleInjectorCase sc(N, N);
        auto result = run_case(sc, true);

        CHECK(result.max_oil_balance_rel < 1e-3);
        CHECK(result.max_water_balance_rel < 1e-3);

        // Probe at 3/4 distance from center to boundary (away from well singularity)
        size_t probe = (N / 2) * N + (3 * N / 4);
        double Sw_p = result.Sw[probe];
        double P_p = result.P[probe] / 101325.0;

        levels.push_back({N, Sw_p, P_p, result.oil_mass});

        double hx = sc.Lx / N;
        log << std::setprecision(10)
            << N << "," << hx << ","
            << result.oil_mass << "," << result.water_mass << ","
            << Sw_p << "," << P_p << ","
            << result.max_oil_balance_rel << ","
            << result.max_water_balance_rel << "\n";

        INFO("  Sw_probe=" << Sw_p << " P_probe=" << P_p << " atm");
    }

    log.close();

    REQUIRE(levels.size() == 3);

    // Convergence: differences between successive grids should decrease
    double dSw_coarse = std::abs(levels[1].Sw_probe - levels[0].Sw_probe);
    double dSw_fine   = std::abs(levels[2].Sw_probe - levels[1].Sw_probe);
    INFO("dSw: coarse=" << dSw_coarse << " fine=" << dSw_fine);
    CHECK(dSw_fine < dSw_coarse);

    double dP_coarse = std::abs(levels[1].P_probe_atm - levels[0].P_probe_atm);
    double dP_fine   = std::abs(levels[2].P_probe_atm - levels[1].P_probe_atm);
    INFO("dP: coarse=" << dP_coarse << " fine=" << dP_fine);
    CHECK(dP_fine < dP_coarse);

    double dM_coarse = std::abs(levels[1].oil_mass - levels[0].oil_mass);
    double dM_fine   = std::abs(levels[2].oil_mass - levels[1].oil_mass);
    INFO("dM_oil: coarse=" << dM_coarse << " fine=" << dM_fine);
    CHECK(dM_fine < dM_coarse);
}


TEST_CASE("Two-well: INJ + PROD with mass balance",
          "[slow][visual][two-well][mass-balance]")
{
    simulation_cases::TwoWellCase sc;
    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    for (size_t i = 0; i < result.Sw.size(); ++i) {
        CHECK(result.Sw[i] >= 0.0);
        CHECK(result.Sw[i] <= 1.0);
        CHECK(result.P[i] > 0.0);
    }
}

TEST_CASE("Near-zero Sw: single injector Sw=0.001",
          "[near-zero-sw][single-injector]")
{
    simulation_cases::SingleInjectorCase sc(41, 41);
    sc.oil_saturation = 0.999; // Sw_init = 0.001

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    for (double s : result.Sw) {
        CHECK(s >= 0.0);
        CHECK(s <= 1.0);
    }
}
