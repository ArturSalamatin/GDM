---
tags:
  - план
  - исследование
  - схема
date: 2026-07-14
issue: RES-007
github: 27
branch: research/res-007/bicgstab-vs-lgmres
status: готов к реализации
audit:
  date: 2026-07-14
  pass: 2
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# RES-007: CPR+BiCGStab vs CPR+LGMRES — может ли BiCGStab стать default

## Гипотеза

CPR+BiCGStab даёт сопоставимую точность при ≥25% экономии wall-clock time по сравнению с CPR+LGMRES на типичных задачах GDM (five-spot, variable debit, multi-layer).

**Нулевая гипотеза (H0):** BiCGStab не даёт устойчивого ускорения ≥25% при сохранении точности (баланс масс, число шагов по времени, число wasted trials).

**Критерий принятия:** на **всех** 4 тестовых сценариях BiCGStab быстрее ≥20% **И** n_time_steps совпадает **И** n_wasted_trials совпадает **И** max(oil_balance_rel, water_balance_rel) < 1e-3.

**Критерий отвержения:** хотя бы на одном сценарии BiCGStab медленнее LGMRES, **ИЛИ** n_time_steps различается, **ИЛИ** баланс масс > 1e-3, **ИЛИ** solver_failed = true.

## Контекст

### Предварительные данные

Из DEBT-053 (2026-07-09, session `2026-07-09 solver-factory-debt-053.md`):

| Config | Тесты | Время Release |
|---|---|---|
| CPR (default, LGMRES) | 294/294 | ~105s |
| CPR_BICGSTAB | 294/294 | ~76s |

Это наблюдение на полном наборе тестов, но без контролируемого сравнения на отдельных сценариях. С тех пор добавлены FEAT-010 (threshold fallback), FEAT-011 (True-IMPES weights), DEBT-054..057 (декомпозиция ReservoirSimulator) — baseline мог измениться.

### Математическое обоснование

**LGMRES** (Baker, Jessup, Manteuffel 2005) — вариант restarted GMRES с augmentation из предыдущих циклов. Каждая итерация: 1 matvec + Hessenberg solve. Хранит базис Крылова размера M+K (M=15, K=5 в текущей конфигурации), потребление памяти O(n·20).

**BiCGStab** (van der Vorst 1992) — Крыловский метод для несимметричных систем. Каждая итерация: 2 matvec + 2 precond apply. Не хранит базис, O(n) памяти. Может быть нестабилен (деление на ноль при near-breakdown), но amgcl реализация содержит защиту.

Для CPR-прекондиционированных двухфазных СЛАУ (давление-насыщенность) оба метода должны сходиться. CPR эффективно решает эллиптическую pressure-часть через AMG, оставляя BiCGStab/LGMRES работу с полной гиперболико-параболической системой.

**Почему BiCGStab может быть быстрее:**
1. Нет накопления базиса → меньше overhead на итерацию при фиксированном n
2. CPR уже хорошо прекондиционирует систему → нужно мало итераций внешнего солвера → overhead LGMRES на Hessenberg (O(M²) на итерацию) перевешивает выигрыш от superlinear convergence
3. На малых системах (10k DOF) фиксированные затраты LGMRES (аллокация базиса 20 векторов) значимы

**Почему LGMRES может быть лучше:**
1. Monotone convergence — LGMRES не может застрять (BiCGStab может)
2. Augmentation ускоряет повторные solve (если структура СЛАУ мало меняется между Newton-итерациями)
3. BiCGStab near-breakdown при плохо обусловленных системах

**Ссылки:**
- van der Vorst, H.A. (1992). SIAM J. Sci. Stat. Comput., 13(2), 631–644.
- Baker, Jessup, Manteuffel (2005). SIAM J. Matrix Anal. Appl., 26(4), 962–984.
- Aziz & Settari (1979). Petroleum Reservoir Simulation, Chapter 8.

### Связанные vault-заметки

- [[переход с блочного AMG на скалярный CPR в production]]
- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[amgcl конфигурация iluk k1 новый оптимум]]

### Инфраструктура

Текущая кодовая база уже содержит всё необходимое:

