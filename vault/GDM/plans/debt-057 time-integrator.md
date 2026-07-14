---
tags:
  - план
  - рефакторинг
date: 2026-07-14
issue: DEBT-057
github: 26
branch: refactor/debt-057/time-integrator
status: реализован
audit:
  date: 2026-07-14
  round: 2
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
estimates:
  steps: 4
  files-changed: 4
  files-created: 3
  lines-new: ~170
  lines-removed: ~40
  manual-required: 0
---

# DEBT-057: Выделить TimeIntegrator из ReservoirSimulator

## Мотивация

Этап 4 из 4 декомпозиции DEBT-003. После DEBT-054 (JacobianAssembler), DEBT-055 (NewtonSolver) и DEBT-056 (MassBalanceTracker) в ReservoirSimulator остаётся time-stepping loop в методе `Solve`. Выделение в TimeIntegrator позволит:
- Тестировать time-stepping изолированно
- Упростить ReservoirSimulator (удалить ~40 строк Solve, Solve станет однострочной делегацией)
- Завершить DEBT-003: ReservoirSimulator станет тонким фасадом

Связанные заметки: [[debt-003 refactoring-reservoir-simulator]], [[debt-054 jacobian-assembler]], [[debt-055 newton-solver]], [[debt-056 mass-balance-tracker]]

## Текущее состояние

### Метод Solve (`ReservoirSimulator.cpp:386-429`)

```cpp
double ReservoirSimulator::Solve(const std::vector<double>& timeMoments)
{
    prof.tic("total");

#ifdef PRINT_DEBUG_INFO
    std::ofstream myfile;
    myfile.open("test_tau.txt", std::ios_base::out);
#endif // PRINT_DEBUG_INFO

    for (size_t i = 1; i < timeMoments.size(); i++)
    {
        while (numPrm.CurrentTimeMoment() < timeMoments[i])
        {
            numPrm.update_maxTauAllowed(timeMoments[i], GetWells());
#ifdef PRINT_DEBUG_INFO
            myfile << numPrm.CurrentIntegrationStep() << "  " << numPrm.CurrentTimeMoment() << std::endl;
#endif // PRINT_DEBUG_INFO

            PerformNewtonLoop(numPrm.CurrentIntegrationStep(),
                numPrm.NextTimeMoment());

            if (numPrm.IsSuccessfullNewtonTrial())
            {
                MassBalance(numPrm.CurrentIntegrationStep());
                Grid.AcceptState();
                numPrm.update_currentMoment();
                AddFlowFieldSnapShot();
                solverProfile_.n_time_steps++;
            }
            else
            {
                numPrm.decrease_schemeTau();
                solverProfile_.n_wasted_trials++;
                continue;
            }
        }
    }
#ifdef PRINT_DEBUG_INFO
    myfile.close();
#endif // PRINT_DEBUG_INFO

    prof.toc("total");
    return numPrm.CurrentSchemeTau();
}
```

### Зависимости Solve

| Обращается к | Где | Как используется в Solve |
|---|---|---|
| `numPrm` | `ReservoirSImulator.h:44` | `CurrentTimeMoment()`, `update_maxTauAllowed()`, `CurrentIntegrationStep()`, `NextTimeMoment()`, `IsSuccessfullNewtonTrial()`, `update_currentMoment()`, `decrease_schemeTau()`, `CurrentSchemeTau()` |
| `solverProfile_` | `ReservoirSImulator.h:69` | `n_time_steps++`, `n_wasted_trials++` |
| `PerformNewtonLoop` | `ReservoirSimulator.cpp:430-434` | Newton loop → `newton_solver_.Solve(...)` |
| `MassBalance` | `ReservoirSimulator.cpp:465-471` | `balance_tracker_.Update(...)` |
| `Grid.AcceptState()` | `OilField` | фиксация решения после успешного trial |
| `AddFlowFieldSnapShot()` | `ReservoirSimulator.cpp:452-464` | snapshot потоков для streamlines |
| `GetWells()` | `ReservoirSImulator.h:46` | передаётся в `update_maxTauAllowed` |
| `prof` | глобальный AMGCL profiler | `tic/toc("total")` |
| `PRINT_DEBUG_INFO` | условная компиляция | фактически мёртвый код (макрос `_PRINT_DEBUG_INFO` ≠ `PRINT_DEBUG_INFO`) |

