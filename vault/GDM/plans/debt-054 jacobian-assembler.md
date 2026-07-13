---
tags:
  - план
  - рефакторинг
date: 2026-07-13
issue: DEBT-054
github: 23
branch: refactor/debt-054/jacobian-assembler
status: реализован
audit:
  date: 2026-07-13
  round: 2
  findings: 0 / 0 / 1
  auto-fixed: 1
  manual-required: 0
---

# DEBT-054: Выделить JacobianAssembler из ReservoirSimulator

## Мотивация

`ReservoirSimulator` — монолит ~1361 строк (.cpp) + ~240 строк inline-кода `AccountForBoundaryConditions` в .h. Сборка Якобиана (матрицы и RHS) — самое внутреннее ядро симулятора, зависящее только от `OilField`, `LinearProblem` и `Wells`. Выделение в отдельный класс позволит:
- Тестировать сборку Якобиана изолированно
- Убрать 240 строк inline из заголовка (ускорение компиляции)
- Подготовить почву для DEBT-055 (NewtonSolver)

Это этап 1 из 4 декомпозиции DEBT-003. Подтип: **рефакторинг** (выделение модуля).

## Текущее состояние (baseline)

- **Тесты:** 296 pass, Release, ~131 сек
- **Файлы:**
  - `HydroSolver/Reservoir/ReservoirSImulator.h` — ~416 строк, включая ~240 строк inline `AccountForBoundaryConditions` (строки 118–358)
  - `HydroSolver/Reservoir/ReservoirSimulator.cpp` — ~1361 строк
- **Затронутые методы:**
  - `AssembleMyProblem` (ReservoirSimulator.cpp:503–538) — оркестрация: ResetProblem → fillMatrixBlockRow (OpenMP) → AccountForBoundaryConditions → wells
  - `fillMatrixBlockRow` (ReservoirSimulator.cpp:539–621) — строка Якобиана для одной ячейки
  - `AccountForBoundaryConditions` (ReservoirSImulator.h:118–358) — граничные условия открытого типа, три цикла по граням (YZ, XZ, XY)
  - `CheckForNAN` (ReservoirSImulator.h:111–116) — debug-проверка, используется в BC и wells
- **Call sites:**
  - `test_JacobianAssembly.cpp` — 12 вызовов `sim.AssembleMyProblem()`
  - `test_amgcl_benchmark.cpp` — 2 вызова `sim.AssembleMyProblem()`
  - `ReservoirSimulator::SingleIteration` — 1 вызов `AssembleMyProblem()`
- **Все поля public** (DEBT-015) — тесты обращаются к `sim.Grid`, `sim.MyProblem` напрямую
- **`B = 2`** — constexpr в `linear_problem` namespace (LinearProblem.h:29), доступен через `using namespace linear_problem`
- **`prof`** — глобальный `amgcl::profiler<>` в LinearProblem.h:15, доступен через `using amgcl::prof`
- **`USE_PARALLEL`** — определён в `Helpers/Defines.h:7`, включает OpenMP в `AssembleMyProblem`
- **`WellName = std::wstring`** — определён в `defines.h:12`

## Целевое состояние

```
ReservoirSimulator
├── JacobianAssembler assembler_   ← НОВЫЙ
├── OilField Grid
├── LinearProblem MyProblem
├── Wells
└── ...остальное без изменений
```

- `JacobianAssembler` — новый класс с одним публичным методом `Assemble()`
- `ReservoirSimulator::AssembleMyProblem` — однострочный делегат к `assembler_.Assemble()`
- `fillMatrixBlockRow`, `AccountForBoundaryConditions`, `CheckForNAN` — удалены из ReservoirSimulator
- `AccountForBoundaryConditions` — перенесён из .h в .cpp (JacobianAssembler.cpp)
- **Backward compat:** `sim.AssembleMyProblem()` продолжает работать — все существующие тесты компилируются без изменений

## Подводные камни

