---
tags:
  - план
  - рефакторинг
date: 2026-07-13
issue: DEBT-055
github: 24
branch: refactor/debt-055/newton-solver
status: реализован
estimates:
  steps: 4
  files-changed: 5
  files-created: 3
  lines-new: ~180
  lines-removed: ~75
  manual-required: 0
audit:
  date: 2026-07-13
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  note: повторный аудит, проблем не найдено
---

# DEBT-055: Выделить NewtonSolver из ReservoirSimulator

## Мотивация

Этап 2 из 4 декомпозиции DEBT-003. После DEBT-054 (JacobianAssembler) в ReservoirSimulator остаётся Newton-логика (~62 строки), переплетённая с полями класса. Выделение в NewtonSolver позволит:
- Тестировать Newton-сходимость изолированно (без time-stepping)
- Подготовить почву для DEBT-057 (TimeIntegrator)
- Уменьшить ReservoirSimulator с ~1217 до ~1150 строк (.cpp)

Подтип: **рефакторинг** (выделение модуля).

## Текущее состояние (baseline)

- **Тесты:** 300 pass, Release, ~135 сек
- **Файлы:**
  - `HydroSolver/Reservoir/ReservoirSimulator.cpp` — 1217 строк
  - `HydroSolver/Reservoir/ReservoirSImulator.h` — 166 строк
- **Затронутые методы:**
  - `PerformNewtonLoop` (строки 439–461) — внешний цикл Ньютона
  - `SingleIteration` (строки 490–501) — assemble → solve → update AMG state
  - `UpdateGrid` (строки 462–488) — применение коррекций, проверка сходимости
- **Call sites в тестах:**
  - `sim.SingleIteration()` — 2 вызова в `tests/test_inactive_cells.cpp` (строки 25, 54)
  - `sim.UpdateGrid()` — 2 вызова в `tests/test_amgcl_benchmark.cpp` (строки 239, 381)
  - `sim.AssembleMyProblem()` — используется в benchmark (строки 221, 363) и unit-тестах
  - `sim.Solve()` — ~15 вызовов в интеграционных тестах (не затрагиваются)
- **Зона неприкосновенности:**
  - `test_amgcl_benchmark.cpp` — ручной Newton loop с кастомным солвером и chrono-инструментированием. НЕ ТРОГАТЬ
  - `sim.Solve()` — остаётся в ReservoirSimulator (DEBT-057)
  - `sim.AssembleMyProblem()` — делегирующий метод, остаётся

## Целевое состояние

```
ReservoirSimulator
  ├── newton_solver_: NewtonSolver      // НОВЫЙ, владеет assembler_
  │     └── assembler_: JacobianAssembler
  ├── Solve(timeMoments)                // остаётся, вызывает newton_solver_.Solve()
  ├── PerformNewtonLoop(tau, t)         // делегирует newton_solver_.Solve()
  ├── SingleIteration(tau, t)           // делегирует newton_solver_ (backward compat)
  ├── AssembleMyProblem(tau, t)         // делегирует newton_solver_ (backward compat)
  └── MassBalance, AddFlowFieldSnapShot, ...  // остаются
```

**Изменения зависимостей:**
- `assembler_` переезжает из ReservoirSimulator в NewtonSolver
- NewtonSolver зависит от: OilField, LinearProblem, NumericalParameters, JacobianAssembler, SolverProfile — все через параметры
- ReservoirSimulator зависит от NewtonSolver (владение)

## Подводные камни