### Решение по интерфейсу

**Эскиз DEBT-003** предполагал, что TimeIntegrator **владеет** `newton_solver_` и принимает Grid, Problem, Wells, refPressure по ссылке (8 параметров). Это ломает паттерн DEBT-054/055/056, где подкомпоненты остаются у ReservoirSimulator, а новый класс — чистая функция.

**Выбранный подход:** TimeIntegrator — stateless оркестратор time-loop. **Не владеет** `newton_solver_`, `balance_tracker_` и другими подсистемами. Все операции, зависящие от ReservoirSimulator, передаются через 4 коллбэка в `StepCallbacks`. TimeIntegrator работает только с `NumericalParameters` (по ссылке) и `SolverProfile` (по ссылке) — это чистые data-классы без тяжёлых зависимостей.

Преимущества:
- Минимальный интерфейс: 4 параметра (timeMoments + numPrm + profile + callbacks)
- Не ломает существующие тесты (`PerformNewtonLoop`, `MassBalance` остаются у ReservoirSimulator)
- Тестируемый с подставными коллбэками
- Паттерн близок к DEBT-054/055/056 (данные передаются, а не захватываются)

### Call sites

| Место | Что вызывает | Нужны изменения? |
|---|---|---|
| `src/main.cpp:78` | `simulator.Solve(timeMoments)` | нет (делегация) |
| `CalculationManager.cpp:68` | `simulator->Solve(...)` | нет |
| `test_smoke.cpp` (2×) | `simulator.Solve(...)` | нет |
| `test_stationary_pressure.cpp` (3×) | `sim.Solve(...)` | нет |
| `test_mass_balance.cpp` (3×) | `sim.Solve(...)` | нет |
| `test_buckley_leverett.cpp` (6×) | `sim.Solve(...)` | нет |
| `test_components.cpp` | `sim.Solve(...)` | нет |
| `test_five_spot.cpp` (2×) | `sim.Solve(...)` | нет |
| `test_visual_verification.cpp` | `sim.Solve(...)` | нет |
| `test_variable_debit.cpp` | `sim.Solve(...)` | нет |
| `test_3d_completions.cpp` | `sim.Solve(...)` | нет |
| `example_runner.h` (2×) | `sim.Solve(...)` | нет |
| `ex_five_spot.cpp` | `sim.Solve(...)` | нет |

Все call sites используют публичный API `sim.Solve(timeMoments)`, который остаётся как делегирующая обёртка — изменения call sites не нужны.

## Целевое состояние

```
ReservoirSimulator
  ├── newton_solver_: NewtonSolver
  ├── balance_tracker_: MassBalanceTracker
  ├── time_integrator_: TimeIntegrator       // НОВЫЙ, time-loop orchestrator
  ├── Solve(timeMoments)                     // делегирует time_integrator_.Integrate(...)
  ├── PerformNewtonLoop(loc_tau, ...)        // остаётся (backward compat для тестов)
  ├── MassBalance(loc_tau)                   // делегирует balance_tracker_
  ├── GetOverallBalance()                    // делегирует balance_tracker_
  └── OilTotal, WaterTotal, ...             // остаются
```

## Варианты решения

### Вариант A: Коллбэки — stateless orchestrator (рекомендуемый)

```cpp
class TimeIntegrator {
public:
    struct StepCallbacks {
        std::function<void(double nextRefMoment)> prepare_step;
        std::function<void(double loc_tau, double nextTime)> perform_newton;
        std::function<void(double loc_tau)> on_accept;  // MassBalance + AcceptState (до update_currentMoment)
        std::function<void()> on_post_update;            // AddFlowFieldSnapShot (после update_currentMoment)
    };

    double Integrate(
        const std::vector<double>& timeMoments,
        NumericalParameters& numPrm,
        SolverProfile& profile,
        const StepCallbacks& callbacks);
};
```

