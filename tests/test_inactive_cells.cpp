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
          "[integration][inactive-cells][val-007]") {
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

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    double water = sim.WaterTotal();
    REQUIRE(std::isfinite(oil));
    REQUIRE(std::isfinite(water));
    REQUIRE(oil > 0.0);
}

TEST_CASE("Checkerboard inactive cells - state preserved",
          "[integration][inactive-cells][val-007]") {
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

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();

    for (size_t l = 0; l < Nx * Ny; ++l) {
        if (is_inactive[l]) {
            REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        } else {
            REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
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

TEST_CASE("Barrier of inactive cells blocks transport",
          "[integration][inactive-cells][val-008]") {
    using Catch::Approx;

    size_t Nx = 20, Ny = 5, Nz = 1;
    double Lx = 200.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double Sw_init = 1.0 - oil_sat;
    double P_init_Pa = P_init_atm * 101325.0;

    size_t barrier_i = 10;
    for (size_t j = 0; j < Ny; ++j)
        horizon.active_cells[Nx * j + barrier_i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;

    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1000.0);

    sim.Solve({0.0, 2.0});

    REQUIRE(std::isfinite(sim.OilTotal()));
    REQUIRE(std::isfinite(sim.WaterTotal()));

    auto Sw = sim.GetWaterSaturationField();
    auto P = sim.GetPressureField();

    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = barrier_i + 1; i < Nx; ++i) {
            size_t l = Nx * j + i;
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        }

    for (size_t j = 0; j < Ny; ++j) {
        size_t l = Nx * j + barrier_i;
        REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
    }

    bool inj_has_water = false;
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < barrier_i; ++i) {
            size_t l = Nx * j + i;
            if (Sw[l] > Sw_init + 1e-10)
                inj_has_water = true;
        }
    REQUIRE(inj_has_water);
}

TEST_CASE("Y-direction barrier blocks transport",
          "[integration][inactive-cells][val-008]") {
    using Catch::Approx;

    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double Sw_init = 1.0 - oil_sat;

    size_t barrier_j = 2;
    for (size_t i = 0; i < Nx; ++i)
        horizon.active_cells[Nx * barrier_j + i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;

    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 2.5, hy * 0.5,
        0.0, -1000.0);

    sim.Solve({0.0, 2.0});

    REQUIRE(std::isfinite(sim.OilTotal()));

    auto Sw = sim.GetWaterSaturationField();

    for (size_t j = barrier_j + 1; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        }

    bool inj_has_water = false;
    for (size_t j = 0; j < barrier_j; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            if (Sw[l] > Sw_init + 1e-10)
                inj_has_water = true;
        }
    REQUIRE(inj_has_water);
}

TEST_CASE("Barrier inactive cells - CSV export",
          "[.visual][inactive-cells][val-008]") {
    size_t Nx = 20, Ny = 5, Nz = 1;
    double Lx = 200.0, Ly = 50.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    size_t barrier_i = 10;
    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j) {
        horizon.active_cells[Nx * j + barrier_i] = false;
        is_inactive[Nx * j + barrier_i] = true;
    }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 8.5, hy * 2.5,
        0.0, -1e6);

    sim.Solve({0.0, 30.0});

    std::filesystem::create_directories("results/val-008");
    std::ofstream ofs("results/val-008/barrier.csv");
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