- ✅ **Потокобезопасность:** `UpdateGrid` использует `#pragma omp parallel for`. Нет mutable state — всё через параметры. Корректно при переносе
- ✅ **`prof` доступен:** определён в `LinearProblem.h` через `__declspec(selectany)`, NewtonSolver будет включать LinearProblem.h
- ✅ **`B` доступен:** constexpr в namespace `linear_problem`, доступен через include
- ✅ **`USE_PARALLEL`:** определён в `Helpers/Defines.h`, доступен через `stdafx.h`
- ⚠️ **`sim.SingleIteration()` в тестах:** `test_inactive_cells.cpp` вызывает `sim.SingleIteration(dt, dt)`. Нужен делегирующий метод в ReservoirSimulator
- ⚠️ **`sim.UpdateGrid()` в benchmark:** `test_amgcl_benchmark.cpp` вызывает `sim.UpdateGrid()` напрямую. Нужен делегирующий метод или оставить public-метод в ReservoirSimulator. Решение: оставить делегирующий `UpdateGrid()` (benchmark также обращается к `sim.Grid.ReverseState()` и `sim.numPrm.*` напрямую — полный перевод на NewtonSolver потребует переписать benchmark, что не входит в scope)
- ✅ **CMake:** новый .cpp добавляется в `target_sources(gdm_core)` секция `# Reservoir`
- ✅ **Связь с другими задачами:** не конфликтует. DEBT-057 (TimeIntegrator) зависит от этой задачи, но не блокируется

---

# Шаги

## Шаг 1: Создать класс NewtonSolver

**Цель:** создать новый класс рядом с ReservoirSimulator, не трогая существующий код

**Файлы:**
- СОЗДАТЬ: `HydroSolver/Reservoir/NewtonSolver.h`
- СОЗДАТЬ: `HydroSolver/Reservoir/NewtonSolver.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt` (строка ~43, секция `# Reservoir`)

**Контекст:**

Сейчас `PerformNewtonLoop` (ReservoirSimulator.cpp:439–461) выполняет:
1. Инициализация Newton-счётчиков через `numPrm`
2. While-цикл: `SingleIteration` → проверка AMG → `UpdateGrid` → проверка Newton-сходимости
3. При провале: `Grid.ReverseState()` и выход

`SingleIteration` (строки 490–501):
1. `AssembleMyProblem(loc_tau, nextTimeMoment)` — делегирует `assembler_.Assemble()`
2. `MyProblem.Solve(maxIter)` → `SolveResult{iters, error, converged}`
3. `numPrm.update_currentAMGState(...)` + обновление `solverProfile_`

`UpdateGrid` (строки 462–488):
1. OpenMP-цикл: `MyProblem.UnpackCellCorrections(l, corr)` → `Grid[l].UpdateState(corr)`
2. Проверка сходимости: относительная ошибка по Sw (< NewtonTol * 3e-3) и P (< NewtonTol * |P|)
3. `std::all_of` → converged

Новый `NewtonSolver::Solve()` объединит все три метода. Владеет `JacobianAssembler assembler_`.

**Что сделать:**

1. Создать `HydroSolver/Reservoir/NewtonSolver.h`:

```cpp
#pragma once

#include "../stdafx.h"
#include "JacobianAssembler.h"
#include "NumericalParameters.h"
#include "../Solver/Grids/OilField.h"
#include "../Solver/Math/LinearProblem.h"

namespace reservoir_simulator
{
	namespace wells { class SomeWell; }
	struct SolverProfile;

	class NewtonSolver
	{
	public:
		void Solve(
			double loc_tau, double nextTimeMoment,
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			NumericalParameters& numPrm,
			double refPressure,
			const std::map<WellName, wells::SomeWell*>& wells,
			SolverProfile& profile);

		void SingleIteration(
			double loc_tau, double nextTimeMoment,
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			NumericalParameters& numPrm,
			double refPressure,
			const std::map<WellName, wells::SomeWell*>& wells,
			SolverProfile& profile);

		bool UpdateGrid(
			grid::OilField& grid,
			linear_problem::LinearProblem& problem,
			const NumericalParameters& numPrm);

		JacobianAssembler assembler_;
	};
} // namespace reservoir_simulator
```

2. Создать `HydroSolver/Reservoir/NewtonSolver.cpp`:

