#pragma once

#include "../HydroSolver/stdafx.h"
#include "../HydroSolver/Solver/Grids/DevelopedHorizon.h"
#include "../HydroSolver/Reservoir/ReservoirSimulator.h"
#include "../HydroSolver/Reservoir/NumericalParameters.h"
#include "../HydroSolver/Data/PhaseFactory.hpp"
#include "../HydroSolver/Data/ExceptionFactory.h"

namespace test_helpers {

inline reservoir_simulator::DevelopedHorizon make_uniform_horizon(
    size_t Nx, size_t Ny, size_t Nz,
    double Lx, double Ly, double hz,
    double perm_mD, double poro,
    double P_init_atm, double oil_saturation)
{
    const size_t N = Nx * Ny * Nz;
    const double hx = Lx / Nx;
    const double hy = Ly / Ny;
    const double perm_SI = perm_mD * 0.9869e-15;
    const double P_init_Pa = P_init_atm * 101325.0;

    reservoir_simulator::DevelopedHorizon h;

    h.grid_size = {Nx, Ny, Nz};
    h.grid_shift = {0.0f, 0.0f};
    h.block_size = {hx, hy};
    h.grid_bounds = reservoir_simulator::GridBounds{0.0, 0.0, Lx, Ly};

    h.volume.assign(N, hx * hy * hz);
    h.x_center.resize(N);
    h.y_center.resize(N);
    h.z_center.resize(N);
    h.cell_thickness.assign(N, hz);

    for (size_t k = 0; k < Nz; ++k)
        for (size_t j = 0; j < Ny; ++j)
            for (size_t i = 0; i < Nx; ++i) {
                size_t l = Nx * Ny * k + Nx * j + i;
                h.x_center[l] = (i + 0.5) * hx;
                h.y_center[l] = (j + 0.5) * hy;
                h.z_center[l] = (k + 0.5) * hz;
            }

    h.porosity.assign(N, poro);
    h.permeability_x.assign(N, perm_SI);
    h.permeability_y.assign(N, perm_SI);
    h.active_cells.assign(N, true);
    h.initial_oil_saturation.assign(N, oil_saturation);
    h.initial_pressure.assign(N, P_init_Pa);

    h.oil = reservoir_simulator::factories::PhaseFactory::CreateDefaultOil();
    h.water = reservoir_simulator::factories::PhaseFactory::CreateDefaultWater();
    h.other = reservoir_simulator::factories::OtherFactory::CreateDefaultOthers();
    h.other.extPressure = P_init_Pa;

    return h;
}

inline reservoir_simulator::NumericalParameters default_num_params()
{
    return reservoir_simulator::NumericalParameters(1e-6, 65, 1e-5, 1e-5);
}

} // namespace test_helpers
