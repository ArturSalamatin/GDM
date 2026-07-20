---
tags:
  - план
  - инфраструктура
date: 2026-07-20
issue: DEBT-006
github: 44
branch: refactor/debt-006/pi-controller-enable
status: реализован
audit:
  date: 2026-07-20
  findings: 0 / 2 / 0
  auto-fixed: 2
  manual-required: 0
---

# DEBT-006: Включить PI-контроллер адаптивного шага по умолчанию

## Контекст

PI-контроллер Söderlind для адаптивного шага по времени (`PIController`) реализован, покрыт тестами (10 юнит + 4 интеграционных), верифицирован через breakthrough (VAL-039, issue #40). Но флаг `use_pi_controller_` по-прежнему `false` — симулятор по умолчанию использует линейную формулу `τ ± factor`.

Задача: переключить дефолт на PI, сохранив возможность отката на линейную формулу.

### Ссылки

- **Issue:** [#44](https://github.com/ArturSalamatin/GDM/issues/44)
- **Vault:** [[PI-контроллер safety=1 и target=12 для Newton-based timestep control]]
- **VAL-039:** [[val-039 pi-controller-breakthrough]] (покрыт тестами 2026-07-19)
- **VAL-032:** Timestep growth после лёгкого шага (зависимость от DEBT-006)

## Подтип: инфраструктура

Переключение дефолтного поведения + адаптация тестов и примеров.

## Текущее состояние

- `NumericalParameters.h:51`: `bool use_pi_controller_ = false;`
- `increase_schemeTau()` / `decrease_schemeTau()`: ветвление `if (use_pi_controller_)` → PI или линейная формула
- Тесты, которые **явно** включают PI: `test_pi_controller_integration.cpp` (4), `test_perf_regression.cpp` (1), `test_amgcl_benchmark.cpp` (5 секций с PI)
- Тесты, которые **не ставят** PI и будут затронуты сменой дефолта: `test_five_spot.cpp`, `test_components.cpp`, `test_buckley_leverett.cpp`, `test_mass_balance.cpp`, `test_smoke.cpp`, `test_stationary_pressure.cpp`, `test_variable_debit.cpp`, `test_visual_verification.cpp`, `test_3d_completions.cpp`, `test_solver_comparison.cpp`, `test_inactive_cells.cpp`, `test_streamlines.cpp`
- `test_NumericalParameters.cpp:56`: `CHECK_FALSE(np.UsesPIController())` — проверка дефолта, **сломается**
- `run_benchmark()` в `test_amgcl_benchmark.cpp:162` и `examples/ex_benchmark_series_ts.cpp:102`: `if (usePIController)` — при `false` не ставит явно `SetUsePIController(false)`, после смены дефолта benchmark "fixed factor" будет работать с PI

## Целевое состояние

1. `use_pi_controller_ = true` по умолчанию
2. Тест дефолта обновлён: `CHECK(np.UsesPIController())`
3. Benchmark-функции: при `usePIController = false` явно выключают PI → сохраняют сравнение "PI vs legacy"
4. Тесты, которые явно включали PI (`SetUsePIController(true)`): удалить лишний вызов (дефолт и так true)
5. Тест VAL-032 добавлен
6. Все существующие тесты зелёные

## Подводные камни

- [x] **Все call sites найдены:** grep по `SetUsePIController`, `use_pi_controller_`, `UsesPIController` — полный список выше
- [x] **Потокобезопасность:** PI-контроллер не используется из параллельных потоков (вызывается из time loop, OpenMP только внутри Newton iteration)
- [x] **Зависимости сборки:** нет — PIController.h/.cpp уже в сборке
- [x] **Обратная совместимость API:** `SetUsePIController(false)` по-прежнему доступен
- [x] **Тесты:** юнит-тест дефолта нужно обновить (строка 56), остальные тесты не проверяют конкретные значения dt
- [x] **Связь с другими задачами:** VAL-032 зависит от DEBT-006 — снимается. VAL-039 уже покрыта и не затронута
- [x] **Производительность:** PI может изменить число шагов и wasted trials. Benchmark "fixed factor" останется для сравнения

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[PI-контроллер safety=1 и target=12 для Newton-based timestep control]]
3. Создай ветку: `git checkout -b refactor/debt-006/pi-controller-enable`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Шаги

### Шаг 1: Переключить дефолт

**Цель:** изменить значение по умолчанию `use_pi_controller_` на `true`

**Файлы:** `HydroSolver/Reservoir/NumericalParameters.h`

**Контекст:**
Строка 51 содержит `bool use_pi_controller_ = false;`. Это единственное место, определяющее дефолт. Все тесты, не вызывающие `SetUsePIController()`, получат PI-контроллер автоматически.

**Что сделать:**
1. В `NumericalParameters.h:51` изменить `false` → `true`

**Изменения:**
До:
```cpp
bool use_pi_controller_ = false;
```
После:
```cpp
bool use_pi_controller_ = true;
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: НЕ прогонять — тест дефолта (шаг 2) сломан, но сборка должна пройти

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2, 3, 4, 5

**Оценка:** 1 строка, 1 минута

---

### Шаг 2: Обновить тест дефолта

**Цель:** тест `NumericalParameters: SetUsePIController toggles PI mode` ожидает `false` по умолчанию — обновить

**Файлы:** `tests/unit/reservoir/test_NumericalParameters.cpp`

**Контекст:**
Строки 53–58 проверяют:
```cpp
CHECK_FALSE(np.UsesPIController());
np.SetUsePIController(true);
CHECK(np.UsesPIController());
```
После смены дефолта `CHECK_FALSE` упадёт. Тест нужно инвертировать: проверять что по умолчанию `true`, переключать на `false`, проверять `false`.

**Что сделать:**
1. Изменить тест:

**Изменения:**
До:
```cpp
TEST_CASE("NumericalParameters: SetUsePIController toggles PI mode",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    CHECK_FALSE(np.UsesPIController());
    np.SetUsePIController(true);
    CHECK(np.UsesPIController());
}
```
После:
```cpp
TEST_CASE("NumericalParameters: SetUsePIController toggles PI mode",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    CHECK(np.UsesPIController());
    np.SetUsePIController(false);
    CHECK_FALSE(np.UsesPIController());
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты должны быть зелёными (если нет — это сигнал о проблемах шага 1, разбирать до продолжения)

**Зависимости:**
- Требует: шаг 1
- Блокирует: ничего (но до прогона тестов лучше выполнить)

**Оценка:** 3 строки, 2 минуты

---

### Шаг 3: Обновить benchmark-функции

**Цель:** при `usePIController = false` явно выключать PI, чтобы benchmark "fixed factor" сохранил старое поведение

**Файлы:**
- `tests/test_amgcl_benchmark.cpp`
- `examples/ex_benchmark_series_ts.cpp`

**Контекст:**
Обе функции `run_benchmark()` имеют паттерн:
```cpp
if (usePIController) {
    sim.numPrm.SetUsePIController(true);
    sim.numPrm.SetPIControllerParams(piParams);
}
```
При `usePIController = false` ничего не делается — раньше дефолт был `false`, всё работало. Теперь дефолт `true`, и "fixed factor" benchmark будет работать с PI — это неправильно, потому что его цель — сравнить PI с линейной формулой.

**Что сделать:**
1. В `tests/test_amgcl_benchmark.cpp` (строка ~184) и `examples/ex_benchmark_series_ts.cpp` (строка ~124): добавить ветку `else`

**Изменения:**
До:
```cpp
if (usePIController) {
    sim.numPrm.SetUsePIController(true);
    sim.numPrm.SetPIControllerParams(piParams);
}
```
После:
```cpp
if (usePIController) {
    sim.numPrm.SetPIControllerParams(piParams);
} else {
    sim.numPrm.SetUsePIController(false);
}
```

Заметь: убран `SetUsePIController(true)` из ветки `if` — дефолт уже `true`, вызов избыточен. Оставлен только `SetPIControllerParams`.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`

**Зависимости:**
- Требует: шаг 1
- Блокирует: ничего

**Оценка:** ~10 строк (2 файла), 5 минут

---

### Шаг 4: Убрать избыточные `SetUsePIController(true)` и исправить условные switch-и

**Цель:** почистить код — явное включение PI теперь избыточно. Но два места используют условное включение (PI vs fixed-dt сравнение) — там нужна замена, не удаление.

**Файлы:**
- `tests/test_pi_controller_integration.cpp` (строки 35, 128, 193, 241, 267)
- `tests/test_perf_regression.cpp` (строка 98)
- `tests/unit/reservoir/test_NumericalParameters.cpp` (строка 86)

**Контекст:**
Большинство вызовов `SetUsePIController(true)` — простые ноопы после смены дефолта. Но два места в `test_pi_controller_integration.cpp` — **условные switch-и**, где `false` ветка должна работать без PI:

1. **Строка 192–193** (тест `PI vs fixed-dt profile comparison`):
   ```cpp
   if (use_pi)
       sim.numPrm.SetUsePIController(true);
   ```
   Лямбда `run_sim(bool use_pi)` вызывается как `run_sim(true)` и `run_sim(false)`. При удалении строки 193, `run_sim(false)` тоже работает с PI → тест PI vs fixed-dt становится PI vs PI.

2. **Строки 263–267** (тест `CSV export for visual verification`):
   `sim2` — «fixed-dt» reference для CSV-экспорта `pi_vs_fixed_sw_profile.csv` и `linear_dt_history.csv`. Не ставит `SetUsePIController(false)` — после смены дефолта оба прогона будут с PI.

**Что сделать:**

**4a. Простые ноопы — удалить:**
1. `test_pi_controller_integration.cpp:35` — `sim.numPrm.SetUsePIController(true);`
2. `test_pi_controller_integration.cpp:128` — `sim.numPrm.SetUsePIController(true);`
3. `test_pi_controller_integration.cpp:241` — `sim.numPrm.SetUsePIController(true);`
4. `test_perf_regression.cpp:98` — `sim.numPrm.SetUsePIController(true);`
5. `test_NumericalParameters.cpp:86` — `np.SetUsePIController(true);`

**4b. Условный switch — заменить:**
6. `test_pi_controller_integration.cpp:192–193`:

До:
```cpp
if (use_pi)
    sim.numPrm.SetUsePIController(true);
```
После:
```cpp
sim.numPrm.SetUsePIController(use_pi);
```

**4c. Добавить явное выключение PI для fixed-dt reference:**
7. `test_pi_controller_integration.cpp:267` (после `sim2.numPrm.set_currentMoment(0.0);`):

До:
```cpp
sim2.numPrm.set_currentMoment(0.0);

test_helpers::add_simple_well(sim2, h2,
```
После:
```cpp
sim2.numPrm.set_currentMoment(0.0);
sim2.numPrm.SetUsePIController(false);

test_helpers::add_simple_well(sim2, h2,
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- Тест `PI controller increase_schemeTau path` (строка 81 в test_NumericalParameters.cpp) после удаления `SetUsePIController(true)` должен по-прежнему идти по PI-ветке — потому что дефолт теперь `true`. Проверить, что тест не сломан.
- Тест `PI vs fixed-dt profile comparison` (строка 177) помечен `[.]` (скрытый) — может не прогоняться в обычном `ctest`. Проверить вручную: `ctest --test-dir build -C Release -R "PI vs fixed"`.

**Зависимости:**
- Требует: шаг 1, шаг 2
- Блокирует: ничего

**Оценка:** ~8 строк (5 удалений, 1 замена, 1 добавление), 7 минут

---

### Шаг 5: Добавить тест VAL-032 — timestep growth после лёгкого шага

**Цель:** закрыть VAL-032 — regression-тест: при малом числе Newton-итераций PI-контроллер увеличивает шаг

**Файлы:** `tests/test_pi_controller_integration.cpp` (добавить тест)

**Контекст:**
VAL-032 формулировка: «один шаг с Newton=1 iter → следующий τ > предыдущего». PI-контроллер вызывает `ComputeMultiplier(1, true)`. При `e_n = 1/65 ≈ 0.015`, `e_target = 12/65 ≈ 0.185`, `pow(e_target/e_n, 0.7) ≈ pow(12, 0.7) ≈ 6.3`, clamped to `max_growth = 2.0`. Шаг удваивается.

Тест проверяет это на уровне NumericalParameters (не через полный Solve), что делает его быстрым и изолированным.

**Тест:**

```
**Тест:** PI controller: timestep growth after easy step (VAL-032)
**Тег:** [unit][level2][reservoir][NumericalParameters][pi-controller][VAL-032]
**Файл:** tests/test_pi_controller_integration.cpp (новый тест)
**Сценарий:** NumericalParameters с PI (дефолт). Установить schemeTau=10, Newton=1, AMG_Error>0. Вызвать increase_schemeTau(). Новый tau > старого.
**Setup:** NumericalParameters с дефолтными параметрами PI
**Ожидание:** CurrentSchemeTau() > 10.0 (PI multiplier > 1 при Newton << target)
**Baseline:** нет (новый тест)
**Backward compat:** если PI выключить (SetUsePIController(false)), линейная формула тоже даёт рост, но с другим множителем
**Предотвращает:** регрессию, при которой PI перестаёт увеличивать шаг при лёгкой сходимости
```

**Что сделать:**
1. Добавить тест в конец `tests/test_pi_controller_integration.cpp`:

```cpp
TEST_CASE("PI controller: timestep growth after easy step (VAL-032)",
          "[unit][level2][reservoir][NumericalParameters][pi-controller][VAL-032]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentMoment(0.0);
    np.set_currentAMG_Error(0.5);
    np.set_currentNewtonIterationCount(1);

    double tau_before = np.CurrentSchemeTau();
    np.increase_schemeTau();
    double tau_after = np.CurrentSchemeTau();

    CHECK(tau_after > tau_before);
    CHECK(tau_after > tau_before * 1.5);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`

**Зависимости:**
- Требует: шаг 1
- Блокирует: ничего

**Оценка:** ~15 строк, 5 минут

---

### Шаг 6: Обновить vault

**Цель:** обновить статус DEBT-006, VAL-032 в vault

**Файлы:**
- `vault/GDM/roadmap/технический долг.md` — DEBT-006: статус → выполняется
- `vault/GDM/roadmap/валидационные кейсы.md` — VAL-032: статус → покрыт тестами, зависимость снята

**Что сделать:**
1. В `технический долг.md`, секция DEBT-006: добавить `- **Статус:** ✅ завершено <дата>` и `- **Ветка:** refactor/debt-006/pi-controller-enable`
2. В `валидационные кейсы.md`, секция VAL-032: обновить статус на `🟢 ПОКРЫТ ТЕСТАМИ (<дата>)`, удалить зависимость от DEBT-006
3. Прокомментировать GitHub issue #44: `Реализовано в ветке refactor/debt-006/pi-controller-enable. Шагов: 6.`

**Зависимости:**
- Требует: шаги 1–5 выполнены, тесты зелёные
- Блокирует: ничего

**Оценка:** 5 минут

---

## Критерии завершения

- [ ] `use_pi_controller_ = true` в `NumericalParameters.h`
- [ ] Тест дефолта обновлён (`CHECK(np.UsesPIController())`)
- [ ] Benchmark-функции: при `usePIController = false` явно выключают PI
- [ ] Избыточные `SetUsePIController(true)` удалены
- [ ] Тест VAL-032 добавлен и зелёный
- [ ] Все существующие тесты зелёные
- [ ] Vault обновлён (DEBT-006, VAL-032)
- [ ] GitHub issue #44 прокомментирован

## Обнаруженные проблемы (аудит 2026-07-20)

Две проблемы найдены и исправлены автоматически:

1. 🟡 **[Измерение 2+5, шаг 4]** `test_pi_controller_integration.cpp:192–193` — условный switch `if (use_pi) SetUsePIController(true)` нельзя просто удалить, иначе тест `PI vs fixed-dt` станет PI vs PI. Заменено на `SetUsePIController(use_pi)`.

2. 🟡 **[Измерение 5, шаг 4]** `test_pi_controller_integration.cpp:263–267` — `sim2` (fixed-dt reference для CSV-экспорта) не ставил `SetUsePIController(false)`. После смены дефолта CSV-файлы `pi_vs_fixed_sw_profile.csv` и `linear_dt_history.csv` стали бы бессмысленными. Добавлен явный `sim2.numPrm.SetUsePIController(false)`.

Остальные измерения (0–6) чистые. VAL-039 покрывает breakthrough-сценарии. Все существующие тесты используют мягкие пороги (`WastedTrialsCount < 50`, Sw bounds, баланс масс) — не должны ломаться от смены стратегии шага.
