---
tags:
  - промпт
  - производительность
  - amgcl
  - cpr
date: 2026-06-21
---

# Фаза 3: CPR-прекондиционер (Constrained Pressure Residual)

## Цель

Реализовать CPR — стандартный двухстадийный прекондиционер для reservoir simulation. Потенциально лучше чем блочный AMG для систем давление–насыщенность, потому что разделяет эллиптическую (давление) и гиперболическую (насыщенность) части.

## Мотивация

Текущий подход: блочный AMG 2×2 (`amg<aggregation, ilu0> + lgmres`) — AMG применяется к полной блочной системе. AMG хорошо работает для эллиптических задач, но блочная система P–S содержит и гиперболическую часть (транспорт S). CPR решает это:

1. **Первая стадия** (S-precond): ILU для полной системы — сглаживает высокочастотные ошибки обоих уравнений
2. **Вторая стадия** (P-precond): AMG для извлечённой скалярной подматрицы давления — убирает низкочастотные ошибки давления

Этот подход используется в ECLIPSE, OPM Flow, и других промышленных симуляторах.

## API amgcl для CPR

### Ключевое ограничение: CPR работает со скалярной матрицей

```cpp
static_assert(
    math::static_rows<typename PPrecond::backend_type::value_type>::value == 1,
    "Pressure backend should have scalar value type!"
);
```

CPR принимает полную скалярную (развёрнутую) CRS-матрицу и сам разделяет переменные через `block_size`. Наш `Matrix().Val()` уже хранит скалярные double — нужно передать без `block_matrix` адаптера.

### Типы

```cpp
using SBackend = amgcl::backend::builtin<double>;

// P-precond: скалярный AMG для давления
using PPrecond = amgcl::amg<
    SBackend,
    amgcl::coarsening::aggregation,
    amgcl::relaxation::ilu0
>;

// S-precond: ILU для полной системы
using SPrecond = amgcl::relaxation::as_preconditioner<
    SBackend,
    amgcl::relaxation::ilu0
>;

// CPR-прекондиционер
using CPR = amgcl::preconditioner::cpr<PPrecond, SPrecond>;

// Полный солвер
using CPRSolver = amgcl::make_solver<CPR, amgcl::solver::lgmres<SBackend>>;
```

### Параметры

```cpp
CPRSolver::params prm;
prm.precond.block_size = 2;      // размер блока (P + S)
prm.precond.active_rows = 0;     // 0 = все строки
// pprecond — параметры AMG для давления
// sprecond — параметры ILU для полной системы
prm.solver.tol = 1e-5;
prm.solver.abstol = 1e-5;
```

### Передача матрицы

```cpp
// Скалярный CRS — прямо из нашей MatrixCSR:
auto& row = sim.MyProblem.Matrix().Row();  // std::vector<size_t>
auto& col = sim.MyProblem.Matrix().Col();  // std::vector<size_t>
auto& val = sim.MyProblem.Matrix().Val();  // std::vector<double>
size_t n = sim.MyProblem.RhsSize();        // = cellNmbr * B

// НЕ использовать block_matrix адаптер!
CPRSolver solve(std::tie(n, row, col, val), prm);
auto [iters, error] = solve(rhs, x);
```

### partial_update() — reuse AMG-иерархии

CPR имеет встроенную поддержку reuse:

```cpp
// При каждой Newton-итерации — обновить только ILU:
solver.precond().partial_update(K, /* update_transfer_ops = */ false);

// При новом временном шаге — обновить ILU + трансферный оператор:
solver.precond().partial_update(K, /* update_transfer_ops = */ true);

// Полная пересборка — только при деградации сходимости
```

## CPR-DRS (Dynamic Row Summing)

Вариант CPR с адаптивным извлечением уравнения давления. Вместо фиксированного `block_size` — использует diagonal dominance и pressure sum criteria:

```cpp
using CPR_DRS = amgcl::preconditioner::cpr_drs<PPrecond, SPrecond>;

// Дополнительные параметры:
prm.precond.eps_dd = 0.2;   // порог diagonal dominance
prm.precond.eps_ps = 0.02;  // порог pressure sum
```

## Schur Pressure Correction

Третий вариант — прекондиционер на основе дополнения Шура. Требует явного разделения на U-блок и P-блок:

```cpp
using SPC = amgcl::preconditioner::schur_pressure_correction<USolver, PSolver>;
```

Более сложная интеграция, требует указания индексов давленческих переменных.

## План реализации

### Этап 1: CPR в benchmark

1. Добавить CPR-конфигурацию в `test_amgcl_benchmark.cpp` как отдельную серию `[seriesCPR]`
2. CPR использует скалярный CRS напрямую (без `block_matrix`)
3. Сравнить с текущим оптимумом (блочный AMG + ilu0 + lgmres)

### Этап 2: CPR в production (если выигрыш > 20%)

1. Добавить CPR-тип в `LinearProblem.h`
2. Реализовать `SolveCPR()` рядом с `Solve()`
3. Переключение через параметр или compile-time typedef

### Этап 3: partial_update (если CPR принят)

1. Хранить `CPRSolver` как `unique_ptr` member в `LinearProblem`
2. `partial_update(K, false)` на каждой Newton-итерации
3. `partial_update(K, true)` при новом временном шаге
4. Полная пересборка — при деградации (iters > 2× avg)

### Конфигурации для benchmark

| ID | Прекондиционер | P-precond | S-precond | Solver |
|---|---|---|---|---|
| CPR1 | cpr | amg<aggr, ilu0> | as_preconditioner<ilu0> | lgmres |
| CPR2 | cpr | amg<aggr, damped_jacobi> | as_preconditioner<ilu0> | lgmres |
| CPR3 | cpr_drs | amg<aggr, ilu0> | as_preconditioner<ilu0> | lgmres |
| CPR4 | cpr | amg<aggr, ilu0> | as_preconditioner<ilu0> | bicgstab |

### Также: голый ILU (без AMG)

| ID | Прекондиционер | Solver |
|---|---|---|
| ILU1 | as_preconditioner<ilu0> (скалярный) | lgmres |
| ILU2 | as_preconditioner<ilu0> (скалярный) | bicgstab |

Голый ILU — нижняя граница: если CPR не быстрее голого ILU, значит AMG-стадия бесполезна.

## Риски

- CPR работает со скалярной матрицей → теряем преимущество блочных операций (SIMD на 2×2 блоках)
- CPR setup дороже: строит AMG для давления + ILU для полной → setup может вырасти
- Выигрыш зависит от того, насколько давление доминирует в ошибке. На маленьких сетках разница может быть незаметна

## Зависимости

- Фаза 1 (перебор конфигураций) должна быть завершена → ✅
- Профилирование через amgcl::profiler → ✅
- Очистка benchmark-артефактов (prompt-очистка-benchmark-артефактов) — желательно, но не блокирует

## Связанные заметки

- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[возможности amgcl для блочных СЛАУ]]
- [[prompt-оптимизация-02-структурные-оптимизации]]
- [[2026-06-21 переход на amgcl profiler]]