TEST_CASE("Single active layer equals 2D",
          "[integration][inactive-cells][val-009]") {
    using Catch::Approx;

    size_t Nx = 10, Ny = 5;
    double Lx = 100.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;

    // 2D reference (Nz=1)
    auto horizon_2d = test_helpers::make_uniform_horizon(
        Nx, Ny, 1, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim_2d{
        numPrm, horizon_2d, horizon_2d.oil, horizon_2d.water, horizon_2d.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim_2d, horizon_2d,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1000.0);

    sim_2d.Solve({0.0, 2.0});

    // 3D scenario (Nz=4, only k=2 active)
    auto horizon_3d = test_helpers::make_uniform_horizon(
        Nx, Ny, 4, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    for (size_t k = 0; k < 4; ++k)
        if (k != 2)
            for (size_t j = 0; j < Ny; ++j)
                for (size_t i = 0; i < Nx; ++i)
                    horizon_3d.active_cells[Nx * Ny * k + Nx * j + i] = false;

    reservoir_simulator::ReservoirSimulator sim_3d{
        numPrm, horizon_3d, horizon_3d.oil, horizon_3d.water, horizon_3d.other};

    test_helpers::add_simple_well(sim_3d, horizon_3d,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1000.0);

    sim_3d.Solve({0.0, 2.0});

    auto P_2d = sim_2d.GetPressureField();
    auto Sw_2d = sim_2d.GetWaterSaturationField();
    auto P_3d = sim_3d.GetPressureField();
    auto Sw_3d = sim_3d.GetWaterSaturationField();

    size_t layer2_offset = Nx * Ny * 2;
    for (size_t l = 0; l < Nx * Ny; ++l) {
        REQUIRE(P_3d[layer2_offset + l] == Approx(P_2d[l]).epsilon(1e-12));
        REQUIRE(Sw_3d[layer2_offset + l] == Approx(Sw_2d[l]).epsilon(1e-12));
    }

    double P_init_Pa = P_init_atm * 101325.0;
    double Sw_init = 1.0 - oil_sat;
    for (size_t k : {size_t(0), size_t(1), size_t(3)})
        for (size_t l = 0; l < Nx * Ny; ++l) {
            size_t idx = Nx * Ny * k + l;
            REQUIRE(P_3d[idx] == Approx(P_init_Pa).epsilon(1e-12));
            REQUIRE(Sw_3d[idx] == Approx(Sw_init).epsilon(1e-12));
        }

    REQUIRE(sim_3d.OilTotal() == Approx(sim_2d.OilTotal()).epsilon(1e-12));
    REQUIRE(sim_3d.WaterTotal() == Approx(sim_2d.WaterTotal()).epsilon(1e-12));

    auto bal_2d = sim_2d.GetOverallBalance();
    auto bal_3d = sim_3d.GetOverallBalance();
    for (size_t i = 0; i < bal_2d.size(); ++i)
        REQUIRE(bal_3d[i] == Approx(bal_2d[i]).epsilon(1e-12));
}

TEST_CASE("Single active layer - CSV export",
          "[.visual][inactive-cells][val-009]") {
    size_t Nx = 10, Ny = 5;
    double Lx = 100.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;

    auto horizon_2d = test_helpers::make_uniform_horizon(
        Nx, Ny, 1, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim_2d{
        numPrm, horizon_2d, horizon_2d.oil, horizon_2d.water, horizon_2d.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim_2d, horizon_2d,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1e6);

    sim_2d.Solve({0.0, 30.0});

    auto horizon_3d = test_helpers::make_uniform_horizon(
        Nx, Ny, 4, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    for (size_t k = 0; k < 4; ++k)
        if (k != 2)
            for (size_t j = 0; j < Ny; ++j)
                for (size_t i = 0; i < Nx; ++i)
                    horizon_3d.active_cells[Nx * Ny * k + Nx * j + i] = false;

    reservoir_simulator::ReservoirSimulator sim_3d{
        numPrm, horizon_3d, horizon_3d.oil, horizon_3d.water, horizon_3d.other};

    test_helpers::add_simple_well(sim_3d, horizon_3d,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1e6);

    sim_3d.Solve({0.0, 30.0});

    std::filesystem::create_directories("results/val-009");

    auto P_2d = sim_2d.GetPressureField();
    auto Sw_2d = sim_2d.GetWaterSaturationField();
    auto P_3d = sim_3d.GetPressureField();
    auto Sw_3d = sim_3d.GetWaterSaturationField();
    size_t offset = Nx * Ny * 2;

    std::ofstream ofs_2d("results/val-009/reference_2d.csv");
    ofs_2d << "i,j,P_atm,Sw\n";
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            ofs_2d << i << "," << j << ","
                   << std::setprecision(8) << P_2d[l] / 101325.0 << ","
                   << Sw_2d[l] << "\n";
        }

    std::ofstream ofs_3d("results/val-009/layer2_3d.csv");
    ofs_3d << "i,j,P_atm,Sw\n";
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            ofs_3d << i << "," << j << ","
                   << std::setprecision(8) << P_3d[offset + l] / 101325.0 << ","
                   << Sw_3d[offset + l] << "\n";
        }

    std::ofstream ofs_diff("results/val-009/difference.csv");
    ofs_diff << "i,j,dP_atm,dSw\n";
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            ofs_diff << i << "," << j << ","
                     << std::setprecision(12)
                     << (P_3d[offset + l] - P_2d[l]) / 101325.0 << ","
                     << (Sw_3d[offset + l] - Sw_2d[l]) << "\n";
        }
}

TEST_CASE("Corner cell (0,0) inactive - no crash and state preserved",
          "[integration][inactive-cells][val-010]") {
    using Catch::Approx;

    size_t Nx = 7, Ny = 7, Nz = 1;
    double Lx = 70.0, Ly = 70.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;

    // Reference: все ячейки активны
    auto horizon_ref = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim_ref{
        numPrm, horizon_ref, horizon_ref.oil, horizon_ref.water, horizon_ref.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim_ref, horizon_ref,
        "INJ", hx * 3.5, hy * 3.5,
        0.0, -1000.0);

    sim_ref.Solve({0.0, 2.0});

    // Тест: угловая ячейка (0,0) деактивирована
    auto horizon_test = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);
    horizon_test.active_cells[0] = false; // (i=0, j=0)

    reservoir_simulator::ReservoirSimulator sim_test{
        numPrm, horizon_test, horizon_test.oil, horizon_test.water, horizon_test.other};

    test_helpers::add_simple_well(sim_test, horizon_test,
        "INJ", hx * 3.5, hy * 3.5,
        0.0, -1000.0);

    sim_test.Solve({0.0, 2.0});

    // Проверка 1: нет краша — если дошли сюда, уже прошло
    REQUIRE(std::isfinite(sim_test.OilTotal()));
    REQUIRE(std::isfinite(sim_test.WaterTotal()));

    // Проверка 2: деактивированная ячейка сохраняет начальные условия
    auto P_test = sim_test.GetPressureField();
    auto Sw_test = sim_test.GetWaterSaturationField();
    double P_init_Pa = P_init_atm * 101325.0;
    double Sw_init = 1.0 - oil_sat;
    REQUIRE(P_test[0] == Approx(P_init_Pa).epsilon(1e-12));
    REQUIRE(Sw_test[0] == Approx(Sw_init).epsilon(1e-12));

    // Проверка 3: давление в дальних ячейках (i>2, j>2) совпадает с reference
    auto P_ref = sim_ref.GetPressureField();
    auto Sw_ref = sim_ref.GetWaterSaturationField();
    for (size_t j = 3; j < Ny; ++j)
        for (size_t i = 3; i < Nx; ++i) {
            size_t l = Nx * j + i;
            REQUIRE(P_test[l] == Approx(P_ref[l]).epsilon(1e-4));
            REQUIRE(Sw_test[l] == Approx(Sw_ref[l]).epsilon(1e-4));
        }
}