```cpp
#include "NewtonSolver.h"
#include "ReservoirSImulator.h"  // SolverProfile definition
#include "Well/Wells.h"

namespace reservoir_simulator
{
	using namespace cell;
	using namespace grid;
	using namespace linear_problem;

	void NewtonSolver::Solve(
		double loc_tau, double nextTimeMoment,
		OilField& grid, LinearProblem& problem,
		NumericalParameters& numPrm, double refPressure,
		const std::map<WellName, wells::SomeWell*>& wells,
		SolverProfile& profile)
	{
		numPrm.set_currentNewtonIterationCount(0);
		numPrm.update_isSuccesfullNewtonTrial(false);
		numPrm.set_currentAMG_maxSolverIterationCount();
		while (!numPrm.IsSuccessfullNewtonTrial())
		{
			SingleIteration(loc_tau, nextTimeMoment, grid, problem,
				numPrm, refPressure, wells, profile);
			profile.n_newton_iters++;

			if (numPrm.IsSuccessfullAMG_Iteration() && numPrm.IsNewtonIterationContinue())
			{
				prof.tic("update");
				numPrm.update_isSuccesfullNewtonTrial(
					UpdateGrid(grid, problem, numPrm));
				prof.toc("update");
			}
			else
			{
				grid.ReverseState();
				break;
			}
		}
	}

	void NewtonSolver::SingleIteration(
		double loc_tau, double nextTimeMoment,
		OilField& grid, LinearProblem& problem,
		NumericalParameters& numPrm, double refPressure,
		const std::map<WellName, wells::SomeWell*>& wells,
		SolverProfile& profile)
	{
		prof.tic("assemble");
		assembler_.Assemble(loc_tau, nextTimeMoment, grid, problem, refPressure, wells);
		prof.toc("assemble");

		auto res = problem.Solve(numPrm.CurrentAMG_maxSolverIterationCount());
		numPrm.update_currentAMGState(
			{ static_cast<int>(res.iters), res.error, res.converged });

		profile.n_amg_solves++;
		profile.total_amg_iters += res.iters;
	}

	bool NewtonSolver::UpdateGrid(
		OilField& grid, LinearProblem& problem,
		const NumericalParameters& numPrm)
	{
		const double tol = 3E-3;
		std::vector<char> f(B * grid.ActiveCellsNmbr(), 1);

#ifdef	USE_PARALLEL
#pragma omp parallel for
#endif
		for (int l = 0; l < grid.ActiveCellsNmbr(); l++)
		{
			double corr[B];
			problem.UnpackCellCorrections(l, corr);
			grid[l].UpdateState(corr);

			const std::vector<double>& stateVaiables = grid[l].GetVariableFieldProperties();

			int i = 0; // saturation
			f[B * l + i] =
				(abs(stateVaiables[i]) < numPrm.NewtonTol() * tol) || (abs(1.0 - stateVaiables[i]) < numPrm.NewtonTol() * tol) ||
				(abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
			i = 1; // pressure
			f[B * l + i] =
				(abs(stateVaiables[i]) < 1E6) ||
				(abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
		}
		return std::all_of(f.begin(), f.end(), [](char x) { return x != 0; });
	}

} // namespace reservoir_simulator
```

3. Добавить в `CMakeLists.txt` после строки 43 (`${HYDRO}/Reservoir/JacobianAssembler.cpp`):

```cmake
    ${HYDRO}/Reservoir/NewtonSolver.cpp
```

4. **НЕ трогать** ReservoirSimulator.h и ReservoirSimulator.cpp

**Изменения (CMakeLists.txt):**

До:
```cmake
    # Reservoir
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp
```

После:
```cmake
    # Reservoir
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NewtonSolver.cpp
    ${HYDRO}/Reservoir/NumericalParameters.cpp
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — без ошибок и warnings
- Тесты: `ctest --test-dir build -C Release` — 300 тестов зелёные (новый класс не используется)

**Подводные камни:**
- **`n_newton_iters++` должен быть в `Solve`, НЕ в `SingleIteration`.** В оригинале счётчик стоит в `PerformNewtonLoop` (строка 447), после вызова `SingleIteration`. Если поместить его в `SingleIteration`, то при прямом вызове `sim.SingleIteration()` (test_inactive_cells.cpp, строки 25, 54) появится лишний инкремент, которого раньше не было. Это не сломает тесты (они не проверяют profile), но изменит семантику
- `SolverProfile` определён в `ReservoirSImulator.h:15–22`. В `NewtonSolver.h` — forward-declaration `struct SolverProfile;` (параметр по ссылке, полный тип не нужен). В `NewtonSolver.cpp` — `#include "ReservoirSImulator.h"` для доступа к полям `profile.n_amg_solves` и т.д. Циклических include нет: NewtonSolver.h НЕ включает ReservoirSImulator.h. На шаге 3, когда ReservoirSImulator.h включит NewtonSolver.h, порядок будет: forward-decl SolverProfile → определение SolverProfile — корректно в C++

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~100 строк (.h ~45 + .cpp ~80), ~30 мин

