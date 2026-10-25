---
tags:
  - промпт
  - производительность
  - amgcl
  - cpr
  - layout
date: 2026-06-24
---

# Сессия 6b: CPR benchmark с Layout::InterleavedPSw

## Предыстория

Сессия 6a (2026-06-23) создала layout-абстракцию: `CRSStructure` + `enum Layout {InterleavedSwP, InterleavedPSw, Blocked}`. Сейчас production использует `Layout::InterleavedSwP`, 35 тестов проходят.

CPR-прекондиционер amgcl жёстко берёт `col % B == 0` как давление. С `InterleavedPSw` col%2==0 → P — правильно.

Цель: замерить CPR vs текущий `amg<aggregation, iluk> + lgmres` на бенчмарке (51×51×4, 730 дней, 7 скважин). Текущий baseline: t_total=43.7с, t_solve=36.6с.

## Этапы

### Этап 0: Layout как параметр ReservoirSimulator

**Файлы:** `ReservoirSImulator.h`, `ReservoirSimulator.cpp`

Текущее состояние — `Layout::InterleavedSwP` захардкожен в конструкторе `ReservoirSimulator.cpp:42`:
```cpp
MyProblem{ LinearProblem{Layout::InterleavedSwP, numPrm.AMG_AbsTol, ...} }
```

Нужно: добавить параметр `Layout layout = Layout::InterleavedSwP` в конструктор `ReservoirSimulator`.

**Изменения:**
1. `ReservoirSImulator.h` — в конструктор `ReservoirSimulator(const NumericalParameters&, ...)` добавить `Layout layout = Layout::InterleavedSwP` как последний параметр (default = backward compatible).
2. `ReservoirSimulator.cpp:42` — заменить `Layout::InterleavedSwP` на `layout`.
3. Проверить, что все существующие вызовы (тесты, main, benchmark) компилируются без изменений (default parameter).

**Pitfall:** `Layout` определён в `reservoir_simulator::linear_problem`. `ReservoirSImulator.h` уже имеет `using namespace linear_problem;` (строка 29), так что `Layout` доступен.

### Этап 0.5: КРИТИЧНО — сортировка столбцов CRS для InterleavedPSw

**Файл:** `CRSStructure.cpp`, функция `buildInterleaved`

**Проблема:** buildInterleaved итерирует внутренний цикл по `physCol` (строки 88, 101, 122), а col_.push_back добавляет `cell * B + crsCol`. Для InterleavedSwP (identity permutation) physCol и crsCol совпадают → столбцы внутри строки отсортированы. Для InterleavedPSw permutation меняет порядок: physCol=0 → crsCol=1, physCol=1 → crsCol=0. Результат: col_ содержит [..., cell*2+1, cell*2+0, ...] — **столбцы внутри CRS-строки не отсортированы по возрастанию**.

**Почему это проблема:**
1. **ILU0 упадёт с assertion.** `ilu0.hpp:154` итерирует столбцы строки i и делает `if (c >= i) break` + `precondition(c == i, "No diagonal value")`. Для CRS row 0 с col_=[1, 0]: первый c=1, c >= 0 → break, precondition(1 == 0) → **CRASH**. CPR передаёт **несортированную** матрицу K в SPrecond = `as_preconditioner<SB, ilu0>` (cpr.hpp:384), `as_preconditioner` не сортирует (as_preconditioner.hpp:107-110).
2. **CPR row iterators (first_scalar_pass, init) НЕ ломаются** — они читают блоки целиком через `col < end = (block+1)*B`, и внутри блока все col попадают в диапазон. col%B корректно различает переменные.
3. **AMG не пострадает** — amg.hpp:202 вызывает `sort_rows(*A)` на внутренней копии. Текущий production (InterleavedSwP + block AMG) работает, потому что identity permutation даёт отсортированные столбцы.
4. **Текущие 35 тестов не ловят баг** — все используют InterleavedSwP (identity), столбцы отсортированы по построению.

