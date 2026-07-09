---
tags:
  - план
  - инфраструктура
date: 2026-07-09
issue: DEBT-053
github: 20
branch: refactor/debt-053/solver-factory
status: готов к реализации
audit:
  date: 2026-07-09
  round: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-053: SolverFactory — compile-time выбор конфигурации СЛАУ

## Мотивация

Солверная конфигурация AMGCL захардкожена в `LinearProblem.h` (строки 36–44): `using CPRPrecond = ...; using CPRSolver = ...`. Бенчмарк (`test_amgcl_benchmark.cpp`, строки 52–74) дублирует те же using-алиасы для 4 альтернативных конфигураций. Переключение конфигурации в production-коде требует ручного редактирования заголовка — это блокирует систематическое тестирование разных солверов на одной задаче и мешает FEAT-010/FEAT-011.

## Подтип: Инфраструктура

CMake-опция + `#ifdef`-блоки, без runtime-полиморфизма.

## Связанные заметки

- [[возможности amgcl для блочных СЛАУ]] — каталог компонентов amgcl
- [[debt-051 amgcl-submodule-and-solvers]] — общий план DEBT-051..053
- [[переход с блочного AMG на скалярный CPR в production]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[локальные патчи AMGCL для GDM]]

## Baseline

- Тесты: 294/294, Release ~122 сек
- Конфигурация: CPR<AMG<aggregation, ilu0>, ilu0> + lgmres
- Submodule amgcl: ветка `experimental/patches` (38fe764)

## Текущее состояние

### LinearProblem.h (строки 36–44)

```cpp
using ScalarBackend = amgcl::backend::builtin<double>;

using CPRPrecond = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;

using CPRSolver = amgcl::make_solver<CPRPrecond, amgcl::solver::lgmres<ScalarBackend>>;
```

`CPRSolver::params prm` — член класса (строка 57).
`CPRSolver solve(...)` — инстанциация в `LinearProblem::Solve()` (строка 129).

### LinearProblem::Solve() (строки 107–144)

Содержит **diagonal regularization** (строки 111–126): при Sw≈0 ставит `1e-6` на диагональ нечётных строк (saturation rows в InterleavedPSw layout). Этот workaround привязан к `B=2` и текущему layout.

### Кто include-ит LinearProblem.h

1. `HydroSolver/Reservoir/ReservoirSImulator.h:7` — через `MyProblem` (член класса типа `LinearProblem`)
2. `HydroSolver/Solver/Math/LinearProblem.cpp:2` — реализация
3. `tests/unit/math/assembly_test_helpers.h:9` — тесты сборки матрицы
4. `tests/unit/math/test_LinearProblemAssembly.cpp:4` — тесты

### Бенчмарк (test_amgcl_benchmark.cpp, строки 52–74)

Дублирует using-алиасы:
- `CPR_AMG_ilu0` + lgmres (≡ production)
- `CPRDRS_AMG_ilu0` + lgmres (CPR-DRS)
- `ILU0_scalar` + lgmres
- `CPR_AMG_iluk` + lgmres (CPR с ILU(k=1) в AMG)

Использует `solve_with_scalar<SolverType>()` (строки 26–50) — template-функцию, которая обходит `LinearProblem::Solve()`. **Не** делает diagonal regularization.

## Целевое состояние

### 5 конфигураций

| CMake-значение | Preconditioner | Krylov solver | Заголовки |
|---|---|---|---|
| `CPR` (default) | CPR<AMG<aggregation, ilu0>, ilu0> | lgmres | cpr.hpp, aggregation.hpp, ilu0.hpp, lgmres.hpp |
| `CPR_BICGSTAB` | CPR<AMG<aggregation, ilu0>, ilu0> | bicgstab | + bicgstab.hpp |
| `CPR_SA` | CPR<AMG<smoothed_aggregation, ilu0>, ilu0> | lgmres | + smoothed_aggregation.hpp |
| `CPR_DRS` | CPR_DRS<AMG<aggregation, ilu0>, ilu0> | lgmres | + cpr_drs.hpp |
| `ILU0` | as_preconditioner<ilu0> | lgmres | без cpr.hpp |

### Новые файлы

- `HydroSolver/Solver/Math/SolverConfig.h` — единственное место определения `using SolverType` / `using PrecondType`

### Изменяемые файлы