---

## Шаг 2: Unit-тесты NewtonSolver

**Цель:** убедиться, что `NewtonSolver::Solve()` даёт **побитово идентичные** P и Sw, что и текущий `ReservoirSimulator::PerformNewtonLoop()`

**Файлы:**
- СОЗДАТЬ: `tests/unit/math/test_NewtonSolver_standalone.cpp`
- ИЗМЕНИТЬ: `CMakeLists.txt` — добавить файл в target `gdm_unit_level4`

**Контекст:**

Паттерн повторяет `test_JacobianAssembler_standalone.cpp`. Ключевое отличие: сравниваем не матрицу, а результат Newton-итерации — поля P и Sw после одного Newton trial.

Алгоритм теста:
1. Создать два одинаковых `ReservoirSimulator` (sim_old, sim_new)
2. sim_old: вызвать `sim_old.PerformNewtonLoop(tau, tau)`
3. sim_new: вызвать `NewtonSolver::Solve(tau, tau, sim_new.Grid, sim_new.MyProblem, sim_new.numPrm, sim_new.RefPressure, sim_new.Wells, profile)`
4. Сравнить поля побитово: `sim_old.GetPressureField()` vs `sim_new.GetPressureField()`, аналогично для Sw
5. Сравнить состояние numPrm: `IsSuccessfullNewtonTrial()`, `CurrentNewtonIterationCount()`

**Тесты:**

```
Тест: NewtonSolver bitwise match 1D no wells
Тег: [NewtonSolver][bitwise]
Файл: tests/unit/math/test_NewtonSolver_standalone.cpp (новый)
Сценарий: 5×1×1, однородный пласт, Sw=0.2, без скважин
Setup: make_sim(5,1,1,...), tau=0.1
Ожидание: P и Sw побитово идентичны, numPrm.IsSuccessfullNewtonTrial() совпадает
Baseline: sim.PerformNewtonLoop
Предотвращает: ошибку при переносе PerformNewtonLoop/SingleIteration/UpdateGrid

Тест: NewtonSolver bitwise match 2D with wells
Тег: [NewtonSolver][bitwise]
Сценарий: 5×5×1, INJ + PROD
Setup: tau=0.1
Ожидание: побитово идентичны
Предотвращает: ошибку при передаче wells и solverProfile

Тест: NewtonSolver bitwise match Sw=0
Тег: [NewtonSolver][bitwise]
Сценарий: 3×3×1, Sw=0 — guards на harmonic mean
Setup: oil_saturation=1.0, tau=0.1
Ожидание: побитово идентичны
Предотвращает: потерю guards при переносе (BUG-007)
```

**Что сделать:**

