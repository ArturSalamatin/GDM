---
tags:
  - баг
  - Newton
  - скважины
  - divergence
date: 2026-06-17
---

# Newton divergence при закачке воды через скважину

## Проявление

При добавлении нагнетательной скважины (закачка воды, `water_mass_rate < 0`) Ньютон перестаёт сходиться после нескольких шагов. Симулятор входит в бесконечный цикл `decrease_tau → retry`.

## Причина (гипотеза)

В `PerformNewtonLoop` при несходимости вызывается `Grid.ReverseState()`, который откатывает S_w и P в ячейках. Однако состояние скважины (`P_Well`, `productions`, `factor`) **не откатывается**. При следующей попытке `AddWellToMatrix` → `SetRefWellPressure` считает `P_Well` от предыдущего (неоткаченного) состояния скважины → получает NaN → `productions = NaN` → exception.

## Воспроизведение

```cpp
test_helpers::add_simple_well(sim, horizon, L"INJ", x, y, 0.0, -1000.0);
sim.Solve({0.0, 1.0});  // exception после ~4-8 шагов
```

## Связанные файлы

- `Wells.cpp:19` — `AddWellToMatrix`, строка 28-29 бросает exception
- `SomeWell.cpp:345` — `UpdateWellState` вычисляет `factor` и `P_Well`
- `Wells.cpp:80` — `SetRefWellPressure`: `P = numer/denom`, `denom` может быть 0
- `ReservoirSimulator.cpp:416-420` — `Grid.ReverseState()` + `decrease_schemeTau`

## Возможное исправление

Добавить `ReverseWellState()` в `SomeWell`, вызывать из `Solve()` при `!IsSuccessfullNewtonTrial`. Или пересчитывать `UpdateWellState` заново при каждом вызове `AssembleMyProblem`.