- `CMakeLists.txt` — опция `GDM_SOLVER` + `target_compile_definitions`
- `HydroSolver/Solver/Math/LinearProblem.h` — заменить hardcoded using на `#include "SolverConfig.h"`, переименовать `CPRSolver` → `SolverType`, `CPRPrecond` → `PrecondType`
- `HydroSolver/Solver/Math/LinearProblem.cpp` — использовать `SolverType`
- `tests/test_amgcl_benchmark.cpp` — удалить дублирование, include-ить `SolverConfig.h`
- `examples/ex_benchmark_series_cpr.cpp` — удалить дублирование include-ов amgcl, include-ить `SolverConfig.h`

## Варианты решения

### Вариант A: `#ifdef` прямо в LinearProblem.h

- Весь `#ifdef`-блок с 5 конфигурациями в LinearProblem.h
- **Плюс:** один файл
- **Минус:** LinearProblem.h раздувается на ~50 строк `#ifdef`, каждый include-ер (ReservoirSimulator.h, тесты) тянет все 5 наборов include-ов через препроцессор

### Вариант B: Отдельный SolverConfig.h (рекомендуемый)

- Новый `SolverConfig.h` с `#ifdef`-логикой и всеми include-ами amgcl
- `LinearProblem.h` включает `SolverConfig.h` и использует `SolverType` / `PrecondType`
- **Плюс:** чистое разделение — LinearProblem.h не знает про конфигурации; бенчмарк тоже может include-ить `SolverConfig.h`
- **Минус:** один дополнительный файл

### Выбор: Вариант B

Причина: SolverConfig.h — естественная точка расширения для FEAT-010 (threshold fallback) и FEAT-011 (True-IMPES weights). Одна точка изменения, один файл для добавления новой конфигурации.

## Поиск подводных камней

- ✅ **Все call sites найдены:** `CPRSolver` / `CPRPrecond` используются в 2 местах: `LinearProblem.h:39,44` (определение) и `LinearProblem.cpp:129` (инстанциация). `CPRSolver::params prm` — `LinearProblem.h:57`. Больше нигде.
- ✅ **Потокобезопасность:** не затронута — солвер создаётся и используется в одном потоке (внутри Newton loop)
- ⚠️ **Зависимости сборки:** при смене конфигурации CMake нужен полный reconfigure (`cmake -B build ...`). Простой rebuild не подхватит изменение `GDM_SOLVER`. Адресовано: документируем в «Как начать работу»
- ✅ **Обратная совместимость API:** `LinearProblem::Solve()` сигнатура не меняется. Бенчмарк использует свою `solve_with_scalar<T>()` — не затронута
- ✅ **Тесты:** все 294 теста используют production `Solve()` через `ReservoirSimulator`. При default `GDM_SOLVER=CPR` поведение идентично текущему
- ✅ **Кодировки:** не затронуты
- ✅ **Платформозависимость:** `__declspec(selectany)` для `amgcl::prof` уже есть, не меняем
- ✅ **Мёртвый код:** не удаляем ничего, только рефакторим
- ✅ **Связь с другими задачами:** FEAT-010/011 — не конфликтует, наоборот, SolverConfig.h станет точкой интеграции
- ✅ **Производительность:** compile-time switch, нулевой runtime overhead
- ⚠️ **ILU0 конфигурация и diagonal regularization:** production `Solve()` регуляризует чётные/нечётные строки для InterleavedPSw. При ILU0 (без CPR) это всё ещё необходимо для Sw≈0. Адресовано: regularization остаётся в `Solve()`, работает для всех конфигураций
- ⚠️ **CPR-DRS params:** `cpr_drs::params` содержит `block_size`, `eps_dd`, `eps_ps` — совместим с `cpr::params`. ✅
- 🔴 **ILU0 params несовместимость:** `prm.precond.block_size = B` (строка 91 LinearProblem.cpp) — `as_preconditioner<ilu0>::params` НЕ имеет поля `block_size`. Не скомпилируется для ILU0. Адресовано: в шаге 4 добавляем `#ifdef`-guard или constexpr-проверку
- 🔴 **BiCGStab не имеет K:** `prm.solver.K = 5` (строка 95 LinearProblem.cpp) — параметр `K` специфичен для LGMRES (augmentation vectors). BiCGStab не имеет этого поля. Не скомпилируется для CPR_BICGSTAB. Адресовано: в шаге 4 добавляем `#ifdef`-guard

