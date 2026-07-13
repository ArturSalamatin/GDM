---
tags:
  - план
  - рефакторинг
date: 2026-07-13
issue: DEBT-003
github: null
branch: refactor/debt-003/reservoir-simulator-decomposition
status: готов к реализации
---

# DEBT-003: Декомпозиция ReservoirSimulator

## Мотивация

`ReservoirSimulator` — монолит ~1361 строк (.cpp) + ~240 строк inline-кода в .h. Содержит 6 зон ответственности: сборку Якобиана, граничные условия, Newton solver, time stepping, mass balance, диагностику (flux/contour). Любое изменение в одном аспекте затрагивает весь файл. Unit-тестирование ядра невозможно без поднятия полного симулятора.

## Принципы декомпозиции

1. **Послойный подход:** выделить внутреннее ядро → тесты → интеграция в существующий код (оба живут одновременно) → замена → удаление старого
2. **SOLID:** каждый новый класс — одна ответственность, зависимости через абстракции (интерфейсы)
3. **Инкрементальность:** после каждого шага проект компилируется и все 296 тестов зелёные
4. **Backward compat:** публичный API `ReservoirSimulator` сохраняется на время миграции, затем `ReservoirSimulator` становится тонким фасадом

## Текущее состояние (baseline)

- Тесты: 296 pass, Release, ~119 сек
- Файлы:
  - `HydroSolver/Reservoir/ReservoirSImulator.h` (~416 строк, включая ~240 строк inline `AccountForBoundaryConditions`)
  - `HydroSolver/Reservoir/ReservoirSimulator.cpp` (~1361 строк)
- Все поля public (DEBT-015)
- Тесты обращаются к внутренностям напрямую: `sim.Grid`, `sim.MyProblem`, `sim.numPrm`, `sim.AssembleMyProblem()`, `sim.UpdateGrid()`, `sim.MassBalance()`

## Целевое состояние

```
ReservoirSimulator (тонкий фасад, ~200 строк)
├── JacobianAssembler      — fillMatrixBlockRow, AccountForBoundaryConditions, well contributions
├── NewtonSolver            — PerformNewtonLoop, SingleIteration, UpdateGrid
├── TimeIntegrator          — Solve (time-stepping loop)
├── MassBalanceTracker      — MassBalance, аккумуляторы
└── FluxDiagnostics         — OverallFluxes, OilContourFlux, WaterContourFlux
```

Порядок выделения (от внутреннего к внешнему):
1. **JacobianAssembler** — самое внутреннее ядро, зависит только от Grid + LinearProblem
2. **NewtonSolver** — использует JacobianAssembler, зависит от Grid + LinearProblem + NumericalParameters
3. **MassBalanceTracker** — простой аккумулятор, зависит от Grid + Wells
4. **TimeIntegrator** — оркестрирует NewtonSolver + MassBalanceTracker
5. **FluxDiagnostics** — OverallFluxes/ContourFlux, зависит только от Grid (отложить, DEBT-014)

## Зависимости между компонентами

```
TimeIntegrator
  ├── NewtonSolver
  │     ├── JacobianAssembler
  │     │     ├── OilField (Grid)
  │     │     ├── LinearProblem
  │     │     └── Wells (well contributions to Jacobian)
  │     ├── LinearProblem::Solve()
  │     ├── NumericalParameters
  │     └── OilField (UpdateGrid: cell state)
  ├── MassBalanceTracker
  │     ├── OilField (OilTotal, WaterTotal)
  │     └── Wells (OilDebitTotal, WaterDebitTotal)
  └── NumericalParameters (time stepping)
```

## Подводные камни

- ✅ **Потокобезопасность:** `fillMatrixBlockRow` вызывается под OpenMP — новый класс не должен иметь mutable state. Передавать данные через аргументы
- ⚠️ **test_amgcl_benchmark.cpp:** дублирует Newton loop вручную, обращаясь к `sim.AssembleMyProblem()`, `sim.MyProblem.Solve()`, `sim.UpdateGrid()`, `sim.Grid.ReverseState()`, `sim.MassBalance()`. Нужно сохранить эти методы на ReservoirSimulator как делегирующие, либо обновить тест
- ⚠️ **test_JacobianAssembly.cpp:** обращается к `sim.Grid`, `sim.MyProblem` напрямую. Те же имена должны оставаться доступны
- ✅ **prof (amgcl::profiler):** используется в SingleIteration, AssembleMyProblem, Solve — глобальный объект, доступен через `using amgcl::prof`
- ✅ **AccountForBoundaryConditions** в .h: inline ~240 строк, переедет в .cpp нового класса
- ✅ **CMake:** новые .cpp нужно добавить в `target_sources(gdm_core)`

