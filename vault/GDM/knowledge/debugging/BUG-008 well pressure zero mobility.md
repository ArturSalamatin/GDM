---
tags:
  - debugging
  - баг
  - скважина
  - деление-на-ноль
date: 2026-07-01
issue: BUG-008
github: 7
---

# BUG-008: деление на ноль в SetRefWellPressure при нулевой подвижности

## Симптом

`P_well = ±Inf` → каскад NaN/Inf в `SetProductions` → `throw std::exception("well production is not determined")`.

## Причина

`WellFixedProduction::SetRefWellPressure()` (`Wells.cpp:80-92`) вычисляет давление скважины как взвешенное среднее: `P_well = numer / denom`, где `denom = Σ factor[l] * OverallMobility(l)`. При `OverallMobility = 0` для всех перфораций → `denom = 0` → деление на ноль.

Два сценария:
1. Все перфорации в непроницаемом барьере (`Permeability = 0`)
2. `NmbrOfOpenedCells() = 0` (пустая скважина)

## Фикс

Guard `if (denom == 0.0)` перед делением. При срабатывании — `P_well = среднее(P_reservoir)` по перфорациям.

Физика: при нулевой подвижности поток отсутствует (уравнение Писмана вырождается), давление скважины уравновешивается с пластовым. `productions = factor * 0 * (P_res - P_well) = 0` при любом `P_well`, но среднее `P_res` — физически осмысленное значение для диагностики.

## Связь с BUG-007

После фикса [[BUG-007 harmonic mean zero division]] `OverallMobility = 0` (а не NaN) при `Permeability = 0`. Но `P_well = Inf` всё равно: `0 * Inf = NaN` → `isfinite` check → throw. BUG-007 не спасает от BUG-008.

## Файлы

| Файл | Изменение |
|---|---|
| `HydroSolver/Reservoir/Well/Wells.cpp:88-95` | guard + avg_P fallback |

## Связанные заметки

- [[code-review-2026-06-28-числовая-устойчивость]] — CR-NUM-001
- [[BUG-007 harmonic mean zero division]] — аналогичный паттерн
- [[дебит скважины определяется формулой Дюпюи с радиусом Писмана]]
