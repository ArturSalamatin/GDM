---
tags:
  - prompt
  - оптимизация
  - cpr
  - production
date: 2026-06-24
---

# Сессия 9: CPR в production — полный переход

## Цель

Заменить блочный AMG-солвер на CPR (Constrained Pressure Residual) в production коде. Не добавить опцию — полностью перейти.

Бенчмарк подтвердил: CPR даёт -32% по времени (27.0 с vs 39.6 с). Блочный путь удаляется.

## Что меняется

### Файл 1: `HydroSolver/Solver/Math/LinearProblem.h`

**Убрать:**
- `#include <amgcl/adapter/block_matrix.hpp>`
- `#include <amgcl/value_type/static_matrix.hpp>`
- `template<unsigned char B> using value_type = amgcl::static_matrix<double, B, B>`
- `template<unsigned char B> using rhs_type = amgcl::static_matrix<double, B, 1>`
- `template<unsigned char B> using BBackend = amgcl::backend::builtin<value_type<B>>`
- `template<unsigned char B> using Solver_AMG = amgcl::make_solver<amgcl::amg<BBackend<B>, ...>, lgmres<BBackend<B>>>`

**Добавить:**
- `#include <amgcl/preconditioner/cpr.hpp>`
- `#include <amgcl/relaxation/as_preconditioner.hpp>` (если не включён через cpr.hpp)
- `#include <amgcl/relaxation/ilu0.hpp>` (уже есть)
- Скалярный backend и CPR typedef:

```cpp
using ScalarBackend = amgcl::backend::builtin<double>;

using CPRPrecond = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;

using CPRSolver = amgcl::make_solver<CPRPrecond, amgcl::solver::lgmres<ScalarBackend>>;
```

**Изменить member:**
- `Solver_AMG<B>::params prm` → `CPRSolver::params prm`

**Инициализация prm в конструкторе (добавить):**
- `prm.precond.block_size = B;` — CPR должен знать размер блока (2)

**Можно удалить (стало лишним):**
- `#include <amgcl/relaxation/iluk.hpp>` — CPR использует ilu0, не iluk

### Файл 2: `HydroSolver/Solver/Math/LinearProblem.cpp`

**Метод `Solve()` — полностью переписать тело:**

Было (блочный путь):
```cpp
auto A = amgcl::adapter::block_matrix<value_type<B>>(
    std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()));
Solver_AMG<B> solve(A, prm);

rhs_type<B> const* fptr = reinterpret_cast<rhs_type<B> const*>(&rhs[0]);
rhs_type<B>* xptr = reinterpret_cast<rhs_type<B>*>(&solutionCorrections[0]);
amgcl::backend::numa_vector<rhs_type<B>> F(fptr, fptr + cellNmbr);
amgcl::backend::numa_vector<rhs_type<B>> X(xptr, xptr + cellNmbr);

auto [iters, error] = solve(F, X);
std::copy(X.data(), X.data() + X.size(), xptr);
```

Стало (скалярный CPR):
```cpp
CPRSolver solve(
    std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()),
    prm);

auto [iters, error] = solve(rhs, solutionCorrections);
```

Ключевые отличия:
1. **Нет `block_matrix_adapter`** — CRS передаётся как скалярная (rhsSize × rhsSize)
2. **Нет `reinterpret_cast`** — rhs и solutionCorrections уже `vector<double>`, передаются напрямую
3. **Нет копирования** в numa_vector и обратно — amgcl принимает `std::vector<double>` напрямую
4. **Результат пишется прямо в solutionCorrections** — amgcl модифицирует переданный вектор in-place

⚠️ **ВНИМАНИЕ: проверить поведение amgcl с std::vector.** В benchmark (`solve_with_scalar`) используется копия:
```cpp
std::vector<double> F(lp.Rhs().begin(), lp.Rhs().end());
std::vector<double> X(lp.SolutionCorrections().begin(), lp.SolutionCorrections().end());
auto [iters, error] = solve(F, X);
std::copy(X.begin(), X.end(), lp.SolutionCorrections().begin());
```
Почему копия? Возможно solve() модифицирует RHS. Или для безопасности. **Нужно проверить**: если amgcl::make_solver::operator() модифицирует F (rhs), то передавать оригинальный rhs нельзя — нужна копия. Это критический момент.

**Безопасный вариант (сначала так):**
```cpp
std::vector<double> F(rhs);
std::vector<double> X(solutionCorrections);
auto [iters, error] = solve(F, X);
solutionCorrections = X;  // или std::copy
```

