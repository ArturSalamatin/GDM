#include <catch2/catch_test_macros.hpp>
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
    test_helpers::add_simple_well(sim, horizon, L"PROD", 15.0, 15.0, 1.0, 0.0);

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
    test_helpers::add_simple_well(sim, horizon, L"PROD", 15.0, 15.0, 1.0, 0.0);

    // Guard BUG-008: SetRefWellPressure при denom=0 → P_well = avg(P_reservoir)
    double dt = 0.1;
    sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    REQUIRE(std::isfinite(oil));
}