## Обнаруженные проблемы

1. **Бенчмарк `run_benchmark_scalar()` не делает diagonal regularization.** При Sw_init≈0 бенчмарк может упасть на CPR/ILU0 даже с патчами, потому что near-zero диагонали не регуляризованы. Это не блокирует DEBT-053 (regularization — отдельная задача, связана с FEAT-010), но нужно документировать.

---

## Шаги реализации

### Шаг 1: Подготовка

**Цель:** создать ветку, зафиксировать baseline

**Файлы:** нет изменений

**Что сделать:**
1. `git checkout -b refactor/debt-053/solver-factory experimental`
2. `cmake -B build -S . -G "Visual Studio 17 2022"`
3. `cmake --build build --config Release`
4. `ctest --test-dir build -C Release --output-on-failure` → зафиксировать: 294 тестов, ~122 сек

**Проверка после этого шага:**
- 294/294 тестов зелёные

**Зависимости:**
- Требует: ничего
- Блокирует: шаги 2–5

**Оценка:** ~0 строк кода, ~5 мин

---

### Шаг 2: Создать SolverConfig.h

**Цель:** вынести определение солверных типов в отдельный заголовок с `#ifdef`-переключением

**Файлы:** `HydroSolver/Solver/Math/SolverConfig.h` (новый)

**Контекст:**
Сейчас `LinearProblem.h` содержит include-ы amgcl (строки 8–16) и using-алиасы (строки 36–44). Эти определения нужно вынести в `SolverConfig.h`, параметризовав через CMake-define `GDM_SOLVER_<VALUE>`.

Имена типов в целевом заголовке: `PrecondType`, `SolverType`, `ScalarBackend`.

**Что сделать:**
1. Создать файл `HydroSolver/Solver/Math/SolverConfig.h`
2. Перенести в него include-ы amgcl и using-алиасы
3. Добавить `#ifdef`-блоки для 5 конфигураций
4. По умолчанию (если ни один `GDM_SOLVER_*` не определён) — CPR (текущее поведение)

**Код (полный файл):**

```cpp
#pragma once

#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>
#include <amgcl/solver/lgmres.hpp>

#if defined(GDM_SOLVER_CPR_DRS)
#include <amgcl/preconditioner/cpr_drs.hpp>
#elif defined(GDM_SOLVER_CPR_SA)
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#elif defined(GDM_SOLVER_CPR_BICGSTAB)
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/solver/bicgstab.hpp>
#elif defined(GDM_SOLVER_ILU0)
// no CPR header needed
#else
// Default: GDM_SOLVER_CPR or undefined
#include <amgcl/preconditioner/cpr.hpp>
#endif

namespace reservoir_simulator {
namespace linear_problem {

using ScalarBackend = amgcl::backend::builtin<double>;

#if defined(GDM_SOLVER_CPR_DRS)

using PrecondType = amgcl::preconditioner::cpr_drs<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#elif defined(GDM_SOLVER_CPR_SA)

using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::smoothed_aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#elif defined(GDM_SOLVER_CPR_BICGSTAB)

using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::bicgstab<ScalarBackend>>;

#elif defined(GDM_SOLVER_ILU0)

using PrecondType = amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#else
// Default: CPR<AMG<aggregation, ilu0>, ilu0> + lgmres

using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;
using SolverType = amgcl::make_solver<PrecondType, amgcl::solver::lgmres<ScalarBackend>>;

#endif

} // namespace linear_problem
} // namespace reservoir_simulator
```

**Подводные камни:**
- `cpr_drs::params` vs `cpr::params` — оба имеют `.solver`, `.precond` подструктуры; `SolverType::params` — это `make_solver::params`, у которого `.solver` (Krylov) и `.precond` (preconditioner). Совместимость обеспечена: каждый `make_solver<P, S>::params` имеет `solver` и `precond` members, привязанные к конкретным P и S. `LinearProblem` устанавливает только `prm.solver.maxiter` (строка 109), `prm.solver.tol` и `prm.solver.abstol` (в конструкторе) — эти поля есть у всех Krylov-солверов
- `ILU0` без CPR: `prm.precond` будет типа `as_preconditioner<ilu0>::params`, а не `cpr::params`. `LinearProblem` обращается к `prm.precond.block_size` в конструкторе (строка 91) — это НЕ скомпилируется для ILU0. Решение: `#ifdef`-guard в шаге 4d