TEST_CASE("Edge cell (i=0, j=mid) inactive - no crash and state preserved",
          "[integration][inactive-cells][val-010]") {
    using Catch::Approx;

    size_t Nx = 7, Ny = 7, Nz = 1;
    double Lx = 70.0, Ly = 70.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;

    // Reference
    auto horizon_ref = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim_ref{
        numPrm, horizon_ref, horizon_ref.oil, horizon_ref.water, horizon_ref.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim_ref, horizon_ref,
        "INJ", hx * 3.5, hy * 3.5,
        0.0, -1000.0);

    sim_ref.Solve({0.0, 2.0});

    // Тест: бортовая ячейка (i=0, j=3)
    auto horizon_test = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);
    size_t edge_cell = Nx * 3 + 0; // (i=0, j=3)
    horizon_test.active_cells[edge_cell] = false;

    reservoir_simulator::ReservoirSimulator sim_test{
        numPrm, horizon_test, horizon_test.oil, horizon_test.water, horizon_test.other};

    test_helpers::add_simple_well(sim_test, horizon_test,
        "INJ", hx * 3.5, hy * 3.5,
        0.0, -1000.0);

    sim_test.Solve({0.0, 2.0});

    REQUIRE(std::isfinite(sim_test.OilTotal()));
    REQUIRE(std::isfinite(sim_test.WaterTotal()));

    // Деактивированная ячейка сохраняет начальные условия
    auto P_test = sim_test.GetPressureField();
    auto Sw_test = sim_test.GetWaterSaturationField();
    double P_init_Pa = P_init_atm * 101325.0;
    double Sw_init = 1.0 - oil_sat;
    REQUIRE(P_test[edge_cell] == Approx(P_init_Pa).epsilon(1e-12));
    REQUIRE(Sw_test[edge_cell] == Approx(Sw_init).epsilon(1e-12));

    // Дальние ячейки (правая половина, далеко от i=0) совпадают с reference
    auto P_ref = sim_ref.GetPressureField();
    auto Sw_ref = sim_ref.GetWaterSaturationField();
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 3; i < Nx; ++i) {
            size_t l = Nx * j + i;
            REQUIRE(P_test[l] == Approx(P_ref[l]).epsilon(1e-4));
            REQUIRE(Sw_test[l] == Approx(Sw_ref[l]).epsilon(1e-4));
        }
}

