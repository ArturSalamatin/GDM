#pragma once

#include "../HydroSolver/stdafx.h"
#include "../HydroSolver/Solver/Grids/DevelopedHorizon.h"
#include "../HydroSolver/Reservoir/ReservoirSimulator.h"
#include "../HydroSolver/Reservoir/NumericalParameters.h"
#include "../HydroSolver/Data/PhaseFactory.hpp"
#include "../HydroSolver/Data/ExceptionFactory.h"
#include "../HydroSolver/Data/wells/GeosPoint.h"
#include "../HydroSolver/Reservoir/Well/WellJobs.h"
#include "../HydroSolver/Reservoir/Well/SetOfPoints.h"

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

// Добавить скважину с постоянным дебитом в симулятор.
// oil_mass_rate, water_mass_rate: кг/день. Положительный = добыча, отрицательный = закачка.
// MER задаёт одну запись с MER-периодом mer_period_days.
// Перфорация: один слой, полная глубина, открыта с t=0.
inline void add_simple_well(
    reservoir_simulator::ReservoirSimulator& sim,
    const reservoir_simulator::DevelopedHorizon& horizon,
    const reservoir_simulator::WellName& name,
    double x, double y,
    double oil_mass_rate,
    double water_mass_rate,
    double r_app = 0.0)
{
    double hz = horizon.cell_thickness[0];
    double nz = static_cast<double>(horizon.grid_size.Nz);

    // MER использует time как конец месячного периода.
    // identify_cur_MER_record: curTime попадает в [firstRecordDate - monthGap, firstRecordDate).
    // monthGap ≈ 30.74 дней. Чтобы покрыть t=0..T, создаём записи с шагом ~30.74 дней,
    // начиная с time=30.74 (покрывает t ∈ [0, 30.74)).
    constexpr double month = 30.74;
    constexpr int n_records = 400;
    reservoir_simulator::mer_descriptor::SingleWell_MER_Data mer_data;
    for (int k = 0; k < n_records; ++k) {
        reservoir_simulator::mer_descriptor::SingleMERrecord rec;
        rec[L"time"] = static_cast<float>((k + 1) * month);
        rec[L"oil_m"] = static_cast<float>(oil_mass_rate * month);
        rec[L"water_m"] = static_cast<float>(water_mass_rate * month);
        rec[L"oil_v"] = static_cast<float>(oil_mass_rate * month / 800.0);
        rec[L"water_v"] = static_cast<float>(water_mass_rate * month / 1000.0);
        rec[L"type"] = 1.0f;
        rec[L"is_work"] = 1.0f;
        rec[L"worked_time"] = static_cast<float>(month);
        rec[L"idle_time"] = 0.0f;
        rec[L"pump_water"] = 0.0f;
        mer_data.push_back(rec);
    }

    // Перфорация: один слой Nz=1, открыта на всю глубину, с t=0.
    reservoir_simulator::JobsInLayer jobs_in_layer;
    jobs_in_layer.emplace_back(0.0, nz * hz, true, 0.0);
    reservoir_simulator::WellJobs well_jobs(name, jobs_in_layer);

    reservoir_simulator::WellPosition pos(x, y);

    // Peaceman radius ≈ 0.2*dx если не задан
    double effective_r_app = r_app;
    if (effective_r_app <= 0.0)
        effective_r_app = 0.2 * horizon.block_size.step_x;

    sim.AddWell_FixedProduction(
        name, mer_data, well_jobs, pos,
        horizon.grid_bounds, horizon.block_size, effective_r_app);
}

} // namespace test_helpers