## Обнаруженные проблемы

1. **AccountForBoundaryConditions в .h** — ~240 строк inline-кода в заголовке. При выделении в JacobianAssembler переедет в .cpp. Это попутно решает проблему раздувания заголовка
2. **test_amgcl_benchmark дублирует Newton loop** — при выделении NewtonSolver нужно решить: либо тест использует NewtonSolver напрямую, либо оставить делегирующие методы на ReservoirSimulator. Выбираем второе (backward compat)

---

# Этапы

Задача разбита на 4 этапа. Каждый этап — самостоятельная единица работы, может быть реализован в отдельной сессии. Каждый этап следует послойному подходу: создание → тесты → интеграция → замена.

---

## Этап 1: JacobianAssembler

Самое внутреннее ядро. Единственная ответственность: сборка матрицы Якобиана и правой части для системы уравнений фильтрации.

### Шаг 1.1: Создать класс JacobianAssembler

**Цель:** создать новый класс с перенесённой логикой сборки Якобиана

**Файлы:**
- СОЗДАТЬ: `HydroSolver/Reservoir/JacobianAssembler.h`
- СОЗДАТЬ: `HydroSolver/Reservoir/JacobianAssembler.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt` — добавить в gdm_core

**Контекст:**
`fillMatrixBlockRow` (ReservoirSimulator.cpp:539-621) собирает строку Якобиана для одной ячейки: аккумуляционный член, потоки через грани (upstream weighting, гармоническое среднее подвижности), диагональный и внедиагональные блоки. `AccountForBoundaryConditions` (ReservoirSImulator.h:118-358) добавляет вклад граничных условий открытого типа (фиксированное давление на границе). Обе функции зависят только от `OilField` (Grid), `LinearProblem` и `RefPressure`.

**Интерфейс:**

```cpp
// HydroSolver/Reservoir/JacobianAssembler.h
#pragma once
#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"

namespace reservoir_simulator {

namespace wells { class SomeWell; }

class JacobianAssembler {
public:
    // Собрать Якобиан и RHS для всех ячеек + граничные условия + скважины
    void Assemble(
        double loc_tau,
        double nextTimeMoment,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure,
        const std::map<WellName, wells::SomeWell*>& wells);

private:
    // Строка Якобиана для одной ячейки (thread-safe, без mutable state)
    void fillMatrixBlockRow(
        size_t l, double loc_tau,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem);

    // Граничные условия открытого типа
    void accountForBoundaryConditions(
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure);
};

} // namespace reservoir_simulator
```

**Что сделать:**
1. Создать `JacobianAssembler.h` с интерфейсом выше
2. Создать `JacobianAssembler.cpp`:
   - Перенести тело `fillMatrixBlockRow` из ReservoirSimulator.cpp:539-621
   - Перенести тело `AccountForBoundaryConditions` из ReservoirSImulator.h:118-358
   - Перенести логику well contributions из `AssembleMyProblem` (ReservoirSimulator.cpp:523-536)
   - Метод `Assemble()` объединяет: ResetProblem → fill_rows (OpenMP) → boundary → wells
   - Адаптировать: вместо `Grid` → параметр `grid`, вместо `MyProblem` → параметр `problem`, вместо `RefPressure` → параметр `refPressure`
   - `prof.tic/toc` вызовы сохранить (prof — глобальный объект)
3. Добавить `${HYDRO}/Reservoir/JacobianAssembler.cpp` в `CMakeLists.txt` (секция `# Reservoir`)
4. **НЕ трогать** ReservoirSimulator — оба класса существуют одновременно

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — компилируется без ошибок и warnings
- Тесты: `ctest --test-dir build -C Release` — 296 тестов зелёные (новый класс ещё не используется)