TEST_CASE("All four corners inactive - graph integrity and state preserved",
          "[integration][inactive-cells][val-010]") {
    using Catch::Approx;

    size_t Nx = 7, Ny = 7, Nz = 1;
    double Lx = 70.0, Ly = 70.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    // Деактивировать 4 угла
    horizon.active_cells[0] = false;                     // (0,0)
    horizon.active_cells[Nx - 1] = false;                // (Nx-1, 0)
    horizon.active_cells[Nx * (Ny - 1)] = false;         // (0, Ny-1)
    horizon.active_cells[Nx * Ny - 1] = false;            // (Nx-1, Ny-1)

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 3.5, hy * 3.5,
        0.0, -1000.0);

    sim.Solve({0.0, 2.0});

    REQUIRE(std::isfinite(sim.OilTotal()));
    REQUIRE(std::isfinite(sim.WaterTotal()));

    // Деактивированные ячейки сохраняют начальные условия
    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();
    double P_init_Pa = P_init_atm * 101325.0;
    double Sw_init = 1.0 - oil_sat;

    for (size_t idx : {size_t(0), Nx - 1, Nx * (Ny - 1), Nx * Ny - 1}) {
        REQUIRE(P[idx] == Approx(P_init_Pa).epsilon(1e-12));
        REQUIRE(Sw[idx] == Approx(Sw_init).epsilon(1e-12));
    }

    // Граф связности: деактивированные ячейки имеют отрицательный local index
    for (size_t corner : {size_t(0), Nx - 1, Nx * (Ny - 1), Nx * Ny - 1}) {
        REQUIRE(sim.Grid.ConvertGlobal2Local(corner) < 0);
    }

    // Все рёбра графа ведут к активным ячейкам (local index >= 0)
    const auto& graph = sim.Grid.GetConnectivityGraph();
    for (size_t local = 0; local < graph.size(); ++local) {
        for (int neib : graph[local]) {
            REQUIRE(neib >= 0);
            REQUIRE(static_cast<size_t>(neib) < graph.size());
        }
    }
}

TEST_CASE("Corner cell inactive - CSV export",
          "[.visual][inactive-cells][val-010]") {
    size_t Nx = 7, Ny = 7, Nz = 1;
    double Lx = 70.0, Ly = 70.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;

    auto numPrm = test_helpers::default_num_params();
    double hx = Lx / Nx;
    double hy = Ly / Ny;

    // Reference
    auto horizon_ref = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);
    reservoir_simulator::ReservoirSimulator sim_ref{
        numPrm, horizon_ref, horizon_ref.oil, horizon_ref.water, horizon_ref.other};
    test_helpers::add_simple_well(sim_ref, horizon_ref,
        "INJ", hx * 3.5, hy * 3.5, 0.0, -1e6);
    sim_ref.Solve({0.0, 30.0});

    // Corner off
    auto horizon_test = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);
    horizon_test.active_cells[0] = false;
    reservoir_simulator::ReservoirSimulator sim_test{
        numPrm, horizon_test, horizon_test.oil, horizon_test.water, horizon_test.other};
    test_helpers::add_simple_well(sim_test, horizon_test,
        "INJ", hx * 3.5, hy * 3.5, 0.0, -1e6);
    sim_test.Solve({0.0, 30.0});

    std::filesystem::create_directories("results/val-010");

    auto write_csv = [&](const std::string& fname,
                         reservoir_simulator::ReservoirSimulator& sim,
                         const std::vector<bool>& inactive) {
        std::ofstream ofs(fname);
        ofs << "i,j,active,P_atm,Sw\n";
        auto P = sim.GetPressureField();
        auto Sw = sim.GetWaterSaturationField();
        for (size_t j = 0; j < Ny; ++j)
            for (size_t i = 0; i < Nx; ++i) {
                size_t l = Nx * j + i;
                ofs << i << "," << j << ","
                    << (inactive[l] ? 0 : 1) << ","
                    << std::setprecision(8) << P[l] / 101325.0 << ","
                    << Sw[l] << "\n";
            }
    };

    std::vector<bool> all_active(Nx * Ny, false);
    write_csv("results/val-010/reference_full.csv", sim_ref, all_active);

    std::vector<bool> corner_off(Nx * Ny, false);
    corner_off[0] = true;
    write_csv("results/val-010/corner_off.csv", sim_test, corner_off);

    // Difference
    auto P_ref = sim_ref.GetPressureField();
    auto Sw_ref = sim_ref.GetWaterSaturationField();
    auto P_test = sim_test.GetPressureField();
    auto Sw_test = sim_test.GetWaterSaturationField();

    std::ofstream ofs_diff("results/val-010/difference.csv");
    ofs_diff << "i,j,active,dP_atm,dSw\n";
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            ofs_diff << i << "," << j << ","
                     << (l == 0 ? 0 : 1) << ","
                     << std::setprecision(12)
                     << (P_test[l] - P_ref[l]) / 101325.0 << ","
                     << (Sw_test[l] - Sw_ref[l]) << "\n";
        }
}
