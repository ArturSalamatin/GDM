---
tags:
  - промпт
  - производительность
  - численные-методы
  - адаптивный-шаг
date: 2026-06-21
---

# Сессия 7: Адаптивный временной шаг

## Предварительно

Прочитай `vault/GDM/00-home/текущие приоритеты.md`.

## Контекст

Текущая стратегия управления шагом — грубая: `factor = 0.15`:
- Успех: `tau_new = tau * (1 + factor)` = `tau * 1.30`
- Откат: `tau_new = tau * (1 - 2*factor)` = `tau * 0.70`

Файл: `HydroSolver/Reservoir/NumericalParameters.h`, строка 19: `static constexpr double factor = 0.15`.
Реализация: `NumericalParameters.cpp`, строки 92–97.

### Проблема

1. Нет обратной связи от качества Newton-итераций. Солвер сходился за 2 итерации → шаг увеличивается на 30%. Солвер сходился за 8 итераций с трудом → шаг всё равно увеличивается на 30%.
2. После отката шаг уменьшается только на 30%. При быстрой дивергенции нужно резче снижать.
3. Нет ограничения на максимальное изменение шага за одну итерацию.

### Текущие числа

На бенчмарке (51×51×4, 730 дней): 187 временных шагов, 0 wasted trials. Шаг начинается маленьким и растёт до максимума. При более сложных сценариях (резкие изменения дебитов, запуск скважин) wasted trials появляются.

## Этап 1: Диагностика — сколько стоят wasted trials

### Инструментация

Добавить в `BenchmarkResult` (или в `SolverProfile`):
- Счётчик wasted trials (уже есть: `solverProfile_.n_wasted_trials`)
- Суммарное время на wasted trials (assembly + solve, которые выбрасываются при откате)

Запустить бенчмарк и посмотреть:
1. Сколько wasted trials на 730 днях? (текущий ответ: 0)
2. Создать стрессовый сценарий: резкий запуск скважины INJ-3 на день 150 с высоким дебитом → ожидаем откаты вокруг t=150

Если wasted trials < 5% от total Newton iterations — адаптивный шаг даст мало. Если > 10% — стоит.

## Этап 2: PI-контроллер

### Теория

PI-контроллер для адаптивного шага (Söderlind, Gustafsson):

```
tau_{n+1} = tau_n * (e_target / e_n)^kP * (e_{n-1} / e_n)^kI
```

Где:
- `e_n` — метрика ошибки на шаге n
- `e_target` — целевая метрика
- `kP`, `kI` — коэффициенты (типично kP=0.4, kI=0.3 для second-order schemes)

### Метрика ошибки для Newton

Для FIM (Fully Implicit Method) хорошая метрика — число Newton-итераций:
```
e_n = newton_iters_n / target_newton_iters
```

Целевое число итераций: 3–4 (хорошая сходимость без лишних итераций).

Упрощённый вариант:
```
tau_{n+1} = tau_n * (target / actual)^alpha
```
с `alpha = 0.5`, `target = 4`, clamped в `[0.5*tau_n, 2.0*tau_n]`.

### Реализация

**Файл: `HydroSolver/Reservoir/NumericalParameters.h`**

Добавить:
```cpp
private:
    int prev_newton_iters_ = 4;  // для PI-компоненты
    static constexpr int target_newton_iters_ = 4;
    static constexpr double kP_ = 0.4;
    static constexpr double kI_ = 0.3;
    static constexpr double max_growth_ = 2.0;
    static constexpr double max_shrink_ = 0.3;

public:
    void adapt_timestep(int newton_iters);
```

**Файл: `HydroSolver/Reservoir/NumericalParameters.cpp`**

```cpp
void NumericalParameters::adapt_timestep(int newton_iters)
{
    if (newton_iters <= 0) newton_iters = 1;
    
    double ratio_P = static_cast<double>(target_newton_iters_) / newton_iters;
    double ratio_I = static_cast<double>(prev_newton_iters_) / newton_iters;
    
    double factor = std::pow(ratio_P, kP_) * std::pow(ratio_I, kI_);
    factor = std::clamp(factor, max_shrink_, max_growth_);
    
    schemeTau = CurrentIntegrationStep() * factor;
    prev_newton_iters_ = newton_iters;
}
```

### Интеграция

**Файл: `HydroSolver/Reservoir/ReservoirSimulator.cpp`**

В `Solve()`, после успешного Newton loop (строка ~409):
```cpp
if (numPrm.IsSuccessfullNewtonTrial())
{
    MassBalance(numPrm.CurrentIntegrationStep());
    Grid.AcceptState();
    numPrm.adapt_timestep(numPrm.CurrentNewtonIterationCount());
    numPrm.update_currentMoment();
    // ...
}
```

Заменить вызов `increase_schemeTau()` в `update_currentMoment()` на `adapt_timestep()`.

### Проверить где вызывается increase_schemeTau

В `NumericalParameters.cpp:57–58`:
```cpp
if (CurrentIntegrationStep() < CurrentTimeStepTillNextSaveMomemnt())
    increase_schemeTau();
```

Это вызывается из `update_currentMoment()`. Нужно убрать `increase_schemeTau()` оттуда и перенести логику в `adapt_timestep()`.

**Логика `update_maxTauAllowed`** (NumericalParameters.cpp:65–90) — ограничивает шаг событиями скважин (смена режима, запуск). Она должна остаться и применяться ПОСЛЕ `adapt_timestep`:
```cpp
schemeTau = std::min(schemeTau, timeStepTillNextSaveMomemnt);
```

### При откате

`decrease_schemeTau()` → резкое уменьшение: `tau * 0.3` вместо `tau * 0.7`. Или сбросить `prev_newton_iters_` в высокое значение (= "прошлый шаг был плохой").

## Этап 3: Бенчмарк

### Метрики

- Число временных шагов (187 → ?)
- Число wasted trials
- Полное время
- Баланс масс

### Стрессовые сценарии

1. **Базовый** (7 скважин, 730 дней) — ожидаем: меньше шагов в начале (когда решение гладкое), но больше в районе t=150 (запуск INJ-3).
2. **Резкое изменение дебита** — `VariableDebitCase` с 10× увеличением на t=200.
3. **Shut-in + restart** — скважина выключается и включается.

### Граница корректности

Адаптивный шаг не должен ухудшать точность:
- Баланс масс: `max_balance_rel < 1e-3` (тот же критерий)
- Физические ограничения: `0 ≤ Sw ≤ 1`, `P > 0`
- Сравнить S_w и P в контрольных точках с фиксированным шагом

## Риски

1. **Стабильность PI-контроллера** — при резком изменении числа итераций может осциллировать. Решение: clamping `[max_shrink, max_growth]`.
2. **Взаимодействие с wasted trial logic** — после отката PI-контроллер может слишком быстро вернуть большой шаг. Решение: сбросить `prev_newton_iters_` при откате.
3. **Event-driven шаг** — `update_maxTauAllowed` ограничивает шаг событиями скважин. PI-контроллер не должен перебивать эти ограничения.

## Ожидаемый эффект

Зависит от сценария:
- Гладкие сценарии без откатов: 187 → 120–150 шагов (–20–35%)
- Сценарии с откатами: меньше wasted trials → –10–20% по времени
- Суммарно: –15–30% total

## Коммиты

```
refactor: PI-контроллер адаптивного временного шага
test: стрессовые сценарии для адаптивного шага
```

## Связанные заметки

- [[prompt-оптимизация-04-iluk-reuse-openmp-adaptive]]
- [[prompt-оптимизация-02-структурные-оптимизации]] — п. 2.4
