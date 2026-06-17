#pragma once

#include "../../HydroSolver/Solver/Grids/DevelopedHorizon.h"
#include "../../HydroSolver/Reservoir/ReservoirSimulator.h"
#include "../../HydroSolver/Reservoir/NumericalParameters.h"

#include <string>
#include <vector>

namespace simulation_cases {

struct SimulationCase {
    virtual ~SimulationCase() = default;

    virtual reservoir_simulator::DevelopedHorizon make_horizon() const = 0;
    virtual reservoir_simulator::NumericalParameters make_num_params() const = 0;
    virtual double ref_pressure_Pa() const = 0;
    virtual double initial_tau() const = 0;
    virtual std::vector<double> save_times() const = 0;
    virtual void add_wells(reservoir_simulator::ReservoirSimulator& sim,
                           const reservoir_simulator::DevelopedHorizon& h) const = 0;
    virtual std::string name() const = 0;
};

} // namespace simulation_cases
