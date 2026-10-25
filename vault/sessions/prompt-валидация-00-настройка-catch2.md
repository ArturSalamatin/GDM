---
tags:
  - промпт
  - валидация
  - catch2
  - cmake
  - инфраструктура
date: 2026-06-17
---

# Промпт: Валидация #00 — установка и настройка Catch2

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

## Цель

Установить фреймворк Catch2 v3 в проект и подготовить CMake-инфраструктуру для тестов. Все последующие валидационные тесты (#01–#05) будут Catch2 TEST_CASE.

## Задание

### Шаг 1. Подключи Catch2 через FetchContent

Отредактируй `CMakeLists.txt`. Добавь загрузку Catch2 v3 через `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.0
)
FetchContent_MakeAvailable(Catch2)
```

Размести это **после** `project(...)` и **до** определения исходников.

**Почему FetchContent, а не вручную:** не нужно хранить сторонний код в репозитории, Catch2 скачивается при первом `cmake -B build`. Версия фиксирована тегом.

### Шаг 2. Выдели ядро солвера в отдельную переменную

Сейчас `GDM_SOURCES` включает `src/main.cpp`. Тесты не должны содержать `main()` из gdm. Разделить:

```cmake
# Ядро солвера — все .cpp кроме main()
set(GDM_CORE_SOURCES
    ${HYDRO}/stdafx.cpp
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp
    ${HYDRO}/Solver/Grids/OilField.cpp
    ${HYDRO}/Solver/Grids/Cells/AbstractCells.cpp
    ${HYDRO}/Solver/Grids/Cells/TwoPhaseFlowCell.cpp
    ${HYDRO}/Solver/Math/LinearProblem.cpp
    ${HYDRO}/Solver/Math/MatrixCSR.cpp
    ${HYDRO}/Solver/Math/SparsityPattern.cpp
    ${HYDRO}/Solver/Math/MathRoutines.cpp
    ${HYDRO}/Reservoir/Well/SomeWell.cpp
    ${HYDRO}/Reservoir/Well/Wells.cpp
    ${HYDRO}/Reservoir/Well/WellJobs.cpp
    ${HYDRO}/Reservoir/Well/SetOfPoints.cpp
    ${HYDRO}/Reservoir/Well/WellTrajectory.cpp
    ${HYDRO}/Anomaly/FlowField/SomeFlowField.cpp
    ${HYDRO}/Anomaly/FlowField/FlowField.cpp
    ${HYDRO}/Anomaly/FlowField/Point.cpp
    ${HYDRO}/Descriptors/Descriptors.cpp
    ${HYDRO}/Descriptors/MER_Descriptor.cpp
    ${HYDRO}/Helpers/LogFile.cpp
)

# Основной исполняемый файл
add_executable(gdm src/main.cpp ${GDM_CORE_SOURCES})
target_include_directories(gdm PRIVATE ${HYDRO} ${HYDRO}/AMGSolver/amgcl)
target_compile_definitions(gdm PRIVATE AMGCL_NO_BOOST)
```

### Шаг 3. Создай тестовый helper-заголовок

Создай файл `tests/test_helpers.h` — общие утилиты для всех тестов:

```cpp
#pragma once

#include "../HydroSolver/stdafx.h"
#include "../HydroSolver/Solver/Grids/DevelopedHorizon.h"
#include "../HydroSolver/Reservoir/ReservoirSimulator.h"
#include "../HydroSolver/Reservoir/NumericalParameters.h"
#include "../HydroSolver/Data/PhaseFactory.hpp"
#include "../HydroSolver/Data/ExceptionFactory.h"

namespace test_helpers {

// Создаёт однородный прямоугольный пласт без скважин
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

// Стандартные NumericalParameters для тестов
inline reservoir_simulator::NumericalParameters default_num_params()
{
    return reservoir_simulator::NumericalParameters(1e-6, 65, 1e-5, 1e-5);
}

} // namespace test_helpers
```

### Шаг 4. Создай заглушку первого теста

Создай `tests/test_smoke.cpp` — минимальный тест, проверяющий что инфраструктура работает:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("Smoke: simulator creates and runs", "[smoke]") {
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1,        // Nx, Ny, Nz (маленькая сетка для скорости)
        500.0, 500.0,    // Lx, Ly
        10.0,            // hz
        100.0,           // perm_mD
        0.2,             // porosity
        200.0,           // P_init_atm
        0.8              // oil_saturation
    );

    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator simulator{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    simulator.numPrm.set_initial_schemeTau(10.0);
    simulator.numPrm.set_currentMoment(0.0);

    simulator.Solve({0.0, 10.0});

    REQUIRE(simulator.OilTotal() > 0.0);
    REQUIRE(simulator.WaterTotal() > 0.0);
}

TEST_CASE("Smoke: saturation bounds", "[smoke]") {
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator simulator{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    simulator.numPrm.set_initial_schemeTau(10.0);
    simulator.numPrm.set_currentMoment(0.0);

    simulator.Solve({0.0, 10.0});

    auto Sw = simulator.GetWaterSaturationField();
    auto So = simulator.GetOilSaturationField();
    for (size_t i = 0; i < Sw.size(); ++i) {
        REQUIRE(Sw[i] >= 0.0);
        REQUIRE(Sw[i] <= 1.0);
        REQUIRE(So[i] >= 0.0);
        REQUIRE(So[i] <= 1.0);
        REQUIRE_THAT(Sw[i] + So[i], Catch::Matchers::WithinAbs(1.0, 1e-12));
    }
}
```

### Шаг 5. Добавь тестовый executable в CMakeLists.txt

```cmake
# --- Тесты (Catch2) ---
add_executable(gdm_tests
    tests/test_smoke.cpp
)
target_link_libraries(gdm_tests PRIVATE Catch2::Catch2WithMain)
target_sources(gdm_tests PRIVATE ${GDM_CORE_SOURCES})
target_include_directories(gdm_tests PRIVATE ${HYDRO} ${HYDRO}/AMGSolver/amgcl)
target_compile_definitions(gdm_tests PRIVATE AMGCL_NO_BOOST)

# Регистрация в CTest
include(CTest)
include(Catch2::Catch2)
catch_discover_tests(gdm_tests)
```

**Ключевые моменты:**
- `Catch2::Catch2WithMain` — Catch2 предоставляет свой `main()`, не нужно писать свой
- `catch_discover_tests` — автоматически находит все TEST_CASE и регистрирует как CTest-тесты
- Все тестовые .cpp добавляются в один executable `gdm_tests`

### Шаг 6. Собери и запусти

```powershell
# Первый раз — скачает Catch2 (~30 сек)
cmake -B build -S . -G "Visual Studio 17 2022"

# Сборка (Debug — быстрее для amgcl)
cmake --build build --config Debug

# Запуск всех тестов через CTest
ctest --test-dir build -C Debug --output-on-failure

# Или напрямую с фильтром по тегу
.\build\Debug\gdm_tests.exe --list-tests
.\build\Debug\gdm_tests.exe [smoke]
```

### Шаг 7. Проверь что всё работает

1. `ctest` показывает 2 теста, оба PASSED
2. `gdm_tests.exe` выводит Catch2-отчёт с зелёными статусами
3. `gdm.exe` по-прежнему работает (не сломали основной executable)

## Структура проекта после этого шага

```
GDM/
├── CMakeLists.txt          # обновлён: FetchContent(Catch2), GDM_CORE_SOURCES, gdm_tests
├── src/
│   └── main.cpp            # без изменений
├── tests/
│   ├── test_helpers.h      # общие утилиты (make_uniform_horizon, default_num_params)
│   └── test_smoke.cpp      # smoke-тесты
├── HydroSolver/            # без изменений
└── build/                  # cmake build dir (не коммитится)
```

## Конвенции для последующих тестов

### Файловая структура

Каждый промпт-валидации (#01–#05) создаёт свой файл в `tests/`:

| Промпт | Файл | Теги Catch2 |
|--------|------|-------------|
| #01 | `tests/test_stationary_pressure.cpp` | `[pressure][analytical]` |
| #02 | `tests/test_mass_balance.cpp` | `[balance][conservation]` |
| #03 | `tests/test_buckley_leverett.cpp` | `[buckley-leverett][analytical][transport]` |
| #04 | `tests/test_components.cpp` | `[components][relperm][fractional-flow][sparsity][jacobian]` |
| #05 | `tests/test_five_spot.cpp` | `[five-spot][2d][benchmark]` |

Все файлы добавляются в `target_sources(gdm_tests ...)` в CMakeLists.txt.

### Теги Catch2

Используй теги для группировки:
- `[smoke]` — быстрые базовые проверки (< 1 сек)
- `[analytical]` — сравнение с аналитическим решением
- `[conservation]` — проверки баланса масс
- `[components]` — unit-тесты отдельных компонент
- `[benchmark]` — тяжёлые тесты (> 10 сек)
- `[pressure]`, `[transport]`, `[relperm]`, `[wells]` — по физической теме

Запуск по тегу: `.\build\Debug\gdm_tests.exe [smoke]` или `ctest -R smoke`.

### Секции и подтесты

Используй `SECTION` для параметрических вариаций:
```cpp
TEST_CASE("Convergence by grid refinement", "[pressure][analytical]") {
    SECTION("Nx = 16") { ... }
    SECTION("Nx = 32") { ... }
    SECTION("Nx = 64") { ... }
}
```

Или `GENERATE` для параметрических тестов:
```cpp
TEST_CASE("Relperm boundary values", "[components][relperm]") {
    auto Sw = GENERATE(0.0, 0.25, 0.5, 0.75, 1.0);
    // тест для каждого значения
}
```

### Матчеры для чисел с плавающей точкой

```cpp
#include <catch2/matchers/catch_matchers_floating_point.hpp>
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

REQUIRE_THAT(value, WithinAbs(expected, 1e-10));     // абсолютная погрешность
REQUIRE_THAT(value, WithinRel(expected, 1e-6));       // относительная погрешность (10^-6 = 0.0001%)
```

### Вспомогательные функции в test_helpers.h

Все общие утилиты — в `tests/test_helpers.h`. Не дублируй между файлами. Если промпт #03 нуждается в `make_1d_horizon()` — добавь в `test_helpers.h`.

## Важные замечания

### amgcl и время компиляции

`LinearProblem.cpp` с amgcl templates компилируется ~10–15 минут в Debug. Это неизбежно при каждой чистой сборке. Инкрементальная сборка (изменение только тестового .cpp) — быстрая.

### /bigobj

MSVC может потребовать `/bigobj` для файлов с amgcl. Он уже включён в CMakeLists.txt.

### Один executable для всех тестов

Все тесты собираются в один `gdm_tests.exe`. Это экономит время линковки (amgcl линкуется один раз). Фильтрация по тегам заменяет отдельные executables.

### .gitignore

Убедись, что `build/` и `_deps/` (Catch2 кэш) в `.gitignore`.

## Критерии успеха

1. `cmake -B build` скачивает Catch2 и конфигурирует без ошибок
2. `cmake --build build --config Debug` собирает и `gdm`, и `gdm_tests`
3. `ctest --test-dir build -C Debug` показывает 2 smoke-теста, оба PASSED
4. `gdm.exe` работает как раньше

## По завершении

1. Обнови `vault/GDM/00-home/текущие приоритеты.md` — отметь что Catch2 настроен
2. Создай `vault/GDM/knowledge/decisions/тестовый фреймворк Catch2 через FetchContent.md`