`prepare_step` → `numPrm.update_maxTauAllowed(nextRefMoment, GetWells())`
`perform_newton` → `PerformNewtonLoop(loc_tau, nextTime)`
`on_accept` → `MassBalance(loc_tau) + Grid.AcceptState()` (до `update_currentMoment`)
`on_post_update` → `AddFlowFieldSnapShot()` (после `update_currentMoment`)

**Почему 4 коллбэка, а не 3:** В оригинальном Solve порядок: `MassBalance → AcceptState → update_currentMoment → AddFlowFieldSnapShot`. `AddFlowFieldSnapShot` использует `numPrm.CurrentTimeMoment()` для временной метки snapshot (строка 462), поэтому должен вызываться ПОСЛЕ `update_currentMoment`. Если объединить все 3 в один `on_accept`, snapshot получит старую метку времени — побитовое несовпадение и физически неверная привязка ко времени.

TimeIntegrator сам вызывает `numPrm.IsSuccessfullNewtonTrial()`, `numPrm.update_currentMoment()`, `numPrm.decrease_schemeTau()` — это чисто NumericalParameters API, без зависимости от Grid/Wells.

**Плюсы:** минимальный интерфейс (4 параметра), TimeIntegrator без зависимостей, тестируемый с подставными коллбэками. Паттерн близок к DEBT-054/055/056.

**Минусы:** 4 `std::function` — overhead вызова (но вызывается 1 раз за time step, не горячий путь). `std::function` через `<functional>` — стандартный заголовок.

### Вариант B: TimeIntegrator владеет NewtonSolver (эскиз DEBT-003)

```cpp
class TimeIntegrator {
    NewtonSolver newton_solver_;
public:
    double Integrate(timeMoments, grid, problem, refPressure, wells,
                     numPrm, profile, callbacks);
};
```

**Плюсы:** TimeIntegrator — полноценный владелец вычислительного pipeline.

**Минусы:** 8 параметров. `newton_solver_` переезжает из ReservoirSimulator → ломает `test_NewtonSolver_standalone.cpp` и `test_MassBalanceTracker_standalone.cpp` (они обращаются к `sim_new.PerformNewtonLoop`). Нужно переписывать тесты или добавлять accessor. Несовместимо с паттерном DEBT-054/055/056.

**Выбор:** Вариант A. Совместим с паттерном предыдущих задач, минимальный diff, не ломает тесты.

## Поиск подводных камней

- [✅] **Все call sites найдены:** `Solve` — 26 вызовов в тестах, examples, main, CalculationManager. Все через публичный API, делегация сохраняет совместимость
- [✅] **Потокобезопасность:** Solve не вызывается из OpenMP-секций
- [✅] **Зависимости сборки:** TimeIntegrator.h включает только `<vector>`, `<functional>`, forward-declarations NumericalParameters и SolverProfile. 4 коллбэка в StepCallbacks — все через `std::function`
- [✅] **Обратная совместимость API:** `sim.Solve(timeMoments)` остаётся. `PerformNewtonLoop`, `MassBalance`, `AddFlowFieldSnapShot` остаются
- [✅] **Тесты:** все существующие тесты компилируются — call sites не меняются
- [✅] **PRINT_DEBUG_INFO:** фактически мёртвый код (макрос `_PRINT_DEBUG_INFO` ≠ `PRINT_DEBUG_INFO`). Не переносить в TimeIntegrator
- [✅] **prof.tic/toc:** глобальный AMGCL profiler. TimeIntegrator может включить через `#include` LinearProblem.h или напрямую. Альтернатива: оставить в ReservoirSimulator::Solve (обёртка вызывает `prof.tic`, затем `time_integrator_.Integrate(...)`, затем `prof.toc`)
- [✅] **Связь с другими задачами:** после DEBT-057 завершается DEBT-003 целиком
- [✅] **Производительность:** Integrate вызывается 1 раз за Solve, коллбэки — 1 раз за time step. 4 `std::function` overhead пренебрежим
- [✅] **Порядок операций:** `on_accept` (MassBalance + AcceptState) → `update_currentMoment` → `on_post_update` (AddFlowFieldSnapShot) — точно воспроизводит порядок из оригинального Solve (строки 409-412). `AddFlowFieldSnapShot` использует `numPrm.CurrentTimeMoment()` для временной метки, поэтому критично вызывать его ПОСЛЕ `update_currentMoment`

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные заметки: [[debt-003 refactoring-reservoir-simulator]]
3. Создай ветку: `git checkout -b refactor/debt-057/time-integrator`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Baseline: **306 тестов, ~168 сек**
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Шаг 1: Создать TimeIntegrator (заголовок + реализация + CMake)

