---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-22
issue: VAL-010
github: 51
branch: val/val-010/inactive-boundary-cell
status: в процессе
audit:
  date: 2026-07-22
  findings: 0 / 1 / 1
  auto-fixed: 2
  manual-required: 0
---

# VAL-010: Неактивная граничная (угловая) ячейка

## Контекст

VAL-009 показал, что деактивация целых слоёв (k-wise) работает корректно: 3D-задача с одним активным слоем даёт побитово идентичные результаты с 2D-задачей. VAL-010 проверяет более тонкий сценарий — деактивацию **единичных** ячеек на **границе** сетки: углы (2 соседа в 2D) и бортовые ячейки (3 соседа в 2D).

Угловые/бортовые ячейки — особый случай в `SetConnectivityGraph_3D` ([AbstractGrid.h:209-276](HydroSolver/Solver/Grids/AbstractGrid.h#L209-L276)): для них ветвления `i > 0`, `j > 0`, `i < Nx-1`, `j < Ny-1` уже отсекают часть соседей. Деактивация такой ячейки исключает дополнительные рёбра графа, и ошибка может проявиться как:
- неверная индексация в `cell_idx_Global2Local` (возврат -1 для деактивированной ячейки)
- некорректный WI, если скважина перфорирована в соседней ячейке
- нарушение баланса масс
- краш (out-of-bounds при доступе к `Cells[]`)

**Подтип:** инвариантный (self-consistency + баланс масс).

**Эталон:** сравнение с прогоном на полной сетке (все ячейки активны). Угловая ячейка далеко от скважины → её отключение не должно существенно менять динамику.

## Связанные заметки

- [[сетка 3D структурированная с линейной индексацией]] — индексация, `IsCellActive`, `connectivityGraph`
- [[BUG-009 active cells filter]] — исправленный баг фильтрации неактивных ячеек (size_t + 1 > 0)
- [[val-009 single-active-layer]] — предыдущий VAL: слойная деактивация

## Зависимости

- VAL-009 (✅ пройден) — базовый механизм неактивных ячеек работает
- Блокирующих BUG/DEBT нет

## Зона неприкосновенности

Эти файлы/тесты **не должны** модифицироваться:
- `HydroSolver/` — ядро симулятора
- `tests/test_helpers.h` — тестовая инфраструктура
- Все существующие TEST_CASE в `tests/test_inactive_cells.cpp` (добавляем **новые**, не трогаем старые)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[сетка 3D структурированная с линейной индексацией]]
3. Ознакомься с существующими тестами: [test_inactive_cells.cpp](tests/test_inactive_cells.cpp) — паттерны тестов VAL-007..009
4. Ознакомься с API: [test_helpers.h](tests/test_helpers.h) — `make_uniform_horizon`, `add_simple_well`, `default_num_params`
5. Ознакомься с графом связности: [AbstractGrid.h:209-276](HydroSolver/Solver/Grids/AbstractGrid.h#L209-L276) — `SetConnectivityGraph_3D`
6. Создай ветку: `git checkout -b val/val-010/inactive-boundary-cell`
7. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
8. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
9. Запомни количество тестов и время — это baseline
10. Начни с шага 1. После каждого шага: сборка + тесты

---

## Шаг 1: Количественный тест — угловая ячейка (0,0) деактивирована

**Цель:** убедиться, что деактивация угловой ячейки (i=0, j=0) не вызывает краш и сохраняет начальные условия в деактивированной ячейке.

**Файлы:** `tests/test_inactive_cells.cpp` (добавить в конец, перед закрывающей `}` если есть, или после последнего TEST_CASE)

**Контекст:**

Угловая ячейка (i=0, j=0) на 2D-сетке имеет ровно 2 соседа: (1,0) и (0,1). При деактивации:
- `cell_idx_Global2Local[0]` = -1 (отрицательное)
- `connectivityGraph` не содержит рёбер к/от ячейки 0
- Ячейки (1,0) и (0,1) теряют одного соседа каждая
- Скважину ставим далеко от угла → влияние деактивации на динамику минимально

Логика построения графа в `SetConnectivityGraph_3D`:
- Для ячейки (0,0): `i > 0` → false, `j > 0` → false → пропускаются рёбра -X и -Y
- Для ячейки (1,0): `active_cells[l-1]` = false → ребро к (0,0) пропускается
- Для ячейки (0,1): `active_cells[l-Nx]` = false → ребро к (0,0) пропускается

Проверяем три инварианта:
1. **Нет краша** — симуляция завершается без ошибок
2. **Давление и Sw в «дальних» ячейках** — не отличаются значимо от full-grid (epsilon = 1e-4 относительно)
3. **Деактивированная ячейка сохраняет начальные условия** (P = P_init, Sw = Sw_init)

Примечание: **нельзя** сравнивать `GetOverallBalance()` между двумя симуляциями с разным объёмом пласта. Деактивация ячейки уменьшает объём на ~2% (1 из 49), что меняет дебиты и накопления. Баланс масс (accumOil == accumOilOutFlux + accumDebet) — свойство **одной** симуляции, а не сравнение **между** двумя.

Для самосогласованности: ставим скважину в центр сетки (далеко от угла). Сетка 7×7 — достаточно большая, чтобы угловая ячейка не влияла.

**Что сделать:**

Добавить TEST_CASE в `tests/test_inactive_cells.cpp`:

```cpp
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
    // Инжектор в центре сетки
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
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все существующие тесты зелёные + новый тест зелёный

**Подводные камни:**
- ✅ `active_cells[0] = false` → `cell_idx_Global2Local[0]` < 0. Логика `SetConnectivityGraph_3D` корректно пропускает эту ячейку (проверено при чтении кода)
- ⚠️ Epsilon для дальних ячеек: 1e-4 — нестрогий, потому что деактивация угловой ячейки меняет объём пласта (одна ячейка из 49 → ~2% объёма). При сильном влиянии можно ослабить до 1e-2

**Зависимости:**
- Требует: baseline (шаг 0: ветка + зелёные тесты)
- Блокирует: шаг 3 (CSV export)

**Оценка:** ~60 строк, ~15 минут

---

## Шаг 2: Количественный тест — бортовая ячейка (i=0, j=mid) деактивирована

**Цель:** проверить деактивацию бортовой ячейки (3 соседа) — дополнительный сценарий к угловой (2 соседа).

**Файлы:** `tests/test_inactive_cells.cpp`

**Контекст:**

Бортовая ячейка (i=0, j=3) на сетке 7×7 имеет 3 соседа: (1,3), (0,2), (0,4). Это ячейка на левом краю в середине по Y. У неё нет соседа по -X (i=0), но есть соседи по ±Y и +X.

В отличие от угловой ячейки, бортовая ячейка ближе к центру по Y → может сильнее влиять на динамику, если скважина ставится по линии j=3.

Ставим скважину в (i=3.5, j=3.5) — центр сетки. Бортовая ячейка (0,3) — далеко от скважины по X.

```cpp
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
```

**Проверка после этого шага:**
- Сборка + тесты, все зелёные

**Подводные камни:**
- ✅ Аналогично шагу 1 — логика `SetConnectivityGraph_3D` для i=0 пропускает ребро -X
- ⚠️ Дальние ячейки: фильтруем по `i >= 3` (правая половина сетки — далеко от бортовой ячейки i=0)

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 3

**Оценка:** ~55 строк, ~10 минут

---

## Шаг 3: Количественный тест — все 4 угла деактивированы

**Цель:** проверить одновременную деактивацию всех 4 угловых ячеек — стресс-тест на симметрию и баланс.

**Файлы:** `tests/test_inactive_cells.cpp`

**Контекст:**

Четыре угловые ячейки на сетке 7×7:
- (0,0) → global index 0
- (6,0) → global index 6
- (0,6) → global index 42
- (6,6) → global index 48

Каждая имеет по 2 соседа. Скважина в центре. Симметрия задачи: все 4 угла одинаково далеки от скважины → давление и Sw должны быть симметричны.

```cpp
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
    // ConvertGlobal2Local — публичный метод SomeGrid
    for (size_t corner : {size_t(0), Nx - 1, Nx * (Ny - 1), Nx * Ny - 1}) {
        REQUIRE(sim.Grid.ConvertGlobal2Local(corner) < 0);
    }

    // Все рёбра графа ведут к активным ячейкам (local index ≥ 0)
    const auto& graph = sim.Grid.GetConnectivityGraph();
    for (size_t local = 0; local < graph.size(); ++local) {
        for (int neib : graph[local]) {
            REQUIRE(neib >= 0);
            REQUIRE(static_cast<size_t>(neib) < graph.size());
        }
    }
}
```

**Проверка после этого шага:**
- Сборка + тесты, все зелёные

**Подводные камни:**
- ✅ `cell_idx_Global2Local` и `cell_idx_Local2Global` — protected поля, недоступны извне. Используем публичные API: `ConvertGlobal2Local(idx)` для проверки деактивации, `GetConnectivityGraph()` для проверки рёбер. Проверка «рёбра не ведут на деактивированные» — через инвариант: все neib в графе ≥ 0 и < graph.size()

**Зависимости:**
- Требует: шаг 1, шаг 2
- Блокирует: шаг 4

**Оценка:** ~55 строк, ~10 минут

---

## Шаг 4: Визуальный тест — CSV export для угловой ячейки

**Цель:** экспорт полей P и Sw для визуальной верификации (overlay reference vs corner-off).

**Файлы:** `tests/test_inactive_cells.cpp`

**Контекст:**

По паттерну VAL-007..009: тест с тегом `[.visual]` не запускается по умолчанию, только явно:
```
ctest --test-dir build -C Release -R "Corner cell.*CSV"
```

Формат CSV: `i,j,active,P_atm,Sw` — тот же, что в checkerboard и barrier тестах.

Два файла:
- `results/val-010/reference_full.csv` — полная сетка
- `results/val-010/corner_off.csv` — угловая ячейка деактивирована

```cpp
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
```

**Проверка после этого шага:**
- Сборка зелёная
- Запустить visual-тест: `ctest --test-dir build -C Release -R "Corner cell.*CSV"`
- Проверить, что файлы `results/val-010/*.csv` созданы

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 5

**Оценка:** ~70 строк, ~15 минут

---

## Шаг 5: Python-скрипт визуализации

**Цель:** overlay plot: full-grid vs corner-off + карта разности.

**Файлы:** `scripts/plot_val010.py` (новый файл)

**Контекст:**

Паттерн — `scripts/plot_val009.py`. Три колонки × две строки: P_ref, P_test, ΔP; Sw_ref, Sw_test, ΔSw. На карте ΔP/ΔSw деактивированная ячейка помечена (чёрный квадрат или hatching).

```python
"""
Визуализация VAL-010: full-grid vs corner-off.

Использование:
    python scripts/plot_val010.py --results-dir results/val-010

Зависимости: numpy, matplotlib
"""
import argparse
import os
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", required=True)
    args = parser.parse_args()

    ref = np.genfromtxt(os.path.join(args.results_dir, "reference_full.csv"),
                        delimiter=",", skip_header=1)
    test = np.genfromtxt(os.path.join(args.results_dir, "corner_off.csv"),
                         delimiter=",", skip_header=1)
    diff = np.genfromtxt(os.path.join(args.results_dir, "difference.csv"),
                         delimiter=",", skip_header=1)

    nx = int(ref[:, 0].max()) + 1
    ny = int(ref[:, 1].max()) + 1

    P_ref = ref[:, 3].reshape(ny, nx)
    Sw_ref = ref[:, 4].reshape(ny, nx)
    P_test = test[:, 3].reshape(ny, nx)
    Sw_test = test[:, 4].reshape(ny, nx)
    active = diff[:, 2].reshape(ny, nx)
    dP = diff[:, 3].reshape(ny, nx)
    dSw = diff[:, 4].reshape(ny, nx)

    fig, axes = plt.subplots(2, 3, figsize=(18, 10))

    im = axes[0, 0].imshow(P_ref, origin="lower", cmap="viridis", aspect="auto")
    axes[0, 0].set_title("P [atm] — full grid")
    plt.colorbar(im, ax=axes[0, 0])

    im = axes[0, 1].imshow(P_test, origin="lower", cmap="viridis", aspect="auto")
    axes[0, 1].set_title("P [atm] — corner (0,0) off")
    plt.colorbar(im, ax=axes[0, 1])
    axes[0, 1].add_patch(Rectangle((-0.5, -0.5), 1, 1,
                                    fill=True, color="black"))

    im = axes[0, 2].imshow(dP, origin="lower", cmap="RdBu_r", aspect="auto")
    axes[0, 2].set_title("ΔP [atm]")
    plt.colorbar(im, ax=axes[0, 2], format="%.2e")

    im = axes[1, 0].imshow(Sw_ref, origin="lower", cmap="Blues", aspect="auto")
    axes[1, 0].set_title("Sw — full grid")
    plt.colorbar(im, ax=axes[1, 0])

    im = axes[1, 1].imshow(Sw_test, origin="lower", cmap="Blues", aspect="auto")
    axes[1, 1].set_title("Sw — corner (0,0) off")
    plt.colorbar(im, ax=axes[1, 1])
    axes[1, 1].add_patch(Rectangle((-0.5, -0.5), 1, 1,
                                    fill=True, color="black"))

    im = axes[1, 2].imshow(dSw, origin="lower", cmap="RdBu_r", aspect="auto")
    axes[1, 2].set_title("ΔSw")
    plt.colorbar(im, ax=axes[1, 2], format="%.2e")

    fig.suptitle("VAL-010: Corner cell (0,0) inactive vs full grid", fontsize=14)
    plt.tight_layout()

    out_path = os.path.join(args.results_dir, "val010_comparison.png")
    plt.savefig(out_path, dpi=150)
    print(f"Saved: {out_path}")
    plt.show()


if __name__ == "__main__":
    main()
```

**Проверка после этого шага:**
- Запустить visual-тест (шаг 4) для генерации CSV
- Запустить: `python scripts/plot_val010.py --results-dir results/val-010`
- Визуально проверить: деактивированная ячейка чёрная, разность ΔP и ΔSw мала вдали от угла

**Зависимости:**
- Требует: шаг 4 (CSV export)

**Оценка:** ~70 строк, ~10 минут

---

## Шаг 6: Обновить vault

**Цель:** зафиксировать результат валидации.

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — обновить статус
- `vault/GDM/knowledge/validation/val-010 неактивная граничная ячейка.md` — новая заметка
- `vault/GDM/00-home/index.md` — добавить ссылку на план

**Что сделать:**

1. В `валидационные кейсы.md` обновить статус VAL-010:
   ```
   - **Статус:** ✅ ПРОЙДЕН <дата>
   ```

2. Создать заметку `vault/GDM/knowledge/validation/val-010 неактивная граничная ячейка.md`:
   ```yaml
   ---
   tags:
     - валидация
     - неактивные ячейки
     - граничные условия
   date: <дата>
   ---
   ```
   Содержание: подтип (инвариантный), сценарии (угловая, бортовая, 4 угла), результаты (tolerance, визуализация), ссылки на [[val-009 single-active-layer]].

3. В `index.md` — добавить ссылку на план:
   ```
   - [[val-010 inactive-boundary-cell]] — VAL-010: неактивная граничная (угловая) ячейка
   ```

4. Прокомментировать GitHub issue #51:
   ```
   gh issue comment 51 --repo ArturSalamatin/GDM --body "Валидация пройдена. Тесты: Corner cell (0,0), Edge cell (i=0,j=mid), All four corners. Все метрики в пределах tolerance."
   ```

**Зависимости:**
- Требует: шаги 1–5 пройдены
- Финальный шаг

**Оценка:** ~5 минут

---

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные (baseline не сломан)
- [ ] 3 новых количественных теста зелёные (corner, edge, 4-corners)
- [ ] Visual тест создаёт CSV
- [ ] Python-скрипт строит overlay plot
- [ ] Деактивированные ячейки сохраняют начальные условия
- [ ] Граф связности не содержит ссылок на деактивированные ячейки
- [ ] Давление и Sw в дальних ячейках совпадают с reference (epsilon ≤ 1e-4)
- [ ] Vault обновлён: заметка в `knowledge/validation/`, статус в реестре
- [ ] GitHub issue #51 прокомментирован

## Тесты

**Тест 1:**
- **Тест:** Corner cell (0,0) inactive - no crash and state preserved
- **Тег:** [integration][inactive-cells][val-010]
- **Файл:** tests/test_inactive_cells.cpp (существующий)
- **Сценарий:** деактивация угловой ячейки (0,0) при инжекции в центр сетки 7×7
- **Setup:** Nx=7, Ny=7, Nz=1, P=200 atm, So=0.8, INJ в центре, Q=-1000 кг/день
- **Эталон:** полная сетка (все ячейки активны)
- **Метрика:** P/Sw в дальних ячейках (relative vs full-grid), init в деактивированной ячейке
- **Tolerance:** дальние ячейки 1e-4, init 1e-12
- **Предотвращает:** краш при деактивации угловой ячейки, утечка состояния в деактивированную ячейку

**Тест 2:**
- **Тест:** Edge cell (i=0, j=mid) inactive - no crash and state preserved
- **Тег:** [integration][inactive-cells][val-010]
- **Файл:** tests/test_inactive_cells.cpp (существующий)
- **Сценарий:** деактивация бортовой ячейки (0,3) при инжекции в центр сетки 7×7
- **Setup:** аналогично тесту 1
- **Эталон:** полная сетка
- **Метрика:** P/Sw в дальних ячейках (i ≥ 3, relative vs full-grid), init в деактивированной ячейке
- **Tolerance:** дальние ячейки 1e-4, init 1e-12
- **Предотвращает:** краш при деактивации бортовой ячейки, некорректная обработка 3 соседей

**Тест 3:**
- **Тест:** All four corners inactive - graph integrity and state preserved
- **Тег:** [integration][inactive-cells][val-010]
- **Файл:** tests/test_inactive_cells.cpp (существующий)
- **Сценарий:** деактивация всех 4 угловых ячеек + проверка графа связности
- **Setup:** Nx=7, Ny=7, Nz=1, INJ в центре
- **Эталон:** self-consistency (начальные условия в деактивированных ячейках + граф без ссылок)
- **Метрика:** P/Sw в деактивированных ячейках = init, граф не содержит ссылок на углы
- **Tolerance:** 1e-12 (побитовое совпадение с init)
- **Предотвращает:** одновременная деактивация нескольких углов ломает индексацию

**Тест 4 (визуальный):**
- **Тест:** Corner cell inactive - CSV export
- **Тег:** [.visual][inactive-cells][val-010]
- **Файл:** tests/test_inactive_cells.cpp (существующий)
- **Сценарий:** CSV export полной сетки и corner-off для визуального сравнения
- **Предотвращает:** визуальные аномалии в полях P/Sw