После верификации можно оптимизировать (если solve не трогает F — убрать копию rhs).

### Файл 3: `HydroSolver/Reservoir/ReservoirSImulator.h`

**Изменить default Layout:**
```cpp
// Было:
Layout layout = Layout::InterleavedSwP
// Стало:
Layout layout = Layout::InterleavedPSw
```

Это единственное изменение. Все вызовы без явного Layout автоматически получат PSw.

### Файл 4: `tests/test_amgcl_benchmark.cpp`

**Удалить:**
- Функцию `solve_with` (блочный) — больше не нужна
- `#include <amgcl/adapter/block_matrix.hpp>`, `#include <amgcl/value_type/static_matrix.hpp>` — если не используются
- Типы `value_type<B>`, `rhs_type<B>`, `BBackend<B>` — если не используются
- Все SECTION-ы из Series A–H, которые используют `solve_with` (блочный путь через `run_benchmark`)

⚠️ **СТОП. `run_benchmark` использует `LinearProblem::Solve()`, а не `solve_with`.**

Перепроверяю. `run_benchmark` вызывает `MyProblem.Solve()` через Newton loop в `sim.Solve()`. То есть `run_benchmark` использует production солвер (LinearProblem::Solve). А `run_benchmark_scalar` — свой Newton loop с `solve_with_scalar`.

**После перехода production на CPR:**
- `run_benchmark` автоматически использует CPR (через `sim.Solve()` → `LinearProblem::Solve()` → CPRSolver)
- `run_benchmark_scalar` и `solve_with_scalar` — дублируют то, что теперь делает production. **Можно удалить** `run_benchmark_scalar`, CPR Series переписать на `run_benchmark` с Layout::InterleavedPSw
- `solve_with` (блочный) — удалить

Но: Series A–H benchmark-и (перебор конфигураций) вызывают `run_benchmark`, который теперь будет CPR. Эти серии тестировали **разные** конфигурации amgcl (damped_jacobi, spai0, разные солверы). С CPR они бессмысленны — CPR уже выбран.

**Решение (принято):**
- `run_benchmark` становится нешаблонным — убрать `SolverType`, убрать параметр `prm` (production солвер берёт параметры из `LinearProblem::prm` в конструкторе)
- Series A–H удаляются (устарели — тестировали блочные конфигурации, выбор сделан)
- `solve_with` (блочный) удаляется
- `solve_with_scalar` и `run_benchmark_scalar` остаются — для будущих экспериментов с альтернативными конфигурациями; они самодостаточны и не зависят от production типов
- Series CPR: переписать на `run_benchmark` (теперь production = CPR)
- Series TS: убрать template параметр, использовать `run_benchmark`

### Файл 5: `HydroSolver/Reservoir/CalculationManager.cpp`

Создаёт `ReservoirSimulator` без Layout → получит default `InterleavedPSw` → OK.

### Файл 6: `src/main.cpp`

Создаёт `ReservoirSimulator` без Layout → получит default `InterleavedPSw` → OK.

### Файл 7: `HydroSolver/tests/simulate_reservoir.cpp`

Создаёт `ReservoirSimulator` без Layout → получит default `InterleavedPSw` → OK.

## Что НЕ меняется

- **CRSStructure** — layout-абстракция уже поддерживает InterleavedPSw. Столбцы уже сортированы.
- **MatrixCSR** — `AddDiagBlock`, `AddOffDiagBlock`, `CopyBlock` работают через `DiagBlocks()`/`OffDiagBlocks()`, которые layout-aware.
- **Assembly** (`ReservoirSimulator::fillMatrixBlockRow`) — пишет блоки в физическом порядке `[Sw, P]`. CRSStructure транслирует в CRS-позиции.
- **`UnpackCellCorrections`** — использует `CRS::GlobalIndex()` с `permPhysicalToCRS_`, layout-aware.
- **`AddDiagBlock` (LinearProblem)** — rhs заполняется через `GlobalIndex`, layout-aware.
- **NumericalParameters** — параметры AMG (tol, abstol, maxiter) не зависят от типа солвера.
- **Newton loop** (`ReservoirSimulator::Solve`) — вызывает `MyProblem.Solve()`, не знает о типе солвера.
- **constexpr B = 2** — остаётся. CPR использует `block_size = B` для извлечения давления.