**Решения (от дешёвого к дорогому):**

**Вариант A: Итерировать по crsCol вместо physCol.** В buildInterleaved заменить внутренние циклы:
```cpp
// Было:
for (unsigned char physCol = 0; physCol < B; physCol++) {
    unsigned char crsCol = permPhysicalToCRS_[physCol];
    ...
}

// Стало:
for (unsigned char crsCol = 0; crsCol < B; crsCol++) {
    unsigned char physCol = permCRSToPhysical_[crsCol];
    ...
}
```
Это гарантирует `col_` отсортированные (crsCol возрастает). **Но** нужно пересчитать `physBlockIdx` и `diagTmp` индексирование — physBlockIdx = physRow * B + physCol, а физический порядок нужен для diagBlocks_.

**Вариант B: sort_rows после buildInterleaved.** Вызвать `amgcl::backend::sort_rows()` на готовой CRS. Проблема: sort_rows переставляет val[], но наш val заполняется позже через CopyBlock. diagBlocks_ указывает на позиции в val[] — sort_rows сломает эти указатели.

**Вариант C: sort col_ + diagTmp/offDiagTmp внутри buildInterleaved.** После заполнения col_ для каждого CRS-блока — отсортировать col_ и обновить diagTmp/offDiagTmp. Сложнее A, но не требует менять логику итерации.

**Рекомендация: Вариант A.** Заменить `for physCol → crsCol` на `for crsCol → physCol`. Нужно изменить 3 места в buildInterleaved (диагональный блок при f=true, диагональный блок при f=false/в цикле, offDiag блок). В каждом месте: внешний цикл по crsCol, вычисляем physCol = permCRSToPhysical_[crsCol], physBlockIdx = physRow * B + physCol.

**Верификация:** после исправления — прогнать 35 тестов с InterleavedSwP (должны пройти как раньше) и с InterleavedPSw (новый тест: собрать матрицу, проверить что col_ отсортированы в каждой строке).

**Добавить unit test:**
```cpp
TEST_CASE("CRS columns sorted for InterleavedPSw", "[crs][layout]") {
    // Создать простую сетку 3×1×1
    // Построить CRSStructure с InterleavedPSw
    // Проверить: для каждой строки i, col[row[i]] < col[row[i]+1] < ... < col[row[i+1]-1]
}
```

### Этап 1: solve_with_scalar — скалярный AMGCL solve

**Файл:** `test_amgcl_benchmark.cpp`

CPR работает со **скалярным** backend (`amgcl::backend::builtin<double>`). Текущий `solve_with` использует блочный backend (`block_matrix` + `rhs_type<B>`). Нужна отдельная функция для скалярного solve.

```cpp
template<typename SolverType>
SolveResult solve_with_scalar(LinearProblem& lp, int maxIter, typename SolverType::params& prm)
{
    prm.solver.maxiter = maxIter;

    auto n = lp.RhsSize();
    auto const& row = lp.Matrix().Row();
    auto const& col = lp.Matrix().Col();
    auto const& val = lp.Matrix().Val();

    SolverType solve(std::tie(n, row, col, val), prm);

    std::vector<double> F(lp.Rhs().begin(), lp.Rhs().end());
    std::vector<double> X(lp.SolutionCorrections().begin(), lp.SolutionCorrections().end());

    auto [iters, error] = solve(F, X);

    std::copy(X.begin(), X.end(), lp.SolutionCorrections().begin());
    return { iters, error, true };
}
```

**Pitfalls:**
- **Копия F и X обязательна.** AMGCL может модифицировать rhs при solve. Без копии — порча данных LinearProblem.
- **Row/Col типы.** amgcl ожидает CRS tuple `std::tie(n, row, col, val)`. Наши row/col — `std::vector<size_t>`, val — `std::vector<double>`. amgcl автоматически конвертирует size_t → ptrdiff_t.
- **`std::tie(n, row, col, val)` — n = rhsSize** (= cellNmbr * B), НЕ cellNmbr. Это размер скалярной системы.