**Проверка после этого шага:**
- Файл создан, но ещё не include-ится → сборка не затронута
- Сборка: `cmake --build build --config Release`
- Тесты: 294/294

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 4

**Оценка:** ~75 строк, ~10 мин

---

### Шаг 3: CMake — добавить опцию GDM_SOLVER

**Цель:** добавить CMake-опцию и передать define в компиляцию

**Файлы:** `CMakeLists.txt`

**Контекст:**
Нужна строковая опция `GDM_SOLVER` с допустимыми значениями. CMake передаёт `GDM_SOLVER_<VALUE>` как compile definition в target `gdm_core`. Все targets, линкующиеся с `gdm_core` через `target_link_libraries`, наследуют PUBLIC definitions.

**Что сделать:**
1. После строки 78 (`target_include_directories(gdm_core PUBLIC ...)`) добавить блок с опцией
2. Использовать `set(GDM_SOLVER "CPR" CACHE STRING ...)` + `set_property(CACHE GDM_SOLVER PROPERTY STRINGS ...)`
3. `target_compile_definitions(gdm_core PUBLIC GDM_SOLVER_${GDM_SOLVER})`

**Изменения:**

До (строки 78–83):
```cmake
target_include_directories(gdm_core PUBLIC ${HYDRO} ${CMAKE_SOURCE_DIR}/amgcl)
option(AMGCL_PROFILING "Enable AMGCL built-in profiling (prof.tic/toc)" ON)
target_compile_definitions(gdm_core PUBLIC AMGCL_NO_BOOST)
```

После:
```cmake
target_include_directories(gdm_core PUBLIC ${HYDRO} ${CMAKE_SOURCE_DIR}/amgcl)
set(GDM_SOLVER "CPR" CACHE STRING "AMGCL solver configuration (CPR, CPR_BICGSTAB, CPR_SA, CPR_DRS, ILU0)")
set_property(CACHE GDM_SOLVER PROPERTY STRINGS CPR CPR_BICGSTAB CPR_SA CPR_DRS ILU0)
target_compile_definitions(gdm_core PUBLIC GDM_SOLVER_${GDM_SOLVER})
option(AMGCL_PROFILING "Enable AMGCL built-in profiling (prof.tic/toc)" ON)
target_compile_definitions(gdm_core PUBLIC AMGCL_NO_BOOST)
```

**Проверка после этого шага:**
- `cmake -B build -S . -G "Visual Studio 17 2022"` — в output видно `GDM_SOLVER=CPR`
- `cmake --build build --config Release`
- Тесты: 294/294 (define `GDM_SOLVER_CPR` добавлен, но SolverConfig.h ещё не используется — no-op)

**Подводные камни:**
- `target_compile_definitions(gdm_core PUBLIC ...)` — PUBLIC означает, что define наследуется тестами и gdm.exe. Это правильно: тесты компилируют код, который include-ит LinearProblem.h
- При невалидном значении `GDM_SOLVER=FOOBAR` → define `GDM_SOLVER_FOOBAR` → SolverConfig.h попадёт в default ветку (`#else`) → CPR. Безопасно

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 4

**Оценка:** ~4 строки, ~5 мин

---

### Шаг 4: Переключить LinearProblem.h на SolverConfig.h

**Цель:** заменить hardcoded include-ы и using-алиасы amgcl на `#include "SolverConfig.h"`, переименовать типы

**Файлы:**
- `HydroSolver/Solver/Math/LinearProblem.h`
- `HydroSolver/Solver/Math/LinearProblem.cpp`

**Контекст:**
`LinearProblem.h` содержит 9 строк include-ов amgcl (8–16), profiler-related (22–23) и 3 строки using (36–44). Заменяем include-ы amgcl на один `#include "SolverConfig.h"`, переименовываем `CPRPrecond` → `PrecondType`, `CPRSolver` → `SolverType`.

Profiler (`amgcl/profiler.hpp` и `__declspec(selectany) amgcl::prof`) остаётся в `LinearProblem.h` — он не зависит от конфигурации солвера и используется в `LinearProblem::Solve()`.

**Что сделать:**

#### 4a. LinearProblem.h

