---
tags:
  - баг
  - SIGSEGV
  - скважины
  - OverallFluxes
date: 2026-06-17
updated: 2026-06-28
---

**GitHub issue:** [#1](https://github.com/ArturSalamatin/GDM/issues/1)

# SIGSEGV при закачке воды через скважину (BUG-001)

## Проявление

При добавлении нагнетательной скважины (закачка воды, `water_mass_rate < 0`) в сетку с `ny=1` (или `nx=1`) симулятор падает с SIGSEGV после первого успешного временного шага. Ньютон сходится нормально (4 итерации), crash происходит в post-processing.

## Причина (подтверждённая, 2026-06-28)

`AddFlowFieldSnapShot()` [ReservoirSimulator.cpp:616-628] безусловно обращается к `j_Y[k]`, но `OverallFluxes()` [строка 804] заполняет `j_Y` только при `ny > 1`. При `ny=1` вектор `j_Y` остаётся пустым → `j_Y[0]` = out-of-bounds → SIGSEGV.

Аналогично `j_X` не заполняется при `nx <= 1`.

## Опровергнутая гипотеза

Ранее считалось, что причина — «denom=0 в SetRefWellPressure → NaN → Newton divergence». Это **опровергнуто**:

- Для модели Кори: `k_ro = (1-S)^3`, `k_rw = S^3`. При `S ∈ [0, 1]`: `(1-S)^3 + S^3 ≥ 1/4 > 0`.
- Следовательно, `MobilityOverall > 0` всегда, и `denom > 0`.
- Экспериментально: Newton сходится за 4 итерации, NaN не возникает.

## Воспроизведение

```cpp
// 10×1×1 сетка, Sw=0.2, P=200atm
test_helpers::add_simple_well(sim, horizon, L"INJ", x, y, 0.0, -1000.0);
sim.Solve({0.0, 1.0});  // SIGSEGV в AddFlowFieldSnapShot
```

## Исправление

В `OverallFluxes()` добавить `else`-блоки для `ny <= 1` и `nx <= 1`, заполняющие `j_Y` / `j_X` пустыми слоями нужного размера (`nz` элементов).

## Связь с другими багами

- **BUG-004** (throw string) — исправляется попутно
- **BUG-008** (denom=0 в SetRefWellPressure) — invalid при `Sw ∈ [0,1]`, пересмотреть

## План

[[bug-001 well-state-rollback]]
