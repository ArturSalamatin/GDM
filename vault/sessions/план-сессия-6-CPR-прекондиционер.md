---
tags:
  - план
  - производительность
  - amgcl
  - cpr
date: 2026-06-21
---

# Сессия 6: CPR-прекондиционер — детальный план

## Текущее состояние

- Total: 43.7 с, solver: 36.6 с (84%), assembly: 2.7 с (6%)
- Солвер: `amg<aggregation, iluk(k=1)> + lgmres(M=15, K=5)`, блочный backend 2×2
- 187 шагов, 1112 Newton, 9277 AMG iterations, avg 8.3 iters/solve
- Ускорение от baseline: 3.1×

## Проблема порядка переменных — КРИТИЧЕСКАЯ

### Как CPR выбирает «давление»

В `cpr.hpp` (строка 341) и `cpr_drs.hpp` (строка 387):
```cpp
if (k[i].col() % B == 0) {
    app += d[i] * k[i].value();
}
```

CPR жёстко берёт **столбцы с `col % B == 0`** как «давленческие». При `B=2` это столбцы 0, 2, 4, ... — **первая переменная в блоке**.

### Наш порядок переменных

Порядок определён в `TwoPhaseFlowCell.cpp`:
- `VariableFieldProperties[0] = Sw` (нормированная водонасыщенность)
- `VariableFieldProperties[1] = P` (давление)

В скалярной CRS:
- Строка `2l` = oil equation (уравнение нефти)
- Строка `2l+1` = water equation (уравнение воды)
- Столбец `2l` = Sw (переменная 0)
- Столбец `2l+1` = P (переменная 1)

В блоке `blDiag[4]` (row-major):
```
[0] = d(oil_eq)/d(Sw)     [1] = d(oil_eq)/d(P)
[2] = d(water_eq)/d(Sw)   [3] = d(water_eq)/d(P)
```

**Проблема:** CPR возьмёт столбцы Sw как «давление» → AMG будет решать систему для насыщенности вместо давления. AMG неэффективен для гиперболических уравнений — сходимость деградирует или солвер расходится.

### Нет параметра маски

Ни `cpr.hpp`, ни `cpr_drs.hpp` не имеют параметра для выбора переменной давления. `weights` в DRS — веса для restriction operator, не для выбора переменной. Логика `col % B == 0` захардкожена.

## Стратегия: перестановка при передаче в CPR

### Почему НЕ менять порядок в MatrixCSR / assembly

Порядок `[0]=Sw, [1]=P` зашит глубоко:
- `TwoPhaseFlowCell.cpp:96-97`: `SWater_Scaled()` = `VariableFieldProperties[0]`, `P()` = `VariableFieldProperties[1]`
- `TwoPhaseFlowCell.cpp:12`: projection Sw в `[0,1]`
- `fillMatrixBlockRow`: все индексы blDiag/rhsBlock/blOffDiag
- `AccountForBoundaryConditions`: три копии (YZ, XZ, XY) с теми же индексами
- `Wells.cpp:32-63`: `rhsPerPerforation[l][0]` = oil, `[1]` = water; `matrixBlockPerPerforation[l][0..3]`
- `UpdateGrid`: `Corrections()[B*l + 0]` = Sw, `[B*l + 1]` = P
- `AbstractCells.h:209`: `UpdateState` складывает corrections с VariableFieldProperties

Изменение порядка — 10+ файлов, высокий риск ошибки.

### Что делать вместо этого

**Перестановку делаем ТОЛЬКО в точке входа CPR-солвера.** Это изолированная операция O(nnz):

1. Перед передачей матрицы в CPR — переставить строки/столбцы скалярной CRS: `[Sw,P]` → `[P,Sw]`
2. Перед передачей rhs — переставить `[oil_rhs, water_rhs]` → `[water_rhs, oil_rhs]`? Нет — rhs привязан к уравнениям, не к переменным. Перестановка строк матрицы = перестановка уравнений, и rhs переставляется так же.
3. После получения solution — переставить обратно: `[δP, δSw]` → `[δSw, δP]`