Заменить строки 8–16 и 36–44.

До (строки 5–44):
```cpp
#undef min
#undef max

#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/solver/lgmres.hpp>
#include <amgcl/profiler.hpp>


#undef min
#undef max

namespace amgcl { __declspec(selectany) profiler<> prof; } // modifier is to avoid multiple redefinitions of the same variable
using amgcl::prof;

namespace reservoir_simulator
{
	namespace linear_problem
	{
		struct SolveResult
		{
			size_t iters;
			double error;
			bool converged;
		};

		using ScalarBackend = amgcl::backend::builtin<double>;

		using CPRPrecond = amgcl::preconditioner::cpr<
			amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
			amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
		>;

		using CPRSolver = amgcl::make_solver<CPRPrecond, amgcl::solver::lgmres<ScalarBackend>>;
```

После:
```cpp
#undef min
#undef max

#include "SolverConfig.h"
#include <amgcl/profiler.hpp>

#undef min
#undef max

namespace amgcl { __declspec(selectany) profiler<> prof; } // modifier is to avoid multiple redefinitions of the same variable
using amgcl::prof;

namespace reservoir_simulator
{
	namespace linear_problem
	{
		struct SolveResult
		{
			size_t iters;
			double error;
			bool converged;
		};
```

Обрати внимание: `ScalarBackend`, `PrecondType`, `SolverType` теперь определены в `SolverConfig.h` внутри `namespace reservoir_simulator::linear_problem`. Поэтому дублировать их здесь не нужно.

#### 4b. Переименовать в LinearProblem.h

Строка 57 (было): `CPRSolver::params prm;`
Стало: `SolverType::params prm;`

#### 4c. LinearProblem.cpp — Solve() (строка 129)

Строка 129 (было): `CPRSolver solve(`
Стало: `SolverType solve(`

#### 4d. LinearProblem.cpp — конструктор (строки 91, 95)

Строки 91 и 95 содержат параметры, специфичные для конкретных конфигураций:

```cpp
prm.precond.block_size = B;  // строка 91 — есть только у CPR и CPR-DRS
prm.solver.tol = AMG_RelTol;  // строка 92 — есть у всех Krylov
prm.solver.abstol = amg_AbsTol;  // строка 93 — есть у всех Krylov
prm.solver.maxiter = 5;  // строка 94 — есть у всех Krylov
prm.solver.K = 5;  // строка 95 — есть ТОЛЬКО у LGMRES
```

**`block_size`:** есть у `cpr::params` и `cpr_drs::params`, но НЕТ у `as_preconditioner<ilu0>::params` (конфигурация ILU0).

**`K`:** есть у `lgmres::params`, но НЕТ у `bicgstab::params` (конфигурация CPR_BICGSTAB).

Решение: `#ifdef`-guards вокруг несовместимых строк.

До (строки 90–96):
```cpp
		{
			prm.precond.block_size = B;
			prm.solver.tol = AMG_RelTol;
			prm.solver.abstol = amg_AbsTol;
			prm.solver.maxiter = 5;
			prm.solver.K = 5;
		}
```

После:
```cpp
		{
#if !defined(GDM_SOLVER_ILU0)
			prm.precond.block_size = B;
#endif
			prm.solver.tol = AMG_RelTol;
			prm.solver.abstol = amg_AbsTol;
			prm.solver.maxiter = 5;
#if !defined(GDM_SOLVER_CPR_BICGSTAB)
			prm.solver.K = 5;
#endif
		}
```

Почему `#ifdef`, а не `if constexpr`: `prm.precond.block_size` и `prm.solver.K` — это обращения к несуществующим полям структуры, которые не скомпилируются даже внутри `if constexpr(false)` в MSVC (MSVC проверяет синтаксис обоих веток в некоторых контекстах). `#ifdef` — единственный надёжный способ

**Проверка после этого шага:**
- Реконфигурация: `cmake -B build -S . -G "Visual Studio 17 2022"`
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 294/294 — поведение идентично baseline (default GDM_SOLVER=CPR → SolverConfig.h определяет те же типы)