**Оценка:** ~120 строк (.h + .cpp), ~30 мин

### Шаг 1.2: Unit-тесты JacobianAssembler

**Цель:** убедиться, что JacobianAssembler даёт тот же результат, что текущий ReservoirSimulator

**Файлы:**
- СОЗДАТЬ: `tests/unit/math/test_JacobianAssembler_standalone.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt` — добавить тест в target

**Контекст:**
Существующий `test_JacobianAssembly.cpp` уже проверяет `sim.AssembleMyProblem()` — конечное-разностное приближение, symmetry, finite values. Новый тест должен проверить, что `JacobianAssembler::Assemble()` даёт **идентичную** матрицу и RHS.

**Тесты:**

```
Тест: JacobianAssembler produces identical matrix
Тег: [jacobian_assembler]
Сценарий: 1D сетка 5×1×1, однородный пласт, Sw=0.5
Setup: создать ReservoirSimulator, вызвать sim.AssembleMyProblem → сохранить матрицу/RHS.
       Создать JacobianAssembler, вызвать Assemble с теми же Grid/Problem → сравнить.
Ожидание: ||A_new - A_old|| == 0, ||rhs_new - rhs_old|| == 0 (побитово)
Предотвращает: регрессию при переносе кода

Тест: JacobianAssembler with boundary conditions
Тег: [jacobian_assembler]
Сценарий: 3×3×1, RefPressure != P_init — граничные условия ненулевые
Ожидание: идентичная матрица/RHS

Тест: JacobianAssembler with wells
Тег: [jacobian_assembler]
Сценарий: 5×5×1, одна скважина-инжектор
Ожидание: идентичная матрица/RHS

Тест: JacobianAssembler with Sw=0
Тег: [jacobian_assembler]
Сценарий: 5×5×1, Sw_init=0 — нулевая подвижность воды
Ожидание: идентичная матрица/RHS (проверяет guards harmonic mean)
```

**Что сделать:**
1. Создать тестовый файл
2. Паттерн: для каждого теста создать два `LinearProblem` с одинаковым connectivity graph, вызвать старый `sim.AssembleMyProblem` и новый `assembler.Assemble`, сравнить `Matrix().toDense()` и `Rhs()` поэлементно через `CHECK(a == Approx(b).margin(0))`
3. Добавить в CMakeLists.txt

**Проверка:**
- Сборка + все тесты зелёные (296 старых + новые)

**Оценка:** ~150 строк, ~45 мин

### Шаг 1.3: Интеграция — ReservoirSimulator делегирует JacobianAssembler

**Цель:** `ReservoirSimulator::AssembleMyProblem` делегирует вызов `JacobianAssembler::Assemble`

**Файлы:**
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSImulator.h` — добавить поле `JacobianAssembler assembler_`, удалить inline-тело `AccountForBoundaryConditions`
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSimulator.cpp` — `AssembleMyProblem` → делегирует `assembler_.Assemble(...)`, удалить `fillMatrixBlockRow`

**Что сделать:**
1. В `ReservoirSImulator.h`:
   - Добавить `#include "JacobianAssembler.h"` и поле `JacobianAssembler assembler_`
   - Удалить inline-тело `AccountForBoundaryConditions` (~240 строк)
   - Удалить объявления `fillMatrixBlockRow`, `AccountForBoundaryConditions`, `CheckForNAN`
2. В `ReservoirSimulator.cpp`:
   - Заменить тело `AssembleMyProblem` на: `assembler_.Assemble(loc_tau, nextTimeMoment, Grid, MyProblem, RefPressure, Wells);`
   - Удалить `fillMatrixBlockRow` (строки 539-621)
   - `CheckForNAN` — перенести в JacobianAssembler как private (или удалить, он за `#ifdef DEBUG_SALAMATIN`)
3. Проверить, что `test_amgcl_benchmark.cpp` по-прежнему компилируется — он вызывает `sim.AssembleMyProblem()`, которая осталась
4. Проверить, что `test_JacobianAssembly.cpp` компилируется — он тоже вызывает `sim.AssembleMyProblem()`

**Проверка:**
- Сборка без warnings
- Все 296+ тестов зелёные
- `ReservoirSImulator.h` уменьшился на ~240 строк