- ✅ **Потокобезопасность:** `fillMatrixBlockRow` вызывается под `#pragma omp parallel for`. Метод не имеет mutable state — обращается к `Grid[l]` (read-only) и `MyProblem.AddDiagBlock/AddOffDiagBlock` (thread-safe по l). В новом классе mutable state тоже не нужен — все данные через параметры
- ✅ **`prof` доступен:** определён в `LinearProblem.h` через `__declspec(selectany)`, JacobianAssembler будет включать LinearProblem.h
- ✅ **`B` доступен:** constexpr в namespace `linear_problem`, доступен через `using namespace linear_problem` или напрямую `linear_problem::B`
- ✅ **`USE_PARALLEL`:** определён в `Helpers/Defines.h`, доступен через `stdafx.h` → `Defines.h`
- ✅ **Backward compat тестов:** `sim.AssembleMyProblem()` остаётся как делегирующий метод
- ⚠️ **`AccountForBoundaryConditions` использует `Grid.Nx/Ny/Nz`:** нужно убедиться, что OilField предоставляет эти методы (проверено: наследует от AbstractGrid → Nx(), Ny(), Nz())
- ⚠️ **`AccountForBoundaryConditions` — 3 копипаста-блока (YZ, XZ, XY):** при переносе копируем as-is. Рефакторинг копипасты — отдельная задача (DEBT-014)
- ✅ **CMake:** новый .cpp добавляется в `target_sources(gdm_core)` секция `# Reservoir`
- ✅ **Связь с другими задачами:** не конфликтует. DEBT-055 (NewtonSolver) зависит от этой задачи, но не блокируется

---

# Шаги

## Шаг 1: Создать класс JacobianAssembler

**Цель:** создать новый класс рядом с ReservoirSimulator, не трогая существующий код

**Файлы:**
- СОЗДАТЬ: `HydroSolver/Reservoir/JacobianAssembler.h`
- СОЗДАТЬ: `HydroSolver/Reservoir/JacobianAssembler.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt` (строка ~42, секция `# Reservoir`)

**Контекст:**

Сейчас `AssembleMyProblem` (ReservoirSimulator.cpp:503–538) выполняет 4 шага:
1. `MyProblem.ResetProblem()` — обнуляет матрицу и RHS
2. `fillMatrixBlockRow(l, loc_tau)` в OpenMP-цикле по ячейкам — аккумуляция + потоки через грани
3. `AccountForBoundaryConditions()` — вклад граничных условий открытого типа
4. Цикл по скважинам: `well->AddWellToMatrix()` → `MyProblem.AddDiagBlock()`

Новый `JacobianAssembler::Assemble()` объединит все 4 шага, принимая Grid, Problem, RefPressure и Wells как параметры.

**Что сделать:**

1. Создать `HydroSolver/Reservoir/JacobianAssembler.h`:

```cpp
#pragma once
#include "../stdafx.h"
#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"

namespace reservoir_simulator {

namespace wells { class SomeWell; }

class JacobianAssembler {
public:
    void Assemble(
        double loc_tau,
        double nextTimeMoment,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure,
        const std::map<WellName, wells::SomeWell*>& wells);

private:
    void fillMatrixBlockRow(
        size_t l, double loc_tau,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem);

    void accountForBoundaryConditions(
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure);
};

} // namespace reservoir_simulator
```

2. Создать `HydroSolver/Reservoir/JacobianAssembler.cpp`:

```cpp
#include "JacobianAssembler.h"
#include "Well/Wells.h"

namespace reservoir_simulator {

using namespace cell;
using namespace grid;
using namespace linear_problem;

void JacobianAssembler::Assemble(
    double loc_tau, double nextTimeMoment,
    OilField& grid, LinearProblem& problem,
    double refPressure,
    const std::map<WellName, wells::SomeWell*>& wells)
{
    prof.tic("reset");
    problem.ResetProblem();
    prof.toc("reset");

    prof.tic("fill_rows");
#ifdef USE_PARALLEL
#pragma omp parallel for
#endif
    for (int l = 0; l < static_cast<int>(grid.ActiveCellsNmbr()); l++)
    {
        fillMatrixBlockRow(l, loc_tau, grid, problem);
    }
    prof.toc("fill_rows");

    prof.tic("boundary");
    accountForBoundaryConditions(grid, problem, refPressure);
    prof.toc("boundary");

    prof.tic("wells");
    for (auto& [name, well] : wells)
    {
        auto [posLocal, matrixBlockPerPerforation, rhsPerPerforation] =
            well->AddWellToMatrix(nextTimeMoment - loc_tau);
        for (size_t l = 0; l < well->NmbrOfOpenedCells(); ++l)
        {
            problem.AddDiagBlock(posLocal[l], matrixBlockPerPerforation[l], rhsPerPerforation[l]);
        }
    }
    prof.toc("wells");
}
```