### Этап 2: run_benchmark_cpr — параметризованный по Layout

**Файл:** `test_amgcl_benchmark.cpp`

Нужна версия `run_benchmark`, которая:
1. Создаёт `ReservoirSimulator` с заданным `Layout`
2. Вызывает `solve_with_scalar` вместо `solve_with`

Можно сделать `run_benchmark_cpr<SolverType>(name, prm, layout)` — по аналогии с `run_benchmark`, но:
- Конструктор sim принимает Layout
- solve вызывает solve_with_scalar

**Минимальное изменение:** отдельный `run_benchmark_scalar<SolverType>`, чтобы не ломать существующие серии A–H. Это **полная копия** `run_benchmark` (~80 строк Newton loop), но с двумя отличиями: (1) конструктор ReservoirSimulator принимает Layout, (2) solve вызывает `solve_with_scalar` вместо `solve_with`. Дублирование неприятно, но параметризация лямбдой — over-engineering для бенчмарка.

**Pitfall — reinterpret_cast в Solve():**
Штатный `LinearProblem::Solve()` использует блочный backend с `reinterpret_cast` rhs/corrections к `rhs_type<B>`. Для CPR мы **не вызываем** `LinearProblem::Solve()` — используем `solve_with_scalar`, который обращается к raw `Rhs()`, `Row()`, `Col()`, `Val()`, `SolutionCorrections()`. Штатный Solve() не вызывается и не мешает.

**Pitfall — Newton convergence:**
`UpdateGrid` использует `UnpackCellCorrections` для чтения corrections. `solve_with_scalar` пишет скалярные corrections обратно в `SolutionCorrections()` в CRS-порядке. `UnpackCellCorrections` вызывает `GlobalIndex(cell, var)` — для InterleavedPSw это `cell * 2 + perm[var]`. Corrections[cell*2+0] = δP, Corrections[cell*2+1] = δSw. UnpackCellCorrections вернёт physical[0] = Corrections[GlobalIndex(l, 0)] = Corrections[l*2 + perm[0]] = Corrections[l*2 + 1] = δSw. physical[1] = Corrections[GlobalIndex(l, 1)] = Corrections[l*2 + 0] = δP. **Правильно.** Layout корректно обрабатывает обратное преобразование.

### Этап 3: Includes и typedef для CPR

**Файл:** `test_amgcl_benchmark.cpp`

Добавить в начало файла:
```cpp
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
```

Typedef-ы для конфигураций:
```cpp
using SB = amgcl::backend::builtin<double>;

// CPR: AMG(aggregation, ilu0) для давления + ILU0 для полной системы + lgmres
using CPR_AMG_ilu0 = amgcl::preconditioner::cpr<
    amgcl::amg<SB, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>
>;
using CPRSolver_AMG_ilu0 = amgcl::make_solver<CPR_AMG_ilu0, amgcl::solver::lgmres<SB>>;

// CPR-DRS: то же, но с dynamic row sum
using CPRDRS_AMG_ilu0 = amgcl::preconditioner::cpr_drs<
    amgcl::amg<SB, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>
>;
using CPRDRSSolver_AMG_ilu0 = amgcl::make_solver<CPRDRS_AMG_ilu0, amgcl::solver::lgmres<SB>>;

// Голый ILU0 (скалярный) для нижней границы
using ILU0_scalar = amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>;
using ILU0Solver_scalar = amgcl::make_solver<ILU0_scalar, amgcl::solver::lgmres<SB>>;

// CPR с iluk(1) для давления
using CPR_AMG_iluk = amgcl::preconditioner::cpr<
    amgcl::amg<SB, amgcl::coarsening::aggregation, amgcl::relaxation::iluk>,
    amgcl::relaxation::as_preconditioner<SB, amgcl::relaxation::ilu0>
>;
using CPRSolver_AMG_iluk = amgcl::make_solver<CPR_AMG_iluk, amgcl::solver::lgmres<SB>>;

// Текущий production (блочный) для сравнения
using Production_Block = Solver_AMG<B>;
```