**Подводные камни:**
- `test_JacobianAssembly.cpp` использует `sim.Grid` и `sim.MyProblem` напрямую — они по-прежнему public, поэтому тесты продолжат работать
- OpenMP `#pragma omp parallel for` теперь внутри `JacobianAssembler::Assemble` — проверить что макрос `USE_PARALLEL` доступен

**Оценка:** ~30 мин

---

## Этап 2: NewtonSolver

Второй слой. Единственная ответственность: нелинейная итерация Ньютона — сборка, решение СЛАУ, проверка сходимости, обновление состояния.

### Шаг 2.1: Создать класс NewtonSolver

**Цель:** выделить Newton loop в отдельный класс

**Файлы:**
- СОЗДАТЬ: `HydroSolver/Reservoir/NewtonSolver.h`
- СОЗДАТЬ: `HydroSolver/Reservoir/NewtonSolver.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt`

**Контекст:**
`PerformNewtonLoop` (ReservoirSimulator.cpp:439-461) управляет итерациями Ньютона. `SingleIteration` (490-501) — одна итерация: Assemble → Solve → update AMG state. `UpdateGrid` (462-488) — применить коррекцию к ячейкам, проверить сходимость. Зависимости: `JacobianAssembler`, `OilField`, `LinearProblem`, `NumericalParameters`, `SolverProfile`.

**Интерфейс:**

```cpp
// HydroSolver/Reservoir/NewtonSolver.h
#pragma once
#include "JacobianAssembler.h"
#include "NumericalParameters.h"

namespace reservoir_simulator {

struct NewtonResult {
    bool converged;
    size_t newton_iters;
    size_t amg_solves;
    size_t total_amg_iters;
};

class NewtonSolver {
public:
    // Выполнить Newton loop для одного временного шага
    NewtonResult Solve(
        double loc_tau,
        double nextTimeMoment,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure,
        const std::map<WellName, wells::SomeWell*>& wells,
        NumericalParameters& numPrm);

private:
    JacobianAssembler assembler_;

    void singleIteration(
        double loc_tau, double nextTimeMoment,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure,
        const std::map<WellName, wells::SomeWell*>& wells,
        NumericalParameters& numPrm);

    bool updateGrid(
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        const NumericalParameters& numPrm);
};

} // namespace reservoir_simulator
```

**Что сделать:**
1. Создать `NewtonSolver.h` с интерфейсом выше
2. Создать `NewtonSolver.cpp`:
   - `Solve()` — перенести тело `PerformNewtonLoop` (строки 439-461), возвращать `NewtonResult` с профилировочными данными
   - `singleIteration()` — перенести тело `SingleIteration` (строки 490-501), вызывать `assembler_.Assemble()`
   - `updateGrid()` — перенести тело `UpdateGrid` (строки 462-488)
   - Сохранить `prof.tic/toc`
3. Добавить в CMakeLists.txt
4. **НЕ трогать** ReservoirSimulator

**Проверка:** сборка без warnings, 296+ тестов зелёные

**Оценка:** ~100 строк, ~30 мин

### Шаг 2.2: Unit-тесты NewtonSolver

**Цель:** проверить NewtonSolver изолированно

**Файлы:**
- СОЗДАТЬ: `tests/unit/test_NewtonSolver.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt`

**Тесты:**

```
Тест: NewtonSolver converges on uniform grid
Тег: [newton_solver]
Сценарий: 5×5×1, однородный пласт, без скважин, Sw=0.5
Ожидание: converged=true, newton_iters=1 (линейная задача)

Тест: NewtonSolver convergence matches ReservoirSimulator
Тег: [newton_solver]
Сценарий: 5×5×1, одна скважина-инжектор, один временной шаг
Setup: вызвать sim.PerformNewtonLoop и NewtonSolver::Solve с идентичным состоянием
Ожидание: идентичное число Newton итераций, идентичное конечное состояние Grid

Тест: NewtonSolver rolls back on divergence
Тег: [newton_solver]
Сценарий: заведомо жёсткая задача (огромный dt)
Ожидание: converged=false, Grid.ReverseState() вызван
```

**Оценка:** ~120 строк, ~45 мин

### Шаг 2.3: Интеграция — ReservoirSimulator делегирует NewtonSolver

