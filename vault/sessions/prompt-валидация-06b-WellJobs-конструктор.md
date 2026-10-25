---
tags:
  - prompt
  - валидация
  - 3D
  - перфорации
date: 2026-06-19
---

# Шаг 2: Новый конструктор WellJobs

## Задача

Добавить конструктор `WellJobs(name, WellJobsPerLayer)` для приёма многопластовых данных из `WellCompletionBuilder`.

## Контекст

Текущие конструкторы `WellJobs` (файл `HydroSolver/Reservoir/Well/WellJobs.h`):

1. **Legacy** (строки 52-57): `WellJobs(name, WellJobsData, LayerAggregationTree, NmbrOfLayers)` — принимает данные из БД с именами слоёв и деревом агрегации. Используется только в `gdm_data` (PostgreSQL).

2. **Упрощённый** (строки 70-72): `WellJobs(name, JobsInLayer)` — принимает один `JobsInLayer` (вектор операций). `RawWellPerforationData{jobs}` создаёт `WellJobsPerLayer` с одним элементом → скважина видит только слой 0.

Нужен третий конструктор, принимающий `WellJobsPerLayer` напрямую.

## Что изменить

### Файл: `HydroSolver/Reservoir/Well/WellJobs.h`

Добавить после существующего упрощённого конструктора (после строки 72):

```cpp
WellJobs(const WellName& well_name, const WellJobsPerLayer& jobs_per_layer) noexcept :
    ItsName{ well_name }, RawWellPerforationData{ jobs_per_layer }
{ }
```

## Почему это безопасно

- `AccumulatePerforations()` (строки 20-45 в WellJobs.cpp) итерирует по `RawWellPerforationData[j]`, проверяя `!jobsInLayer.empty()`. Пустые пласты пропускаются — это штатное поведение.
- `AddWell_FixedProduction` (строки 113-149 в ReservoirSimulator.cpp) создаёт `ItsGlobalIDs` для каждого слоя `for (i = 0; i < nz())`, а затем вызывает `AccumulatePerforations(ActiveCells)`. Результат — `PerforationsOfWell` (map) — содержит только пласты, где были операции.
- Существующие тесты используют упрощённый конструктор (один слой) — они не затрагиваются.

## Проверка

Убедись, что проект собирается и все 30 существующих тестов проходят:

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Новый конструктор на этом шаге ещё не используется тестами — он нужен для шага 3.
