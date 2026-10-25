---
tags:
  - промпт
  - производительность
  - amgcl
  - cpr
date: 2026-06-21
---

# Сессия 6: CPR-прекондиционер

## Предварительно

Прочитай:
- `vault/GDM/00-home/текущие приоритеты.md`
- `vault/GDM/knowledge/decisions/amgcl конфигурация iluk k1 новый оптимум.md`
- `vault/GDM/sessions/prompt-оптимизация-03-CPR-прекондиционер.md` — API amgcl для CPR

## Контекст

Текущий солвер: блочный AMG 2×2 (`amg<aggregation, iluk> + lgmres`). AMG работает с полной блочной системой давление–насыщенность. Проблема: AMG хорошо подходит для эллиптической подсистемы (давление), но бесполезен для гиперболической (насыщенность). CPR разделяет задачу:

- **S-precond (ILU)**: сглаживает всю систему — одна итерация ILU для полной матрицы
- **P-precond (AMG)**: решает скалярную подсистему давления — AMG без блочного overhead

### Ключевая разница с текущим подходом

Текущий: AMG строит иерархию для блочной 2×2 матрицы → coarsening оперирует с `static_matrix<2,2>`, все уровни блочные.

CPR: AMG строит иерархию для скалярной матрицы давления (size = cellNmbr, не cellNmbr×2) → вдвое меньше неизвестных на каждом уровне, нет блочного overhead. Плюс ILU для полной системы сглаживает высокочастотные ошибки обоих уравнений.

## API amgcl

### Типы

```cpp
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>

using SBackend = amgcl::backend::builtin<double>;

// P-precond: скалярный AMG для давления
using PPrecond = amgcl::amg<
    SBackend,
    amgcl::coarsening::aggregation,
    amgcl::relaxation::ilu0        // или iluk
>;

// S-precond: ILU для полной системы (скалярный backend!)
using SPrecond = amgcl::relaxation::as_preconditioner<
    SBackend,
    amgcl::relaxation::ilu0
>;

// CPR-прекондиционер
using CPR = amgcl::preconditioner::cpr<PPrecond, SPrecond>;

// Полный солвер
using CPRSolver = amgcl::make_solver<CPR, amgcl::solver::lgmres<SBackend>>;
```

### Передача матрицы — СКАЛЯРНАЯ CRS

CPR работает со скалярной (развёрнутой) матрицей. Наша `MatrixCSR` хранит данные именно в скалярном формате — `Row()`, `Col()`, `Val()` уже содержат развёрнутые индексы с `B×B` блоками. Размерность: `n = rhsSize = cellNmbr × B`.

```cpp
size_t n = lp.RhsSize();          // cellNmbr * 2
auto& row = lp.Matrix().Row();    // size n+1
auto& col = lp.Matrix().Col();    // nnz
auto& val = lp.Matrix().Val();    // nnz
CPRSolver solve(std::tie(n, row, col, val), prm);
```

**НЕ использовать `block_matrix` адаптер** — CPR сам разделяет переменные через `block_size`.

### Параметры

```cpp
CPRSolver::params prm;
prm.precond.block_size = 2;      // B = 2 (pressure + saturation)
prm.precond.active_rows = 0;     // 0 = все строки
prm.precond.pprecond.coarsening.aggr.eps_strong = 0.0; // может понадобиться
prm.solver.tol = 1e-5;
prm.solver.abstol = 1e-5;
prm.solver.maxiter = 100;
prm.solver.M = 15;               // lgmres inner
prm.solver.K = 3;                // lgmres augmented
```

### partial_update — reuse AMG-иерархии

CPR имеет встроенную поддержку partial_update:
```cpp
// Newton-итерация: обновить только ILU, AMG-иерархия давления остаётся
solver.precond().partial_update(K, /* update_transfer_ops = */ false);

// Новый временной шаг: обновить ILU + трансферные операторы
solver.precond().partial_update(K, /* update_transfer_ops = */ true);
```

Это даёт reuse AMG бесплатно, без ручного unique_ptr (шаг 3 из сессии 4).

### RHS и solution — скалярные double*

```cpp
auto [iters, error] = solve(rhs, x);  // std::vector<double>
```

