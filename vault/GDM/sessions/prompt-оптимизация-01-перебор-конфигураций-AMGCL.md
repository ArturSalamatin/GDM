---
tags:
  - промпт
  - производительность
  - amgcl
  - перебор
date: 2026-06-20
updated: 2026-06-21
---

# Фаза 1: Систематический перебор конфигураций AMGCL

## Цель

Найти оптимальную комбинацию (Krylov solver, coarsening, relaxation, параметры AMG) для блочной СЛАУ 2×2 на 3D-сетке 51×51×4. Профилирование через `amgcl::profiler` (макрос `AMGCL_PROFILING` включён в CMake).

## Текущее состояние

Бенчмарк-инфраструктура готова: `tests/test_amgcl_benchmark.cpp`, таргет `gdm_benchmark`.

- `run_benchmark<SolverType>()` — шаблонная функция, прогоняет 200-дневной сценарий на 51×51×4
- `prof.reset()` перед каждым прогоном, `std::cout << prof` после — иерархический профиль amgcl
- `BenchmarkResult` — счётчики (time_steps, newton, amg_solves, total_amg_iters, balance)
- CSV-вывод в `results/amgcl_benchmark.csv`
- Защита от бесконечных откатов: `MAX_CONSECUTIVE_ROLLBACKS = 15`
- `prm.solver.tol` и `prm.solver.abstol` копируются из production `numPrm`

### Уже выполненные серии (Phase 1, 2026-06-20)

**Series A — Krylov solver** (AMG<aggregation, damped_jacobi>):
- gmres M=5/15/30, bicgstab, bicgstabl L=2, lgmres M=15, fgmres M=15, idrs s=4
- **Победитель: lgmres M=15** (5% быстрее gmres)

**Series B — Relaxation** (lgmres + aggregation):
- damped_jacobi, spai0, ilu0, gauss_seidel, chebyshev
- **Победитель: ilu0** (37% быстрее, 2.3× меньше итераций)
- spai0, chebyshev — не сходятся с блочным 2×2 бэкендом

**Series C — Coarsening** (lgmres + ilu0):
- aggregation vs smoothed_aggregation
- **Победитель: aggregation** (10% быстрее)
- ruge_stuben — несовместим (нет `operator<=` для `static_matrix<2,2>`)

**Текущий оптимум: `amg<aggregation, ilu0> + lgmres`** → 2.5× ускорение солвера.

## Новые серии для перебора

### Series D — параметры AMG-цикла

Зафиксировано: `amg<aggregation, ilu0> + lgmres`. Варьируем параметры AMG.

| ID | Параметр | Значение | Примечание |
|---|---|---|---|
| D1 | ncycle=1, npre=1, npost=1 | default | baseline (текущий оптимум) |
| D2 | ncycle=2 (W-цикл) | npre=1, npost=1 | двойной проход по грубым уровням |
| D3 | npre=2, npost=1 | ncycle=1 | усиленное пре-сглаживание |
| D4 | npre=1, npost=2 | ncycle=1 | усиленное пост-сглаживание |
| D5 | npre=2, npost=2 | ncycle=1 | двойное сглаживание |
| D6 | npre=0, npost=2 | ncycle=1 | только пост-сглаживание |

Настройка:
```cpp
using S = Solver_AMG<B>;  // amg<aggregation, ilu0> + lgmres
S::params prm;
prm.precond.ncycle = ...;
prm.precond.npre = ...;
prm.precond.npost = ...;
```

### Series E — ilu-семейство relaxation

Зафиксировано: `lgmres + aggregation`. Варьируем relaxation из семейства ILU.

| ID | Relaxation | Примечание |
|---|---|---|
| E1 | ilu0 (damping=1.0) | baseline |
| E2 | ilu0 (damping=0.8) | демпфированный ILU(0) |
| E3 | iluk (k=1) | ILU с fill-in level 1 — дороже setup, лучше сглаживание |
| E4 | ilut (p=2, tau=1e-2) | ILU с пороговым отбрасыванием |

Настройка damping:
```cpp
prm.precond.relax.damping = 0.8;
```

Для iluk/ilut — новые типы:
```cpp
#include <amgcl/relaxation/iluk.hpp>
#include <amgcl/relaxation/ilut.hpp>

using Solver_ILUK = amgcl::make_solver<
    amgcl::amg<BBackend<B>, amgcl::coarsening::aggregation, amgcl::relaxation::iluk>,
    amgcl::solver::lgmres<BBackend<B>>
>;
```

**Внимание:** iluk и ilut могут быть несовместимы с блочным бэкендом (как spai0/spai1). Если не компилируется или не сходится — пометить `failed`.

### Series F — параметры lgmres

Зафиксировано: `amg<aggregation, ilu0>`. Варьируем параметры lgmres.

| ID | M (inner) | K (augmented) | always_reset | Примечание |
|---|---|---|---|---|
| F1 | 15 | 3 | true | baseline |
| F2 | 5 | 3 | true | минимальный Krylov-базис (с ilu0 может хватить) |
| F3 | 10 | 3 | true | промежуточный |
| F4 | 30 | 3 | true | щедрый |
| F5 | 15 | 1 | true | минимум augmented vectors |
| F6 | 15 | 5 | true | больше augmented vectors |
| F7 | 10 | 2 | true | компактный вариант |

Настройка:
```cpp
prm.solver.M = 10;
prm.solver.K = 2;
```

### Series G — over_interp для aggregation coarsening

Зафиксировано: `amg<aggregation, ilu0> + lgmres`. Варьируем `over_interp`.

| ID | over_interp | Примечание |
|---|---|---|
| G1 | 2.0 | default для блочного бэкенда |
| G2 | 1.0 | без over-interpolation |
| G3 | 1.5 | промежуточный |
| G4 | 3.0 | агрессивный |

Настройка:
```cpp
prm.precond.coarsening.over_interp = 1.5f;
```

## Реализация

Добавить серии D–G как новые SECTION-ы в `test_amgcl_benchmark.cpp`. Каждая серия — отдельный тег (`[seriesD]`, `[seriesE]`, `[seriesF]`, `[seriesG]`).

Для iluk/ilut — добавить includes и explicit instantiations `SolveWith` в benchmark.

## Порядок выполнения

1. **Series D** (AMG params, ~20 мин): 6 конфигураций, тот же тип солвера
2. **Series F** (lgmres params, ~25 мин): 7 конфигураций, тот же тип солвера
3. **Series G** (over_interp, ~15 мин): 4 конфигурации, тот же тип солвера
4. **Series E** (ilu-семейство, ~20 мин): 4 конфигурации, могут потребовать новые типы

Series D/F/G варьируют только параметры текущего `Solver_AMG<B>`, не требуют новых типов. Series E требует новых typedef для iluk/ilut.

## Критерий валидности

Каждая конфигурация должна пройти:
- `max_oil_balance_rel < 1e-3`
- `max_water_balance_rel < 1e-3`
- `Sw ∈ [0, 1]`, `P > 0` на каждом шаге
- Отсутствие `solver_failed` (< 15 consecutive rollbacks)

## Формат результатов

`results/amgcl_benchmark.csv` — append. Профиль amgcl выводится в stdout через `std::cout << prof`.

## Связанные заметки

- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[2026-06-20 оптимизация AMGCL солвера lgmres ilu0]]
- [[2026-06-21 переход на amgcl profiler]]
- [[возможности amgcl для блочных СЛАУ]]