**Цель:** Новый класс TimeIntegrator компилируется, но нигде не используется.

**Файлы:**
- `HydroSolver/Reservoir/TimeIntegrator.h` — создать
- `HydroSolver/Reservoir/TimeIntegrator.cpp` — создать
- `CMakeLists.txt` — добавить source

**Контекст:**
TimeIntegrator — stateless оркестратор time-stepping loop. Не владеет Newton, Grid, Wells или другими подсистемами. Принимает `NumericalParameters&` и `SolverProfile&` по ссылке (чистые data-классы), а все операции, зависящие от ReservoirSimulator (`PerformNewtonLoop`, `MassBalance`, `Grid.AcceptState`, `AddFlowFieldSnapShot`, `numPrm.update_maxTauAllowed`) получает через 4 коллбэка в `StepCallbacks`.

Важно: `on_accept` вызывается ДО `update_currentMoment`, а `on_post_update` — ПОСЛЕ. Это точно воспроизводит порядок из оригинального Solve: `MassBalance → AcceptState → update_currentMoment → AddFlowFieldSnapShot`. `AddFlowFieldSnapShot` использует `numPrm.CurrentTimeMoment()` для временной метки snapshot, поэтому должен вызываться после обновления момента времени.

`prof.tic/toc("total")` — глобальный AMGCL profiler. Оставляем в ReservoirSimulator::Solve (обёртка вызывает prof.tic, затем Integrate, затем prof.toc), чтобы TimeIntegrator не зависел от AMGCL.

`PRINT_DEBUG_INFO` — фактически мёртвый код (макрос `_PRINT_DEBUG_INFO` ≠ `PRINT_DEBUG_INFO` в `Defines.h`). Не переносить.

**Что сделать:**

1. Создать `HydroSolver/Reservoir/TimeIntegrator.h`:

```cpp
#pragma once
#include <vector>
#include <functional>

namespace reservoir_simulator {

class NumericalParameters;
struct SolverProfile;

class TimeIntegrator {
public:
    struct StepCallbacks {
        std::function<void(double nextRefMoment)> prepare_step;
        std::function<void(double loc_tau, double nextTime)> perform_newton;
        std::function<void(double loc_tau)> on_accept;  // до update_currentMoment
        std::function<void()> on_post_update;            // после update_currentMoment
    };

    double Integrate(
        const std::vector<double>& timeMoments,
        NumericalParameters& numPrm,
        SolverProfile& profile,
        const StepCallbacks& callbacks);
};

} // namespace reservoir_simulator
```

2. Создать `HydroSolver/Reservoir/TimeIntegrator.cpp`:

```cpp
#include "TimeIntegrator.h"
#include "NumericalParameters.h"
#include "ReservoirSImulator.h"

namespace reservoir_simulator {

double TimeIntegrator::Integrate(
    const std::vector<double>& timeMoments,
    NumericalParameters& numPrm,
    SolverProfile& profile,
    const StepCallbacks& callbacks)
{
    for (size_t i = 1; i < timeMoments.size(); i++)
    {
        while (numPrm.CurrentTimeMoment() < timeMoments[i])
        {
            callbacks.prepare_step(timeMoments[i]);

            callbacks.perform_newton(
                numPrm.CurrentIntegrationStep(),
                numPrm.NextTimeMoment());

            if (numPrm.IsSuccessfullNewtonTrial())
            {
                callbacks.on_accept(numPrm.CurrentIntegrationStep());
                numPrm.update_currentMoment();
                callbacks.on_post_update();
                profile.n_time_steps++;
            }
            else
            {
                numPrm.decrease_schemeTau();
                profile.n_wasted_trials++;
            }
        }
    }
    return numPrm.CurrentSchemeTau();
}

} // namespace reservoir_simulator
```