**Стоп — это некорректно.** CPR берёт `col % B == 0` для давления. Если переставить столбцы (Sw↔P), то `col % 2 == 0` = P. Но перестановка столбцов = перестановка переменных в решении. А перестановка строк = перестановка уравнений. Нужно менять и строки, и столбцы.

### Детальная перестановка для B=2

Для каждой ячейки `l` со строками `(2l, 2l+1)` и для каждого блока `(2j, 2j+1)`:

Исходный блок (row-major в CRS):
```
A[2l,   2j]   A[2l,   2j+1]     d(oil)/d(Sw)   d(oil)/d(P)
A[2l+1, 2j]   A[2l+1, 2j+1]     d(water)/d(Sw) d(water)/d(P)
```

После перестановки строк (2l↔2l+1) И столбцов (2j↔2j+1):
```
A'[2l,   2j]   A'[2l,   2j+1]     d(water)/d(P) d(water)/d(Sw)
A'[2l+1, 2j]   A'[2l+1, 2j+1]     d(oil)/d(P)   d(oil)/d(Sw)
```

Теперь `col % 2 == 0` = P (столбец 0 нового блока). CPR построит матрицу давления App из элементов `A'[i, col % 2 == 0]` = `d(eq)/d(P)` — корректно!

### Реализация перестановки

В CRS-формате элементы хранятся по строкам. Перестановка строк `2l` ↔ `2l+1` при B=2:

```cpp
void swap_block_rows_cols(
    size_t np,                          // число блоков = cellNmbr
    const std::vector<size_t>& row_in,  // размер 2*np + 1
    const std::vector<size_t>& col_in,  // размер nnz
    const std::vector<double>& val_in,  // размер nnz
    std::vector<size_t>& row_out,
    std::vector<size_t>& col_out,
    std::vector<double>& val_out)
```

Для B=2, перестановка внутри каждого 2×2 блока:
- Строка `2l` содержит [oil_eq для всех соседей]
- Строка `2l+1` содержит [water_eq для всех соседей]

Swap строк = поменять местами содержимое строк `2l` и `2l+1` в CRS.
Swap столбцов = для каждого элемента: если `col % 2 == 0` → `col + 1`, если `col % 2 == 1` → `col - 1`.

**Внимание:** swap строк в CRS нетривиален — нужно переставить элементы в `col[]` и `val[]`, потому что строки `2l` и `2l+1` могут иметь разное число ненулевых элементов.

**Упрощение для нашего случая:** обе строки `2l` и `2l+1` имеют ОДИНАКОВУЮ структуру (same sparsity pattern) — это два уравнения для одной ячейки, обе зависят от давления и насыщенности в тех же соседних ячейках. Значит `row[2l+1] - row[2l] == row[2l+2] - row[2l+1]` для всех `l`.

В этом случае swap строк = swap парами `(val[row[2l]+k], val[row[2l+1]+k])` и `(col[row[2l]+k], col[row[2l+1]+k])` для всех `k`.

Swap столбцов (после swap строк): для каждого элемента `col[i]`: `col[i] ^= 1` (XOR 1 меняет 0↔1, 2↔3, ...).

**Для rhs и solution:** аналогично. `rhs[2l]` ↔ `rhs[2l+1]`, `x[2l]` ↔ `x[2l+1]`.

### Стоимость перестановки

- Матрица: O(nnz) = ~292K элементов = ~2.3 MB. Один проход = ~0.3 мс.
- Rhs/solution: O(2 × cellNmbr) = ~21K элементов. ~0.02 мс.
- Per Newton: 2 × 0.3 мс = 0.6 мс (матрица + обратная перестановка solution).
- Per benchmark: 1112 × 0.6 мс = ~0.7 с.
- ~2% от current total (43.7 с). Приемлемо.

**Альтернатива:** можно перестановку делать ОДИН раз (переставленные row/col/val как отдельные векторы в LinearProblem) и пересобирать каждый Newton iteration. Стоимость та же.