## Подводные камни и потенциальные проблемы

### 1. amgcl::make_solver может модифицировать RHS

**Риск:** Если `solve(F, X)` модифицирует F (правую часть), а мы передаём оригинальный `rhs`, то rhs будет повреждён. Newton loop использует rhs только для одного solve, потом делает `ResetProblem()`. Но если `Solve()` вызывается дважды без `Reset` — проблема.

**Проверка:** В Newton loop (`ReservoirSimulator::Solve`, строка ~490): `ResetProblem()` вызывается перед каждой assembly → rhs пересоздаётся. Значит повреждение rhs не критично.

**Но для безопасности:** в первой итерации использовать копию. Оптимизировать позже.

### 2. CPR setup на каждом Newton-шаге

Production код создаёт `CPRSolver` заново на каждом вызове `Solve()` (строка 113: `Solver_AMG<B> solve(A, prm)`). С блочным AMG это работало. С CPR — AMG hierarchy для давления тоже пересоздаётся каждый раз. Это та же ситуация, что в benchmark — benchmark тоже создаёт солвер на каждом вызове. Результат benchmark валиден, 27.0 с включает все setup-ы.

**Оптимизация (сессия 10):** reuse AMG hierarchy для давления между Newton-итерациями (partial_update). Матрица давления меняется слабо между Newton-шагами — можно reuse coarsening, обновляя только smoother. Потенциал: ещё 10–20%.

### 3. `prm.precond.block_size = B` — не забыть

CPR по умолчанию ставит `block_size = 2` (из `static_rows<double>::value == 1`). Для нашего B=2 это совпадает. Но **явно задать** `prm.precond.block_size = B` в конструкторе LinearProblem — чтобы при изменении B (трёхфазная модель, B=3) не ломалось.

### 4. Параметры lgmres: K=5

В конструкторе LinearProblem: `prm.solver.K = 5`. Это параметр lgmres (число сохранённых correction vectors). С CPR значение K=5 — корректно (в benchmark CPR1 тоже K=5). Убедиться, что `CPRSolver::params::solver` имеет поле K (lgmres-специфичное). Это поле есть — lgmres тот же.

### 5. Параметры lgmres: M (inner vectors)

В benchmark: `prm.solver.M = 15`. В production конструкторе LinearProblem M не задаётся → default = 15. OK, совпадает.

### 6. Параметры CPR: pprecond (AMG для давления)

В benchmark CPR1: defaults (aggregation, ilu0 smoother, все параметры по умолчанию). В production: не задано = defaults. OK.

### 7. Параметры CPR: sprecond (ILU0 для полной системы)

В benchmark CPR1: defaults (ilu0, damping = 1.0). OK.

### 8. Удаление `#include <amgcl/relaxation/iluk.hpp>`

Production использовал iluk(k=1) в AMG smoother. CPR использует ilu0. Но **benchmark** (`test_amgcl_benchmark.cpp`) всё ещё использует iluk в CPR_AMG_iluk typedef. Удалять iluk include из LinearProblem.h — OK. Из benchmark — нет.

### 9. Компиляция: время

`cpr.hpp` включает `amg.hpp` + `as_preconditioner.hpp`. `amg.hpp` уже включён. `cpr.hpp` добавит ~500 строк шаблонного кода. Компиляция LinearProblem.cpp замедлится на 2–5 секунд. Приемлемо.

### 10. Все 45+ тестов должны пройти

Тесты создают `ReservoirSimulator` без Layout (default). Default станет `InterleavedPSw`. Assembly через CRS layout-aware. Solve через CPR. UnpackCorrections через GlobalIndex layout-aware.

**Риск:** Тесты проверяют физические результаты (баланс масс, насыщенность, давление). CPR даёт другую арифметику (другой прекондиционер → другие итерации → другие roundoff errors). Результаты должны совпадать до tolerance тестов (1e-3 для баланса масс). На benchmark совпадение подтверждено (oil_rel < 2e-10). Но на других сценариях (single injector, five-spot, variable debit) — нужно прогнать и проверить.

### 11. Тест streamlines

`test_streamlines.cpp` использует velocity field после simulation. Velocity вычисляется из P и Sw. С CPR значения P и Sw могут отличаться в roundoff. Тест streamlines не проверяет численные значения строго — только качественно (streamlines существуют, не пусты). OK.

### 12. Benchmark `run_benchmark` перестанет компилироваться