Примечания:
- `#include "ReservoirSImulator.h"` нужен для определения `SolverProfile` (struct определён в ReservoirSImulator.h:16-23). Альтернатива: вынести SolverProfile в отдельный заголовок — out of scope для DEBT-057, но TODO для будущего рефакторинга.
- `continue` из оригинального Solve убран — `else` и так завершает тело while-итерации, `continue` был лишним
- `PRINT_DEBUG_INFO` не переносится (мёртвый код)
- Порядок операций в Integrate: `on_accept` → `update_currentMoment` → `on_post_update` → `n_time_steps++` — точно воспроизводит оригинальный Solve (строки 409-413)

3. В `CMakeLists.txt` — добавить `${HYDRO}/Reservoir/TimeIntegrator.cpp` в секцию `# Reservoir` (строки 41-45). Вставить перед или после существующих файлов:

До:
```
    # Reservoir
    ${HYDRO}/Reservoir/MassBalanceTracker.cpp
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NewtonSolver.cpp
```
После:
```
    # Reservoir
    ${HYDRO}/Reservoir/MassBalanceTracker.cpp
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NewtonSolver.cpp
    ${HYDRO}/Reservoir/TimeIntegrator.cpp
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 306 тестов зелёные (TimeIntegrator создан, но не используется)

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2, 3

**Оценка:** ~55 строк, ~5 минут

---

## Шаг 2: Побитовые тесты TimeIntegrator

**Цель:** Доказать что `TimeIntegrator::Integrate()` с правильными коллбэками даёт побитово идентичные результаты с `ReservoirSimulator::Solve()`.

**Файлы:**
- `tests/unit/math/test_TimeIntegrator_standalone.cpp` — создать
- `CMakeLists.txt` — добавить в gdm_unit_level4

**Контекст:**
Паттерн из DEBT-055/056: два идентичных симулятора, один использует старый код (`sim_old.Solve`), другой — `TimeIntegrator::Integrate()` с коллбэками, привязанными к `sim_new`. Побитовое сравнение полей P, Sw и баланса масс.

Для побитового теста нужно:
1. Создать два одинаковых симулятора `sim_old` и `sim_new`
2. Прогнать `sim_old.Solve(timeMoments)` на первом
3. Вручную вызвать `TimeIntegrator::Integrate(timeMoments, sim_new.numPrm, sim_new.solverProfile_, callbacks)` на втором, где callbacks привязаны к методам sim_new
4. Сравнить `sim_old.GetPressureField()` vs `sim_new.GetPressureField()`, `sim_old.GetWaterSaturationField()` vs `sim_new.GetWaterSaturationField()`, `sim_old.GetOverallBalance()` vs `sim_new.GetOverallBalance()`

Ключевая деталь: коллбэки должны точно воспроизводить логику Solve с сохранением порядка:
- `prepare_step(nextRef)` → `sim_new.numPrm.update_maxTauAllowed(nextRef, sim_new.GetWells())`
- `perform_newton(tau, nextTime)` → `sim_new.PerformNewtonLoop(tau, nextTime)`
- `on_accept(tau)` → `sim_new.MassBalance(tau); sim_new.Grid.AcceptState();` (до update_currentMoment)
- `on_post_update()` → `sim_new.AddFlowFieldSnapShot();` (после update_currentMoment — snapshot использует numPrm.CurrentTimeMoment())

**Что сделать:**

1. Создать `tests/unit/math/test_TimeIntegrator_standalone.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/TimeIntegrator.h"
#include "../../../HydroSolver/Reservoir/ReservoirSImulator.h"

using namespace reservoir_simulator;

namespace {

ReservoirSimulator make_sim(
    size_t Nx, size_t Ny, size_t Nz,
    double Lx, double Ly, double hz,
    double perm_mD, double poro,
    double P_init_atm, double oil_saturation)
{
    auto h = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz,
        perm_mD, poro, P_init_atm, oil_saturation);
    auto np = test_helpers::default_num_params();
    return ReservoirSimulator(np, h, h.oil, h.water, h.other);
}