**Цель:** `PerformNewtonLoop`, `SingleIteration`, `UpdateGrid` делегируют NewtonSolver

**Файлы:**
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSImulator.h` — добавить `NewtonSolver newton_solver_`, убрать `assembler_` (теперь внутри NewtonSolver), убрать объявления `SingleIteration`, `UpdateGrid`
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSimulator.cpp` — `PerformNewtonLoop` делегирует, удалить `SingleIteration`, `UpdateGrid`

**Что сделать:**
1. `ReservoirSImulator.h`:
   - Заменить `JacobianAssembler assembler_` на `NewtonSolver newton_solver_`
   - Оставить `AssembleMyProblem` как делегирующий (для backward compat с test_amgcl_benchmark)
   - Оставить `UpdateGrid` как делегирующий (для backward compat с test_amgcl_benchmark)
   - Убрать `SingleIteration` из public API (не используется тестами напрямую)
2. `ReservoirSimulator.cpp`:
   - `PerformNewtonLoop`: делегирует `newton_solver_.Solve(...)`, обновляет `solverProfile_`
   - `AssembleMyProblem`: делегирует `newton_solver_.assembler().Assemble(...)` — нужен getter для assembler, либо отдельный метод
   - `UpdateGrid`: делегирует `newton_solver_.updateGrid(...)` — нужен public accessor

**Важно:** `test_amgcl_benchmark.cpp` использует `sim.AssembleMyProblem()`, `sim.UpdateGrid()`, `sim.MassBalance()`, `sim.MyProblem.Solve()` в своём ручном Newton loop. Эти методы должны остаться доступны. Два варианта:
- (A) Оставить как делегирующие методы на ReservoirSimulator ← **выбираем**
- (B) Переписать benchmark на использование NewtonSolver — более чистое решение, но ломает тест

**Проверка:**
- Все тесты зелёные
- `ReservoirSimulator.cpp` уменьшился на ~70 строк

**Оценка:** ~30 мин

---

## Этап 3: MassBalanceTracker

Простой аккумулятор. Единственная ответственность: учёт массы нефти и воды в пласте, потоков через границы и дебитов скважин.

### Шаг 3.1: Создать класс MassBalanceTracker

**Цель:** выделить аккумуляторы массового баланса

**Файлы:**
- СОЗДАТЬ: `HydroSolver/Reservoir/MassBalanceTracker.h`
- СОЗДАТЬ: `HydroSolver/Reservoir/MassBalanceTracker.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt`

**Контекст:**
`MassBalance` (ReservoirSimulator.cpp:636-653) обновляет аккумуляторы: `prevOil`, `curOil`, `accumOil`, `accumOilOutFlux`, `accumDebet` и аналогичные для воды. Зависит от: `OilTotal()`, `WaterTotal()`, `OilContourFlux()`, `WaterContourFlux()`, `OilDebitTotal()`, `WaterDebitTotal()`. Все эти методы — read-only обходы Grid и Wells.

**Интерфейс:**

```cpp
class MassBalanceTracker {
public:
    struct State {
        double oil_in_place;
        double water_in_place;
        double accum_oil;
        double accum_water;
        double accum_oil_outflux;
        double accum_water_outflux;
        double accum_oil_debit;
        double accum_water_debit;
        double time;
    };

    void Initialize(double initial_oil, double initial_water);

    void Update(double dt,
                double current_oil, double current_water,
                double oil_contour_flux, double water_contour_flux,
                double oil_debit, double water_debit);

    const State& GetState() const;
    std::vector<double> GetOverallBalance() const;

private:
    State state_{};
};
```

**Что сделать:**
1. Создать файлы, перенести логику из `MassBalance` и аккумуляторных полей
2. `Initialize` — из конструктора ReservoirSimulator (строки 66-74)
3. `Update` — из `MassBalance` (строки 636-653)
4. `GetOverallBalance` — из `GetOverallBalance` (строки 215-220)
5. Добавить в CMakeLists.txt

**Проверка:** сборка, 296+ тестов зелёные

**Оценка:** ~60 строк, ~20 мин

### Шаг 3.2: Unit-тесты MassBalanceTracker

**Файлы:** СОЗДАТЬ `tests/unit/test_MassBalanceTracker.cpp`

