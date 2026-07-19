---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-19
issue: VAL-040
github: 42
branch: val/val-040/perf-regression-baseline
status: в процессе
audit:
  date: 2026-07-19
  pass: 7
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# VAL-040: Regression-тест на производительность (51×51×4)

## Контекст

Фаза 3 (стабилизация ядра) завершена: декомпозиция ReservoirSimulator, новые солверы, True-IMPES weights, PI-контроллер. Нужен автоматический тест, который зафиксирует текущую производительность и предотвратит регрессию при будущих изменениях.

Существующий `test_amgcl_benchmark.cpp` сравнивает конфигурации солверов (21×21×4, 200 дней, `[.slow]`), но:
- не фиксирует baseline
- не проверяет regression (нет CHECK/REQUIRE на iteration count)
- сетка 21×21×4 — меньше целевой 51×51×4

## Подтип

**Инвариантный** — проверяем, что детерминистические метрики (Newton-итерации, AMG-итерации, число шагов) не выросли сверх tolerance. Wall-clock проверяем мягко (WARN).

## Эталонное решение

Self-consistency: baseline = результат первого прогона на текущем коде (ветка `experimental`). Фиксируется как `constexpr` в коде теста. При намеренном изменении производительности (новый солвер, другая схема) — baseline обновляется вручную с комментарием в commit message.

## Сценарий GDM

| Параметр | Значение |
|---|---|
| Сетка | 51×51×4 = 10 404 ячейки |
| Lx, Ly | 500 м × 500 м |
| hz (на слой) | 10 м |
| Проницаемость | 100 мД (однородная) |
| Пористость | 0.2 |
| P_init | 200 атм |
| Sw_init | 0.2 (oil_saturation = 0.8) |
| Скважины | Five-spot: 1 INJ (центр) + 4 PROD (углы) |
| Дебиты | INJ: 50 м³/сут, PROD: 12.5 м³/сут каждая |
| Время | 100 дней |
| Snapshot dt | 10 дней |
| Солвер | CPR_BICGSTAB (production default) |
| Timestep | PI-контроллер (default params) |
| Initial tau | 1.0 дня |

Five-spot выбран как наиболее представительный сценарий: однородный пласт, симметрия, breakthrough за ~30–50 дней.

## Метрики

| Метрика | Проверка | Tolerance | Обоснование |
|---|---|---|---|
| Newton-итерации (суммарно) | REQUIRE | ≤ baseline × 1.05 | Детерминистическая, не зависит от железа |
| Timesteps | REQUIRE | ≤ baseline × 1.05 | Детерминистическая |
| Wasted trials | REQUIRE | ≤ baseline + 2 | Допуск на пограничные шаги |
| Баланс масс | REQUIRE | < 1e-4 | Ослаблен vs 1e-6: 100 дней five-spot с PI накапливает ошибку; benchmark использует 1e-3 |
| Wall-clock time | WARN | ≤ baseline × 1.10 | Зависит от железа, информативный |

## Подводные камни

- ✅ **Детерминизм:** Newton-итерации и timesteps детерминистичны при фиксированных параметрах (нет randomness в солвере). OpenMP в assembly — per-cell `#pragma omp parallel for` без reduction/atomic, каждый поток пишет в свою ячейку → результат не зависит от scheduling. AMGCL SpMV может быть параллельным (builtin backend + OpenMP), но per-row без reduction → детерминистичен. Wall-clock — нет, поэтому только WARN.
- ✅ **PI-контроллер:** при фиксированных параметрах даёт фиксированную последовательность шагов. Baseline включает PI.
- ⚠️ **Debug vs Release:** тест запускается только в Release (`#ifdef NDEBUG`). В Debug — skip или уменьшенная сетка.
- ✅ **Тег `[.slow]`:** тест обнаруживается ctest через `catch_discover_tests` с `TEST_SPEC "[.slow]"`. Запуск: `ctest -R perf` (substring match по имени теста).
- ⚠️ **Баланс масс tolerance:** существующий benchmark использует 1e-3. Для 100 дней five-spot 1e-6 может быть слишком жёстко — зависит от числа шагов и накопления ошибки округления. При первом прогоне зафиксировать реальное значение; если > 1e-6 — ослабить до 1e-4.
- ⚠️ **CMake structure:** тестовых CMakeLists.txt в проекте нет. Все targets определены в корневом `CMakeLists.txt`. Новый файл нужно добавить в существующий target `gdm_benchmark` (не создавать `tests/CMakeLists.txt`).
- ⚠️ **WARN в Catch2:** `WARN(expr)` — это message, не assertion. Для мягкой проверки wall-clock использовать: `if (r.t_total > baseline_time * 1.10) WARN("Wall-clock regression: ...")` или вывод через `std::cout`.

