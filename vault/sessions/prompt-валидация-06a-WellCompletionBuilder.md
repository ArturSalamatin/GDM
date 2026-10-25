---
tags:
  - prompt
  - валидация
  - 3D
  - перфорации
date: 2026-06-19
---

# Шаг 1: WellCompletionBuilder

## Задача

Создать fluent-фабрику `WellCompletionBuilder` в файле `tests/well_completion_builder.h`. Аналог `WellScheduleBuilder`, но для истории перфораций (ГТМ), а не для дебита (MER).

## Контекст

### Как устроены перфорации в ядре

Типы данных (файл `HydroSolver/Reservoir/Well/WellJobs.h`):

```
JobsInLayer = vector<WellJobTime>          // операции в одном пласте
WellJobsPerLayer = vector<JobsInLayer>     // операции во всех пластах [layer_id][job_id]
```

`WellJobTime` (файл `HydroSolver/Reservoir/Well/SetOfPoints.h`):
- Наследует `WellJob`, который содержит `Segment(start, end)` и `bool is_open`
- Добавляет `double timeMoment`
- Конструктор: `WellJobTime(start, end, is_open, time)`

`AccumulatePerforations()` в `WellJobs.cpp` (строки 20-45):
- Итерирует по `RawWellPerforationData[j]` для каждого пласта j
- Для каждого пласта собирает `AccumulatedPerforations` из последовательности `WellJobTime`
- `SetOfPoints` выполняет sweep-line для вычисления текущего множества открытых интервалов
- `TotalLength()` возвращает суммарную длину перфорации

### Как `WellScheduleBuilder` организован (для аналогии)

Файл `tests/well_schedule_builder.h`:
- Конструктор: `WellScheduleBuilder(name, x, y)`
- Fluent-методы: `inject_water()`, `produce_oil()`, `shut_in()`, `for_days()`
- `build()` возвращает `WellSchedule` с MER-данными + один `JobsInLayer`
- `add_to_sim()` создаёт `WellJobs` и вызывает `sim.AddWell_FixedProduction()`

## Что реализовать

### Файл: `tests/well_completion_builder.h`

```cpp
#pragma once

#include "test_helpers.h"

namespace test_helpers {

class WellCompletionBuilder {
public:
    // nz — количество пластов, hz — глубина одного пласта (м)
    WellCompletionBuilder(size_t nz, double hz);

    // Открыть пласт layer_id полностью (0, hz) в момент time
    WellCompletionBuilder& open_layer(size_t layer_id, double time);

    // Закрыть пласт layer_id полностью (0, hz) в момент time
    WellCompletionBuilder& close_layer(size_t layer_id, double time);

    // Открыть интервал (start, end) внутри пласта layer_id в момент time
    // Координаты локальные: 0 <= start < end <= hz
    WellCompletionBuilder& open_interval(size_t layer_id,
                                         double start, double end,
                                         double time);

    // Закрыть интервал (start, end) внутри пласта layer_id в момент time
    WellCompletionBuilder& close_interval(size_t layer_id,
                                          double start, double end,
                                          double time);

    // Собрать WellJobsPerLayer для передачи в WellJobs
    reservoir_simulator::WellJobsPerLayer build() const;

private:
    size_t nz_;
    double hz_;
    // jobs_[layer_id] = вектор операций в этом пласте
    reservoir_simulator::WellJobsPerLayer jobs_;
};

} // namespace test_helpers
```

### Реализация (inline в .h)

Конструктор:
```cpp
WellCompletionBuilder(size_t nz, double hz)
    : nz_(nz), hz_(hz), jobs_(nz) {}
```

`open_layer`:
```cpp
WellCompletionBuilder& open_layer(size_t layer_id, double time) {
    jobs_[layer_id].emplace_back(0.0, hz_, true, time);
    return *this;
}
```

`close_layer`:
```cpp
WellCompletionBuilder& close_layer(size_t layer_id, double time) {
    jobs_[layer_id].emplace_back(0.0, hz_, false, time);
    return *this;
}
```

`open_interval`:
```cpp
WellCompletionBuilder& open_interval(size_t layer_id,
                                     double start, double end,
                                     double time) {
    jobs_[layer_id].emplace_back(start, end, true, time);
    return *this;
}
```

`close_interval`:
```cpp
WellCompletionBuilder& close_interval(size_t layer_id,
                                      double start, double end,
                                      double time) {
    jobs_[layer_id].emplace_back(start, end, false, time);
    return *this;
}
```

`build`:
```cpp
reservoir_simulator::WellJobsPerLayer build() const {
    return jobs_;
}
```

## Проверка корректности

Перед переходом к следующему шагу убедись, что `WellCompletionBuilder` компилируется. Создай минимальный тест:

```cpp
auto completions = WellCompletionBuilder(4, 10.0)
    .open_layer(0, 0.0)
    .open_layer(1, 0.0)
    .close_layer(0, 100.0)
    .open_interval(2, 3.0, 7.0, 200.0)
    .build();

// completions.size() == 4
// completions[0].size() == 2 (open + close)
// completions[1].size() == 1 (open)
// completions[2].size() == 1 (partial open)
// completions[3].size() == 0 (no operations)
```

## Частичное вскрытие — как это работает

Ядро GDM уже поддерживает произвольные интервалы. `SetOfPoints` реализует алгебру отрезков через sweep-line:

1. `open (2, 8)` при t=0 → перфорация `[2, 8]`, длина = 6
2. `close (4, 6)` при t=100 → перфорация `[2, 4] ∪ [6, 8]`, длина = 4
3. `open (3, 5)` при t=200 → перфорация `[2, 5] ∪ [6, 8]`, длина = 5

`WellCompletionBuilder` просто генерирует последовательность `WellJobTime` — всю алгебру делает ядро.

Физический смысл:
- `(0, hz)` — полное вскрытие пласта (стандарт)
- `(0, hz/2)` — скважина вскрыта в верхней половине пласта
- `(hz/3, 2*hz/3)` — вскрыта только средняя часть пласта

Длина перфорации напрямую влияет на `factor` в формуле Дюпюи (файл `SomeWell.cpp`, строки 345-356):
```
factor[i] = l[i] * 2π / ln(r_app / r_well)
```
где `l[i]` — длина открытого интервала в пласте i.