void compare_solve(ReservoirSimulator& sim_old,
                   ReservoirSimulator& sim_new,
                   const std::vector<double>& timeMoments)
{
    // Old path: ReservoirSimulator::Solve
    sim_old.Solve(timeMoments);

    // New path: TimeIntegrator::Integrate with callbacks
    TimeIntegrator integrator;
    TimeIntegrator::StepCallbacks callbacks;
    callbacks.prepare_step = [&](double nextRef) {
        sim_new.numPrm.update_maxTauAllowed(nextRef, sim_new.GetWells());
    };
    callbacks.perform_newton = [&](double tau, double nextTime) {
        sim_new.PerformNewtonLoop(tau, nextTime);
    };
    callbacks.on_accept = [&](double tau) {
        sim_new.MassBalance(tau);
        sim_new.Grid.AcceptState();
    };
    callbacks.on_post_update = [&]() {
        sim_new.AddFlowFieldSnapShot();
    };

    integrator.Integrate(timeMoments, sim_new.numPrm,
        sim_new.solverProfile_, callbacks);

    // Compare pressure fields
    auto P_old = sim_old.GetPressureField();
    auto P_new = sim_new.GetPressureField();
    REQUIRE(P_old.size() == P_new.size());
    for (size_t i = 0; i < P_old.size(); ++i) {
        INFO("P[" << i << "]");
        REQUIRE(P_old[i] == P_new[i]);
    }

    // Compare saturation fields
    auto Sw_old = sim_old.GetWaterSaturationField();
    auto Sw_new = sim_new.GetWaterSaturationField();
    REQUIRE(Sw_old.size() == Sw_new.size());
    for (size_t i = 0; i < Sw_old.size(); ++i) {
        INFO("Sw[" << i << "]");
        REQUIRE(Sw_old[i] == Sw_new[i]);
    }

    // Compare mass balance
    auto bal_old = sim_old.GetOverallBalance();
    auto bal_new = sim_new.GetOverallBalance();
    REQUIRE(bal_old.size() == bal_new.size());
    for (size_t i = 0; i < bal_old.size(); ++i) {
        INFO("balance[" << i << "]");
        REQUIRE(bal_old[i] == bal_new[i]);
    }

    // Compare solver profiles
    REQUIRE(sim_old.solverProfile_.n_time_steps ==
            sim_new.solverProfile_.n_time_steps);
    REQUIRE(sim_old.solverProfile_.n_wasted_trials ==
            sim_new.solverProfile_.n_wasted_trials);
}

} // anonymous namespace

TEST_CASE("TimeIntegrator: bitwise match 1D 5x1x1 no wells",
    "[TimeIntegrator][bitwise]")
{
    auto sim_old = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto sim_new = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_solve(sim_old, sim_new, {0.0, 10.0});
}

TEST_CASE("TimeIntegrator: bitwise match 2D 5x5x1 with wells",
    "[TimeIntegrator][bitwise]")
{
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();

    auto sim_old = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);
    auto sim_new = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);

    test_helpers::add_simple_well(sim_old, horizon, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_old, horizon, L"PROD", 450.0, 250.0, 5.0, 0.0);
    test_helpers::add_simple_well(sim_new, horizon, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_new, horizon, L"PROD", 450.0, 250.0, 5.0, 0.0);

    compare_solve(sim_old, sim_new, {0.0, 10.0});
}

TEST_CASE("TimeIntegrator: bitwise match multi-step",
    "[TimeIntegrator][bitwise]")
{
    auto sim_old = make_sim(5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto sim_new = make_sim(5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_solve(sim_old, sim_new, {0.0, 5.0, 10.0, 20.0});
}
```

2. В `CMakeLists.txt` — добавить `tests/unit/math/test_TimeIntegrator_standalone.cpp` в `gdm_unit_level4` (рядом с test_NewtonSolver_standalone.cpp, ~строка 214).

До:
```
    tests/unit/math/test_MassBalanceTracker_standalone.cpp
    tests/unit/math/test_NewtonSolver_standalone.cpp
)
```
После:
```
    tests/unit/math/test_MassBalanceTracker_standalone.cpp
    tests/unit/math/test_NewtonSolver_standalone.cpp
    tests/unit/math/test_TimeIntegrator_standalone.cpp
)
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 309 тестов (306 + 3 новых), все зелёные
- Побитовое совпадение: P, Sw, balance, solverProfile — для всех сценариев

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 3