Не нужна перепаковка в `rhs_type<B>` / `numa_vector`.

## Этап 1: CPR в benchmark

### Конфигурации

Добавить в `test_amgcl_benchmark.cpp` серию `[seriesCPR]`:

| ID | Описание | P-precond relax | S-precond relax |
|---|---|---|---|
| CPR1 | cpr + ilu0/ilu0 | ilu0 | ilu0 |
| CPR2 | cpr + iluk(1)/ilu0 | iluk(k=1) | ilu0 |
| CPR3 | cpr + ilu0/iluk(1) | ilu0 | iluk(k=1) |
| CPR4 | cpr + dj/ilu0 | damped_jacobi | ilu0 |

Также протестировать CPR-DRS:

| ID | Описание |
|---|---|
| DRS1 | cpr_drs + ilu0/ilu0 |
| DRS2 | cpr_drs + iluk(1)/ilu0 |

И голый ILU (нижняя граница — если CPR не быстрее, AMG-стадия бесполезна):

| ID | Описание |
|---|---|
| ILU_S1 | as_preconditioner<ilu0> (скалярный) + lgmres |
| ILU_S2 | as_preconditioner<iluk> (скалярный) + lgmres |

### Реализация solve_with для CPR

CPR работает со скалярным backend — `solve_with` нужно адаптировать:

```cpp
template<>
SolveResult solve_with<CPRSolver>(LinearProblem& lp, int maxIter, CPRSolver::params& prm)
{
    prm.solver.maxiter = maxIter;

    size_t n = lp.RhsSize();
    auto const& row = lp.Matrix().Row();
    auto const& col = lp.Matrix().Col();
    auto const& val = lp.Matrix().Val();

    prof.tic("setup");
    CPRSolver solve(std::tie(n, row, col, val), prm);
    prof.toc("setup");

    // Скалярные векторы — не нужна перепаковка в rhs_type<B>
    std::vector<double> F(lp.Rhs().begin(), lp.Rhs().end());
    std::vector<double> X(lp.SolutionCorrections().begin(), lp.SolutionCorrections().end());

    prof.tic("solve");
    auto [iters, error] = solve(F, X);
    prof.toc("solve");

    std::copy(X.begin(), X.end(), lp.SolutionCorrections().begin());
    return { iters, error, true };
}
```

Или лучше: отдельная функция `solve_with_scalar<SolverType>()`.

### Порядок переменных в MatrixCSR

**КРИТИЧЕСКАЯ ПРОВЕРКА:** CPR предполагает, что переменные чередуются: `[P₀, S₀, P₁, S₁, ..., Pₙ, Sₙ]` при `block_size=2`. Нужно проверить, что наша MatrixCSR использует именно такой порядок.

В `fillMatrixBlockRow` (ReservoirSimulator.cpp):
```cpp
rhsBlock[0] = (prevMass[0] - cell.OilMass()) / loc_tau;   // уравнение 0 для ячейки l
rhsBlock[1] = (prevMass[1] - cell.WaterMass()) / loc_tau; // уравнение 1 для ячейки l
```

И в `blDiag`:
```cpp
blDiag[0] → d(eq0)/d(var0) = d(oil_mass_eq)/d(S_w)
blDiag[1] → d(eq0)/d(var1) = d(oil_mass_eq)/d(P)
blDiag[2] → d(eq1)/d(var0) = d(water_mass_eq)/d(S_w)
blDiag[3] → d(eq1)/d(var1) = d(water_mass_eq)/d(P)
```

Блочная матрица [eq0_var0, eq0_var1; eq1_var0, eq1_var1] = [dOil/dSw, dOil/dP; dWater/dSw, dWater/dP].

В скалярной CRS строка `2*l` — уравнение нефти для ячейки l, строка `2*l+1` — уравнение воды.

CPR с `block_size=2` будет считать `var[2*l]` — первая переменная (S_w), `var[2*l+1]` — вторая (P).

**Для CPR "давление" — это переменная с индексом 1 внутри блока (нечётные строки/столбцы).** По умолчанию CPR извлекает первую переменную блока (индекс 0) как "давление". Если порядок [Sw, P], то нужно либо:
1. Задать `prm.precond.block_size = 2` и оставить default — CPR возьмёт Sw как "давление" (неправильно!).
2. Поменять порядок переменных (Sw, P) → (P, Sw) — большой рефакторинг.
3. Проверить, есть ли параметр `pressure_variable` или аналог в amgcl CPR.