**Pitfall — компиляция CPR с блочным backend:**
`cpr.hpp` имеет `static_assert(math::static_rows<PPrecond::backend_type::value_type>::value == 1)` — PPrecond ДОЛЖЕН быть скалярным. SPrecond ДОЛЖЕН иметь тот же backend, что и PPrecond. CPR принимает скалярную CRS напрямую (не `block_matrix`).

### Этап 4: Серия CPR — конфигурации бенчмарка

**Файл:** `test_amgcl_benchmark.cpp`

```cpp
TEST_CASE("AMGCL benchmark: Series CPR — CPR preconditioner",
          "[benchmark][amgcl][seriesCPR][.slow]")
{
    fs::create_directories("results");

    // --- InterleavedPSw (col%2==0 → P, правильно для CPR) ---

    SECTION("CPR1: cpr<amg_ilu0, ilu0> + lgmres, PswLayout") {
        using S = CPRSolver_AMG_ilu0;
        S::params prm;
        prm.precond.block_size = 2;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR1_cpr_ilu0_PswLayout", prm, Layout::InterleavedPSw);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("CPR2: cpr<amg_iluk, ilu0> + lgmres, PswLayout") {
        using S = CPRSolver_AMG_iluk;
        S::params prm;
        prm.precond.block_size = 2;
        prm.precond.pprecond.relax.k = 1;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR2_cpr_iluk_PswLayout", prm, Layout::InterleavedPSw);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    SECTION("CPR3: cpr_drs<amg_ilu0, ilu0> + lgmres, PswLayout") {
        using S = CPRDRSSolver_AMG_ilu0;
        S::params prm;
        prm.precond.block_size = 2;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR3_cprdrs_ilu0_PswLayout", prm, Layout::InterleavedPSw);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // --- Нижние границы ---

    SECTION("CPR4: голый ilu0 (скалярный) + lgmres, SwPLayout") {
        using S = ILU0Solver_scalar;
        S::params prm;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR4_ilu0_scalar_SwPLayout", prm, Layout::InterleavedSwP);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // --- Проверка гипотезы: CPR с Sw как «давление» (без перестановки) ---

    SECTION("CPR5: cpr<amg_ilu0, ilu0> + lgmres, SwPLayout (Sw как давление)") {
        using S = CPRSolver_AMG_ilu0;
        S::params prm;
        prm.precond.block_size = 2;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark_scalar<S>("CPR5_cpr_ilu0_SwPLayout_WRONG", prm, Layout::InterleavedSwP);
        report(r);
        append_csv(csv_path, r);
        // Может не сойтись — CHECK, не REQUIRE
        CHECK(r.balance_ok);
    }

    // --- Baseline (текущий production, блочный) ---

    SECTION("CPR_BL: production iluk block (baseline)") {
        using S = Production_Block;
        S::params prm;
        prm.precond.relax.k = 1;
        prm.solver.M = 15;
        prm.solver.K = 5;
        auto r = run_benchmark<S>("CPR_BL_production_block", prm);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }
}
```

**Pitfalls:**
1. **`prm.precond.block_size = 2`** — обязателен для скалярного CPR. По умолчанию 2 (для скалярного backend), но лучше задать явно.
2. **`prm.precond.pprecond.relax.k = 1`** — для iluk в PPrecond. Путь к параметрам: `prm.precond` = CPR params, `prm.precond.pprecond` = PPrecond (AMG) params, `prm.precond.pprecond.relax` = relaxation params, `.k` = fill level.
3. **`prm.solver.tol / abstol`** — нужно задать через `sim.numPrm.AMG_RelTol / AMG_AbsTol`, как в `run_benchmark`. Скопировать логику из `run_benchmark`.
4. **CPR5 (Sw как давление)** — контрольный эксперимент. Если CPR сходится с Sw → AMG не обязательно нужна перестановка. Если расходится → подтверждение, что InterleavedPSw критичен.
5. **Время компиляции.** Каждый typedef CPR — тяжёлый шаблон. Инстанцирование 5 CPR-конфигураций + блочный baseline может занять 2-3 минуты компиляции.