1. **SolverFactory** (DEBT-053): CMake-опция `-DGDM_SOLVER=CPR` / `CPR_BICGSTAB` переключает солвер в compile-time через `SolverConfig.h`
2. **Benchmark** (`tests/test_amgcl_benchmark.cpp`):
   - `run_benchmark()` — production-солвер, полный цикл (assembly → solve → Newton → balance), chrono-замеры
   - `BenchmarkResult` — n_time_steps, n_newton_iters, n_amg_solves, n_wasted_trials, total_amg_iters, max_oil/water_balance_rel, t_total, t_assembly, t_solve
   - `report()` — вывод в stdout
   - `append_csv()` — CSV-экспорт
3. **AMGCL profiler** (`prof.tic/toc`): setup, solve, assemble, reset, fill_rows, boundary, wells, update — гранулярные замеры
4. **SolverProfile**: n_time_steps, n_newton_iters, n_amg_solves, n_wasted_trials, total_amg_iters

**Что НЕ нужно менять в production-коде:** ничего. Эксперимент полностью покрывается переключением CMake-опции и добавлением тестового файла.

---

## Дизайн эксперимента

### Baseline
- Солвер: `CPR` (default) = `cpr<amg<aggregation, ilu0>, ilu0, true_impes_weights> + lgmres`
- Сборка: `cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=CPR`

### Альтернатива
- Солвер: `CPR_BICGSTAB` = `cpr<amg<aggregation, ilu0>, ilu0, true_impes_weights> + bicgstab`
- Сборка: `cmake -B build-bicgstab -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=CPR_BICGSTAB`

### Сценарии

| # | Сценарий | Описание | Тест |
|---|---|---|---|
| S1 | Benchmark multi-well | 21×21×4 (Release), 6 скважин, переменный дебит, 200 дней | `[benchmark][amgcl][seriesCPR]` секция CPR1 |
| S2 | Five-spot 21×21×1 | INJ + PROD, 100 дней, однородный пласт | `[five-spot][2d][benchmark]` |
| S3 | Variable debit 4 сценария | 11×11×1, переменные режимы закачки | `[variable-debit]` (4 TEST_CASEs) |
| S4 | Все тесты | 309 тестов, полный набор | `ctest --test-dir build -C Release` |

**Фиксированные параметры:** AMGCL (aggregation, ILU0, True-IMPES weights), физика (двухфазная, несжимаемая), initial conditions, PVT — всё через test_helpers defaults.

**Варьируемый параметр:** только Крыловский солвер (LGMRES vs BiCGStab), через `-DGDM_SOLVER`.

### Метрики

| Метрика | Источник | Допустимое расхождение |
|---|---|---|
| Wall-clock total (S1) | `chrono` в `run_benchmark` | ожидаем BiCGStab ≥20% быстрее |
| Wall-clock solve (S1) | `chrono_solve` в `run_benchmark` | основной источник ускорения |
| Wall-clock assembly (S1) | `chrono_assembly` в `run_benchmark` | должно совпадать (±2%) |
| n_time_steps (S1) | `BenchmarkResult` | **побитовое совпадение** |
| n_wasted_trials (S1) | `BenchmarkResult` | **побитовое совпадение** |
| n_newton_iters (S1) | `BenchmarkResult` | **побитовое совпадение** |
| total_amg_iters (S1) | `BenchmarkResult` | информативно (BiCGStab считает иначе) |
| max_oil_balance_rel (S1) | `BenchmarkResult` | < 1e-3 |
| max_water_balance_rel (S1) | `BenchmarkResult` | < 1e-3 |
| Wall-clock total (S4) | `ctest` total time | ожидаем ≥25% быстрее |
| Tests passed (S4) | `ctest` | 309/309 для обоих |

**Почему n_time_steps и n_newton_iters должны побитово совпадать:** Newton convergence проверяется через `UpdateGrid()` → `IsSuccessfullNewtonTrial()`, что зависит от решения СЛАУ. Если CPR-прекондиционированная система хорошо обусловлена, и оба солвера сходятся до одинаковой точности (AMG_RelTol), Newton-коррекции будут идентичны → одинаковое число шагов. Расхождение означает, что солверы дают разные решения при одинаковом tolerance — это сигнал для анализа.

---

## Поиск подводных камней