## Связанные заметки

- [[стратегия тестирования GDM]] — пункт 4
- [[roadmap долгосрочный план развития GDM]] — фаза 3, чеклист

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b val/val-040/perf-regression-baseline`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Запомни: 313 тестов, ~96 сек — это baseline
6. Начни с шага 1. После каждого шага: сборка + тесты

---

## Шаг 1: Создать тестовый файл и подключить к CMake

**Цель:** новый TEST_CASE, five-spot 51×51×4, 100 дней, PI-контроллер, CPR_BICGSTAB. Прогон + сбор метрик. Файл сразу подключён к сборке.

**Файлы:**
- `tests/test_perf_regression.cpp` (новый)
- `CMakeLists.txt` (добавить в target `gdm_benchmark`)

**Контекст:**
Переиспользуем паттерн из `test_amgcl_benchmark.cpp`: `MultiLayerCase` + ручной time-stepping loop с подсчётом Newton-итераций, AMG-итераций, wasted trials. Отличие: простой five-spot (1 INJ + 4 PROD) вместо 6 скважин benchmark'а. Солвер — production CPR_BICGSTAB (через `LinearProblem::Solve()`). Файл добавляется в существующий target `gdm_benchmark` в корневом `CMakeLists.txt` (строка 132). Отдельный `tests/CMakeLists.txt` не нужен — его нет в проекте. Тест обнаруживается ctest через `catch_discover_tests(gdm_benchmark ... TEST_SPEC "[.slow]")`.

Ссылка: `examples/ex_benchmark_51x51x4.cpp` использует ту же сетку 51×51×4 с 6 скважинами (730 дней). Наш тест проще: five-spot, 100 дней — быстрее для regression.

**Что сделать:**

1. Создать `tests/test_perf_regression.cpp`
2. Определить параметры сценария (constexpr):
   ```cpp
   #ifdef NDEBUG
   constexpr size_t PNx = 51, PNy = 51, PNz = 4;
   constexpr double Ptotal_time = 100.0;
   #else
   constexpr size_t PNx = 11, PNy = 11, PNz = 2;
   constexpr double Ptotal_time = 10.0;
   #endif
   constexpr double PLx = 500.0, PLy = 500.0, Phz = 10.0;
   constexpr double Psnapshot_dt = 10.0;
   constexpr double Pinit_tau = 1.0;
   ```
   **Внимание:** `MultiLayerCase::initial_tau()` возвращает hardcoded 5.0. Для нашего теста нужно `Pinit_tau = 1.0` — вызвать `sim.numPrm.set_initial_schemeTau(Pinit_tau)` явно (как benchmark при `init_tau > 0`).
3. Определить скважины five-spot:
   - INJ в центре (250, 250), 50 м³/сут, все 4 слоя
   - PROD-1 (25, 25), PROD-2 (475, 25), PROD-3 (25, 475), PROD-4 (475, 475), по 12.5 м³/сут, все 4 слоя
   - Координаты — для сетки 500×500 м, первая ячейка ~(4.9, 4.9), последняя ~(495, 495)
4. Скопировать `run_benchmark` logic (time-stepping loop, profiling из `test_amgcl_benchmark.cpp:162–302`). Обязательно включить PI-контроллер: `sim.numPrm.SetUsePIController(true)` (по умолчанию = false в NumericalParameters.h:51). Без этого вызова тест работает с fixed-tau — другой режим, другой baseline. `SetPIControllerParams` вызывать НЕ нужно — default params (alpha=0.7, beta=0.2, target=12, max_iters=65, safety=1.0) достаточны, как в VAL-039 integration tests
5. Возвращать struct с метриками: `n_time_steps`, `n_newton_iters`, `n_wasted_trials`, `total_amg_iters`, `max_balance_rel`, `t_total`, `t_assembly`, `t_solve`
6. TEST_CASE: `"Performance regression: five-spot 51x51x4"`, теги `[perf][.slow]`
7. Вызвать прогон, вывести report в stdout
8. В корневом `CMakeLists.txt` (строка 132–135) добавить `tests/test_perf_regression.cpp` в target `gdm_benchmark`:
   ```cmake
   add_executable(gdm_benchmark
       tests/test_amgcl_benchmark.cpp
       tests/test_solver_comparison.cpp
       tests/test_perf_regression.cpp
   )
   ```

**Проверка после этого шага:**
- Конфигурация: `cmake -B build -S . -G "Visual Studio 17 2022"` — без ошибок
- Сборка: `cmake --build build --config Release`
- Прогон: `ctest --test-dir build -C Release -R "Performance regression"`
- Тест проходит (пока без CHECK на baseline — только прогон + вывод метрик)

**Оценка:** ~160 строк (тест) + 1 строка (CMake), ~15 минут

---

## Шаг 2: Зафиксировать baseline

**Цель:** запустить тест, получить метрики, записать как constexpr baseline.

**Файлы:** `tests/test_perf_regression.cpp`

**Контекст:**
После шага 1 тест запускается и выводит метрики. Нужно зафиксировать полученные значения как baseline. Метрики детерминистичны (PI-контроллер + Newton + AMG), поэтому повторный прогон даёт те же числа (±0 для итераций, ±epsilon для баланса).

**Что сделать:**

1. Запустить тест: `ctest --test-dir build -C Release -R "Performance regression" --output-on-failure`
2. Из вывода извлечь: `n_time_steps`, `n_newton_iters`, `n_wasted_trials`, `max_balance_rel`
3. Записать baseline как constexpr:
   ```cpp
   constexpr size_t BASELINE_TIMESTEPS = ???;
   constexpr size_t BASELINE_NEWTON_ITERS = ???;
   constexpr size_t BASELINE_WASTED_TRIALS = ???;
   constexpr double BASELINE_TIME_S = ???;
   ```
4. Добавить assertions:
   ```cpp
   REQUIRE(r.n_time_steps <= static_cast<size_t>(BASELINE_TIMESTEPS * 1.05));
   REQUIRE(r.n_newton_iters <= static_cast<size_t>(BASELINE_NEWTON_ITERS * 1.05));
   REQUIRE(r.n_wasted_trials <= BASELINE_WASTED_TRIALS + 2);
   REQUIRE(r.max_balance_rel < 1e-4);  // tolerance ослаблен vs 1e-6; при прогоне проверить реальное значение
   if (r.t_total > BASELINE_TIME_S * 1.10)
       WARN("Wall-clock regression: " << r.t_total << "s vs baseline " << BASELINE_TIME_S << "s");
   ```
   Примечание: `WARN("msg")` — это message-macro в Catch2, не assertion. Для wall-clock это правильно — информируем, не ломаем.
5. Повторно запустить — тест должен быть зелёным

**Проверка после этого шага:**
- `ctest --test-dir build -C Release -R "Performance regression"` — зелёный
- `ctest --test-dir build -C Release` — все 314 тестов зелёные (новый perf-тест видим в ctest, т.к. `catch_discover_tests` регистрирует `[.slow]` как обычный ctest-тест)

**Подводные камни:**
- `catch_discover_tests(gdm_benchmark ... TEST_SPEC "[.slow]")` регистрирует только `[.slow]` тесты из бинарника. Имя теста в ctest: `benchmark.Performance regression: five-spot 51x51x4`. Фильтр: `-R "Performance regression"`.
- Tolerance баланса масс: начни с 1e-4. Если реальный < 1e-6 — ужесточи.

**Оценка:** ~20 строк изменений, ~10 минут (из них ~90 сек прогон)

---

## Шаг 3: CSV-экспорт для истории

**Цель:** каждый прогон записывает метрики в CSV для отслеживания тренда.

**Файлы:** `tests/test_perf_regression.cpp`

**Контекст:**
Паттерн уже есть в `test_amgcl_benchmark.cpp` (`append_csv`). Переиспользуем: CSV с header, append-режим. Путь: `results/perf_regression.csv`.

**Что сделать:**

1. Добавить функцию `append_perf_csv`:
   ```cpp
   void append_perf_csv(const PerfResult& r) {
       fs::create_directories("results");
       const std::string path = "results/perf_regression.csv";
       bool exists = fs::exists(path);
       std::ofstream ofs(path, std::ios::app);
       if (!exists)
           ofs << "time_steps,newton_iters,wasted_trials,total_amg_iters,"
                  "max_balance_rel,t_total_s,t_assembly_s,t_solve_s\n";
       ofs << r.n_time_steps << ","
           << r.n_newton_iters << ","
           << r.n_wasted_trials << ","
           << r.total_amg_iters << ","
           << std::scientific << r.max_balance_rel << ","
           << std::fixed << std::setprecision(3)
           << r.t_total << "," << r.t_assembly << "," << r.t_solve << "\n";
   }
   ```
2. Вызвать в TEST_CASE после прогона

**Проверка после этого шага:**
- `ctest --test-dir build -C Release -R perf`
- Файл `build/results/perf_regression.csv` создан, содержит 1 строку данных

**Оценка:** ~20 строк, ~5 минут

---

## ~~Шаг 4:~~ (объединён с шагом 1)

CMake-подключение включено в шаг 1 — файл добавляется в target `gdm_benchmark` в корневом `CMakeLists.txt` одновременно с созданием.

---

## Шаг 5: Обновить vault и документацию

**Цель:** зафиксировать результат в vault, обновить статус.

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — статус VAL-040
- `vault/GDM/00-home/текущие приоритеты.md` — отметить завершение

**Что сделать:**

1. В `валидационные кейсы.md`: изменить статус VAL-040 на `🟢 ПОКРЫТ ТЕСТАМИ`
2. В `текущие приоритеты.md`: отметить `Regression-тест на производительность` как ✅, указать baseline
3. Прокомментировать GitHub issue #42: baseline, метрики, ветка
4. Закрыть issue (или оставить для merge)

**Проверка после этого шага:**
- Vault актуален
- `ctest --test-dir build -C Release` — 314 тестов зелёные (perf-тест включён в общий прогон)
- `ctest --test-dir build -C Release -R perf` — 1 тест зелёный

**Оценка:** ~5 минут

---

## Критерии завершения

- [ ] `test_perf_regression.cpp` создан, компилируется
- [ ] Тест запускается на 51×51×4 в Release, на 11×11×2 в Debug
- [ ] Baseline зафиксирован (constexpr)
- [ ] REQUIRE на Newton-итерации и timesteps
- [ ] WARN на wall-clock
- [ ] CSV-экспорт работает
- [ ] Все 313 существующих тестов зелёные
- [ ] Perf-тест зелёный при `ctest -R perf`
- [ ] Vault обновлён
- [ ] GitHub issue #42 прокомментирован

---

## Оценка

- **Файлов:** 2 (новый `tests/test_perf_regression.cpp` + правка `CMakeLists.txt`)
- **Строк:** ~180
- **Время:** ~35 минут (включая прогон 51×51×4 ~90 сек)
- **Шагов:** 4 (шаг 4 объединён с шагом 1)