- `fillMatrixBlockRow` — скопировать тело из ReservoirSimulator.cpp:539–621, заменив `Grid` → `grid`, `MyProblem` → `problem`
- `accountForBoundaryConditions` — скопировать тело из ReservoirSImulator.h:118–358, заменив `Grid` → `grid`, `MyProblem` → `problem`, `RefPressure` → `refPressure`
- Скопировать `CheckForNAN` как `static` функцию в `JacobianAssembler.cpp` (вверху файла, вне класса). Сохранить все `#ifdef DEBUG_SALAMATIN` / `CheckForNAN` вызовы в accountForBoundaryConditions и в well loop — поведение должно быть идентичным. Макрос `DEBUG_SALAMATIN` определён в `stdafx.h:31`

3. Добавить в `CMakeLists.txt` после строки 42 (`${HYDRO}/Reservoir/ReservoirSimulator.cpp`):

```cmake
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
```

4. **НЕ трогать** ReservoirSimulator.h и ReservoirSimulator.cpp — оба класса существуют одновременно

**Изменения (CMakeLists.txt):**

До:
```cmake
    # Reservoir
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp
```

После:
```cmake
    # Reservoir
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — без ошибок и warnings
- Тесты: `ctest --test-dir build -C Release` — 296 тестов зелёные (новый класс не используется)

**Подводные камни:**
- `fillMatrixBlockRow` использует `const auto& prevMass = cell.PreviousState_Mass_ref()` — убедиться что метод доступен на TwoPhaseFlowCell
- `AccountForBoundaryConditions` использует `Grid.ConvertGlobal2Local()` (возвращает `long int`, может быть -1 для неактивных ячеек) — скопировать логику `if (l < 0) continue` без изменений
- `wells` — const map reference, но `AddWellToMatrix` не const (меняет внутреннее состояние well). В `Assemble` параметр wells принимается как `const std::map<WellName, wells::SomeWell*>&` — указатели не const, поэтому `well->AddWellToMatrix()` компилируется

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~320 строк (.h ~35 + .cpp ~285), ~45 мин

---

## Шаг 2: Unit-тесты JacobianAssembler

**Цель:** убедиться, что `JacobianAssembler::Assemble()` даёт **побитово идентичную** матрицу и RHS, что и текущий `ReservoirSimulator::AssembleMyProblem()`

**Файлы:**
- СОЗДАТЬ: `tests/unit/math/test_JacobianAssembler_standalone.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt` — добавить файл в target `gdm_unit_level4`

**Контекст:**

Существующий `test_JacobianAssembly.cpp` проверяет свойства Якобиана (symmetry, finite values, J·dx≈ΔF). Новый тест проверяет **эквивалентность** нового assembler со старым — побитовое сравнение матрицы и RHS.

Паттерн теста:
1. Создать `ReservoirSimulator sim` через `make_sim()` (паттерн из test_JacobianAssembly.cpp:15–24)
2. Вызвать `sim.AssembleMyProblem(tau, tau)` → сохранить `sim.MyProblem.Matrix().toDense()` и `sim.MyProblem.Rhs()`
3. Обнулить problem: `sim.MyProblem.ResetProblem()`
4. Создать `JacobianAssembler assembler`
5. Вызвать `assembler.Assemble(tau, tau, sim.Grid, sim.MyProblem, sim.RefPressure, sim.Wells)`
6. Сравнить матрицу и RHS поэлементно: `CHECK(new_val == Approx(old_val).margin(0))`

**Тесты:**

```
Тест: JacobianAssembler identical matrix 1D
Тег: [unit][level4][jacobian_assembler]
Файл: tests/unit/math/test_JacobianAssembler_standalone.cpp (новый)
Сценарий: 5×1×1 сетка, однородный пласт, Sw=0.2, без скважин, Layout::InterleavedPSw
Setup: make_sim(5,1,1, Layout::InterleavedPSw), tau=86400
Ожидание: Matrix и RHS побитово идентичны
Baseline: sim.AssembleMyProblem
Предотвращает: ошибку при переносе fillMatrixBlockRow

