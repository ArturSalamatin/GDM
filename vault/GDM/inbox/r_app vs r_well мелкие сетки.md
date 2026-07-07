---
tags: [inbox, обнаружено-при-реализации]
date: 2026-07-03
source: VAL-001
---
# r_app ≤ r_well при мелких сетках → деление на ноль в PI скважины

Обнаружено при реализации [[val-001 buckley-leverett-1d]].

`add_simple_well()` в `test_helpers.h` вычисляет `r_app = 0.2 * hx`. При `hx ≤ 0.5 м` получается `r_app ≤ 0.1 м = r_well`. Формула Пикмана: `factor = 2π / ln(r_app / r_well)` → `ln(1) = 0` или `ln(<1) < 0` → бесконечный или отрицательный PI → exception "well production is not determined".

Обход: явный `r_app = max(0.2*hx, 0.2)` в вызывающем коде (сделано в CSV-тесте VAL-001).

Правильное решение: либо проверка `r_app > r_well` в `SomeWell::UpdateWellState()`, либо формула Пикмана с `r_eq = 0.14*sqrt(dx² + dy²)` вместо `0.2*dx`.
