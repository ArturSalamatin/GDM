---
tags:
  - промпт
  - валидация
  - давление
  - аналитика
  - catch2
date: 2026-06-17
---

# Промпт: Валидация #01 — стационарное однофазное давление

Скопируй этот промпт в новую сессию Claude Code целиком. Проект находится в `d:\Lessons\Grants\2027\GDM`.

---

## Контекст проекта

GDM — двухфазный (нефть–вода) гидродинамический симулятор. Обе фазы несжимаемые. C++23, CMake + Visual Studio 2022 (MSVC), Windows 10.

**Правила работы описаны в `CLAUDE.md` — прочитай его перед началом.**
Прочитай `vault/GDM/00-home/текущие приоритеты.md` для понимания текущей фазы.

Проект уже собирается и запускается:
```powershell
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Debug
.\build\Debug\gdm.exe
```

### Тестовый фреймворк

**ВАЖНО:** промпт #00 (настройка Catch2) должен быть выполнен до этого шага. Проверь:
- Catch2 v3 подключен через FetchContent в `CMakeLists.txt`
- Переменная `GDM_CORE_SOURCES` содержит все .cpp ядра солвера (без `src/main.cpp`)
- Тестовый executable `gdm_tests` уже определён
- Файл `tests/test_helpers.h` существует с `make_uniform_horizon()` и `default_num_params()`
- Smoke-тесты (`tests/test_smoke.cpp`) проходят: `ctest --test-dir build -C Debug`

Если чего-то нет — сначала выполни промпт #00.

## Цель этого теста

Проверить, что **солвер корректно решает уравнение давления** в простейшем случае, когда аналитическое решение известно.

### Физика задачи

Однофазный стационарный поток в 1D пласте. Если зафиксировать насыщенность (S_w = const), уравнение неразрывности для несжимаемой фазы сводится к:

∇·(k·k_r/μ · ∇P) = 0

Для однородного пласта с постоянным k, k_r, μ — это уравнение Лапласа:

d²P/dx² = 0

Аналитическое решение: **P(x) = P_L + (P_R − P_L) · x / L** — линейный профиль.

### Как это реализовано в GDM

Граничные условия задаются через `AccountForBoundaryConditions()` в `ReservoirSimulator.h` (строки ~104–263). Метод добавляет поток через граничные грани пласта. На границе используется давление `RefPressure` (поле `ReservoirSimulator::RefPressure`). Поток через границу: `q = (k·k_r/μ) · (P_cell - P_ref) · A / (Δx/2)`.

Это эквивалентно граничному условию третьего рода (Robin): если P_cell = P_ref, поток = 0.

Скважин нет. Если начальное давление во всех ячейках = RefPressure, то поток через границы = 0, решение тривиально.

Для нетривиального теста нужно:
1. Задать **неоднородное** начальное давление (например, линейный профиль)
2. Или задать **разные давления** на противоположных границах
3. Или добавить скважины с фиксированным давлением

## Задание

### Шаг 1. Изучи механизм граничных условий

Прочитай `AccountForBoundaryConditions()` в `ReservoirSimulator.h` (строки ~104–263). Разберись:
- Как `RefPressure` влияет на RHS и матрицу
- Что происходит при `dp > 0` (поток наружу) и `dp < 0` (поток внутрь)
- Как обрабатываются X-границы (i=0, i=Nx-1) и Y-границы (j=0, j=Ny-1)

### Шаг 2. Создай тестовый файл

Создай файл `tests/test_stationary_pressure.cpp`.