**Ещё лучше:** NOT копировать матрицу, а передать CPR адаптер, который переставляет row/col на лету. Но amgcl принимает CRS tuple `std::tie(n, row, col, val)` и копирует в internal `build_matrix`. Перестановку нужно делать до передачи.

## Этапы реализации

### Этап 0: Проверка гипотезы — CPR с Sw как «давление» (без перестановки)

Прежде чем тратить усилия на перестановку — проверить, что будет, если CPR построит AMG для Sw. Может сработать: уравнение нефти содержит лапласиан давления, и Sw-столбцы содержат производные подвижности — AMG может сойтись, просто менее эффективно.

Реализация: добавить CPR-конфигурацию в бенчмарк БЕЗ перестановки. Если сходится за разумное число итераций — перестановка не критична. Если расходится или iters >> 20 — перестановка обязательна.

Файл: `test_amgcl_benchmark.cpp`, серия `[seriesCPR]`.

### Этап 1: CPR benchmark (скалярный backend, без перестановки)

#### Типы

```cpp
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>

using SBackend = amgcl::backend::builtin<double>;

// CPR: AMG для «давления» (или Sw) + ILU для полной системы
using PPrecond_ilu0 = amgcl::amg<SBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>;
using SPrecond_ilu0 = amgcl::relaxation::as_preconditioner<SBackend, amgcl::relaxation::ilu0>;
using CPR_ilu0 = amgcl::preconditioner::cpr<PPrecond_ilu0, SPrecond_ilu0>;
using CPRSolver_ilu0 = amgcl::make_solver<CPR_ilu0, amgcl::solver::lgmres<SBackend>>;

// Аналогично для iluk, и для cpr_drs
```

#### solve_with для CPR (скалярный backend)

CPR работает со скалярным backend. Нельзя использовать `block_matrix` адаптер. Нужна отдельная функция (или специализация):

```cpp
template<typename SolverType>
SolveResult solve_with_scalar(LinearProblem& lp, int maxIter, typename SolverType::params& prm)
{
    prm.solver.maxiter = maxIter;

    auto n = lp.RhsSize();          // cellNmbr * 2
    auto const& row = lp.Matrix().Row();
    auto const& col = lp.Matrix().Col();
    auto const& val = lp.Matrix().Val();

    auto ta = clock::now();
    SolverType solve(std::tie(n, row, col, val), prm);
    auto setup_time = clock::now() - ta;

    // Скалярные rhs и solution — копии из LinearProblem
    std::vector<double> F(lp.Rhs().begin(), lp.Rhs().end());
    std::vector<double> X(lp.SolutionCorrections().begin(), lp.SolutionCorrections().end());

    auto ts = clock::now();
    auto [iters, error] = solve(F, X);
    auto solve_time = clock::now() - ts;

    std::copy(X.begin(), X.end(), lp.SolutionCorrections().begin());
    return { iters, error, true };
}
```

**Подводный камень:** `lp.Rhs()` и `lp.SolutionCorrections()` — ссылки на внутренние вектора. Копии F и X нужны, потому что AMGCL может модифицировать rhs при solve.

#### Конфигурации бенчмарка

| ID | Тип | P-relax | S-relax | Перестановка |
|---|---|---|---|---|
| CPR1 | cpr | ilu0 | ilu0 | нет |
| CPR2 | cpr | iluk(1) | ilu0 | нет |
| CPR3 | cpr_drs | ilu0 | ilu0 | нет |
| CPR4 | cpr_drs | iluk(1) | ilu0 | нет |
| ILU_S1 | as_preconditioner | ilu0 | — | — |
| ILU_S2 | as_preconditioner | iluk(1) | — | — |

ILU_S1/S2 — нижняя граница: если голый ILU (без AMG-стадии) не хуже CPR, то AMG-стадия бесполезна.

#### Параметры CPR

```cpp
CPRSolver::params prm;
prm.precond.block_size = 2;
prm.precond.active_rows = 0;  // все строки
prm.solver.tol = 1e-5;
prm.solver.abstol = 1e-5;
prm.solver.maxiter = 100;
prm.solver.M = 15;
prm.solver.K = 5;
```

