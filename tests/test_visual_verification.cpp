#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "simulation_cases/SingleInjectorCase.h"

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
                         const simulation_cases::SingleInjectorCase& sc,
                         const std::vector<double>& times)
{
    std::ofstream ofs(dir + "/metadata.json");
    ofs << "{\n";
    ofs << "  \"case\": \"" << sc.name() << "\",\n";
    ofs << "  \"Nx\": " << sc.Nx << ", \"Ny\": " << sc.Ny << ",\n";
    ofs << "  \"Lx\": " << sc.Lx << ", \"Ly\": " << sc.Ly << ",\n";
    ofs << "  \"hz\": " << sc.hz << ",\n";
    ofs << "  \"perm_mD\": " << sc.perm_mD << ", \"poro\": " << sc.poro << ",\n";
    ofs << "  \"P_init_atm\": " << sc.P_init_atm << ",\n";
    ofs << "  \"oil_saturation_init\": " << sc.oil_saturation << ",\n";
    double hx = sc.Lx / sc.Nx, hy = sc.Ly / sc.Ny;
    double cx = (sc.Nx / 2 + 0.5) * hx;
    double cy = (sc.Ny / 2 + 0.5) * hy;
    ofs << "  \"well_x\": " << cx << ", \"well_y\": " << cy << ",\n";
    ofs << "  \"well_type\": \"injector\",\n";
    ofs << "  \"Q_inj_vol_m3_day\": " << sc.Q_inj_vol << ",\n";
    ofs << "  \"save_times\": [";
    for (size_t k = 0; k < times.size(); ++k) {
        if (k > 0) ofs << ", ";
        ofs << times[k];
    }
    ofs << "]\n}\n";
}

} // namespace


TEST_CASE("Visual verification: single injector with mass balance",
          "[visual][single-injector][mass-balance]")
{
    simulation_cases::SingleInjectorCase sc;
    auto horizon = sc.make_horizon();
    auto numPrm = sc.make_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = sc.ref_pressure_Pa();
    sim.numPrm.set_initial_schemeTau(sc.initial_tau());
    sim.numPrm.set_currentMoment(0.0);

    sc.add_wells(sim, horizon);

    const auto times = sc.save_times();
    const std::string out_dir = "results/" + sc.name();
    fs::create_directories(out_dir);

    // --- Initial state (t=0) ---
    auto Sw = sim.GetWaterSaturationField();
    auto P = sim.GetPressureField();
    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();

    write_snapshot_csv(out_dir + "/snapshot_000.csv", sc.Nx, sc.Ny, Sw, P);

    // Mass balance log
    std::ofstream balance_log(out_dir + "/mass_balance.csv");
    balance_log << "t,oil_mass,water_mass,"
                   "accumOil,accumOilOutFlux,accumOilDebet,oil_residual,"
                   "accumWater,accumWaterOutFlux,accumWaterDebet,water_residual\n";
    balance_log << std::setprecision(10)
                << 0.0 << ","
                << oil_mass_0 << "," << water_mass_0 << ","
                << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << ","
                << 0.0 << "," << 0.0 << "," << 0.0 << "," << 0.0 << "\n";

    INFO("Initial: oil_mass=" << oil_mass_0 << " water_mass=" << water_mass_0);

    // Relative tolerance for mass balance residual
    const double balance_tol = 1e-3;

    // --- Step-by-step solve ---
    for (size_t step = 1; step < times.size(); ++step) {
        double t_prev = times[step - 1];
        double t_next = times[step];

        sim.Solve({t_prev, t_next});

        // Extract fields
        Sw = sim.GetWaterSaturationField();
        P = sim.GetPressureField();

        // Write snapshot
        char snap_name[64];
        std::snprintf(snap_name, sizeof(snap_name),
                      "/snapshot_%03zu.csv", step);
        write_snapshot_csv(out_dir + snap_name, sc.Nx, sc.Ny, Sw, P);

        // Physical constraints
        for (size_t i = 0; i < Sw.size(); ++i) {
            REQUIRE(Sw[i] >= 0.0);
            REQUIRE(Sw[i] <= 1.0);
            REQUIRE(P[i] > 0.0);
        }

        // Mass balance from kernel accumulators
        // GetOverallBalance: {curTime, accumOil, accumOilOutFlux, accumDebet,
        //                     accumWater, accumWaterOutFlux, accumWaterDebet}
        auto bal = sim.GetOverallBalance();
        double accumOil         = bal[1];
        double accumOilOutFlux  = bal[2];
        double accumOilDebet    = bal[3];
        double accumWater       = bal[4];
        double accumWaterOutFlux = bal[5];
        double accumWaterDebet  = bal[6];

        // Balance: ΔM + outflux - debet_accum = 0
        // accumDebet = -∫(Debit*dt), debit > 0 = production, < 0 = injection
        double oil_residual   = accumOil + accumOilOutFlux - accumOilDebet;
        double water_residual = accumWater + accumWaterOutFlux - accumWaterDebet;

        // Normalize by initial mass to get relative error
        double oil_rel  = (oil_mass_0 > 0)
            ? std::abs(oil_residual) / oil_mass_0 : std::abs(oil_residual);
        double water_rel = (water_mass_0 > 0)
            ? std::abs(water_residual) / water_mass_0 : std::abs(water_residual);

        INFO("t=" << t_next
             << " oil_res=" << oil_residual << " (rel=" << oil_rel << ")"
             << " water_res=" << water_residual << " (rel=" << water_rel << ")");

        CHECK(oil_rel < balance_tol);
        CHECK(water_rel < balance_tol);

        // Water mass must increase (injection with no production)
        double water_mass = sim.WaterTotal();
        double oil_mass = sim.OilTotal();
        CHECK(water_mass > water_mass_0);

        // Log
        balance_log << std::setprecision(10)
                    << t_next << ","
                    << oil_mass << "," << water_mass << ","
                    << accumOil << "," << accumOilOutFlux << ","
                    << accumOilDebet << "," << oil_residual << ","
                    << accumWater << "," << accumWaterOutFlux << ","
                    << accumWaterDebet << "," << water_residual << "\n";
    }

    balance_log.close();
    write_metadata_json(out_dir, sc, times);
}