1. Создать `tests/unit/math/test_NewtonSolver_standalone.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/NewtonSolver.h"
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

void compare_newton(ReservoirSimulator& sim_old,
                    ReservoirSimulator& sim_new,
                    double tau)
{
    // Старый путь
    sim_old.PerformNewtonLoop(tau, tau);
    auto P_old  = sim_old.GetPressureField();
    auto Sw_old = sim_old.GetWaterSaturationField();

    // Новый путь
    NewtonSolver solver;
    SolverProfile profile{};
    solver.Solve(tau, tau, sim_new.Grid, sim_new.MyProblem,
        sim_new.numPrm, sim_new.RefPressure, sim_new.Wells, profile);
    auto P_new  = sim_new.GetPressureField();
    auto Sw_new = sim_new.GetWaterSaturationField();

    // Сравнение P
    REQUIRE(P_old.size() == P_new.size());
    for (size_t i = 0; i < P_old.size(); ++i) {
        INFO("P[" << i << "]");
        REQUIRE(P_old[i] == P_new[i]);
    }

    // Сравнение Sw
    REQUIRE(Sw_old.size() == Sw_new.size());
    for (size_t i = 0; i < Sw_old.size(); ++i) {
        INFO("Sw[" << i << "]");
        REQUIRE(Sw_old[i] == Sw_new[i]);
    }

    // Состояние numPrm
    REQUIRE(sim_old.numPrm.IsSuccessfullNewtonTrial() ==
            sim_new.numPrm.IsSuccessfullNewtonTrial());
    REQUIRE(sim_old.numPrm.CurrentNewtonIterationCount() ==
            sim_new.numPrm.CurrentNewtonIterationCount());
}

} // anonymous namespace

TEST_CASE("NewtonSolver: bitwise match 1D 5x1x1 no wells",
    "[NewtonSolver][bitwise]")
{
    auto sim_old = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto sim_new = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_newton(sim_old, sim_new, 0.1);
}

TEST_CASE("NewtonSolver: bitwise match 2D 5x5x1 with wells",
    "[NewtonSolver][bitwise]")
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

    compare_newton(sim_old, sim_new, 0.1);
}

TEST_CASE("NewtonSolver: bitwise match Sw=0 (oil_saturation=1.0)",
    "[NewtonSolver][bitwise]")
{
    auto sim_old = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    auto sim_new = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    compare_newton(sim_old, sim_new, 0.1);
}
```

2. В `CMakeLists.txt` добавить файл в `gdm_unit_level4` (строка ~210):

До:
```cmake
add_executable(gdm_unit_level4
    tests/unit/math/test_MatrixAssembly.cpp
    tests/unit/math/test_LinearProblemAssembly.cpp
    tests/unit/math/test_JacobianAssembly.cpp
    tests/unit/math/test_JacobianAssembler_standalone.cpp
)
```

После:
```cmake
add_executable(gdm_unit_level4
    tests/unit/math/test_MatrixAssembly.cpp
    tests/unit/math/test_LinearProblemAssembly.cpp
    tests/unit/math/test_JacobianAssembly.cpp
    tests/unit/math/test_JacobianAssembler_standalone.cpp
    tests/unit/math/test_NewtonSolver_standalone.cpp
)
```

**Проверка после этого шага:**
- Сборка: без ошибок
- Тесты: 300 старых + 3 новых = 303 тестов зелёные

**Подводные камни:**
- Два `ReservoirSimulator` — нужно убедиться, что оба инициализируются идентично. `make_sim` детерминирован — нет random state
- `PerformNewtonLoop` вызывает `numPrm.set_currentAMG_maxSolverIterationCount()` — NewtonSolver::Solve тоже должен. Проверить: строка 443 в ReservoirSimulator.cpp
- Тест со скважинами: `add_simple_well` дублируется для обоих sim. Это корректно — каждый sim владеет своими wells
- `SolverProfile profile{}` — zero-initialized. В sim_old он `sim_old.solverProfile_`, уже обнулён конструктором

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 3

**Оценка:** ~100 строк, ~30 мин

---

## Шаг 3: Интеграция — ReservoirSimulator делегирует NewtonSolver

**Цель:** `ReservoirSimulator::PerformNewtonLoop` делегирует `newton_solver_.Solve()`, старый код удаляется. `assembler_` переезжает в NewtonSolver

**Файлы:**
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSImulator.h`
- ИЗМЕНИТЬ: `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**

После шага 2 мы знаем, что NewtonSolver даёт побитово идентичный результат. Теперь безопасно заменить старый код на делегирование. Ключевые ограничения:
1. `sim.SingleIteration()` вызывается в `test_inactive_cells.cpp` (строки 25, 54) — нужен делегирующий метод
2. `sim.UpdateGrid()` вызывается в `test_amgcl_benchmark.cpp` (строки 239, 381) — нужен делегирующий метод
3. `sim.AssembleMyProblem()` — уже делегирует `assembler_`, теперь через `newton_solver_.assembler_`... но проще оставить отдельный делегирующий метод

**Что сделать:**

