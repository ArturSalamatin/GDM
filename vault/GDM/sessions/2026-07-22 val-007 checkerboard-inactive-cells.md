---
tags:
  - сессия
  - валидация
  - inactive-cells
date: 2026-07-22
---

# 2026-07-22: VAL-007 шахматная деактивация ячеек

## Что сделано

1. **Аудит плана VAL-007** (повторный) — 0/0/0, план готов
2. **Реализация VAL-007** — 5 шагов плана выполнены, ветка `val/val-007/checkerboard-inactive-cells`
3. **Ревизия тестов** — скважина убрана из всех checkerboard-тестов

## Ключевое открытие: вырожденность якобиана

При анализе визуализации обнаружено, что скважина в тестах бессмысленна:

- **Несжимаемая жидкость** (`c_oil = c_water = 0` в `CreateDefaultOil/Water`) + **изолированная ячейка** (нет активных соседей при шахматном паттерне) → `DerivativeMassOilByP = 0`, `DerivativeMassWaterByP = 0`
- **Единственная перфорация** → скважинная производная `dP_well/dP_res = 1` → вклад скважины в столбец P якобиана: `rhoO * PI * f_o * lambda * (1 - 1) = 0`
- **Итого:** столбец P в якобиане — нули. Матрица вырождена по давлению. Newton находит коррекцию через `dSw`, давление остаётся = P_init
- Тест проверял не физику скважины, а артефакт вырожденности

**Решение:** убрана скважина. Тесты теперь чисто проверяют структурную изоляцию (граф связности, `CellsInactive`, SparsityPattern).

## Тесты

| До | После |
|---|---|
| 316 Release | 318 Release (+2 visible, +1 hidden visual) |
| 316 Debug | 318 Debug |

Тесты:
- `"Checkerboard inactive cells - no crash"` — smoke
- `"Checkerboard inactive cells - state preserved"` — P и Sw всех ячеек = init
- `"Checkerboard inactive cells - CSV export"` — `[.visual]`, генерирует `results/val-007/checkerboard.csv`

## Файлы

- `tests/test_inactive_cells.cpp` — 3 новых теста (без скважины)
- `scripts/plot_checkerboard.py` — визуализация шахматного паттерна
- `vault/GDM/roadmap/валидационные кейсы.md` — VAL-007 → ✅ ПРОЙДЕН
- `vault/GDM/plans/val-007 checkerboard-inactive-cells.md` — status → реализован

## Коммиты

```
9a2f40c vault: VAL-007 начало реализации
dde3214 test: VAL-007 checkerboard inactive cells (3 tests)
57fa800 test: VAL-007 visual test CSV export + Python plot script
6eb6934 vault: VAL-007 пройден (шахматная деактивация)
d65652f test: VAL-007 убрана скважина из checkerboard-тестов
```

## Связанные заметки

- [[val-007 checkerboard-inactive-cells]] — план
- [[BUG-009 active cells filter]] — зависимость (исправлена ранее)
- [[сетка 3D структурированная с линейной индексацией]] — архитектура индексации