**Оценка:** ~110 строк, ~10 минут

---

## Шаг 3: Интеграция TimeIntegrator в ReservoirSimulator

**Цель:** ReservoirSimulator владеет `time_integrator_`, метод `Solve` делегирует `time_integrator_.Integrate(...)`.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSImulator.h` — изменить
- `HydroSolver/Reservoir/ReservoirSimulator.cpp` — изменить

**Контекст:**
Паттерн делегации идентичен DEBT-054/055/056: старый метод остаётся, но его тело заменяется вызовом нового класса. В данном случае `Solve` вызывает `time_integrator_.Integrate(...)` с коллбэками, привязанными к методам ReservoirSimulator.

`prof.tic/toc("total")` остаётся в Solve (обёртка), не переносится в TimeIntegrator.

**Что сделать:**

1. В `ReservoirSImulator.h`:
   - Добавить `#include "TimeIntegrator.h"` (рядом с `#include "MassBalanceTracker.h"`, строка ~10)
   - Добавить поле `TimeIntegrator time_integrator_;` рядом с `NewtonSolver newton_solver_;` (строка ~71)

До:
```cpp
#include "MassBalanceTracker.h"
#include "NewtonSolver.h"
```
После:
```cpp
#include "MassBalanceTracker.h"
#include "NewtonSolver.h"
#include "TimeIntegrator.h"
```

До (строка ~71):
```cpp
		NewtonSolver newton_solver_;
```
После:
```cpp
		NewtonSolver newton_solver_;
		TimeIntegrator time_integrator_;
```

2. В `ReservoirSimulator.cpp`:
   - Метод `Solve` (строки 386-429) — заменить тело на делегацию:

До:
```cpp
	double ReservoirSimulator::Solve(const std::vector<double>& timeMoments)
	{
		prof.tic("total");

#ifdef PRINT_DEBUG_INFO
		std::ofstream myfile;
		myfile.open("test_tau.txt", std::ios_base::out);
#endif // PRINT_DEBUG_INFO

		for (size_t i = 1; i < timeMoments.size(); i++)
		{
			while (numPrm.CurrentTimeMoment() < timeMoments[i])
			{
				numPrm.update_maxTauAllowed(timeMoments[i], GetWells());
#ifdef PRINT_DEBUG_INFO
				myfile << numPrm.CurrentIntegrationStep() << "  " << numPrm.CurrentTimeMoment() << std::endl;
#endif // PRINT_DEBUG_INFO

				PerformNewtonLoop(numPrm.CurrentIntegrationStep(),
					numPrm.NextTimeMoment());

				if (numPrm.IsSuccessfullNewtonTrial())
				{
					MassBalance(numPrm.CurrentIntegrationStep());
					Grid.AcceptState();
					numPrm.update_currentMoment();
					AddFlowFieldSnapShot();
					solverProfile_.n_time_steps++;
				}
				else
				{
					numPrm.decrease_schemeTau();
					solverProfile_.n_wasted_trials++;
					continue;
				}
			}
		}
#ifdef PRINT_DEBUG_INFO
		myfile.close();
#endif // PRINT_DEBUG_INFO

		prof.toc("total");
		return numPrm.CurrentSchemeTau();
	}
```
После:
```cpp
	double ReservoirSimulator::Solve(const std::vector<double>& timeMoments)
	{
		prof.tic("total");

		TimeIntegrator::StepCallbacks callbacks;
		callbacks.prepare_step = [this](double nextRef) {
			numPrm.update_maxTauAllowed(nextRef, GetWells());
		};
		callbacks.perform_newton = [this](double tau, double nextTime) {
			PerformNewtonLoop(tau, nextTime);
		};
		callbacks.on_accept = [this](double tau) {
			MassBalance(tau);
			Grid.AcceptState();
		};
		callbacks.on_post_update = [this]() {
			AddFlowFieldSnapShot();
		};

		auto result = time_integrator_.Integrate(
			timeMoments, numPrm, solverProfile_, callbacks);

		prof.toc("total");
		return result;
	}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 309 тестов, все зелёные
- Побитовые тесты из шага 2 подтверждают идентичность
- Тесты `test_mass_balance`, `test_buckley_leverett`, `test_stationary_pressure`, `test_visual_verification`, `test_variable_debit` — зелёные (используют `sim.Solve(...)`)

**Подводные камни:**
- `[this]` capture — ReservoirSimulator::Solve не const, поэтому `[this]` безопасен
- Лямбды не переживают вызов Integrate (создаются на стеке, уничтожаются после return) — lifetime safe
- `prof.tic/toc` остаётся снаружи Integrate — обёртка добавляет 2 строки, но сохраняет ту же семантику профилирования

**Зависимости:**
- Требует: шаг 1, шаг 2
- Блокирует: шаг 4

**Оценка:** ~20 строк добавлений, ~35 строк удалений, ~10 минут

---

## Шаг 4: Vault и GitHub

**Цель:** Обновить документацию, статус задачи DEBT-057 и DEBT-003, прокомментировать issues.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md` — обновить статус DEBT-057 и DEBT-003
- `vault/GDM/00-home/текущие приоритеты.md` — добавить строку DEBT-057 ✅, DEBT-003 ✅
- `vault/GDM/00-home/index.md` — добавить ссылку на план

