---
tags:
  - баг
  - числовая-устойчивость
  - фикс
date: 2026-07-01
issue: BUG-007
github: 6
---

# BUG-007: Деление на ноль в гармоническом среднем подвижности

## Симптом

NaN в матрице Якобиана и dependent properties ячейки при `MobilityOverall() = 0`.

## Причина

Гармоническое среднее подвижности `H(a,b) = 2ab/(a+b)` без проверки `a+b > 0`. При `Permeability = 0` (непроницаемый барьер) обе подвижности = 0, знаменатель = 0, результат NaN.

Дополнительно: `F_Oil = λ_oil / λ_total` и `Derivative_F_Oil = (...) / λ_total` в `UpdateDependentFieldProperties()` делят на `MobilityOverall()` — NaN при инициализации ячейки.

## Критичность

Средняя. При текущей модели Corey с `std::max(0.0, ...)` в `RelativePermeability`, `MobilityOverall > 0` всегда при `Permeability > 0`. Баг латентный — проявляется при `Permeability = 0` или будущих расширениях модели relperm.

## Фикс

Guard clauses в 3 местах:

1. **`TwoPhaseFlowCell.h`** — `if (MobilityOverall() > 0.0)` перед делениями F_Oil и Derivative_F_Oil, иначе = 0
2. **`ReservoirSimulator.cpp:580`** — `if (denom == 0.0) continue;` в `fillMatrixBlockRow`
3. **`ReservoirSimulator.cpp:871,1001`** — ternary guard в `OverallFluxes`

Физическое обоснование: нулевая подвижность = нулевой поток = нулевой вклад.

## Тест

`TwoPhaseFlowCell: zero permeability gives finite properties` — ячейка с `Permeability = 0`, проверка `MobilityOverall == 0`, `F_Oil == 0`, `isfinite(OilMass)`.

## Связь

- [[code-review-2026-06-28-числовая-устойчивость]] — CR-NUM-001, где баг обнаружен
- [[BUG-006 solve converged always true]] — BUG-006 маскировал NaN (converged = true)
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — BUG-002, следствие NaN в Якобиане