1. В `ReservoirSImulator.h`:
   - Заменить `#include "JacobianAssembler.h"` на `#include "NewtonSolver.h"` (NewtonSolver.h уже включает JacobianAssembler.h)
   - Заменить поле `JacobianAssembler assembler_;` (строка 73) на `NewtonSolver newton_solver_;`
   - **Оставить** объявления `PerformNewtonLoop`, `SingleIteration`, `UpdateGrid`, `AssembleMyProblem` — все станут делегирующими

2. В `ReservoirSimulator.cpp`:
   - **Заменить** тело `PerformNewtonLoop` (строки 439–461) на:
     ```cpp
     void ReservoirSimulator::PerformNewtonLoop(double loc_tau, double nextTimeMoment)
     {
         newton_solver_.Solve(loc_tau, nextTimeMoment, Grid, MyProblem,
             numPrm, RefPressure, Wells, solverProfile_);
     }
     ```
   - **Заменить** тело `UpdateGrid` (строки 462–488) на:
     ```cpp
     bool ReservoirSimulator::UpdateGrid()
     {
         return newton_solver_.UpdateGrid(Grid, MyProblem, numPrm);
     }
     ```
   - **Заменить** тело `SingleIteration` (строки 490–501) на:
     ```cpp
     void ReservoirSimulator::SingleIteration(double loc_tau, double nextTimeMoment)
     {
         newton_solver_.SingleIteration(loc_tau, nextTimeMoment, Grid, MyProblem,
             numPrm, RefPressure, Wells, solverProfile_);
     }
     ```
   - **Заменить** тело `AssembleMyProblem` (строки 503–506) на:
     ```cpp
     void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
     {
         newton_solver_.assembler_.Assemble(loc_tau, nextTimeMoment,
             Grid, MyProblem, RefPressure, Wells);
     }
     ```

**Изменения (ReservoirSImulator.h):**

До (строки 11, 73):
```cpp
#include "JacobianAssembler.h"
...
    JacobianAssembler assembler_;
```

После:
```cpp
#include "NewtonSolver.h"
...
    NewtonSolver newton_solver_;
```

**Изменения (ReservoirSimulator.cpp) — 4 метода заменяются делегирующими:**

До (~62 строки кода):
```cpp
void ReservoirSimulator::PerformNewtonLoop(double loc_tau, double nextTimeMoment)
{
    // 22 строки
}
bool ReservoirSimulator::UpdateGrid()
{
    // 26 строк
}
void ReservoirSimulator::SingleIteration(double loc_tau, double nextTimeMoment)
{
    // 11 строк
}
void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
{
    assembler_.Assemble(loc_tau, nextTimeMoment, Grid, MyProblem, RefPressure, Wells);
}
```

После (~16 строк делегирования):
```cpp
void ReservoirSimulator::PerformNewtonLoop(double loc_tau, double nextTimeMoment)
{
    newton_solver_.Solve(loc_tau, nextTimeMoment, Grid, MyProblem,
        numPrm, RefPressure, Wells, solverProfile_);
}
bool ReservoirSimulator::UpdateGrid()
{
    return newton_solver_.UpdateGrid(Grid, MyProblem, numPrm);
}
void ReservoirSimulator::SingleIteration(double loc_tau, double nextTimeMoment)
{
    newton_solver_.SingleIteration(loc_tau, nextTimeMoment, Grid, MyProblem,
        numPrm, RefPressure, Wells, solverProfile_);
}
void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
{
    newton_solver_.assembler_.Assemble(loc_tau, nextTimeMoment,
        Grid, MyProblem, RefPressure, Wells);
}
```

**Проверка после этого шага:**
- Сборка: без ошибок и warnings
- Тесты: `ctest --test-dir build -C Release` — все 303 тестов зелёные
- Убедиться что `test_inactive_cells` и `test_amgcl_benchmark` проходят (они используют делегирующие методы)

**Подводные камни:**
- `AssembleMyProblem` теперь обращается к `newton_solver_.assembler_` — публичный доступ. Альтернатива: метод-обёртка `NewtonSolver::Assemble(...)` → но это лишний уровень индирекции ради одного call site
- `test_amgcl_benchmark` вызывает `sim.UpdateGrid()` (строки 239, 381) — делегирующий метод сохраняет сигнатуру `bool UpdateGrid()`, benchmark компилируется без изменений
- `test_amgcl_benchmark` также вызывает `sim.AssembleMyProblem()` (строки 221, 363) — делегирующий метод сохранён