Тест: JacobianAssembler identical matrix 2D with BC
Тег: [unit][level4][jacobian_assembler]
Сценарий: 3×3×1 сетка, RefPressure=200 (отличается от P_init=100) — граничные условия ненулевые
Ожидание: побитово идентичны
Предотвращает: ошибку при переносе AccountForBoundaryConditions

Тест: JacobianAssembler identical matrix with wells
Тег: [unit][level4][jacobian_assembler]
Сценарий: 5×5×1, одна скважина-инжектор
Setup: НЕ использовать make_sim — создать horizon и sim раздельно:
       auto h = test_helpers::make_uniform_horizon(5,5,1, 100,100,10, 100,0.2, 200,0.8);
       auto np = test_helpers::default_num_params();
       ReservoirSimulator sim(np, h, h.oil, h.water, h.other, Layout::InterleavedPSw);
       test_helpers::add_simple_well(sim, h, L"INJ", 50, 50, 0.0, -1000.0);
       Причина: add_simple_well требует horizon для перфораций
Ожидание: побитово идентичны
Предотвращает: ошибку при переносе well contributions

Тест: JacobianAssembler identical matrix Sw=0
Тег: [unit][level4][jacobian_assembler]
Сценарий: 5×5×1, Sw_init=0 — нулевая подвижность воды, guards на harmonic mean denom==0
Setup: make_sim(5, 5, 1, Layout::InterleavedPSw, 1.0)
       oil_saturation=1.0 → Sw = 1 - So = 0 (см. make_uniform_horizon, 10-й параметр)
Ожидание: побитово идентичны
Предотвращает: потерю guard `if (denom == 0.0) continue` при переносе (см. BUG-007)
```

**Что сделать:**

1. Создать `tests/unit/math/test_JacobianAssembler_standalone.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "Reservoir/ReservoirSimulator.h"
#include "Reservoir/JacobianAssembler.h"
#include "Solver/Grids/DevelopedHorizon.h"
#include "Data/PhaseFactory.hpp"
#include "../../test_helpers.h"

using namespace reservoir_simulator;
using namespace reservoir_simulator::linear_problem;
using Catch::Approx;

// oil_saturation: доля нефти (So). Sw = 1 - So. Для Sw=0 передать 1.0.
// make_uniform_horizon(Nx,Ny,Nz, Lx,Ly,hz, perm_mD,poro, P_init_atm,oil_saturation)
static ReservoirSimulator make_sim(int nx, int ny, int nz, Layout layout,
                                    double oil_saturation = 0.8)
{
    auto h = test_helpers::make_uniform_horizon(
        nx, ny, nz, 100.0, 100.0, 10.0, 100.0, 0.2, 200.0, oil_saturation);
    auto np = test_helpers::default_num_params();
    return ReservoirSimulator(np, h, h.oil, h.water, h.other, layout);
}