- ✅ **Изоляция фактора:** единственное изменение — `-DGDM_SOLVER`, весь остальной код идентичен
- ✅ **Baseline воспроизводим:** детерминированный код (нет OpenMP в production), повторный запуск даёт ±1% по времени
- ⚠️ **BiCGStab near-breakdown:** amgcl содержит защиту (rho/omega checks), но на вырожденных системах (Sw≈0) может дать другое число итераций → мониторить `solver_failed`
- ⚠️ **amgcl iter count semantics:** LGMRES считает итерации как restarts×M, BiCGStab считает каждый double-step как 1 итерацию. `total_amg_iters` не сравнимы напрямую — использовать только t_solve
- ✅ **Время прогона:** S1 ~4s (benchmark), S2 ~1s (five-spot), S3 ~3s (variable debit), S4 ~105s (CPR) / ~76s (BiCGStab). Суммарно < 5 минут
- ✅ **Блокирующие баги:** нет — FEAT-010/011 уже применены, zero pivot защита встроена
- ✅ **Зависимости:** DEBT-053 ✅, FEAT-011 ✅ — всё готово

---

## Этапы реализации

### Шаг 1: Подготовка и baseline CPR (LGMRES)

**Цель:** зафиксировать baseline — результаты production-солвера CPR+LGMRES на всех 4 сценариях.

**Файлы:** нет изменений в коде

**Контекст:**
Сборка с `-DGDM_SOLVER=CPR` (default). Прогоняем benchmark и полный набор тестов. Результаты — baseline для сравнения.

**Что сделать:**
1. Создать ветку `research/res-007/bicgstab-vs-lgmres` от `experimental`
2. Собрать с default CPR:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
3. Прогнать S1 (benchmark):
   ```powershell
   ctest --test-dir build -C Release -R "Series CPR" --output-on-failure
   ```
   Сохранить stdout (report) — n_time_steps, n_newton_iters, n_amg_solves, n_wasted_trials, total_amg_iters, balance, t_total, t_assembly, t_solve.
4. Прогнать S2 (five-spot):
   ```powershell
   ctest --test-dir build -C Release -R "Five-spot" --output-on-failure
   ```
5. Прогнать S3 (variable debit):
   ```powershell
   ctest --test-dir build -C Release -R "Variable debit" --output-on-failure
   ```
6. Прогнать S4 (все тесты):
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   Записать: total tests, passed, failed, wall-clock.
7. Записать все результаты в таблицу (markdown в vault или CSV).

**Проверка после этого шага:**
- Все тесты зелёные (309/309)
- Результаты benchmark сохранены

**Подводные камни:**
- Benchmark тесты имеют тег `[.slow]`, но зарегистрированы в CTest через `catch_discover_tests(gdm_benchmark, TEST_SPEC "[.slow]")` — они **входят** в обычный `ctest` (2 из 309 тестов). Запуск по отдельности: `ctest -R "Series CPR"`.
- Series CPR содержит 4 SECTION-а. Только CPR1 (`run_benchmark`) использует production solver (зависит от `-DGDM_SOLVER`). CPR2-4 (`run_benchmark_scalar`) используют hardcoded типы — не зависят от GDM_SOLVER. Для сравнения RES-007 релевантен только CPR1.

**Зависимости:** нет

**Оценка:** ~0 строк кода, ~10 минут (сборка + прогон)

---

### Шаг 2: Прогон CPR_BICGSTAB

**Цель:** получить результаты альтернативного солвера BiCGStab на тех же 4 сценариях.

**Файлы:** нет изменений в коде

**Контекст:**
Пересборка с `-DGDM_SOLVER=CPR_BICGSTAB`. Отдельная build-директория чтобы не ломать baseline. Прогоняем те же сценарии.

**Что сделать:**
1. Собрать с CPR_BICGSTAB в отдельную директорию:
   ```powershell
   cmake -B build-bicgstab -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=CPR_BICGSTAB
   cmake --build build-bicgstab --config Release
   ```
2. Прогнать S1 (benchmark):
   ```powershell
   ctest --test-dir build-bicgstab -C Release -R "Series CPR" --output-on-failure
   ```
3. Прогнать S2 (five-spot):
   ```powershell
   ctest --test-dir build-bicgstab -C Release -R "Five-spot" --output-on-failure
   ```