**Подключи Catch2 и helpers:**
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
```

### Шаг 3. Тест: тривиальное равновесие

```cpp
TEST_CASE("Stationary pressure: trivial equilibrium", "[pressure][analytical]") {
    // P_init = RefPressure → нулевой поток → P не меняется
    auto horizon = test_helpers::make_uniform_horizon(
        32, 1, 1,            // Nx, Ny, Nz
        1000.0, 100.0,       // Lx, Ly
        10.0,                // hz
        100.0, 0.2,          // perm_mD, porosity
        200.0, 0.0           // P_init_atm, oil_sat (чистая вода → однофазный)
    );
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);

    // RefPressure по умолчанию = 100.0 (в коде), а P_init = 200 атм
    // Нужно выставить RefPressure = P_init, чтобы поток = 0
    sim.RefPressure = 200.0 * 101325.0;

    auto P_before = sim.GetPressureField();
    sim.Solve({0.0, 100.0});
    auto P_after = sim.GetPressureField();

    for (size_t i = 0; i < P_before.size(); ++i) {
        REQUIRE_THAT(P_after[i], WithinRel(P_before[i], 1e-10));
    }
}
```

### Шаг 4. Тест: нетривиальный профиль давления

Сценарий: P_init ≠ RefPressure → поток через границы → давление релаксирует к RefPressure.

```cpp
TEST_CASE("Stationary pressure: relaxation to boundary", "[pressure][analytical]") {
    constexpr size_t Nx = 32;
    constexpr double Lx = 1000.0, Ly = 100.0, hz = 10.0;
    constexpr double P_ref_atm = 200.0;
    constexpr double P_ref_Pa = P_ref_atm * 101325.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, Ly, hz, 100.0, 0.2, 250.0, 0.0);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_ref_Pa;

    // Много шагов для выхода на стационар
    sim.Solve({0.0, 10.0, 20.0, 50.0, 100.0, 500.0, 1000.0});

    auto P = sim.GetPressureField();

    // Для несжимаемых фаз и одинакового RefPressure на всех границах
    // стационар: P = P_ref во всех ячейках
    for (size_t i = 0; i < P.size(); ++i) {
        REQUIRE_THAT(P[i], WithinRel(P_ref_Pa, 1e-3));
    }
}
```

**ВНИМАНИЕ:** этот тест может НЕ работать, если для несжимаемых фаз нет слагаемого ∂P/∂t и давление устанавливается мгновенно. В этом случае достаточно одного шага. Проверь по результату.

### Шаг 5. Тест с источником (скважиной) — линейный профиль

**ВАЖНО:** в текущей реализации `RefPressure` — одно значение для всего пласта. Чтобы задать разные давления на разных границах, нужен один из вариантов:
1. Модифицировать `AccountForBoundaryConditions()` чтобы давление зависело от позиции ячейки
2. Использовать скважины с фиксированным дебитом
3. Использовать `RefPressure` как давление границы + источник в первой ячейке

Самый рабочий вариант: `RefPressure = P_R` (давление правой границы) + нагнетательная скважина с постоянным дебитом в первой ячейке (i=0).

Аналитика для 1D с источником на одном конце и фиксированным давлением на другом:
```
P(x) = P_R + Q·μ/(k·A) · (L − x)
```
где A = hy·hz — площадь сечения, Q — объёмный расход (м³/день).

```cpp
TEST_CASE("Stationary pressure: linear profile with source",
          "[pressure][analytical]") {
    // Этот тест требует добавления скважины.
    // Исследуй API AddWell_FixedProduction() и формат MER.
    // См. подробности ниже в секции "Добавление скважин".
    
    SECTION("Nx = 32") {
        // ... реализация с Nx=32
        // REQUIRE_THAT(L2_error, WithinAbs(0.0, tolerance_32));
    }
    SECTION("Nx = 64") {
        // ... реализация с Nx=64
        // REQUIRE_THAT(L2_error, WithinAbs(0.0, tolerance_64));
        // CHECK(L2_error_64 < L2_error_32 * 0.3); // порядок ~2
    }
}
```

### Шаг 6. Тест сходимости по сетке

```cpp
TEST_CASE("Stationary pressure: grid convergence order",
          "[pressure][analytical][convergence]") {
    std::vector<size_t> grids = {16, 32, 64, 128};
    std::vector<double> errors;

    for (auto Nx : grids) {
        // ... создать horizon, добавить скважину, решить
        double L2_error = /* вычислить */;
        errors.push_back(L2_error);
    }

    // Проверка порядка сходимости
    for (size_t i = 1; i < errors.size(); ++i) {
        double order = std::log2(errors[i-1] / errors[i]);
        INFO("Grid " << grids[i] << ": error = " << errors[i]
             << ", order = " << order);
        CHECK(order > 1.5);  // ожидаем ~2 для 5-точечной схемы
    }
}
```

### Шаг 7. Зарегистрируй в CMakeLists.txt

Добавь файл в `gdm_tests`:
```cmake
target_sources(gdm_tests PRIVATE tests/test_stationary_pressure.cpp)
```

### Шаг 8. Собери и запусти

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -R pressure
# или
.\build\Debug\gdm_tests.exe [pressure]
```

