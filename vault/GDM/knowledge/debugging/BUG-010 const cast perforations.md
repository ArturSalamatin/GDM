---
tags:
  - debugging
  - баг
  - const-correctness
date: 2026-07-02
---

# BUG-010: const_cast на перфорациях в SomeWell — undefined behavior

## Симптом

5 мест в `SomeWell.cpp` использовали `const_cast` для мутации объектов `AccumulatedPerforations` и `SetOfPerforations`, полученных через const-геттер `PerforationConfiguration()`. Формально UB по стандарту C++.

## Причина

`PerforationConfiguration()` — единственный геттер для поля `ItsAccumulatedPerforations`, возвращает `const PerforationsOfWell&`. Конструктор и `BringFirst/LastPerforationToFirstMER()` — non-const методы, которым нужно мутировать перфорации. Автор обошёл const через `const_cast`.

## Решение

1. Заменить `PerforationConfiguration()` на прямой доступ к полю `ItsAccumulatedPerforations` в 3 методах (конструктор, `BringFirstPerforationToFirstMER`, `BringLastPerforationToLastMER`)
2. Добавить non-const перегрузку `getPerforationsSet()` в `AccumulatedPerforations` для `MoveDate()` на элементах вектора
3. Убрать все 5 `const_cast`

## Файлы

- `HydroSolver/Reservoir/Well/SomeWell.cpp` — 5 `const_cast` устранены
- `HydroSolver/Reservoir/Well/SetOfPoints.h` — non-const `getPerforationsSet()`
- `HydroSolver/Reservoir/Well/SetOfPoints.cpp` — реализация non-const перегрузки

## Связанные задачи

- [[code-review-2026-06-28-баги-и-корректность]] (CR-BUG-006)
- DEBT-031: оставшиеся 5 `const_cast` в `RawWellFactory.cpp` и `MER_Descriptor.cpp`

**GitHub issue:** [#10](https://github.com/ArturSalamatin/GDM/issues/10)
