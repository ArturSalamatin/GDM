---
tags:
  - решение
  - CPR
  - true-impes
  - layout
date: 2026-07-13
---

# True-IMPES weights корректны через анализ layout InterleavedPSw

## Контекст

True-IMPES weights для CPR decoupling вычисляются из диагонального блока Якобиана. Цель — найти весовой вектор `w`, который при умножении на блок-строку Якобиана обнуляет столбец Sw, оставляя уравнение только по давлению.

## Анализ корректности

В InterleavedPSw layout `permCRSToPhysical = {1, 0}`:
- CRS row 0 = WaterEq (physRow 1)
- CRS row 1 = OilEq (physRow 0)
- Column 0 = P, Column 1 = Sw

Блок `v.data()` в cpr.hpp содержит (column-major, 2×2):
```
v.data()[0] = A[0] = dWaterEq/dP    (CRS row 0, col 0)
v.data()[1] = A[1] = dOilEq/dP      (CRS row 1, col 0)
v.data()[2] = A[2] = dWaterEq/dSw   (CRS row 0, col 1)
v.data()[3] = A[3] = dOilEq/dSw     (CRS row 1, col 1)
```

True-IMPES weights в `invert_impl(..., true_impes_weights)`:
```cpp
w0 =  A[1*B+1] =  A[3] = dOilEq/dSw
w1 = -A[1*B+0] = -A[2] = -dWaterEq/dSw
```

Проверка: `w0 * dWaterEq/dSw + w1 * dOilEq/dSw = dOilEq/dSw · dWaterEq/dSw - dWaterEq/dSw · dOilEq/dSw = 0` ✓

Это правильная формула nullspace: весовой вектор из **второго столбца** блока (Sw-столбец), но со скрещёнными знаками — стандартный приём для обнуления Sw-вклада.

## Ключевой вывод

При Sw=0 с несжимаемыми фазами:
- `dOilEq/dSw` = ненулевое (зависит от rhoO * V / tau)
- `dWaterEq/dSw` = ненулевое (зависит от rhoW * V / tau)
- `dWaterEq/dP` = 0 (нет потока воды, нет сжимаемости)

True-IMPES weights: `w = (dOilEq/dSw, -dWaterEq/dSw)` — оба ненулевые. Давление pressure equation: `w0·dWaterEq/dP + w1·dOilEq/dP = w1·dOilEq/dP` — ненулевое, если dOilEq/dP ≠ 0 (что верно при наличии нефтяного потока).

**Проблема Sw=0 была НЕ в weights, а в регуляризации диагонали** — weights давали корректное давление, но ILU0 падал на zero pivot в WaterEq-строке.

## Связанные заметки

- [[регуляризация диагонали Якобиана для всех строк а не только Sw]]
- [[layout абстракция отделяет топологию сетки от CRS маппинга]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
