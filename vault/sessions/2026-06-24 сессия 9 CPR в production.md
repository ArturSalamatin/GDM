---
tags:
  - сессия
  - cpr
  - production
  - солвер
date: 2026-06-24
---

# Сессия 9: CPR в production — полный переход

## Цель

Полностью заменить блочный AMG (`block_matrix_adapter<static_matrix<2,2>>`) на скалярный CPR в production коде. Без опции переключения — единственный солвер.

## Что сделано

### Код

1. **`LinearProblem.h`** — удалены `block_matrix.hpp`, `static_matrix.hpp`, `value_type<B>`, `rhs_type<B>`, `BBackend<B>`, `Solver_AMG<B>`. Заменены на:
   ```cpp
   using ScalarBackend = amgcl::backend::builtin<double>;
   using CPRPrecond = amgcl::preconditioner::cpr<
       amgcl::amg<ScalarBackend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
       amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
   >;
   using CPRSolver = amgcl::make_solver<CPRPrecond, amgcl::solver::lgmres<ScalarBackend>>;
   ```
   Параметр `prm.precond.block_size = B` (=2) сообщает CPR размер блока для extraction давления.

2. **`LinearProblem.cpp`** — `Solve()` пересоздаёт CPRSolver каждый Newton step (reuse невозможен без partial_update). Добавлена регуляризация Sw-диагоналей: для нечётных строк (Sw в InterleavedPSw) с |diag| < 1e-20 ставится 1e-6.

3. **`MatrixCSR.h/cpp`** — добавлен non-const `Val()` accessor для регуляризации.

4. **`ReservoirSImulator.h`** — default layout = `InterleavedPSw` (было InterleavedSwP). CPR требует pressure = col%B==0.

5. **`ilu0.hpp`** — zero pivot fallback: `D[i] = 1` вместо `precondition(crash)`. ILU factorization может обнулить pivot через elimination на тонких сетках, даже если начальная диагональ ненулевая.

### Тесты

6. **`test_amgcl_benchmark.cpp`** — удалены блочные серии A–H и шаблон `solve_with<SolverType>`. `run_benchmark` вызывает production solver через `sim.MyProblem.Solve()`. Серия CPR оставлена для экспериментальных конфигураций.

7. **`test_five_spot.cpp`** — `oil_saturation` изменён с 1.0 на 0.999 (Sw_init = 0.001). При точно Sw=0 CPR diverges.

## Проблема: zero pivot в ILU0

Две причины:
1. **ILU-produced pivots** — на сетке 41×41 ILU(0) elimination обнуляет pivot даже при Sw=0.2. Решение: fallback D[i]=1 в ilu0.hpp.
2. **Вырожденный Якобиан при Sw=0** — вся строка водной фазы нулевая → CPR block inversion → near-singular pressure matrix → NaN. Решение: Sw_init ≥ 0.0001.

Протестированные подходы, которые НЕ работают при Sw=0:
- row-norm epsilon в ilu0.hpp
- iluk(k=0) в sprecond
- smoothed_aggregation в pprecond
- регуляризация только Sw-диагоналей

Подробнее: [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]

## Производительность

Честное сравнение (одинаковые условия, одинаковые Sw_init):

| Тест | Блочный AMG | CPR | Ускорение |
|---|---|---|---|
| Variable debit: two injection rates | 1.55 с | 0.81 с | 1.9× |
| Variable debit: injection with shut-in | 1.10 с | 0.52 с | 2.1× |
| Variable debit: increasing injection | 1.97 с | 0.86 с | 2.3× |
| Variable debit: alternating injectors | 3.22 с | 0.77 с | 4.2× |
| 3D completions: 7-well smoke | 2.49 с | 1.30 с | 1.9× |
| 3D completions: delayed start | 1.04 с | 0.52 с | 2.0× |
| 3D completions: layer closure | 1.06 с | 0.48 с | 2.2× |
| 3D completions: partial perforation | 0.84 с | 0.45 с | 1.9× |

Диапазон: **1.9–4.2×**, согласуется с бенчмарком сессии 6b (CPR -32%).

Полный прогон 45 тестов: ~35 с (было ~670 с, но old five-spot зависал на timeout — нечестное сравнение).

## Верификация

4 полных прогона подряд — 45/46 стабильно (46-й = benchmark stub). Проверено:

- **Массовый баланс**: 26463 assertions в Visual verification
- **Sw bounds**: 0 ≤ Sw ≤ 1 для каждой ячейки
- **Четвертная симметрия**: five-spot, 4 угла Sw = 0.55340647 (8 знаков)
- **Давление**: P ≈ 200 атм (20.05–22.3 МПа), реалистичное
- **Newton**: 4–6 итераций на поздних шагах

## Коммиты

- `c3334a7` feat: CPR-солвер вместо блочного AMG в production
- `db5c8a4` refactor: benchmark и five-spot под CPR-солвер
- `60ca8be` vault: результаты сессии 9 — CPR в production

## Что дальше

- ☐ partial_update (reuse AMG давления между Newton steps) — потенциально ещё +20–30%
- ☐ PI-контроллер = true по умолчанию (после верификации на нестационарных задачах)
- ☐ Рефакторинг Newton loop (фаза 3)

## Связанные заметки

- [[переход с блочного AMG на скалярный CPR в production]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[2026-06-24 сессия 6b-7 CPR benchmark и PI-контроллер]]
- [[prompt-оптимизация-09-CPR-в-production]]