`run_benchmark` — шаблон по `SolverType`, принимает `SolverType::params`. После удаления блочных типов, вызовы вроде `run_benchmark<Solver_AMG<B>>` не скомпилируются — `Solver_AMG<B>` больше не существует.

**Решение (принято):**
- `run_benchmark` → нешаблонный, без параметра `prm`
- `solve_with` (блочный) → удалить
- `solve_with_scalar`, `run_benchmark_scalar` → оставить (самодостаточны, для будущих экспериментов)
- Series A–H → удалить (устарели)
- Series CPR → переписать на `run_benchmark`
- Series TS → убрать template параметр

## Порядок выполнения

### Шаг 1: LinearProblem.h — typedef-ы

1. Убрать include-ы: `block_matrix.hpp`, `static_matrix.hpp`
2. Убрать typedef-ы: `value_type`, `rhs_type`, `BBackend`, `Solver_AMG`
3. Добавить include: `<amgcl/preconditioner/cpr.hpp>`, `<amgcl/relaxation/as_preconditioner.hpp>`
4. Добавить typedef-ы: `ScalarBackend`, `CPRPrecond`, `CPRSolver`
5. Заменить `Solver_AMG<B>::params prm` → `CPRSolver::params prm`
6. Убрать `#include <amgcl/relaxation/iluk.hpp>` (не используется CPR)

### Шаг 2: LinearProblem.cpp — конструктор

Добавить `prm.precond.block_size = B;` после существующих `prm.solver.*` строк.

### Шаг 3: LinearProblem.cpp — Solve()

Заменить тело Solve() на скалярный CPR-путь:
```cpp
SolveResult LinearProblem::Solve(int maxIter)
{
    prm.solver.maxiter = maxIter;

    prof.tic("setup");
    CPRSolver solve(
        std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()),
        prm);
    prof.toc("setup");

    std::vector<double> F(rhs);
    std::vector<double> X(solutionCorrections);

    prof.tic("solve");
    auto [iters, error] = solve(F, X);
    prof.toc("solve");

    solutionCorrections = std::move(X);

    return { iters, error, true };
}
```

### Шаг 4: ReservoirSImulator.h — default Layout

Изменить `Layout layout = Layout::InterleavedSwP` → `Layout layout = Layout::InterleavedPSw`.

### Шаг 5: test_amgcl_benchmark.cpp — очистка

1. Удалить `solve_with` (блочный template, строки ~40–66)
2. Удалить `#include <amgcl/adapter/block_matrix.hpp>`, `#include <amgcl/value_type/static_matrix.hpp>`
3. Удалить блочные using-ы, если они остались только для `solve_with`: `value_type<B>`, `rhs_type<B>`, `BBackend<B>`
4. `solve_with_scalar` и `run_benchmark_scalar` — **оставить** (самодостаточны, для экспериментов)
5. `run_benchmark` → убрать `template<typename SolverType>`, убрать параметр `prm`. Сигнатура:
   ```cpp
   BenchmarkResult run_benchmark(const std::string& config_name,
                                  Layout layout = Layout::InterleavedPSw,
                                  bool usePIController = false,
                                  PIControllerParams piParams = {},
                                  double snapshot_dt = 5.0,
                                  double init_tau = -1.0)
   ```
6. Series A–H: удалить целиком (устарели)
7. Series CPR: переписать вызовы с `run_benchmark_scalar<S>(name, prm, layout)` на `run_benchmark(name, layout)`. Удалить CPR_BL section (production теперь и есть CPR).
8. Series TS: убрать template параметр, вызывать `run_benchmark(name, ...)`

### Шаг 6: Сборка и тесты

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Все 45+ тестов должны пройти. Если тест падает — проверить:
1. Не повреждён ли rhs (проблема #1)
2. Не нарушен ли порядок переменных при UnpackCorrections
3. Не вышел ли CPR за tolerance

### Шаг 7: Верификация производительности

Запустить benchmark CPR_BL (production block, теперь CPR):
```
ctest --test-dir build -C Release -R "CPR_BL" --output-on-failure
```
Или один из TS тестов. Убедиться что время ~27 с (CPR), не ~40 с (block AMG).

## Связанные заметки

- [[2026-06-24 сессия 6b-7 CPR benchmark и PI-контроллер]]
- [[layout абстракция отделяет топологию сетки от CRS маппинга]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[prompt-оптимизация-06b-CPR-benchmark]]
