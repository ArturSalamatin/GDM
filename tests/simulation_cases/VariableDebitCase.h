#pragma once

#include "SimulationCase.h"
#include "../well_schedule_builder.h"
#include <functional>
#include <string>

namespace simulation_cases {

class VariableDebitCase : public SimulationCase {
public:
    using BuilderSetup = std::function<
        std::vector<test_helpers::WellScheduleBuilder>(double hx, double hy)>;

    size_t Nx_, Ny_;
    static constexpr size_t Nz = 1;
    double Lx_, Ly_;
    static constexpr double hz = 10.0;
    static constexpr double perm_mD = 100.0;
    static constexpr double poro = 0.2;
    static constexpr double P_init_atm = 200.0;
    static constexpr double oil_saturation = 0.8;

    double total_time_;
    double snapshot_dt_;
    std::string case_name_;
    BuilderSetup setup_;
    std::vector<WellInfo> wells_info_;

    VariableDebitCase(const std::string& case_name,
                      size_t nx, size_t ny,
                      double lx, double ly,
                      double total_time, double snapshot_dt,
                      BuilderSetup setup,
                      std::vector<WellInfo> wells)
        : Nx_(nx), Ny_(ny), Lx_(lx), Ly_(ly),
          total_time_(total_time), snapshot_dt_(snapshot_dt),
          case_name_(case_name), setup_(std::move(setup)),
          wells_info_(std::move(wells)) {}

    reservoir_simulator::DevelopedHorizon make_horizon() const override {
        return test_helpers::make_uniform_horizon(
            Nx_, Ny_, Nz, Lx_, Ly_, hz, perm_mD, poro, P_init_atm, oil_saturation);
    }

    reservoir_simulator::NumericalParameters make_num_params() const override {
        return test_helpers::default_num_params();
    }

    double ref_pressure_Pa() const override {
        return P_init_atm * 101325.0;
    }

    double initial_tau() const override { return 5.0; }

    std::vector<double> save_times() const override {
        std::vector<double> times;
        for (double t = 0.0; t <= total_time_; t += snapshot_dt_)
            times.push_back(t);
        if (times.back() < total_time_)
            times.push_back(total_time_);
        return times;
    }

    void add_wells(reservoir_simulator::ReservoirSimulator& sim,
                   const reservoir_simulator::DevelopedHorizon& h) const override {
        double hx = Lx_ / Nx_, hy = Ly_ / Ny_;
        auto builders = setup_(hx, hy);
        for (auto& b : builders)
            b.set_hz(hz).add_to_sim(sim, h);
    }

    std::string name() const override { return case_name_; }

    size_t nx() const override { return Nx_; }
    size_t ny() const override { return Ny_; }
    double lx() const override { return Lx_; }
    double ly() const override { return Ly_; }

    std::vector<WellInfo> wells_info() const override { return wells_info_; }
};

} // namespace simulation_cases
