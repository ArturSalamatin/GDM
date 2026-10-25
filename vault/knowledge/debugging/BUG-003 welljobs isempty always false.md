---
tags:
  - debugging
  - баг
  - скважины
date: 2026-07-02
issue: BUG-003
github: 12
---

# BUG-003: WellJobs::IsEmpty() всегда возвращает false

## Симптом

Скважина с полностью пустыми перфорациями (все слои без job records) не отсеивается фильтром `IsEmpty()`. Warning `WellOperationDataIsEmpty` никогда не срабатывает.

## Причина

`WellJobs::IsEmpty()` (`WellJobs.cpp:94-99`) проверяла `!RawWellPerforationData.empty()` — пуст ли весь `vector<vector<WellJobTime>>`. Конструктор инициализирует его `NmbrOfLayers` пустыми векторами, поэтому `.empty()` на верхнем уровне всегда `false`.

Правильная проверка: `!RawWellPerforationData[i].empty()` — есть ли job records в конкретном слое `i`.

## Фикс

Одна строка: `RawWellPerforationData.empty()` → `RawWellPerforationData[i].empty()`.

## Call sites

- `ReservoirSimulator.cpp:91` — `if (horizon.well_jobs.at(name).IsEmpty()) { continue; }` — корректно обрабатывает `true`
- `RawWellFactory.cpp:120` — `if (!jobs.IsEmpty()) { jobs_container.emplace(name, jobs); }` — корректно обрабатывает `true`

## Тесты

3 новых теста в `tests/unit/wells/test_WellJobs.cpp`:
- `IsEmpty returns true when all layers empty` — 3 пустых слоя
- `IsEmpty returns false when one layer has jobs` — 1 из 3 слоёв непуст
- `IsEmpty returns true when zero layers` — 0 слоёв

## Связанные заметки

- [[bug-003 welljobs-isempty]] — план
- [[code-review-2026-06-28-баги-и-корректность]] — первоисточник