### Этап 2: CPR benchmark с перестановкой

Если этап 1 показал деградацию сходимости — добавить перестановку.

#### Функция перестановки

```cpp
// Переставить строки/столбцы CRS: [Sw,P] → [P,Sw]
// Для B=2: swap строк 2l ↔ 2l+1, swap столбцов col ^= 1
void swap_variable_order(
    size_t n,                            // = rhsSize = cellNmbr * 2
    const std::vector<size_t>& row_in,
    const std::vector<size_t>& col_in,
    const std::vector<double>& val_in,
    std::vector<size_t>& row_out,
    std::vector<size_t>& col_out,
    std::vector<double>& val_out)
{
    size_t np = n / 2;
    row_out.resize(n + 1);
    col_out.resize(col_in.size());
    val_out.resize(val_in.size());

    row_out[0] = 0;
    for (size_t ip = 0; ip < np; ++ip) {
        size_t r0 = 2 * ip;      // old row 0 (oil eq)
        size_t r1 = 2 * ip + 1;  // old row 1 (water eq)

        size_t len0 = row_in[r0 + 1] - row_in[r0];
        size_t len1 = row_in[r1 + 1] - row_in[r1];

        // New row r0 ← old row r1 (water eq → new row 0 = "pressure eq")
        row_out[r0 + 1] = row_out[r0] + len1;
        // New row r1 ← old row r0 (oil eq → new row 1)
        row_out[r1 + 1] = row_out[r0 + 1] + len0;

        size_t dst0 = row_out[r0];
        size_t src1 = row_in[r1];
        for (size_t k = 0; k < len1; ++k) {
            col_out[dst0 + k] = col_in[src1 + k] ^ 1;  // swap col within block
            val_out[dst0 + k] = val_in[src1 + k];
        }

        size_t dst1 = row_out[r0 + 1];
        size_t src0 = row_in[r0];
        for (size_t k = 0; k < len0; ++k) {
            col_out[dst1 + k] = col_in[src0 + k] ^ 1;
            val_out[dst1 + k] = val_in[src0 + k];
        }
    }
}
```

**Подводный камень:** `col ^= 1` переставляет столбцы ВНУТРИ каждого 2×2 блока. Но столбцы в CRS должны быть отсортированы в каждой строке! После `col ^= 1` пары `(2j, 2j+1)` меняются на `(2j+1, 2j)`, что НАРУШАЕТ сортировку.

**Решение:** `col ^= 1` меняет 2j↔2j+1. Пара `(2j, 2j+1)` → `(2j+1, 2j)`. Нужно ещё поменять местами соответствующие val элементы:

Для каждого блока в строке, элементы идут парами: `(col=2j, val=a0), (col=2j+1, val=a1)` → после swap `(col=2j+1, val=a0), (col=2j, val=a1)` → sort: `(col=2j, val=a1), (col=2j+1, val=a0)`.

Значит достаточно swap val внутри каждой пары, WITHOUT меняя col:

```cpp
// Для каждой строки, swap val-пар:
for (size_t k = 0; k < len; k += 2) {
    col_out[dst + k]     = col_in[src + k];      // col не меняем!
    col_out[dst + k + 1] = col_in[src + k + 1];
    val_out[dst + k]     = val_in[src + k + 1];   // swap val
    val_out[dst + k + 1] = val_in[src + k];
}
```

**Но есть нюанс:** пары `(2j, 2j+1)` предполагают, что столбцы идут парами. Это верно для нашего blPattern (полный 2×2 блок), но нужно проверить.

**Проверка:** SparsityPattern хранит столбцы в отсортированном порядке. Для каждой строки, столбцы = все `2j` и `2j+1` для ячейки `j` = self + neighbours. Каждый блок 2×2 даёт 2 столбца `(2j, 2j+1)`, идущих подряд. Гарантировано.

#### Перестановка rhs и solution