4. Прогнать S3 (variable debit):
   ```powershell
   ctest --test-dir build-bicgstab -C Release -R "Variable debit" --output-on-failure
   ```
5. Прогнать S4 (все тесты):
   ```powershell
   ctest --test-dir build-bicgstab -C Release --output-on-failure
   ```
6. Записать все результаты в ту же таблицу.

**Проверка после этого шага:**
- Все тесты зелёные (309/309) — если нет, зафиксировать какие упали
- Benchmark завершился без `solver_failed`
- Результаты сохранены

**Подводные камни:**
- BiCGStab может дать другое число AMG-итераций (amgcl считает BiCGStab-итерации иначе) → не сравнивать total_amg_iters напрямую, только t_solve
- Если benchmark упал (solver_failed) — зафиксировать на каком шаге, какой residual

**Зависимости:** шаг 1 (для сравнения)

**Оценка:** ~0 строк кода, ~8 минут (сборка + прогон)

---

### Шаг 3: Детальное сравнение — тест с per-timestep метриками

**Цель:** создать тест, который выводит per-timestep статистику для обоих солверов (не только итоговые числа).

**Файлы:**
- `tests/test_solver_comparison.cpp` (новый)
- `CMakeLists.txt` (строка ~124, добавить в `gdm_benchmark`)

**Контекст:**
Benchmark `run_benchmark` выводит суммарные метрики. Для детального анализа нужны per-timestep данные: сколько Newton-итераций на каждом шаге, сколько amg-итераций, residual. Это позволит увидеть, есть ли отдельные шаги, где BiCGStab значимо хуже/лучше.

Тест не зависит от `-DGDM_SOLVER` — он использует production solver (какой бы ни был скомпилирован) и выводит CSV.

**Что сделать:**
1. Создать `tests/test_solver_comparison.cpp`:
   - Сценарий: five-spot 21×21×1, INJ + PROD, 100 дней
   - На каждом Newton-шаге записывать: time_moment, newton_iter, amg_iters, amg_error, dt
   - Вывод в CSV: `results/solver_comparison_<solver_name>.csv`
   - Использовать `#ifdef GDM_SOLVER_CPR_BICGSTAB` для имени файла
   - Тег: `[research][solver-comparison][.slow]`

2. Добавить в CMakeLists.txt

**Изменения:**

Новый файл `tests/test_solver_comparison.cpp`:
```cpp
#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include <chrono>
#include <fstream>
#include <filesystem>

using namespace reservoir_simulator;
namespace fs = std::filesystem;

TEST_CASE("RES-007: per-timestep solver comparison",
          "[research][solver-comparison][.slow]")
{
    fs::create_directories("results");

#ifdef GDM_SOLVER_CPR_BICGSTAB
    const std::string solver_name = "bicgstab";
#else
    const std::string solver_name = "lgmres";
#endif

    std::string csv_path = "results/solver_comparison_" + solver_name + ".csv";
    std::ofstream csv(csv_path);
    csv << "time,dt,newton_iters,amg_solves,total_amg_iters,"
        << "t_step_s,balance_oil_rel,balance_water_rel\n";

    // Five-spot 21x21x1
    auto h = test_helpers::make_uniform_horizon(
        21, 21, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();
    ReservoirSimulator sim(np, h, h.oil, h.water, h.other);

    test_helpers::add_simple_well(sim, h, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim, h, L"PROD", 450.0, 250.0, 5.0, 0.0);

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();

    std::vector<double> times;
    for (double t = 0.0; t <= 100.0; t += 5.0) times.push_back(t);

    using clock = std::chrono::high_resolution_clock;

    for (size_t i = 1; i < times.size(); ++i) {
        SolverProfile step_profile{};
        // snapshot profile before
        auto prof_before = sim.solverProfile_;

        auto t0 = clock::now();
        // solve one interval
        sim.Solve({sim.numPrm.CurrentTimeMoment(), times[i]});
        double dt_s = std::chrono::duration<double>(clock::now() - t0).count();

        // delta
        size_t d_newton = sim.solverProfile_.n_newton_iters - prof_before.n_newton_iters;
        size_t d_amg = sim.solverProfile_.n_amg_solves - prof_before.n_amg_solves;
        size_t d_amg_iters = sim.solverProfile_.total_amg_iters - prof_before.total_amg_iters;

        auto bal = sim.GetOverallBalance();
        double oil_rel = (oil_mass_0 > 0)
            ? std::abs(bal[1] + bal[2] - bal[3]) / oil_mass_0 : 0.0;
        double water_rel = (water_mass_0 > 0)
            ? std::abs(bal[4] + bal[5] - bal[6]) / water_mass_0 : 0.0;

        csv << times[i] << ","
            << (times[i] - times[i-1]) << ","
            << d_newton << ","
            << d_amg << ","
            << d_amg_iters << ","
            << dt_s << ","
            << oil_rel << ","
            << water_rel << "\n";

        CHECK(oil_rel < 1e-3);
        CHECK(water_rel < 1e-3);
    }

    csv.close();
    INFO("CSV written to: " << csv_path);
    SUCCEED();
}
```