// Сравнить матрицу и RHS побитово
static void compare_assembly(ReservoirSimulator& sim, double tau,
                              double refPressure = -1.0)
{
    if (refPressure > 0) sim.RefPressure = refPressure;

    // Старый путь
    sim.AssembleMyProblem(tau, tau);
    auto old_dense = sim.MyProblem.Matrix().toDense();
    auto old_rhs = sim.MyProblem.Rhs();

    // Новый путь (Assemble() начинается с ResetProblem(), повторный вызов не нужен)
    JacobianAssembler assembler;
    assembler.Assemble(tau, tau, sim.Grid, sim.MyProblem, sim.RefPressure, sim.Wells);
    auto new_dense = sim.MyProblem.Matrix().toDense();
    auto new_rhs = sim.MyProblem.Rhs();

    // Сравнение матрицы
    REQUIRE(new_dense.size() == old_dense.size());
    for (size_t i = 0; i < old_dense.size(); i++) {
        for (size_t j = 0; j < old_dense[i].size(); j++) {
            INFO("matrix[" << i << "][" << j << "]");
            CHECK(new_dense[i][j] == Approx(old_dense[i][j]).margin(0));
        }
    }

    // Сравнение RHS
    REQUIRE(new_rhs.size() == old_rhs.size());
    for (size_t i = 0; i < old_rhs.size(); i++) {
        INFO("rhs[" << i << "]");
        CHECK(new_rhs[i] == Approx(old_rhs[i]).margin(0));
    }
}
```

2. Добавить 4 TEST_CASE (1D, 2D с BC, с скважинами, Sw=0)

3. В `CMakeLists.txt` добавить файл в `gdm_unit_level4` (строка ~208):

До:
```cmake
add_executable(gdm_unit_level4
    tests/unit/math/test_MatrixAssembly.cpp
    tests/unit/math/test_LinearProblemAssembly.cpp
    tests/unit/math/test_JacobianAssembly.cpp
)
```

После:
```cmake
add_executable(gdm_unit_level4
    tests/unit/math/test_MatrixAssembly.cpp
    tests/unit/math/test_LinearProblemAssembly.cpp
    tests/unit/math/test_JacobianAssembly.cpp
    tests/unit/math/test_JacobianAssembler_standalone.cpp
)
```

**Проверка после этого шага:**
- Сборка: без ошибок
- Тесты: 296 старых + 4 новых = 300 тестов зелёные

**Подводные камни:**
- `make_sim` принимает `oil_saturation` (10-й параметр `make_uniform_horizon`). Sw = 1 - oil_saturation. Для Sw=0 передать `1.0`
- Тест со скважинами: `add_simple_well` требует `horizon` — создавать `horizon` и `sim` раздельно (не через `make_sim`), паттерн из `test_five_spot.cpp`
- `sim.MyProblem.Matrix().toDense()` → `vector<vector<double>>` (проверено: MatrixCSR.h:65)
- `sim.MyProblem.Rhs()` → `const vector<double>&` (проверено: LinearProblem.h:73)

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 3

**Оценка:** ~150 строк, ~45 мин

---

## Шаг 3: Интеграция — ReservoirSimulator делегирует JacobianAssembler

**Цель:** `ReservoirSimulator::AssembleMyProblem` делегирует `assembler_.Assemble()`, старый код удаляется

**Файлы:**
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSImulator.h`
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**

После шага 2 мы знаем, что JacobianAssembler даёт побитово идентичный результат. Теперь безопасно заменить старый код на делегирование. Ключевое ограничение: `sim.AssembleMyProblem()` должен продолжать работать — 14 call sites в тестах.

**Что сделать:**

1. В `ReservoirSImulator.h`:
   - Добавить `#include "JacobianAssembler.h"` (после строки 7)
   - Добавить поле `JacobianAssembler assembler_;` (рядом с `SolverProfile solverProfile_`, строка ~71)
   - **Удалить** inline-тело `AccountForBoundaryConditions` (строки 118–358, ~240 строк)
   - **Удалить** объявление `fillMatrixBlockRow` (строка 109)
   - **Удалить** inline-тело `CheckForNAN` (строки 111–116)
   - **Оставить** объявление `AssembleMyProblem` (строка 107) — оно станет делегирующим

2. В `ReservoirSimulator.cpp`:
   - **Заменить** тело `AssembleMyProblem` (строки 503–538) на:
     ```cpp
     void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
     {
         assembler_.Assemble(loc_tau, nextTimeMoment, Grid, MyProblem, RefPressure, Wells);
     }
     ```
   - **Удалить** `fillMatrixBlockRow` (строки 539–621, ~82 строки)

**Изменения (ReservoirSImulator.h):**

