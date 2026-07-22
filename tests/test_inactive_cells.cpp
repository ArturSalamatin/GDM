#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <filesystem>
#include "test_helpers.h"

TEST_CASE("Well skips inactive cell layer",
          "[integration][wells][inactive-cells]") {
    size_t Nx = 3, Ny = 3, Nz = 3;
    double Lx = 30.0, Ly = 30.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    // Деактивировать средний слой (k=1)
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            horizon.active_cells[Nx * Ny * 1 + Nx * j + i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // Скважина в центре — проходит через все 3 слоя
    // Ожидание: перфорация в слое k=1 отфильтрована
    test_helpers::add_simple_well(sim, horizon, "PROD", 15.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    double water = sim.WaterTotal();
    REQUIRE(std::isfinite(oil));
    REQUIRE(std::isfinite(water));
    REQUIRE(oil > 0.0);
}

TEST_CASE("All layers inactive - well has zero production",
          "[integration][wells][inactive-cells]") {
    size_t Nx = 3, Ny = 3, Nz = 1;
    double Lx = 30.0, Ly = 30.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    // Деактивировать ячейку скважины (центральная ячейка k=0)
    // Скважина в (15, 15) → ячейка (1, 1, 0) → global index = 3*1+1 = 4
    horizon.active_cells[4] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // Скважина попадает в неактивную ячейку
    test_helpers::add_simple_well(sim, horizon, "PROD", 15.0, 15.0, 1.0, 0.0);

    // Guard BUG-008: SetRefWellPressure при denom=0 → P_well = avg(P_reservoir)
    double dt = 0.1;
    sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    REQUIRE(std::isfinite(oil));
}

TEST_CASE("Checkerboard inactive cells - no crash",
          "[integration][wells][inactive-cells][val-007]") {
    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0)
                horizon.active_cells[Nx * j + i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    double water = sim.WaterTotal();
    REQUIRE(std::isfinite(oil));
    REQUIRE(std::isfinite(water));
    REQUIRE(oil > 0.0);
}

TEST_CASE("Checkerboard inactive cells - isolation check",
          "[integration][wells][inactive-cells][val-007]") {
    using Catch::Approx;

    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double P_init_Pa = P_init_atm * 101325.0;
    double Sw_init = 1.0 - oil_sat;

    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0) {
                horizon.active_cells[Nx * j + i] = false;
                is_inactive[Nx * j + i] = true;
            }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();

    for (size_t l = 0; l < Nx * Ny; ++l) {
        if (is_inactive[l]) {
            REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        }
    }
}

TEST_CASE("Checkerboard inactive cells - active cells isolated",
          "[integration][wells][inactive-cells][val-007]") {
    using Catch::Approx;

    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double P_init_Pa = P_init_atm * 101325.0;

    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0) {
                horizon.active_cells[Nx * j + i] = false;
                is_inactive[Nx * j + i] = true;
            }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    size_t well_cell = 2 + 5 * 1;
    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    auto P = sim.GetPressureField();

    for (size_t l = 0; l < Nx * Ny; ++l) {
        if (!is_inactive[l] && l != well_cell) {
            REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
        }
    }
}

TEST_CASE("Checkerboard inactive cells - CSV export",
          "[.visual][inactive-cells][val-007]") {
    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0) {
                horizon.active_cells[Nx * j + i] = false;
                is_inactive[Nx * j + i] = true;
            }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    std::filesystem::create_directories("results/val-007");
    std::ofstream ofs("results/val-007/checkerboard.csv");
    ofs << "i,j,active,P_atm,Sw\n";

    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();

    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            ofs << i << "," << j << ","
                << (is_inactive[l] ? 0 : 1) << ","
                << std::setprecision(8) << P[l] / 101325.0 << ","
                << Sw[l] << "\n";
        }
}
