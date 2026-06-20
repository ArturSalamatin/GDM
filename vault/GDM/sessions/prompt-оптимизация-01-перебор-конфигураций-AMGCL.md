---
tags:
  - промпт
  - производительность
  - amgcl
  - перебор
date: 2026-06-20
updated: 2026-06-20
---

# Фаза 1: Систематический перебор конфигураций AMGCL

## Цель

Найти оптимальную комбинацию (Krylov solver, coarsening, relaxation, параметры) для блочной СЛАУ 2×2 на 3D-сетке 51×51×4.

## Реализация: единый test_amgcl_benchmark.cpp с SECTION-ами

Создать файл `tests/test_amgcl_benchmark.cpp` с одним TEST_CASE и множеством SECTION-ов. Каждая секция — одна конфигурация AMGCL. Все шаблоны инстанцируются в одном бинарнике (одна компиляция, ~60 сек на MSVC).

### Архитектура бенчмарка

```cpp
#include <catch2/catch_test_macros.hpp>
#include "simulation_cases/MultiLayerCase.h"
#include <chrono>
#include <fstream>

// Все необходимые amgcl includes
#include <amgcl/make_solver.hpp>
#include <amgcl/amg.hpp>
#include <amgcl/solver/gmres.hpp>
#include <amgcl/solver/bicgstab.hpp>
#include <amgcl/solver/bicgstabl.hpp>
#include <amgcl/solver/lgmres.hpp>
#include <amgcl/solver/fgmres.hpp>
#include <amgcl/solver/idrs.hpp>
#include <amgcl/solver/preonly.hpp>
#include <amgcl/coarsening/aggregation.hpp>
#include <amgcl/coarsening/smoothed_aggregation.hpp>
#include <amgcl/coarsening/ruge_stuben.hpp>
#include <amgcl/coarsening/smoothed_aggr_emin.hpp>
#include <amgcl/relaxation/damped_jacobi.hpp>
#include <amgcl/relaxation/spai0.hpp>
#include <amgcl/relaxation/spai1.hpp>
#include <amgcl/relaxation/ilu0.hpp>
#include <amgcl/relaxation/iluk.hpp>
#include <amgcl/relaxation/ilut.hpp>
#include <amgcl/relaxation/chebyshev.hpp>
#include <amgcl/relaxation/gauss_seidel.hpp>
#include <amgcl/relaxation/as_preconditioner.hpp>
#include <amgcl/preconditioner/cpr.hpp>
#include <amgcl/preconditioner/cpr_drs.hpp>
#include <amgcl/preconditioner/schur_pressure_correction.hpp>

// Шаблонная функция бенчмарка — принимает тип солвера
template<typename SolverType>
BenchmarkResult run_benchmark(const std::string& config_name,
                               typename SolverType::params& prm);
```

Каждый SECTION задаёт конкретный typedef `SolverType`, настраивает `prm`, и вызывает `run_benchmark<SolverType>(name, prm)`.

### Важно: LinearProblem нужно параметризовать

Текущий `LinearProblem` жёстко задаёт тип `Solver_AMG<B>` в заголовке. Для бенчмарка нужно:

**Вариант A**: Скопировать логику `LinearProblem::Solve()` в бенчмарк — работать напрямую с CRS-данными матрицы, создавать солверы разных типов. Это чище: бенчмарк не меняет production-код.

**Вариант B**: Шаблонизировать `LinearProblem` по типу солвера — ломает API, слишком инвазивно.

**Вариант C**: В бенчмарке получить CRS-данные через `Matrix().Row()`, `Matrix().Col()`, `Matrix().Val()` + RHS, и решать через amgcl напрямую, минуя `LinearProblem::Solve()`.

→ **Выбрать Вариант A**: бенчмарк работает с сырыми CRS-данными из LinearProblem, вызывая amgcl напрямую. Для этого:

1. Добавить в `LinearProblem` публичные accessor-ы для CRS-данных (если нет):
   - `Matrix().Row()` — ptr (уже есть)
   - `Matrix().Col()` — col (уже есть)
   - `Matrix().Val()` — val (уже есть)
   - `rhs` — правая часть (уже есть)
   - `cellNmbr`, `rhsSize` — размеры (уже есть)

2. В бенчмарке после `AssembleMyProblem` забрать данные и решать через произвольный amgcl-тип.

3. Проверить, что решение совпадает с production-решателем (невязка < tol).

## Матрица конфигураций для перебора

### Серия A: Krylov-солверы (AMG<aggregation, damped_jacobi> зафиксирован)

| ID | Солвер | maxiter | Примечание |
|---|---|---|---|
| A1 | gmres(M=5) | 5 | **baseline** |
| A2 | gmres(M=15) | 15 | увеличенный Krylov-базис |
| A3 | gmres(M=30) | 30 | щедрый |
| A4 | bicgstab | 15 | без ортогонализации |
| A5 | bicgstabl(l=2) | 15 | стабильнее BiCGStab |
| A6 | lgmres | 15 | augmented GMRES |
| A7 | fgmres | 15 | flexible — обязателен для переменного precond |
| A8 | idrs(s=4) | 15 | IDR(s) — часто быстрее BiCGStab |

### Серия B: Relaxation (AMG<aggregation, ?> + солвер-победитель из серии A)