**Подводные камни:**
- `#include "SolverConfig.h"` внутри `LinearProblem.h` — относительный путь работает, потому что оба файла в одной директории (`HydroSolver/Solver/Math/`)
- `SolverConfig.h` уже открывает `namespace reservoir_simulator::linear_problem` и закрывает его. `LinearProblem.h` тоже открывает этот namespace. Двойное открытие namespace — допустимо в C++, не ошибка
- Тесты, которые include-ят `LinearProblem.h` (`assembly_test_helpers.h`, `test_LinearProblemAssembly.cpp`), получат `SolverConfig.h` транзитивно. Для них ничего не меняется
- **КРИТИЧНО:** `prm.precond.block_size = B` (строка 91) не скомпилируется для ILU0 (нет поля `block_size`). `prm.solver.K = 5` (строка 95) не скомпилируется для CPR_BICGSTAB (BiCGStab не имеет K). Решение: `#ifdef`-guards (см. шаг 4d)

**Зависимости:**
- Требует: шаги 2, 3
- Блокирует: шаг 5

**Оценка:** ~20 строк изменений, ~10 мин

---

### Шаг 5: Рефакторинг бенчмарка

**Цель:** удалить дублирование using-алиасов, использовать `SolverConfig.h`

**Файлы:**
- `tests/test_amgcl_benchmark.cpp`
- `examples/ex_benchmark_series_cpr.cpp`

**Контекст:**
Бенчмарк (`test_amgcl_benchmark.cpp`, строки 9–18) и example (`ex_benchmark_series_cpr.cpp`, строки 10–19) дублируют include-ы amgcl, а строки 52–74 дублируют using-алиасы для 4 конфигураций. Часть этих using-ов (CPR_AMG_ilu0) идентична production-типу. После шага 4 оба файла получают `PrecondType`/`SolverType` через `LinearProblem.h` → `SolverConfig.h`.

Оба файла должны продолжать тестировать **все** конфигурации, а не только текущую. Поэтому полностью удалять дублирование нельзя — нужны все 5 типов одновременно. Но можно:
1. Удалить дублирующие include-ы amgcl (уже приходят через `LinearProblem.h` → `SolverConfig.h`)
2. Добавить include-ы, которые нужны для всех конфигураций, но не текущей (cpr_drs.hpp, bicgstab.hpp, smoothed_aggregation.hpp, iluk.hpp)
3. Сохранить using-алиасы (они определяют конкретные типы для `solve_with_scalar<T>()`)

**Что сделать:**

Заменить строки 9–18 (include-ы amgcl):

До:
```cpp
#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/solver/lgmres.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/iluk.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
```

После:
```cpp
#include "Solver/Math/SolverConfig.h"
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/relaxation/iluk.hpp>
```

Это гарантирует, что бенчмарк имеет все заголовки для всех 5 конфигураций, независимо от текущего `GDM_SOLVER`. Общие include-ы (adapter, make_solver, amg, lgmres, aggregation, ilu0, as_preconditioner) приходят через `SolverConfig.h`.

#### 5b. examples/ex_benchmark_series_cpr.cpp

Аналогичная замена include-ов (строки 10–19):

До:
```cpp
#include <amgcl/adapter/crs_tuple.hpp>
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/solver/lgmres.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/iluk.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
```

После:
```cpp
#include "Solver/Math/SolverConfig.h"
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/relaxation/iluk.hpp>
```

Include path `"Solver/Math/SolverConfig.h"` работает для examples, потому что `gdm.exe` линкуется с `gdm_core` и наследует `target_include_directories(gdm_core PUBLIC ${HYDRO})`.

**Подводные камни:**
- Include path `"Solver/Math/SolverConfig.h"` — работает, потому что `target_include_directories(gdm_core PUBLIC ${HYDRO})`, а тесты и examples линкуются с `gdm_core`. `${HYDRO}` = `HydroSolver`, поэтому `Solver/Math/SolverConfig.h` резолвится правильно
- Бенчмарк и example включают `cpr.hpp` явно, даже если текущая конфигурация — ILU0 (где `SolverConfig.h` не включает `cpr.hpp`). Это нужно для using-алиасов `CPR_AMG_ilu0` и т.д.
- При конфигурации CPR_DRS `SolverConfig.h` уже включает `cpr_drs.hpp`, повторное включение — безопасно благодаря `#pragma once`

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 294/294

**Зависимости:**
- Требует: шаг 4
- Блокирует: шаг 6

**Оценка:** ~20 строк изменений (оба файла), ~10 мин

---

### Шаг 6: Верификация всех конфигураций

