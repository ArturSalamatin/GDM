#include "example_runner.h"
#include "../tests/test_helpers.h"

#include <fstream>
#include <iomanip>
#include <iostream>

int main() {
    constexpr size_t Nx = 21, Ny = 21, Nz = 1;
    constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double rho_water = 1000.0, rho_oil = 800.0;
    constexpr double Q_inj_vol = 50.0;
    constexpr double Q_prod_vol = 12.5;

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
        L"INJ", 10.5 * hx, 10.5 * hy, 0.0, -Q_inj_vol * rho_water);
    test_helpers::add_simple_well(sim, horizon,
        L"P1", 0.5 * hx, 0.5 * hy, Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P2", 0.5 * hx, (Ny - 0.5) * hy, Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P3", (Nx - 0.5) * hx, 0.5 * hy, Q_prod_vol * rho_oil, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        L"P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy, Q_prod_vol * rho_oil, 0.0);

    std::vector<double> timeMoments = {0.0};
    for (double t = 5.0; t <= 500.0; t += 5.0)
        timeMoments.push_back(t);

    std::string out_dir = "results/five_spot";
    std::filesystem::create_directories(out_dir);

    auto Sw0 = sim.GetWaterSaturationField();
    auto P0 = sim.GetPressureField();
    examples::write_snapshot_csv(out_dir + "/snapshot_000.csv", Nx, Ny, Sw0, P0);

    std::cout << "[five_spot] starting..." << std::endl;

    for (size_t step = 1; step < timeMoments.size(); ++step) {
        sim.Solve({timeMoments[step - 1], timeMoments[step]});

        auto Sw = sim.GetWaterSaturationField();
        auto P = sim.GetPressureField();
        char snap_name[64];
        std::snprintf(snap_name, sizeof(snap_name), "/snapshot_%03zu.csv", step);
        examples::write_snapshot_csv(out_dir + snap_name, Nx, Ny, Sw, P);
    }

    // metadata
    {
        std::ofstream ofs(out_dir + "/metadata.json");
        ofs << "{\n"
            << "  \"case\": \"five_spot\",\n"
            << "  \"Nx\": " << Nx << ", \"Ny\": " << Ny << ",\n"
            << "  \"Lx\": " << Lx << ", \"Ly\": " << Ly << ",\n"
            << "  \"wells\": [\n"
            << "    {\"name\": \"INJ\", \"type\": \"injector\", \"x\": " << 10.5*hx << ", \"y\": " << 10.5*hy << "},\n"
            << "    {\"name\": \"P1\", \"type\": \"producer\", \"x\": " << 0.5*hx << ", \"y\": " << 0.5*hy << "},\n"
            << "    {\"name\": \"P2\", \"type\": \"producer\", \"x\": " << 0.5*hx << ", \"y\": " << (Ny-0.5)*hy << "},\n"
            << "    {\"name\": \"P3\", \"type\": \"producer\", \"x\": " << (Nx-0.5)*hx << ", \"y\": " << 0.5*hy << "},\n"
            << "    {\"name\": \"P4\", \"type\": \"producer\", \"x\": " << (Nx-0.5)*hx << ", \"y\": " << (Ny-0.5)*hy << "}\n"
            << "  ],\n"
            << "  \"save_times\": [";
        for (size_t k = 0; k < timeMoments.size(); ++k) {
            if (k > 0) ofs << ", ";
            ofs << timeMoments[k];
        }
        ofs << "]\n}\n";
    }

    std::cout << "[five_spot] done -> " << out_dir << std::endl;
    return 0;
}
