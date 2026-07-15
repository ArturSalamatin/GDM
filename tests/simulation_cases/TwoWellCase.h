#pragma once

#include "SimulationCase.h"
#include "../test_helpers.h"

namespace simulation_cases {

class TwoWellCase : public SimulationCase {
public:
    size_t Nx, Ny;
    static constexpr size_t Nz = 1;
    static constexpr double Lx = 250.0, Ly = 250.0, hz = 10.0;
    static constexpr double perm_mD = 100.0, poro = 0.2;
    static constexpr double P_init_atm = 200.0;
    static constexpr double oil_saturation = 0.8;
    static constexpr double rho_oil = 800.0;
    static constexpr double rho_water = 1000.0;
    static constexpr double Q_vol = 50.0; // m3/day

    explicit TwoWellCase(size_t nx = 31, size_t ny = 31)
        : Nx(nx), Ny(ny) {}

    reservoir_simulator::DevelopedHorizon make_horizon() const override {
        return test_helpers::make_uniform_horizon(
            Nx, Ny, Nz, Lx, Ly, hz, perm_mD, poro, P_init_atm, oil_saturation);
    }

    reservoir_simulator::NumericalParameters make_num_params() const override {
        return test_helpers::default_num_params();
    }

    double ref_pressure_Pa() const override {
        return P_init_atm * 101325.0;
    }

    double initial_tau() const override { return 1.0; }

    std::vector<double> save_times() const override {
        std::vector<double> times;
        for (double t = 0.0; t <= 730.0; t += 5.0)
            times.push_back(t);
        return times;
    }

    void add_wells(reservoir_simulator::ReservoirSimulator& sim,
                   const reservoir_simulator::DevelopedHorizon& h) const override {
        double hx = Lx / Nx, hy = Ly / Ny;

        double inj_x = (Nx / 4 + 0.5) * hx;
        double inj_y = (Ny / 2 + 0.5) * hy;

        double prod_x = (3 * Nx / 4 + 0.5) * hx;
        double prod_y = (Ny / 2 + 0.5) * hy;

        test_helpers::add_simple_well(sim, h,
            "INJ", inj_x, inj_y,
            0.0, -Q_vol * rho_water);

        test_helpers::add_simple_well(sim, h,
            "PROD", prod_x, prod_y,
            Q_vol * rho_oil, 0.0);
    }

    std::string name() const override {
        return "two_well_" + std::to_string(Nx) + "x" + std::to_string(Ny);
    }

    size_t nx() const override { return Nx; }
    size_t ny() const override { return Ny; }
    double lx() const override { return Lx; }
    double ly() const override { return Ly; }

    std::vector<WellInfo> wells_info() const override {
        double hx = Lx / Nx, hy = Ly / Ny;
        double inj_x = (Nx / 4 + 0.5) * hx;
        double inj_y = (Ny / 2 + 0.5) * hy;
        double prod_x = (3 * Nx / 4 + 0.5) * hx;
        double prod_y = (Ny / 2 + 0.5) * hy;
        return {{"INJ", "injector", inj_x, inj_y},
                {"PROD", "producer", prod_x, prod_y}};
    }
};

} // namespace simulation_cases