**Исследовать это ПЕРВЫМ ДЕЛОМ.** Прочитать `cpr.hpp` — метод `first_scalar_pass`:
```cpp
np = N / B;  // число "давленческих" неизвестных
// ... строит матрицу давления из строк с индексами 0, B, 2B, ...
```

Если CPR берёт строки 0, 2, 4, ... — это строки Sw (уравнение нефти). Для reservoir simulation нужно, чтобы AMG работал с давлением, а не насыщенностью. Варианты:
- Поменять порядок (var0 = P, var1 = Sw) — переписать fillMatrixBlockRow
- Использовать `cpr_drs` — он адаптивно определяет "давленческие" строки
- Или проверить, что CPR с Sw тоже работает (Sw-уравнение содержит лапласиан давления)

### Верификация

1. Сборка, все 35 тестов (production solver не меняется)
2. Запуск `[seriesCPR]`
3. Сравнение с текущим оптимумом (iluk + lgmres, ~75 с)

## Этап 2: CPR в production (если выигрыш > 15%)

### Изменения в LinearProblem

1. Добавить `CPRSolver` typedef в `LinearProblem.h`
2. Добавить `SolveCPR()` метод
3. `Solve()` вызывает `SolveCPR()` или блочный AMG в зависимости от compile-time define

Или: заменить `Solver_AMG` на CPR-тип целиком.

### Изменения в benchmark

Обновить `solve_with` — для CPR-типов использовать скалярную передачу матрицы.

## Этап 3: partial_update (если CPR принят)

### Архитектура

```cpp
class LinearProblem {
    std::unique_ptr<CPRSolver> solver_;
    bool needs_full_rebuild_ = true;
    bool needs_transfer_update_ = true;

public:
    void InvalidateSetup() { needs_full_rebuild_ = true; }
    void InvalidateTransfer() { needs_transfer_update_ = true; }

    SolveResult Solve(int maxIter) {
        auto K = std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val());
        if (needs_full_rebuild_ || !solver_) {
            solver_ = std::make_unique<CPRSolver>(K, prm);
            needs_full_rebuild_ = false;
            needs_transfer_update_ = false;
        } else if (needs_transfer_update_) {
            solver_->precond().partial_update(K, true);
            needs_transfer_update_ = false;
        } else {
            solver_->precond().partial_update(K, false);
        }
        auto [iters, error] = (*solver_)(rhs, solutionCorrections);
        return { iters, error, true };
    }
};
```

Вызовы из `ReservoirSimulator`:
- Начало нового временного шага: `MyProblem.InvalidateTransfer()`
- Wasted trial (откат): `MyProblem.InvalidateSetup()`
- Деградация (iters > 2× avg): `MyProblem.InvalidateSetup()`

## Риски

1. **Порядок переменных** — CPR может взять не ту переменную как "давление". Это killer — AMG на насыщенности не сойдётся. Решение: проверить first, потом код.
2. **Скалярный backend** — теряем SIMD-преимущество блочных операций (static_matrix<2,2>). На малых сетках может не окупиться.
3. **Setup cost** — CPR строит AMG для давления + ILU для полной → setup дороже. Окупается за счёт меньшего числа итераций.
4. **Совместимость iluk с скалярным backend** — iluk в amgcl может работать иначе со скалярами, проверить компиляцию.

## Ожидаемый эффект

Если CPR работает корректно и порядок переменных правильный:
- Итерации: 8.3 → 4–6 (CPR эффективнее для P-S систем)
- Время: зависит от баланса setup/solve. Оптимистично: –20–30%.

## Коммиты

```
perf: CPR benchmark — серия [seriesCPR], скалярный backend
perf: CPR в production — замена блочного AMG на CPR-прекондиционер
perf: partial_update — reuse AMG-иерархии давления
```

## Связанные заметки

- [[prompt-оптимизация-03-CPR-прекондиционер]] — справка по API
- [[amgcl конфигурация iluk k1 новый оптимум]]
- [[возможности amgcl для блочных СЛАУ]]