**Зависимости:**
- Требует: шаг 2
- Блокирует: шаг 4

**Оценка:** ~16 строк нового кода, -46 строк (net diff: -30), ~20 мин

---

## Шаг 4: Vault

**Цель:** обновить статусы и ссылки

**Файлы:**
- ИЗМЕНИТЬ: `vault/GDM/roadmap/технический долг.md` — статус DEBT-055
- ИЗМЕНИТЬ: `vault/GDM/00-home/текущие приоритеты.md`
- ИЗМЕНИТЬ: `vault/GDM/00-home/index.md` — ссылка на план

**Что сделать:**

1. В `технический долг.md`: изменить статус DEBT-055 с `🔴 ОТКРЫТ` на `✅ реализовано <дата>`
2. В `текущие приоритеты.md`:
   - Обновить строку `DEBT-054`: добавить `DEBT-055: ✅ NewtonSolver выделен (<дата>, ветка refactor/debt-055/newton-solver, +3 теста)`
   - Обновить `Следующий: DEBT-056 (MassBalanceTracker)` или `DEBT-057 (TimeIntegrator)`
   - Обновить snapshot: тесты 303
3. В `index.md`: добавить ссылку на [[debt-055 newton-solver]] в секцию Plans

**Проверка после этого шага:**
- Финальная сборка + тесты: все 303 зелёные
- `gh issue comment 24 --repo ArturSalamatin/GDM --body "Реализовано. 3 побитовых теста, все 303 тестов зелёные."`

**Зависимости:**
- Требует: шаг 3

**Оценка:** ~10 строк, ~10 мин

---

## Критерии завершения

- [ ] Все 4 шага выполнены
- [ ] Все 300 существующих тестов зелёные
- [ ] 3 новых побитовых теста зелёные (итого 303)
- [ ] `assembler_` удалён из ReservoirSimulator, переехал в NewtonSolver
- [ ] `PerformNewtonLoop`, `SingleIteration`, `UpdateGrid`, `AssembleMyProblem` — делегирующие (backward compat)
- [ ] `test_amgcl_benchmark` и `test_inactive_cells` работают без изменений
- [ ] CMakeLists.txt: NewtonSolver.cpp в gdm_core, тест в gdm_unit_level4
- [ ] Vault обновлён: статус DEBT-055, приоритеты, index
- [ ] GitHub issue #24 прокомментирован

## Рекомендуемые коммиты

1. `refactor: DEBT-055 создать NewtonSolver` (шаг 1)
2. `test: DEBT-055 побитовые тесты NewtonSolver` (шаг 2)
3. `refactor: DEBT-055 интеграция NewtonSolver в ReservoirSimulator` (шаг 3)
4. `vault: результаты DEBT-055 NewtonSolver` (шаг 4)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки (ссылки ниже)
3. Создай ветку: `git checkout -b refactor/debt-055/newton-solver`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Baseline: 300 тестов, ~135 сек
7. Начни с шага 1. После каждого шага: сборка + тесты

## Связанные заметки

- [[debt-003 refactoring-reservoir-simulator]] — общий план декомпозиции (этапы 1–4)
- [[технический долг]] — записи DEBT-003, DEBT-054, DEBT-055
- [[debt-054 jacobian-assembler]] — паттерн для этой задачи (DEBT-054 полностью аналогичен)
- [[стратегия тестирования GDM]] — паттерны тестов
- DEBT-057 (TimeIntegrator) — блокируется этой задачей

## Оценка общая

| Шаг | Файлов | Строк | Время |
|---|---|---|---|
| 1: Создать NewtonSolver | 2 создать, 1 изменить | ~100 | ~30 мин |
| 2: Unit-тесты | 1 создать, 1 изменить | ~100 | ~30 мин |
| 3: Интеграция | 2 изменить | -30 (чистый diff) | ~20 мин |
| 4: Vault | 3 изменить | ~10 | ~10 мин |
| **Итого** | **3 создать, 5 изменить** | **~180** | **~90 мин** |
