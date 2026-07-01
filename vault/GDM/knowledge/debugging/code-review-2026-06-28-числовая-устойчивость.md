---
tags:
  - code-review
  - численные-методы
  - устойчивость
date: 2026-06-28
---

# Code review 2026-06-28 — числовая устойчивость

## CR-NUM-001: Деление на ноль в гармоническом среднем подвижности

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:579-581`

```cpp
double denom = OverallMobilityCell + OverallMobilityNeighbour;
double MeanOverallMobility = 2 * OverallMobilityNeighbour * OverallMobilityCell / denom;
```

Если `OverallMobilityCell = OverallMobilityNeighbour = 0` (обе ячейки содержат фазу с нулевой подвижностью), `denom = 0` и происходит деление на ноль → NaN/Inf в матрице. Это возможно при `Sw_init = 0` и чисто нефтяном пласте вблизи закрытых скважин.

Связано с [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] (BUG-002).

---

## CR-NUM-002: Деление на ноль в `WellFixedProduction::SetRefWellPressure`

**Файл:** `HydroSolver/Reservoir/Well/Wells.cpp:79-91`

```cpp
double denom = 0;
for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
{
    double temp = factor[l] * OverallMobility(l);
    denom += temp;
}
double P = numer / denom;
```

Если `OverallMobility = 0` для всех перфораций (начальная нефтенасыщенность = 1, вода ещё не подошла), `denom = 0` → `P = inf` → NaN каскадом.

Связано с [[Newton divergence при закачке воды через скважину]] (BUG-001).

---

## CR-NUM-003: `pow(x, permPower - 1)` для `x = 0` при `permPower = 3`

**Файл:** `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.cpp:65-66`

```cpp
double DerivativeRelativePermeabilityOil() const { return -pow(std::max(0.0, SOil_Scaled()), permPower - 1) * permPower; }
double DerivativeRelativePermeabilityWater() const { return pow(std::max(0.0, SWater_Scaled()), permPower - 1) * permPower; }
```

При `SOil_Scaled() = 0` или `SWater_Scaled() = 0` это `pow(0, 2) = 0` — корректно. Но при дробном `permPower` (если расширить до модели Кори с нецелым показателем), `pow(0, 0.5)` всё ещё `0`, но `permPower - 1 < 0` даст `pow(0, -0.5) = inf`.

Пока `permPower = 3` (целый, `static const int`), это не проблема. Но при переходе на настраиваемый показатель Кори — станет.

---

## CR-NUM-004: Ненадёжная проверка сходимости Ньютона

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:456-481`

```cpp
const double tol = 3E-3;
f[B * l + i] =
    (abs(stateVaiables[i]) < numPrm.NewtonTol() * tol) || 
    (abs(1.0 - stateVaiables[i]) < numPrm.NewtonTol() * tol) ||
    (abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
```

Для давления (i=1): абсолютный порог `1E-4 * 3E-3 = 3E-7` бессмысленно мал — давление порядка ~1.7E7 Па, это `3E-7/1.7E7 ≈ 2E-14` относительно. Фактически критерий: `|corr| / |P| < 1E-4`. 

Для насыщенности (i=0): при `Sw_scaled ≈ 0` или `≈ 1` первые два условия (`|Sw| < 3E-7` или `|1-Sw| < 3E-7`) срабатывают и принимают состояние как сошедшееся, даже если коррекция ещё значима. Это маскирует проблемы вблизи границ насыщенности.

---

## CR-NUM-005: `decrease_schemeTau` может дать отрицательный шаг

**Файл:** `HydroSolver/Reservoir/NumericalParameters.cpp:103-116`

```cpp
schemeTau = CurrentIntegrationStep() * (1 - 2 * factor);  // factor = 0.15
```

`1 - 2 * 0.15 = 0.7` — сейчас безопасно. Но если `factor ≥ 0.5`, шаг станет ≤ 0. Нет нижней клампы на положительность. С PI-контроллером `min_shrink` защищает, но без него — нет.

---

## CR-NUM-006: `Solve()` всегда возвращает `converged = true`

**Файл:** `HydroSolver/Solver/Math/LinearProblem.cpp:143`
**GitHub issue:** [#5](https://github.com/ArturSalamatin/GDM/issues/5)

```cpp
return { iters, error, true };
```

Независимо от результата AMG-решателя, `converged` всегда `true`. Информация о расхождении теряется. `numPrm.update_currentAMGState` проверяет `converged`, но получает всегда `true`, полагаясь только на `isfinite(error)`.