До (строки 107–358):
```cpp
    void AssembleMyProblem(double loc_tau, double nextTimeMoment);

    void fillMatrixBlockRow(size_t l, double loc_tau);

    void CheckForNAN(std::vector<double> someArr)
    {
        for (auto& a : someArr)
            if (!isfinite(a))
                throw std::exception("bad well rhs");
    }

    void AccountForBoundaryConditions()
    {
        // ~240 строк inline-кода...
    }
```

После:
```cpp
    void AssembleMyProblem(double loc_tau, double nextTimeMoment);
```

Добавить поле (рядом со строкой 71):
```cpp
    JacobianAssembler assembler_;
```

**Изменения (ReservoirSimulator.cpp):**

До (строки 503–621):
```cpp
void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
{
    // 35 строк оркестрации
}
void ReservoirSimulator::fillMatrixBlockRow(size_t l, double loc_tau)
{
    // 82 строки
}
```

После (строки 503–506):
```cpp
void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
{
    assembler_.Assemble(loc_tau, nextTimeMoment, Grid, MyProblem, RefPressure, Wells);
}
```

**Проверка после этого шага:**
- Сборка: без ошибок и **без warnings**
- Тесты: все 300 тестов зелёные (296 старых + 4 новых из шага 2)
- `ReservoirSImulator.h` уменьшился на ~250 строк (с ~416 до ~170)
- `ReservoirSimulator.cpp` уменьшился на ~117 строк (35 + 82)

**Подводные камни:**
- `test_JacobianAssembly.cpp` вызывает `sim.AssembleMyProblem()` — продолжит работать через делегирование ✅
- `test_amgcl_benchmark.cpp` вызывает `sim.AssembleMyProblem()` — продолжит работать ✅
- `SingleIteration` вызывает `AssembleMyProblem()` — продолжит работать ✅
- `CheckForNAN` использовался в `AccountForBoundaryConditions` (в .h) — переехал в JacobianAssembler.cpp ✅
- `#ifdef DEBUG_SALAMATIN` вокруг `CheckForNAN` в well loop — теперь внутри JacobianAssembler.cpp ✅
- OpenMP `#pragma omp parallel for` — теперь внутри `JacobianAssembler::Assemble`, `USE_PARALLEL` доступен через `stdafx.h` ✅

**Зависимости:**
- Требует: шаг 2
- Блокирует: ничего (задача завершена)

**Оценка:** ~30 мин

---

## Шаг 4: Очистка и обновление vault

**Цель:** финализировать задачу, обновить документацию

**Файлы:**
- ИЗМЕНИТЬ: `vault/GDM/roadmap/технический долг.md` — статус DEBT-054
- ИЗМЕНИТЬ: `vault/GDM/00-home/index.md` — ссылка на план
- ИЗМЕНИТЬ: `vault/GDM/00-home/текущие приоритеты.md` — обновить статус

**Что сделать:**

1. В `vault/GDM/roadmap/технический долг.md`:
   - DEBT-054: статус `🔴 ОТКРЫТ` → `✅ реализовано YYYY-MM-DD`
   - DEBT-003: обновить «DEBT-054 — ✅»

2. В `vault/GDM/00-home/index.md`:
   - Добавить `[[debt-054 jacobian-assembler]]` в секцию Plans

3. Прокомментировать GitHub issue #23:
   ```
   gh issue comment 23 --repo ArturSalamatin/GDM --body "Реализовано. JacobianAssembler выделен, 4 unit-теста, backward compat сохранён."
   ```

4. Проверить итоговые метрики:
   - `ReservoirSImulator.h`: ~170 строк (было ~416)
   - `ReservoirSimulator.cpp`: ~1244 строк (было ~1361)
   - Новые файлы: JacobianAssembler.h (~35), JacobianAssembler.cpp (~285), тест (~150)
   - Тестов: 300 (было 296)

**Зависимости:**
- Требует: шаг 3

**Оценка:** ~15 мин

---

## Тестовая стратегия

