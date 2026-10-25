---
tags:
  - debugging
  - баг
  - исправлено
date: 2026-07-01
---

# BUG-009: фильтр ActiveCells не работает — size_t + 1 > 0 всегда true

## Симптом

Перфорации скважины в неактивных ячейках не фильтруются. `ActiveCells` всегда заполняется `true`.

## Корневая причина

Цепочка из двух дефектов:

1. **BUG-011:** `ConvertTriple2Local()` в `AbstractGrid.h` объявлен как `size_t`, но внутри вызывает `ConvertGlobal2Local()`, который возвращает `long int`. Для неактивных ячеек `-1` превращается в `SIZE_MAX` (~2⁶⁴).

2. **BUG-009:** В `ReservoirSimulator.cpp` проверка `cellIdx+1 > 0` тождественно истинна для unsigned типа. `SIZE_MAX + 1 = 0` (unsigned wrap), но MSVC оптимизирует `unsigned + 1 > 0` в `true`.

Дополнительно: доступ `Grid[SIZE_MAX]` выполнялся ДО проверки фильтра — out-of-bounds UB.

## Решение

1. `ConvertTriple2Local` → возвращает `long int` (согласовано с `ConvertGlobal2Local`)
2. `operator()(i,j,k)` → `long int idx`, `static_cast<int>(idx)` для `operator[]`
3. Guard `cellIdx >= 0` в `AddWell_FixedProduction` ПЕРЕД доступом к `Grid[]`
4. Placeholder `well_local_position[l] = 0`, `cells_[l] = &(Grid[0])` для неактивных ячеек
5. Конструктор `SomeGrid` — `active_cells.size()` вместо `totalCellNmbr` (latent UB из-за порядка member-init)
6. `add_simple_well` — `WellJobsPerLayer` с `Nz` элементами вместо одного (OOB в `RemovePerfsAtInactiveCells` при Nz > 1)

## Связанные заметки

- [[code-review-2026-06-28-баги-и-корректность]] — CR-BUG-002
- [[bug-009 active-cells-filter]] — план исправления
- [[BUG-008 well pressure zero mobility]] — guard при `denom = 0` для скважины без активных перфораций

**GitHub issue:** [#8](https://github.com/ArturSalamatin/GDM/issues/8)