**Что сделать:**

1. В `известные баги и технический долг.md` — обновить статус DEBT-057 на ✅, DEBT-003 на ✅
2. В `текущие приоритеты.md` — добавить строки:
   ```
   - **DEBT-057:** ✅ TimeIntegrator выделен (2026-07-XX, ветка refactor/debt-057/time-integrator, +3 теста)
   - **DEBT-003:** ✅ декомпозиция ReservoirSimulator завершена (4/4: JacobianAssembler, NewtonSolver, MassBalanceTracker, TimeIntegrator)
   ```
3. В `index.md` — добавить:
   ```
   - [[debt-057 time-integrator]] — DEBT-057: выделить TimeIntegrator из ReservoirSimulator (этап 4 DEBT-003)
   ```
4. GitHub:
   - `gh issue comment 26 --repo ArturSalamatin/GDM --body "Реализовано. +3 побитовых теста. Merge → experimental."`
   - `gh issue comment 20 --repo ArturSalamatin/GDM --body "DEBT-003 завершена. Все 4 этапа: DEBT-054 JacobianAssembler, DEBT-055 NewtonSolver, DEBT-056 MassBalanceTracker, DEBT-057 TimeIntegrator."`

**Зависимости:**
- Требует: шаг 3

**Оценка:** ~5 минут

---

## Критерии завершения

- [ ] TimeIntegrator.h + TimeIntegrator.cpp созданы
- [ ] `Solve()` делегирует `time_integrator_.Integrate(...)` с коллбэками
- [ ] `PerformNewtonLoop`, `MassBalance`, `AddFlowFieldSnapShot` остаются у ReservoirSimulator
- [ ] Все существующие тесты зелёные (306)
- [ ] 3 новых побитовых теста зелёные (309 всего)
- [ ] CMakeLists.txt обновлён
- [ ] Vault обновлён: статус DEBT-057 + DEBT-003, приоритеты, index
- [ ] GitHub issues #26 (DEBT-057) и #20 (DEBT-003) прокомментированы
- [ ] Производительность не деградировала (Solve — не горячий путь, `std::function` overhead пренебрежим)

## Обнаруженные проблемы

- `SolverProfile` определён в `ReservoirSImulator.h` (строки 16-23). TimeIntegrator.cpp включает `ReservoirSImulator.h` для его определения. В будущем стоит вынести SolverProfile в отдельный заголовок. Не блокирует DEBT-057, но кандидат на DEBT (избыточный include chain).

## Связанные заметки

- [[debt-003 refactoring-reservoir-simulator]] — общий план декомпозиции (этап 4)
- [[debt-054 jacobian-assembler]] — этап 1 (завершён)
- [[debt-055 newton-solver]] — этап 2 (завершён)
- [[debt-056 mass-balance-tracker]] — этап 3 (завершён)
- [[технический долг]] — реестр DEBT-057