### Этап 5: Параметры CPR — тюнинг

**Файл:** `test_amgcl_benchmark.cpp`

Параметры по умолчанию для CPR::params:
- `block_size = 2` (наш B)
- `active_rows = 0` (все строки — правильно)
- PPrecond (AMG): coarsening + relaxation — стандартные
- SPrecond (ILU): damping и fill level

Параметры CPR-DRS (cpr_drs.hpp):
- `eps_dd = 0.2` — порог для "pressure-like" строк по диагональному доминированию
- `eps_ps = 0.02` — порог по off-diagonal coupling

Не менять дефолты на первой итерации. Тюнить только если CPR1/CPR2/CPR3 сходятся, но медленнее ожиданий.

### Этап 6: Анализ результатов и решение

Таблица ожидаемых результатов:

| Конфигурация | Ожидание | Действие |
|---|---|---|
| CPR1 < baseline | CPR в production | Этап 7 |
| CPR1 ≈ baseline | Тюнить параметры или попробовать cpr_drs | — |
| CPR1 > baseline × 1.5 | CPR не подходит для этой задачи | Пропустить CPR, перейти к сессии 7 |
| CPR5 сходится | Перестановка не нужна, layout — бонус | — |
| CPR5 расходится | InterleavedPSw обязателен для CPR | Подтверждение layout-подхода |

### Этап 7: CPR в production (условно)

**Только если CPR1 или CPR3 даёт >15% ускорения t_solve.**

Нужно:
1. Добавить скалярный solve path в `LinearProblem::Solve()` (или отдельный метод `SolveCPR()`)
2. Layout::InterleavedPSw в production конструкторе ReservoirSimulator
3. `partial_update` — reuse AMG hierarchy для давления (cpr.hpp:158). Вызывать `partial_update(K, false)` (update_transfer_ops=false) если матрица изменилась, но структура та же (Newton iteration → структура та же, значения другие).

**Pitfall — partial_update:**
`partial_update` пересоздаёт SPrecond (ILU), но сохраняет PPrecond (AMG). AMG hierarchy для давления строится один раз и переиспользуется. Это отменяет setup cost для AMG (4-7 с в текущем baseline), оставляя только ILU setup (~0.3-1 с).

Для partial_update нужно хранить `shared_ptr<SolverType>` между Newton-итерациями внутри одного timestep. Сейчас `Solve()` создаёт солвер каждый раз. Нужен `std::optional<CPRSolver>` или два-фазный init/solve.

**Pitfall — несовместимость с reuse AMG из сессии 4:**
Сессия 4 (reuse AMG) дала SIGSEGV с `block_matrix_adapter`. CPR.partial_update — другой механизм (встроенный в CPR), не зависит от `block_matrix_adapter`. Должен работать.

## Критерии успеха

1. ✅ Benchmark компилируется и запускается для всех 6 конфигураций
2. ✅ CPR1 (cpr + InterleavedPSw) сходится (balance_ok = true)
3. 📊 Таблица: t_total, t_solve, avg_iters для всех конфигураций
4. Решение: CPR в production или skip

## Файлы для изменения

| Файл | Изменение |
|---|---|
| `ReservoirSImulator.h` | Layout parameter в конструкторе |
| `ReservoirSimulator.cpp` | Layout parameter в member init list |
| `test_amgcl_benchmark.cpp` | solve_with_scalar, run_benchmark_scalar, Series CPR |

## Связанные заметки

- [[2026-06-23 сессия 6a layout абстракция]]
- [[layout абстракция отделяет топологию сетки от CRS маппинга]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[план-сессия-6-CPR-прекондиционер]]
- [[возможности amgcl для блочных СЛАУ]]
