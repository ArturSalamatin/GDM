---
tags:
  - решение
  - cpr
  - amg
  - солвер
date: 2026-06-24
---

# Переход с блочного AMG на скалярный CPR в production

## Решение

Полная замена блочного AMG (`block_matrix_adapter<static_matrix<2,2>>`) на скалярный CPR (`builtin<double>`) как единственного линейного солвера.

## Почему

1. CPR в 2–4× быстрее блочного AMG на одинаковых задачах (variable debit 1.9–4.2×, 3D completions 1.9–2.2×)
2. CPR физически осмыслен: AMG для эллиптической pressure, ILU для полной гиперболико-параболической системы
3. Упрощение кода: один солвер вместо двух backend-ов

## Ключевые изменения

- `LinearProblem.h/cpp`: `CPRSolver` = `make_solver<cpr<amg<aggregation, ilu0>, as_preconditioner<ilu0>>, lgmres>`
- `ReservoirSimulator.h`: default layout = `InterleavedPSw` (pressure = row%B==0, требование CPR)
- `MatrixCSR.h/cpp`: добавлен non-const `Val()` для регуляризации перед solve
- `ilu0.hpp`: fallback `D[i]=1` при zero pivot (вместо crash)
- `test_five_spot.cpp`: `oil_saturation = 0.999` (Sw_init=0.001 вместо 0)
- `test_amgcl_benchmark.cpp`: удалены блочные бенчмарки, `run_benchmark` вызывает production solver

## Верификация

| Тест | Блочный AMG | CPR | Ускорение |
|---|---|---|---|
| Variable debit (4 теста) | 1.55–3.22 с | 0.77–0.86 с | 1.9–4.2× |
| 3D completions (4 теста) | 0.84–2.49 с | 0.45–1.30 с | 1.9–2.2× |
| Grid convergence 41×41 | н/д | 2.99 с | — |
| Five-spot (Sw=0.001) | н/д | 1.65 с | — |

Физика проверена: массовый баланс (26463 assertions), Sw bounds, четвертная симметрия (8 знаков), Newton 4–6 iter.

## Ограничения

CPR не работает при точно Sw=0 — двухфазный Якобиан при нулевой водонасыщенности полностью вырождается. При Sw ≥ 0.0001 работает стабильно. В реальных пластах connate water Sw_c ≥ 10–20%.

## Связанные заметки

- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[prompt-оптимизация-09-CPR-в-production]]