```cpp
// [oil_rhs_0, water_rhs_0, oil_rhs_1, water_rhs_1, ...] →
// [water_rhs_0, oil_rhs_0, water_rhs_1, oil_rhs_1, ...]
void swap_pairs(std::vector<double>& v) {
    for (size_t i = 0; i < v.size(); i += 2)
        std::swap(v[i], v[i+1]);
}
```

#### solve_with для CPR с перестановкой

```cpp
template<typename SolverType>
SolveResult solve_with_cpr_swapped(LinearProblem& lp, int maxIter, typename SolverType::params& prm)
{
    prm.solver.maxiter = maxIter;

    auto n = lp.RhsSize();
    // Перестановка матрицы
    auto row_s = swap_matrix_rows(lp.Matrix().Row(), ...);
    // ... (см. реализацию выше)

    SolverType solve(std::tie(n, row_s, col_s, val_s), prm);

    std::vector<double> F(lp.Rhs().begin(), lp.Rhs().end());
    swap_pairs(F);  // rhs: [oil,water] → [water,oil]

    std::vector<double> X(n, 0.0);

    auto [iters, error] = solve(F, X);

    swap_pairs(X);  // solution: [δP,δSw] → [δSw,δP]
    std::copy(X.begin(), X.end(), lp.SolutionCorrections().begin());
    return { iters, error, true };
}
```

### Этап 3: Анализ результатов

Сравнить:

| Метрика | Block AMG (current) | CPR naive | CPR swapped | CPR-DRS | ILU only |
|---|---|---|---|---|---|
| avg iters/solve | 8.3 | ? | ? | ? | ? |
| t_total | 43.7 с | ? | ? | ? | ? |
| t_solve | 36.6 с | ? | ? | ? | ? |
| balance_ok | YES | ? | ? | ? | ? |

Ожидания:
- CPR naive (Sw as «pressure»): iters > 15 или divergence
- CPR swapped (P as pressure): iters 4–6
- CPR-DRS: автоматически адаптирует, но всё равно `col % B == 0` для App
- ILU only: iters ~15–20 (нижняя граница, без AMG)

### Этап 4: CPR в production (если выигрыш > 15%)

Если CPR swapped даёт t_total < 37 с (–15%):

1. Добавить CPR-типы в `LinearProblem.h`
2. Добавить `SolveCPR()` метод с перестановкой встроенной
3. `Solve()` вызывает `SolveCPR()`
4. Compile-time переключение: `#ifdef USE_CPR`

### Этап 5: partial_update (если CPR принят)

CPR имеет `partial_update(K, update_transfer_ops)`:
- `update_transfer_ops = false`: обновить только ILU, AMG-иерархия давления остаётся
- `update_transfer_ops = true`: обновить ILU + трансферные операторы (Fpp, App)

Архитектура:
```cpp
class LinearProblem {
    std::unique_ptr<CPRSolver> solver_;
    bool needs_full_rebuild_ = true;

    SolveResult Solve(int maxIter) {
        auto [row_s, col_s, val_s] = swap_matrix(Matrix());
        auto K = std::tie(rhsSize, row_s, col_s, val_s);

        if (needs_full_rebuild_ || !solver_) {
            solver_ = std::make_unique<CPRSolver>(K, prm);
            needs_full_rebuild_ = false;
        } else {
            solver_->precond().partial_update(K, false);
        }

        // swap rhs, solve, swap solution back
    }
};
```

**Подводный камень:** `partial_update` пересоздаёт ILU (`std::make_shared<SPrecond>(K_ptr, prm.sprecond, bprm)`) — это O(nnz). AMG-иерархия остаётся. На скалярном backend (cellNmbr неизвестных для AMG vs 2*cellNmbr для ILU) — setup AMG дешёвый.

**Когда пересоздавать полностью:**
- Wasted trial (откат шага): `InvalidateSetup()`
- Деградация (iters > 2× avg за последние 5 шагов)

**Когда partial_update с `update_transfer_ops = true`:**
- Начало нового временного шага (структура матрицы та же, значения изменились)

**Когда partial_update с `update_transfer_ops = false`:**
- Последующие Newton-итерации внутри одного шага