**Цель:** проверить, что проект собирается и тесты проходят с каждой из 5 конфигураций

**Файлы:** нет изменений в коде

**Что сделать:**

Для каждой конфигурации:

```powershell
cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=<VALUE>
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Значения: `CPR`, `CPR_BICGSTAB`, `CPR_SA`, `CPR_DRS`, `ILU0`

**Ожидания:**
- `CPR` (default): 294/294 — идентично baseline
- `CPR_BICGSTAB`: 294/294 или близко — BiCGStab может дать чуть другую сходимость
- `CPR_SA`: 294/294 — smoothed aggregation обычно лучше или сопоставимо с aggregation
- `CPR_DRS`: 294/294 или близко — Dynamic Row Summing может изменить число итераций
- `ILU0`: возможны расхождения на тонких сетках (ILU0 без AMG — слабый прекондиционер для больших систем)

Документируй результаты: количество тестов, время, упавшие тесты (если есть).

**Подводные камни:**
- При переключении `GDM_SOLVER` нужен **полный reconfigure** (`cmake -B build ...`), иначе define не обновится. Если build-директория переиспользуется, CMake cache хранит старое значение — нужно явно передать `-DGDM_SOLVER=<VALUE>` или удалить cache
- Некоторые конфигурации могут не сходиться на текущих tolerance (AMG_RelTol, AMG_AbsTol). Это не баг DEBT-053 — это ожидаемое поведение. Документируй

**Проверка после этого шага:**
- Таблица результатов по 5 конфигурациям
- `CPR` должен быть идентичен baseline

**Зависимости:**
- Требует: шаги 2–5
- Блокирует: шаг 7

**Оценка:** ~0 строк кода, ~30 мин (5 конфигураций × 5 мин сборка + тесты)

---

### Шаг 7: Обновить vault и закрыть

**Цель:** обновить статус задачи, добавить коммент в GitHub issue

**Файлы:**
- `vault/GDM/roadmap/технический долг.md` — статус DEBT-053
- `vault/GDM/00-home/текущие приоритеты.md` — отметить выполнение
- `vault/GDM/00-home/index.md` — ссылка на план

**Что сделать:**
1. В `технический долг.md`: DEBT-053 статус → `✅ реализовано <дата>`
2. В `текущие приоритеты.md`: отметить DEBT-053 ✅
3. `gh issue comment 20 --repo ArturSalamatin/GDM --body "Реализовано. Таблица результатов по конфигурациям: ..."`

**Проверка после этого шага:**
- Vault консистентен

**Зависимости:**
- Требует: шаг 6
- Блокирует: ничего

**Оценка:** ~10 строк vault, ~5 мин

---

## Тестовая стратегия

Для DEBT-053 ключевое — **неизменность поведения при default конфигурации**.

**Тест 1: Regression (существующий)**
- 294 существующих теста
- При `GDM_SOLVER=CPR` (default) — результаты идентичны baseline
- Предотвращает: regression при рефакторинге

**Тест 2: Multi-config build (ручной)**
- Сборка с каждой из 5 конфигураций
- Проверка: компилируется без ошибок и warnings
- Предотвращает: ошибки `#ifdef`, отсутствующие include-ы

**Тест 3: Multi-config run (ручной)**
- Запуск тестов с каждой конфигурацией
- Документация: какие тесты проходят/падают для каждой конфигурации
- Предотвращает: неожиданные различия в сходимости

Новых Catch2-тестов не требуется — задача чисто инфраструктурная.

## Критерии завершения

- [ ] Все шаги 1–7 выполнены
- [ ] 294/294 тестов зелёные с default config (CPR)
- [ ] Проект собирается с каждой из 5 конфигураций без ошибок
- [ ] Таблица результатов по конфигурациям задокументирована
- [ ] CMakeLists.txt содержит опцию GDM_SOLVER
- [ ] SolverConfig.h создан и используется
- [ ] Бенчмарк рефакторен (не дублирует include-ы)
- [ ] Vault обновлён
- [ ] GitHub issue #20 прокомментирован

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[возможности amgcl для блочных СЛАУ]] — каталог компонентов
3. Создай ветку: `git checkout -b refactor/debt-053/solver-factory experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Baseline: 294 тестов, Release ~122 сек
7. Начни с шага 2 (SolverConfig.h). После каждого шага: сборка + тесты