| ID | Relaxation | Примечание |
|---|---|---|
| B1 | damped_jacobi | baseline |
| B2 | spai0 | sparse approximate inverse — лучший кандидат для блочных |
| B3 | spai1 | расширенный SPAI |
| B4 | ilu0 | ILU(0) — мощнее, дороже setup |
| B5 | iluk(k=1) | ILU(k) |
| B6 | chebyshev | полиномиальный |
| B7 | gauss_seidel | проверить поддержку для блочных типов |

### Серия C: Coarsening (relaxation-победитель + солвер-победитель)

| ID | Coarsening | Примечание |
|---|---|---|
| C1 | aggregation | baseline |
| C2 | smoothed_aggregation | сглаженная — лучшее качество V-cycle |
| C3 | ruge_stuben | классический RS-AMG (может потребовать `as_scalar` обёртку для блочных) |
| C4 | smoothed_aggr_emin | energy-minimizing prolongation |

### Серия D: Параметры AMG (тройка-победитель зафиксирована)

| ID | Параметр | Значения |
|---|---|---|
| D1 | ncycle | 1 (V), 2 (W) |
| D2 | npre / npost | 1/1, 2/1, 1/2, 2/2 |
| D3 | coarse_enough | 100, 500, 1000, 2000 |
| D4 | direct_coarse | false, true |
| D5 | damping (если Jacobi) | 0.5, 2/3, 0.8, 1.0 |
| D6 | aggr.eps_strong | 0.08, 0.25, 0.5 |
| D7 | over_interp (если SA) | 1.0, 1.5, 2/3 |

### Серия E: Специализированные предобуславливатели

| ID | Предобуславливатель | Примечание |
|---|---|---|
| E1 | `as_preconditioner<ilu0>` (без AMG) | голый ILU(0) |
| E2 | `as_preconditioner<spai0>` (без AMG) | голый SPAI0 |
| E3 | `cpr<amg, ilu0>` | CPR: AMG для давления + ILU для полной |
| E4 | `cpr_drs<amg, ilu0>` | CPR-DRS: с dynamic row summing |
| E5 | `schur_pressure_correction<amg, ilu0>` | Шурово дополнение |

## Особые указания по серии E (CPR/CPR-DRS/Schur)

### API amgcl для CPR (из анализа cpr.hpp)

CPR работает со **скалярной** (развёрнутой) матрицей, не с блочной:
- `PPrecond` — скалярный бэкенд (`backend::builtin<double>`)
- `SPrecond` — скалярный бэкенд (тот же)
- Параметр `block_size = 2` указывает CPR, как разделять переменные

```cpp
// Скалярные типы для CPR
using SBackend = amgcl::backend::builtin<double>;
using PPrecond = amgcl::amg<SBackend, amgcl::coarsening::smoothed_aggregation,
                             amgcl::relaxation::spai0>;
using SPrecond = amgcl::relaxation::as_preconditioner<SBackend,
                             amgcl::relaxation::ilu0>;
using CPR = amgcl::preconditioner::cpr<PPrecond, SPrecond>;
using CPRSolver = amgcl::make_solver<CPR, amgcl::solver::bicgstab<SBackend>>;
```

Это значит: для CPR нужно передать матрицу в **скалярном** (не блочном) формате. Наш CRS уже хранит скалярные данные (`Matrix().Val()` — `std::vector<double>`), просто amgcl::adapter::block_matrix переупаковывает их в блоки. Для CPR — передать напрямую скалярный CRS.

Параметры CPR:
```cpp
prm.precond.block_size = 2;
prm.precond.active_rows = 0;  // 0 = все строки
// pprecond — параметры AMG для давления
// sprecond — параметры ILU для полной системы
```

### CPR имеет partial_update()

```cpp
void partial_update(const Matrix &K, bool update_transfer_ops = true, ...);
```

Оставляет AMG-иерархию нетронутой, обновляет только ILU + трансферный оператор. Это прямая поддержка reuse! (Используется в фазе 2, пункт 2.1.)

### CPR-DRS — дополнительные параметры

```cpp
prm.precond.eps_dd = 0.2;   // порог для diagonal dominance
prm.precond.eps_ps = 0.02;  // порог для pressure sum
```

## Формат результатов

Файл `results/3d_fine_51x51/benchmark_summary.csv`:
```
config,solver,coarsening,relaxation,maxiter,total_s,setup_s,solve_s,avg_iters,max_iters,n_newton,n_wasted,amg_levels,op_complexity,balance_ok
A1,gmres(5),aggregation,damped_jacobi,5,...,...,...,...,...,...,...,...,...,true
```

## Порядок перебора

1. **Серия A** (~1 час): Krylov-солверы → лучший
2. **Серия B** (~1 час): relaxation → лучший
3. **Серия C** (~30 мин): coarsening → лучший
4. **Серия D** (~1 час): параметры AMG → оптимальные
5. **Серия E** (~1.5 часа): CPR/CPR-DRS/Schur/голый ILU

## Критерий валидности

Каждая конфигурация должна пройти:
- `max_oil_balance_rel < 1e-3`
- `max_water_balance_rel < 1e-3`
- `Sw ∈ [0, 1]`, `P > 0` на каждом шаге

Если не проходит — записать `balance_ok=false`, но сохранить в таблице.