## Подводные камни и рекомендации

### 1. Сортировка столбцов после перестановки

CRS требует отсортированных столбцов в каждой строке. При перестановке `col ^= 1` сортировка нарушается. Решение: swap val внутри каждой пары `(2j, 2j+1)`, col не трогать. Это работает ТОЛЬКО если каждый блок полный (оба столбца `2j` и `2j+1` присутствуют).

Проверить: `blPattern` в `MatrixCSR` конструкторе. Если `blPattern = vector<bool>(true, B*B)` — все элементы блока присутствуют.

### 2. reinterpret_cast для rhs/solution

Текущий блочный солвер использует `reinterpret_cast<rhs_type<B>*>` для перепаковки rhs в `static_matrix<2,1>`. CPR со скалярным backend этого не требует — rhs и solution = `std::vector<double>`.

### 3. Компиляция iluk с скалярным backend

`amgcl::relaxation::iluk` может не работать со скалярным backend. Проверить компиляцию. Если не компилируется — использовать ilu0 для P-precond.

### 4. AMGCL_PROFILING + OpenMP

`amgcl::profiler` не thread-safe. CPR использует `#pragma omp parallel` внутри `first_scalar_pass`. Если `AMGCL_PROFILING` включён, возможны race conditions в profiler. Наши chrono-таймеры работают корректно — не зависят от profiler.

### 5. CRS copy в конструкторе CPR

Строка 118 в `cpr.hpp`: `init(std::make_shared<build_matrix>(K), ...)`. Конструктор копирует входную матрицу K. Это O(nnz). Для partial_update — тоже копия (строка 165). Дополнительная стоимость ~0.3 мс/solve — пренебрежимо.

### 6. Scatter operator

CPR scatter записывает коррекцию давления в строку с `i == 0` блока (строка 371 cpr.hpp: `if (i == 0) ++nnz`). После перестановки `i == 0` = water equation. Scatter добавляет `xp` к `x[2l]` — это перестановленный порядок, где `x[2l]` = δP. Корректно.

### 7. Verify: balance и physical constraints

После каждой конфигурации CPR проверять:
- `balance_ok` (oil_rel < 1e-3, water_rel < 1e-3)
- Sw ∈ [0, 1], P > 0
- Число шагов = 187 (то же, что у блочного AMG)
- Wasted trials = 0

### 8. Стоимость перестановки vs выигрыш

Перестановка = ~0.7 с per benchmark (1112 × 0.6 мс). Если CPR экономит 10 с на solve — чистый выигрыш ~9.3 с. Если CPR не быстрее — откатить.

## Ожидаемый эффект

Оптимистичный: CPR с correct pressure → iters 4–6 (vs 8.3), setup скалярного AMG дешевле блочного → t_solve ~22–28 с → t_total ~27–33 с (–25–38%).

Пессимистичный: CPR не даёт выигрыша по итерациям, setup дороже из-за двух прекондиционеров → t_solve ~38–42 с → не стоит.

## Порядок действий

1. Добавить CPR types в benchmark (скалярный backend)
2. Реализовать `solve_with_scalar<CPRSolver>()`
3. Запустить CPR1–CPR4 БЕЗ перестановки (этап 0/1)
4. Если сходится плохо → реализовать перестановку → CPR5–CPR8 С перестановкой
5. Если выигрыш > 15% → CPR в production (этап 4)
6. partial_update (этап 5) — последний, только если CPR принят

## Коммиты

```
perf: CPR benchmark — серия [seriesCPR], скалярный backend
perf: перестановка переменных [Sw,P]→[P,Sw] для CPR
perf: CPR в production — замена блочного AMG
perf: partial_update — reuse AMG-иерархии давления
```

## Связанные заметки

- [[prompt-оптимизация-06-CPR-прекондиционер]] — исходный промпт
- [[2026-06-21 сессия 5 оптимизация assembly]] — текущий baseline
- [[amgcl конфигурация iluk k1 новый оптимум]] — текущий оптимум
- [[возможности amgcl для блочных СЛАУ]] — справочник API
