#pragma once

#include "../tests/simulation_cases/SimulationCase.h"
#include "../tests/simulation_cases/MultiLayerCase.h"

#include <fstream>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>

namespace fs = std::filesystem;

namespace examples {

inline void write_snapshot_csv(const std::string& path,
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

inline void write_layer_csv(const std::string& path,
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

inline void write_metadata_json(const std::string& dir,
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

inline void write_metadata_json_3d(const std::string& dir,
                                    const simulation_cases::MultiLayerCase& sc,
                                    const std::vector<double>& times)
{
    std::ofstream ofs(dir + "/metadata.json");
    ofs << "{\n";
    ofs << "  \"case\": \"" << sc.name() << "\",\n";
    ofs << "  \"Nx\": " << sc.nx() << ", \"Ny\": " << sc.ny()
        << ", \"Nz\": " << sc.nz() << ",\n";
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

inline void run_case(const simulation_cases::SimulationCase& sc,
                     const std::string& results_root)
{
    std::string out_dir = results_root + "/" + sc.name();
    fs::create_directories(out_dir);
    std::cerr << "[" << sc.name() << "] running..." << std::flush;

    std::ofstream solver_log(out_dir + "/solver.log");
    std::wofstream wnull("NUL");
    auto* cout_buf = std::cout.rdbuf(solver_log.rdbuf());
    auto* wcout_buf = std::wcout.rdbuf(wnull.rdbuf());

    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(sc.initial_tau());
    sim.numPrm.set_currentMoment(0.0);

    sc.add_wells(sim, horizon);

    const auto times = sc.save_times();

    auto Sw0 = sim.GetWaterSaturationField();
    auto P0 = sim.GetPressureField();
    write_snapshot_csv(out_dir + "/snapshot_000.csv", sc.nx(), sc.ny(), Sw0, P0);

    std::ofstream balance_log(out_dir + "/mass_balance.csv");
    balance_log << "t,oil_mass,water_mass,"
                   "accumOil,accumOilOutFlux,accumOilDebet,oil_residual,"
                   "accumWater,accumWaterOutFlux,accumWaterDebet,water_residual\n";
    balance_log << std::setprecision(10)
                << 0.0 << ","
                << sim.OilTotal() << "," << sim.WaterTotal() << ","
                << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << ","
                << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << "\n";

    for (size_t step = 1; step < times.size(); ++step) {
        sim.Solve({times[step - 1], times[step]});

        auto Sw = sim.GetWaterSaturationField();
        auto P = sim.GetPressureField();

        char snap_name[64];
        std::snprintf(snap_name, sizeof(snap_name), "/snapshot_%03zu.csv", step);
        write_snapshot_csv(out_dir + snap_name, sc.nx(), sc.ny(), Sw, P);

        auto bal = sim.GetOverallBalance();
        double oil_residual   = bal[1] + bal[2] - bal[3];
        double water_residual = bal[4] + bal[5] - bal[6];

        balance_log << std::setprecision(10)
                    << times[step] << ","
                    << sim.OilTotal() << "," << sim.WaterTotal() << ","
                    << bal[1] << "," << bal[2] << ","
                    << bal[3] << "," << oil_residual << ","
                    << bal[4] << "," << bal[5] << ","
                    << bal[6] << "," << water_residual << "\n";
    }

    balance_log.close();
    write_metadata_json(out_dir, sc, times);

    std::cout.rdbuf(cout_buf);
    std::wcout.rdbuf(wcout_buf);
    std::cerr << " done" << std::endl;
}

inline void run_case_3d(const simulation_cases::MultiLayerCase& sc,
                        const std::string& results_root)
{
    std::string out_dir = results_root + "/" + sc.name();
    fs::create_directories(out_dir);
    std::cerr << "[" << sc.name() << "] running..." << std::flush;

    std::ofstream solver_log(out_dir + "/solver.log");
    auto* cout_buf = std::cout.rdbuf(solver_log.rdbuf());
    auto* wcout_buf = std::wcout.rdbuf(
        static_cast<std::wstreambuf*>(nullptr));

    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(sc.initial_tau());
    sim.numPrm.set_currentMoment(0.0);

    sc.add_wells(sim, horizon);

    const auto times = sc.save_times();

    auto Sw0 = sim.GetWaterSaturationField();
    auto P0 = sim.GetPressureField();
    for (size_t k = 0; k < sc.nz(); ++k) {
        char fname[128];
        std::snprintf(fname, sizeof(fname), "/snapshot_000_layer_%zu.csv", k);
        write_layer_csv(out_dir + fname, sc.nx(), sc.ny(), k, Sw0, P0);
    }

    std::ofstream balance_log(out_dir + "/mass_balance.csv");
    balance_log << "t,oil_mass,water_mass,"
                   "accumOil,accumOilOutFlux,accumOilDebet,oil_residual,"
                   "accumWater,accumWaterOutFlux,accumWaterDebet,water_residual\n";
    balance_log << std::setprecision(10)
                << 0.0 << ","
                << sim.OilTotal() << "," << sim.WaterTotal() << ","
                << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << ","
                << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << "\n";

    size_t total_cells = sc.nx() * sc.ny() * sc.nz();

    for (size_t step = 1; step < times.size(); ++step) {
        sim.Solve({times[step - 1], times[step]});

        auto Sw = sim.GetWaterSaturationField();
        auto P = sim.GetPressureField();

        for (size_t k = 0; k < sc.nz(); ++k) {
            char fname[128];
            std::snprintf(fname, sizeof(fname),
                          "/snapshot_%03zu_layer_%zu.csv", step, k);
            write_layer_csv(out_dir + fname, sc.nx(), sc.ny(), k, Sw, P);
        }

        auto bal = sim.GetOverallBalance();
        double oil_residual   = bal[1] + bal[2] - bal[3];
        double water_residual = bal[4] + bal[5] - bal[6];

        balance_log << std::setprecision(10)
                    << times[step] << ","
                    << sim.OilTotal() << "," << sim.WaterTotal() << ","
                    << bal[1] << "," << bal[2] << ","
                    << bal[3] << "," << oil_residual << ","
                    << bal[4] << "," << bal[5] << ","
                    << bal[6] << "," << water_residual << "\n";
    }

    balance_log.close();
    write_metadata_json_3d(out_dir, sc, times);

    std::cout.rdbuf(cout_buf);
    std::wcout.rdbuf(wcout_buf);
    std::cerr << " done" << std::endl;
}

} // namespace examples