## Добавление скважин — подробности

Для теста с линейным профилем нужна нагнетательная скважина. Цепочка данных:

```
1. SingleWell_MER_Data = vector<SingleMERrecord>
   SingleMERrecord = map<wstring, float>
   Ключи: L"time", L"oil", L"water", L"worked_time", L"idle_time", L"type", L"is_work"

2. WellJobs = (name, WellJobsData, LayerAggregationTree, NmbrOfLayers)
   WellJobsData = map<LayerName, vector<JobDescriptor>>
   JobDescriptor = tuple<int, int, int, pair<float, float>>
   // (timestamp_days, type: 1=open/0=close, is_grp, (start_depth, end_depth))

3. WellPosition = GeosPoint(x, y)

4. AddWell_FixedProduction(name, mer_data, well_jobs, position, grid_bounds, block_size, r_well)
```

Если конструирование WellJobs окажется слишком громоздким — рассмотри добавление helper-функции `add_simple_well()` в `test_helpers.h`.

### Знак дебита

Комментарий в `MER_Data`: `"well production is negative when water goes into"`. Т.е.:
- Добыча нефти: `L"oil"` = положительное число (кг/день)
- Закачка воды: `L"water"` = отрицательное число (кг/день)

Проверь по коду `WellFixedProduction::AddWellToMatrix()` в `Wells.cpp`.

### Единицы

- Давление: Па
- Время: дни
- Дебит в MER: кг/день
- Проницаемость: м²
- Вязкость: Па·день (после конвертации: мПа·с × 1/(86400×1000))

### Переменные в симуляторе

- `VariableFieldProperties[0]` = S̃_w = (S_w − S_wc) / (1 − S_or − S_wc)
- `VariableFieldProperties[1]` = P (Па)

Для однофазного теста: S_w = 1, S_wc = 0, S_or = 0 → S̃_w = 1.0. Используй `oil_saturation = 0.0` в `make_uniform_horizon()` (S_oil = 0 → S_w = 1).

### Доступ к результатам

```cpp
std::vector<double> P = simulator.GetPressureField();     // size = Nx*Ny*Nz
std::vector<double> Sw = simulator.GetWaterSaturationField();
```

### Solve() и timeMoments

Цикл начинается с `i = 1`, т.е. `timeMoments[0]` — начальный момент:
```cpp
simulator.Solve({0.0, T});  // 1 интервал от 0 до T
```

Для несжимаемых фаз (сжимаемость = 0) нет слагаемого ∂P/∂t → давление устанавливается за 1 шаг.

## Критерии успеха

1. `ctest -R pressure` — все тесты PASSED
2. Тривиальное равновесие: давление не меняется (отн. ошибка < 10⁻¹⁰)
3. Если реализован тест с источником: линейный профиль с ошибкой O(h²)
4. Порядок сходимости ≈ 2

## По завершении

1. Создай vault-заметку в `vault/GDM/knowledge/validation/` с результатами
2. Если найден баг — заметку в `vault/GDM/knowledge/debugging/`
3. Обнови `vault/GDM/00-home/текущие приоритеты.md`
