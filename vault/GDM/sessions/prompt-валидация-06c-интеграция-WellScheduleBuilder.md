---
tags:
  - prompt
  - валидация
  - 3D
  - перфорации
date: 2026-06-19
---

# Шаг 3: Интеграция WellScheduleBuilder + WellCompletionBuilder

## Задача

Расширить `WellScheduleBuilder`, чтобы он мог принимать `WellCompletionBuilder` для задания многопластовых перфораций вместо захардкоженного одного слоя.

## Текущее поведение

В файле `tests/well_schedule_builder.h`:

Метод `build()` (строки 87-94):
```cpp
WellSchedule build() const {
    reservoir_simulator::JobsInLayer jobs;
    jobs.emplace_back(0.0, hz_, true, 0.0);  // ← один пласт, полная глубина, t=0
    return WellSchedule{ name_, mer_data_, jobs, x_, y_, r_app_ };
}
```

Метод `add_to_sim()` (строки 98-112):
```cpp
void add_to_sim(...) const {
    auto schedule = build();
    reservoir_simulator::WellJobs well_jobs(schedule.name, schedule.jobs);
    //                                                     ↑ один JobsInLayer
    sim.AddWell_FixedProduction(name, mer_data, well_jobs, pos, ...);
}
```

## Что изменить

### 1. Расширить структуру `WellSchedule`

Добавить поле для многопластовых перфораций:

```cpp
struct WellSchedule {
    reservoir_simulator::WellName name;
    reservoir_simulator::mer_descriptor::SingleWell_MER_Data mer_data;
    reservoir_simulator::WellJobsPerLayer jobs_per_layer;  // ← ЗАМЕНИТЬ jobs на jobs_per_layer
    double x, y;
    double r_app;
};
```

### 2. Добавить метод `set_completions`

```cpp
WellScheduleBuilder& set_completions(const WellCompletionBuilder& completions) {
    completions_ = completions.build();
    has_completions_ = true;
    return *this;
}
```

Новое приватное поле:
```cpp
reservoir_simulator::WellJobsPerLayer completions_;
bool has_completions_ = false;
```

### 3. Изменить `build()`

```cpp
WellSchedule build() const {
    reservoir_simulator::WellJobsPerLayer jpl;
    if (has_completions_) {
        jpl = completions_;
    } else {
        // обратная совместимость: один пласт, полная глубина
        reservoir_simulator::JobsInLayer jobs;
        jobs.emplace_back(0.0, hz_, true, 0.0);
        jpl.push_back(jobs);
    }
    return WellSchedule{ name_, mer_data_, jpl, x_, y_, r_app_ };
}
```

### 4. Изменить `add_to_sim()`

```cpp
void add_to_sim(reservoir_simulator::ReservoirSimulator& sim,
                const reservoir_simulator::DevelopedHorizon& horizon) const {
    auto schedule = build();

    double effective_r = schedule.r_app;
    if (effective_r <= 0.0)
        effective_r = 0.2 * horizon.block_size.step_x;

    reservoir_simulator::WellJobs well_jobs(schedule.name, schedule.jobs_per_layer);
    //                                                      ↑ WellJobsPerLayer
    reservoir_simulator::WellPosition pos(schedule.x, schedule.y);

    sim.AddWell_FixedProduction(
        schedule.name, schedule.mer_data, well_jobs, pos,
        horizon.grid_bounds, horizon.block_size, effective_r);
}
```

## Обратная совместимость

Все существующие тесты не вызывают `set_completions()` → `has_completions_ == false` → поведение идентично текущему (один пласт, полная глубина).

## Проверка

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Все 30 тестов должны пройти без изменений.

## Пример использования (для шага 5)

```cpp
auto completions = WellCompletionBuilder(4, 10.0)
    .open_layer(0, 0.0)
    .open_layer(1, 0.0);

WellScheduleBuilder(L"INJ-1", 125.0, 125.0)
    .set_completions(completions)
    .inject_water(40.0).for_days(600.0)
    .add_to_sim(sim, horizon);
```