1. **Regression:** все 296 существующих тестов зелёные после каждого шага
2. **Побитовое сравнение:** 4 новых unit-теста сравнивают JacobianAssembler со старым AssembleMyProblem
3. **Конфигурации:**
   - 1D (5×1×1) — простейший случай, только fillMatrixBlockRow
   - 2D (3×3×1) с BC — AccountForBoundaryConditions вносит ненулевой вклад
   - 2D (5×5×1) со скважинами — well contributions
   - 2D (5×5×1) Sw=0 — guard на нулевую подвижность

---

## Критерии завершения

- [ ] `JacobianAssembler.h` и `.cpp` созданы, компилируются без warnings
- [ ] 4 unit-теста побитового сравнения зелёные
- [ ] `ReservoirSimulator::AssembleMyProblem` делегирует `assembler_.Assemble()`
- [ ] `fillMatrixBlockRow`, `AccountForBoundaryConditions`, `CheckForNAN` удалены из ReservoirSimulator
- [ ] `AccountForBoundaryConditions` перенесён из .h в .cpp
- [ ] Все 300 тестов зелёные
- [ ] ReservoirSImulator.h уменьшился на ~250 строк
- [ ] Backward compat: test_amgcl_benchmark, test_JacobianAssembly компилируются без изменений
- [ ] Vault обновлён
- [ ] GitHub issue #23 прокомментирован

---

## Обнаруженные проблемы

1. **`AccountForBoundaryConditions` — 3 копипаста-блока (YZ, XZ, XY).** Каждый блок ~80 строк, отличается только формулой `faceArea` и индексацией. При переносе в JacobianAssembler копируем as-is. Рефакторинг (шаблон по направлению) — отдельная задача, связана с DEBT-014.

2. **XY-граница отключена:** строка 279 в ReservoirSImulator.h — `if (false && (nz > 1))`. Этот блок никогда не выполняется. При переносе копируем as-is (сохраняем семантику). Решение об удалении мёртвого кода — отдельно.

3. **`DensityOil()`/`DensityWater()` в BC не умножаются в XY-блоке** (строки 320–321 vs 162–163): в XY-блоке `f_oil = cell.F_Oil()` без `* cell.DensityOil()`, тогда как в YZ и XZ — с плотностью. Это может быть ошибкой, но XY-блок отключен (`if (false)`), поэтому не влияет на результат. Копируем as-is.

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай: [[debt-003 refactoring-reservoir-simulator]] — общий план декомпозиции
3. Создай ветку: `git checkout -b refactor/debt-054/jacobian-assembler`
4. Собери:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline):
   ```powershell
   ctest --test-dir build -C Release
   ```
6. Запомни: **296 тестов, ~131 сек** — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты
8. Каждый шаг — отдельный коммит:
   - `refactor: DEBT-054 создать JacobianAssembler`
   - `test: DEBT-054 unit-тесты побитового сравнения`
   - `refactor: DEBT-054 ReservoirSimulator делегирует JacobianAssembler`
   - `vault: DEBT-054 обновить статусы`

## Связанные заметки

- [[debt-003 refactoring-reservoir-simulator]] — общий план декомпозиции (этапы 1–4)
- [[технический долг]] — записи DEBT-003, DEBT-054
- [[граф зависимостей классов gdm_core]] — текущая архитектура
- [[структура проекта и конвенции кода]] — конвенции
- [[стратегия тестирования GDM]] — паттерны тестов
- DEBT-055 (NewtonSolver) — блокируется этой задачей
- DEBT-014 (дублирование flux-логики) — связан, но не блокируется

## Оценка общая

| Шаг | Файлов | Строк | Время |
|---|---|---|---|
| 1: Создать JacobianAssembler | 2 создать, 1 изменить | ~320 | ~45 мин |
| 2: Unit-тесты | 1 создать, 1 изменить | ~150 | ~45 мин |
| 3: Интеграция + удаление | 2 изменить | -367 (чистый diff) | ~30 мин |
| 4: Vault | 3 изменить | ~10 | ~15 мин |
| **Итого** | **3 создать, 7 изменить** | **~480 новых, -367 удалённых** | **~2 ч 15 мин** |