**Тесты:**

```
Тест: MassBalanceTracker accumulates correctly
Тег: [mass_balance]
Сценарий: Initialize(100, 50), Update(1.0, 95, 55, 2.0, 3.0, 5.0, 1.0)
Ожидание: accum_oil = 95-100 = -5, accum_oil_outflux = 2.0, accum_oil_debit = -5.0

Тест: MassBalanceTracker multiple steps
Тег: [mass_balance]
Сценарий: три последовательных Update
Ожидание: аккумуляторы складываются корректно

Тест: MassBalanceTracker matches ReservoirSimulator
Тег: [mass_balance]
Сценарий: запуск sim.Solve на 3 шага, сравнение аккумуляторов
Ожидание: идентичные значения
```

**Оценка:** ~80 строк, ~20 мин

### Шаг 3.3: Интеграция — ReservoirSimulator делегирует MassBalanceTracker

**Цель:** убрать аккумуляторные поля из ReservoirSimulator, делегировать MassBalanceTracker

**Что сделать:**
1. Заменить поля `prevOil, curOil, accumOil, accumOilOutFlux, accumDebet, prevWater, curWater, accumWater, accumWaterOutFlux, accumWaterDebet, curTime` на `MassBalanceTracker balance_tracker_`
2. `MassBalance(dt)` → делегирует `balance_tracker_.Update(...)`
3. `GetOverallBalance()` → делегирует `balance_tracker_.GetOverallBalance()`
4. В конструкторе: `balance_tracker_.Initialize(OilTotal(), WaterTotal())`
5. Тесты `test_mass_balance.cpp` вызывают `sim.Solve()` → работают через делегирование

**Проверка:** все тесты зелёные

**Оценка:** ~20 мин

---

## Этап 4: TimeIntegrator и финализация

### Шаг 4.1: Создать класс TimeIntegrator

**Цель:** выделить time-stepping loop

**Файлы:**
- СОЗДАТЬ: `HydroSolver/Reservoir/TimeIntegrator.h`
- СОЗДАТЬ: `HydroSolver/Reservoir/TimeIntegrator.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt`

**Контекст:**
`Solve` (ReservoirSimulator.cpp:395-438) — цикл по временным моментам: для каждого шага вызвать Newton → проверить сходимость → MassBalance → AcceptState → AddFlowFieldSnapShot, или decrease_schemeTau при провале.

**Интерфейс:**

```cpp
class TimeIntegrator {
public:
    struct StepCallbacks {
        std::function<void()> on_accept;       // AcceptState + AddFlowFieldSnapShot
        std::function<void(double)> on_balance; // MassBalance(dt)
    };

    double Integrate(
        const std::vector<double>& timeMoments,
        grid::OilField& grid,
        linear_problem::LinearProblem& problem,
        double refPressure,
        const std::map<WellName, wells::SomeWell*>& wells,
        NumericalParameters& numPrm,
        SolverProfile& profile,
        const StepCallbacks& callbacks);

private:
    NewtonSolver newton_solver_;
};
```

**Что сделать:**
1. Перенести логику `Solve` (строки 395-438)
2. Коллбэки `on_accept` и `on_balance` — для операций, которые зависят от ReservoirSimulator (flowFields, balance_tracker)

**Оценка:** ~60 строк, ~20 мин

### Шаг 4.2: Unit-тесты TimeIntegrator

**Тесты:**

```
Тест: TimeIntegrator produces identical results
Тег: [time_integrator]
Сценарий: 5×5×1, инжектор+продюсер, 3 временных шага
Setup: запустить sim.Solve() и TimeIntegrator::Integrate() с идентичным начальным состоянием
Ожидание: идентичные поля давления и насыщенности после интегрирования

Тест: TimeIntegrator handles wasted trials
Тег: [time_integrator]
Сценарий: большой начальный dt → wasted trial → уменьшение dt → сходимость
Ожидание: profile.n_wasted_trials > 0, итоговое решение корректно
```

**Оценка:** ~100 строк, ~30 мин

### Шаг 4.3: Интеграция — ReservoirSimulator делегирует TimeIntegrator

**Цель:** `ReservoirSimulator::Solve` делегирует TimeIntegrator