CMakeLists.txt — добавить в `gdm_benchmark` (строка ~124):
```cmake
add_executable(gdm_benchmark
    tests/test_amgcl_benchmark.cpp
    tests/test_solver_comparison.cpp      # ← добавить
)
```
Тест автоматически обнаружится через `catch_discover_tests(gdm_benchmark, TEST_SPEC "[.slow]")` и получит prefix `benchmark.`. Не добавлять в `gdm_unit_level4` — это не unit-тест, а benchmark.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "solver-comparison" --output-on-failure`
- CSV создан в `results/`
- Все остальные тесты зелёные

**Подводные камни:**
- `Solve()` внутри использует `time_integrator_.Integrate()`, который обновляет `solverProfile_` — дельта между вызовами корректно показывает per-interval статистику
- CSV пишется в cwd тестового процесса — обычно `build/` (зависит от ctest)

**Зависимости:** шаг 1

**Оценка:** ~70 строк, ~15 минут

---

### Шаг 4: Прогнать per-timestep тест для обоих солверов

**Цель:** получить CSV с per-timestep данными для CPR и CPR_BICGSTAB.

**Файлы:** нет изменений

**Что сделать:**
1. Прогнать в build (CPR):
   ```powershell
   ctest --test-dir build -C Release -R "solver-comparison" --output-on-failure
   ```
   Скопировать `build/results/solver_comparison_lgmres.csv` в vault или scratchpad.

2. Прогнать в build-bicgstab (CPR_BICGSTAB):
   ```powershell
   cmake --build build-bicgstab --config Release
   ctest --test-dir build-bicgstab -C Release -R "solver-comparison" --output-on-failure
   ```
   Скопировать `build-bicgstab/results/solver_comparison_bicgstab.csv`.

**Проверка после этого шага:**
- Оба CSV существуют и содержат 20 строк данных (100 дней / 5 дней = 20 интервалов)
- Все CHECK прошли (balance < 1e-3)

**Зависимости:** шаг 3

**Оценка:** ~0 строк, ~5 минут

---

### Шаг 5: Анализ и вывод

**Цель:** сравнить результаты, сформулировать вывод, создать таблицу.

**Файлы:**
- `vault/GDM/knowledge/numerics/RES-007 результаты bicgstab vs lgmres.md` (новый)

**Что сделать:**
1. Составить сводную таблицу:

```
| Метрика | CPR (LGMRES) | CPR_BICGSTAB | Δ% |
|---|---|---|---|
| S1: t_total | ... | ... | ... |
| S1: t_solve | ... | ... | ... |
| S1: t_assembly | ... | ... | ... |
| S1: n_time_steps | ... | ... | — |
| S1: n_wasted_trials | ... | ... | — |
| S1: n_newton_iters | ... | ... | — |
| S1: balance_ok | ... | ... | — |
| S2: t_total | ... | ... | ... |
| S3: t_total (avg 4) | ... | ... | ... |
| S4: tests passed | ... | ... | — |
| S4: t_total | ... | ... | ... |
```

2. Проверить критерии:
   - ✅/❌ BiCGStab ≥20% быстрее на всех сценариях?
   - ✅/❌ n_time_steps совпадает?
   - ✅/❌ n_wasted_trials совпадает?
   - ✅/❌ balance_ok для обоих?
   - ✅/❌ Все 309 тестов зелёные?

3. Сформулировать вывод:
   - Гипотеза подтверждена/отвергнута
   - Рекомендация

4. Создать vault-заметку с результатами:
   ```
   vault/GDM/knowledge/numerics/RES-007 результаты bicgstab vs lgmres.md
   ```

5. Если гипотеза подтверждена — предложить:
   - Изменить default в CMakeLists.txt: `set(GDM_SOLVER "CPR_BICGSTAB" CACHE STRING ...)`
   - Это будет отдельный коммит (не в рамках RES, а как DEBT или прямое изменение)

6. Обновить vault:
   - `vault/GDM/roadmap/исследования и эксперименты.md`: статус RES-007
   - `vault/GDM/00-home/текущие приоритеты.md`: RES-007 результат
   - `vault/GDM/00-home/index.md`: ссылка на новую заметку

**Проверка после этого шага:**
- Vault-заметка содержит таблицу, вывод, рекомендацию
- Статус RES-007 обновлён

**Зависимости:** шаги 1, 2, 4

**Оценка:** ~0 строк кода, ~20 минут (анализ + vault)

---

### Шаг 6: Очистка

**Цель:** убрать экспериментальные артефакты, прокомментировать issue.

**Файлы:**
- `build-bicgstab/` — удалить
- GitHub issue #27

**Что сделать:**
1. Удалить `build-bicgstab/` (экспериментальная сборка, не нужна)
2. Решить судьбу `test_solver_comparison.cpp`:
   - Если полезен как regression — оставить (тег `[.slow]` не мешает обычным тестам)
   - Если одноразовый — удалить, убрать из CMakeLists.txt
3. Прокомментировать GitHub issue #27 с выводом
4. Если гипотеза подтверждена — не закрывать issue (закроется при merge в experimental)
5. Если отвергнута — закрыть issue с комментарием

**Проверка после этого шага:**
- Все тесты зелёные (309/309 или 310/310 если тест оставлен)
- GitHub issue прокомментирован

**Зависимости:** шаг 5

**Оценка:** ~5 минут

---

## Тесты

### Существующие тесты (regression)

Все 309 тестов должны оставаться зелёными для обоих конфигураций (CPR и CPR_BICGSTAB). Это главный критерий — эксперимент не ломает production.

### Новый тест

**Тест:** RES-007: per-timestep solver comparison
**Тег:** [research][solver-comparison][.slow]
**Файл:** tests/test_solver_comparison.cpp (новый)
**Сценарий:** five-spot 21×21×1, INJ+PROD, 100 дней, per-interval метрики
**Setup:** default_num_params, add_simple_well
**Ожидание:** balance_oil_rel < 1e-3, balance_water_rel < 1e-3 на каждом интервале
**Предотвращает:** BiCGStab нестабильность на отдельных шагах (даже если суммарно balance_ok)

---

## Критерии завершения

- [ ] Baseline CPR (LGMRES) зафиксирован: S1, S2, S3, S4
- [ ] CPR_BICGSTAB прогнан: S1, S2, S3, S4
- [ ] Per-timestep данные собраны для обоих солверов
- [ ] Сводная таблица составлена
- [ ] Вывод сформулирован (подтверждена/отвергнута)
- [ ] Все 309 тестов зелёные для обоих конфигураций
- [ ] Vault обновлён: заметка с результатами, статус RES-007, приоритеты
- [ ] GitHub issue #27 прокомментирован
- [ ] build-bicgstab удалена
- [ ] Если подтверждено — предложено изменение default

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - `vault/GDM/knowledge/decisions/переход с блочного AMG на скалярный CPR в production.md`
   - `vault/GDM/knowledge/decisions/amgcl конфигурация lgmres ilu0 aggregation.md`
3. Посмотри существующий benchmark: `tests/test_amgcl_benchmark.cpp` (строки 145–303 — `BenchmarkResult`, `run_benchmark`)
4. Посмотри SolverConfig: `HydroSolver/Solver/Math/SolverConfig.h` (CPR vs CPR_BICGSTAB switching)
5. Создай ветку: `git checkout -b research/res-007/bicgstab-vs-lgmres experimental`
6. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
7. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
8. Запомни: **309 тестов, ~105s** — baseline CPR (LGMRES)
9. Начни с шага 1