**Что сделать:**
1. Заменить `NewtonSolver newton_solver_` на `TimeIntegrator time_integrator_` в ReservoirSimulator
2. `Solve()` → делегирует `time_integrator_.Integrate(...)`, передаёт коллбэки для AcceptState/MassBalance/FlowField
3. Удалить `PerformNewtonLoop` из ReservoirSimulator (или оставить делегирующий для benchmark)
4. Удалить старый код `Solve` из ReservoirSimulator.cpp

**Проверка:** все тесты зелёные

**Оценка:** ~20 мин

### Шаг 4.4: Очистка и финализация

**Цель:** привести ReservoirSimulator в финальное состояние тонкого фасада

**Что сделать:**
1. Убедиться, что `ReservoirSimulator` — фасад: конструктор, `Solve()`, accessors (`GetOilSaturationField`, etc.), `AddWell_FixedProduction`, IO-методы
2. Проверить, что делегирующие методы для backward compat минимальны
3. Обновить vault:
   - Статус DEBT-003 → ✅ РЕАЛИЗОВАНО
   - Обновить [[текущие приоритеты]]
   - Создать заметку в `knowledge/decisions/` о декомпозиции
   - Обновить [[граф зависимостей классов gdm_core]]
   - Обновить [[инвентаризация кодовой базы 2026-06-27]]
4. Подсчитать: ReservoirSimulator.cpp должен уменьшиться с ~1361 строк до ~300-400 строк (IO, accessors, well management, фасадные методы)

**Оценка:** ~30 мин

---

## FluxDiagnostics — отложено

`OverallFluxes` (~275 строк), `OilContourFlux` (~130 строк), `WaterContourFlux` (~70 строк) — всего ~475 строк диагностического кода. Связано с DEBT-014 (дублирование ~450 строк логики потоков). Выделение в отдельный класс `FluxDiagnostics` рационально делать вместе с устранением дублирования (DEBT-014). Откладываем до отдельного плана.

---

## Критерии завершения

- [ ] 4 новых класса: JacobianAssembler, NewtonSolver, MassBalanceTracker, TimeIntegrator
- [ ] ReservoirSimulator — тонкий фасад (~300-400 строк в .cpp, ~100 строк в .h без inline BC)
- [ ] AccountForBoundaryConditions убран из .h в .cpp JacobianAssembler
- [ ] Все 296 существующих тестов зелёные
- [ ] Новые unit-тесты: ~10 тестов для 4 классов
- [ ] Каждый класс тестируем изолированно
- [ ] Производительность не деградировала (время тестов ~119 сек ± 10%)
- [ ] Vault обновлён

## Оценка общая

| Этап | Файлов | Строк (новых) | Время |
|---|---|---|---|
| 1: JacobianAssembler | 3 создать, 3 изменить | ~270 | ~1.5 ч |
| 2: NewtonSolver | 3 создать, 2 изменить | ~220 | ~1.5 ч |
| 3: MassBalanceTracker | 3 создать, 2 изменить | ~140 | ~1 ч |
| 4: TimeIntegrator | 3 создать, 2 изменить | ~160 | ~1.5 ч |
| **Итого** | **12 создать, 9 изменить** | **~790** | **~5.5 ч** |

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай: [[структура проекта и конвенции кода]], [[граф зависимостей классов gdm_core]]
3. Создай ветку: `git checkout -b refactor/debt-003/reservoir-simulator-decomposition`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"`; `cmake --build build --config Release`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни: 296 тестов, ~119 сек — это baseline
7. Начни с Этапа 1, Шаг 1.1. После каждого шага: сборка + тесты
8. Каждый этап — отдельный коммит (или несколько): `refactor: DEBT-003 выделить JacobianAssembler`

## Связанные заметки

- [[технический долг]] — запись DEBT-003
- [[code-review-2026-06-28-архитектурные-ограничения]] — исходный анализ
- [[граф зависимостей классов gdm_core]] — текущая архитектура
- [[структура проекта и конвенции кода]] — конвенции
- [[стратегия тестирования GDM]] — паттерны тестов
- DEBT-014 (дублирование flux-логики) — отложено, делать после DEBT-003
- DEBT-015 (все поля public) — частично решается при декомпозиции
