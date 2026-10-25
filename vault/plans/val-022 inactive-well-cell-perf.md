---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-28
issue: VAL-022
github: 55
branch: val/val-022/inactive-well-cell-perf
status: реализован
audit:
  date: 2026-07-28
  round: 2
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# VAL-022: Неактивная ячейка скважины в одном слое, активная в других (Nz=3, k=0 off)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай `tests/test_inactive_cells.cpp` — паттерн setup для неактивных ячеек (тесты VAL-009, VAL-010, VAL-019)
3. Прочитай `tests/well_completion_builder.h` — API `WellCompletionBuilder::open_layer(layer_id, time)`
4. Прочитай `tests/well_schedule_builder.h` — API `WellScheduleBuilder::set_completions()`, `inject_water()`, `produce_oil()`, `add_to_sim()`
5. Прочитай `tests/test_helpers.h` — API `make_uniform_horizon`, `default_num_params`
6. Создай ветку: `git checkout -b val/val-022/inactive-well-cell-perf experimental`
7. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
8. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
9. Запомни количество тестов и время — это baseline (на момент создания плана: 327 тестов)
10. Начни с шага 1. После каждого шага: сборка + тесты

## Подтип

**Инвариантный.** Проверяем self-consistency: при деактивации верхнего слоя (k=0) в трёхслойной модели, скважины перфорированные **во всех трёх слоях** (включая неактивный k=0) корректно работают — перфорация в неактивном слое автоматически отфильтрована `RemovePerfsAtInactiveCells`, баланс массы соблюдается, рабочие слои k=1 и k=2 дают ненулевой дебит.

## Суть задачи

Сетка Nx × Ny × 3. Верхний слой k=0 деактивирован. INJ (инжектор) и PROD (продюсер) перфорированы **во всех трёх** слоях (k=0, k=1, k=2) — в отличие от VAL-019, где перфорации задаются только в активных слоях. Ожидание:

1. `RemovePerfsAtInactiveCells` автоматически очищает `RawWellPerforationData[0]` для k=0 (т.к. `ActiveCells[0] == false`)
2. Скважины работают только в слоях k=1 и k=2
3. Неактивный слой сохраняет начальные значения P и Sw
4. Баланс массы соблюдается (≤ 1e-3 относительная невязка)
5. Оба рабочих слоя получают ненулевой вклад через перфорации

**Ключевое отличие от VAL-019:** VAL-019 использует `WellCompletionBuilder` с `open_layer(0).open_layer(2)` — перфорации задаются *только* в активных слоях, неактивный слой (k=1) вообще не получает перфораций. VAL-022 проверяет другой путь: перфорации заданы **во всех слоях** (включая неактивный k=0), и `RemovePerfsAtInactiveCells` должен автоматически отфильтровать перфорации в неактивной ячейке. Это ближе к реальной ситуации: инженер задаёт перфорации по всей глубине, а симулятор сам убирает неработающие.

Также отличие в позиции неактивного слоя: VAL-019 — средний (k=1), VAL-022 — верхний (k=0). Это проверяет граничный случай: первый элемент массива ActiveCells.

## Физический контекст

GDM — двухфазный (нефть–вода) симулятор, IMPES-схема. Неактивные ячейки исключаются из линейной системы:
- `OilField::SetActiveCells` формирует массив только активных ячеек
- `cell_idx_Global2Local[global]` < 0 для неактивных
- `WellJobs::RemovePerfsAtInactiveCells` (`WellJobs.cpp:110–119`) итерирует `IsActiveCell` и очищает `RawWellPerforationData[i]` для слоёв с `IsActiveCell[i] == false`
- `GetPressureField` / `GetWaterSaturationField` возвращают вектор размера Nx×Ny×Nz, неактивные ячейки сохраняют начальные значения

**Z-связи в графе связности закомментированы** (`AbstractGrid.h:228–234, 263–269`). Слои не обмениваются жидкостью по z-направлению. Каждый рабочий слой — независимая 2D-задача. Деактивация слоя k=0 не влияет на потоки в k=1 и k=2 (они и так изолированы). Проверяем корректность *фильтрации перфораций* и *индексации* при наличии неактивного слоя.

## Механизм фильтрации перфораций

Файл `HydroSolver/Reservoir/ReservoirSimulator.cpp`, строки 121–146:
```
for (size_t i = 0; i < nz(); ++i)
    ItsGlobalIDs.push_back({ItsCellId_X, ItsCellId_Y, i});
...
for (size_t l = 0; l < well_local_position.size(); ++l) {
    ptrdiff_t cellIdx = Grid.ConvertTriple2Local(ItsGlobalIDs[l]);
    bool isActive = cellIdx >= 0;
    ActiveCells.push_back(isActive);
}
perforationsOfWell.AccumulatePerforations(ActiveCells);
```

Для Nz=3 с k=0 неактивным: `ActiveCells = {false, true, true}`. `RemovePerfsAtInactiveCells` (`WellJobs.cpp:110–119`) очистит `RawWellPerforationData[0]`.

При использовании `WellCompletionBuilder(3, hz).open_layer(0, 0.0).open_layer(1, 0.0).open_layer(2, 0.0)`:
- `jobs_[0]` содержит перфорацию — будет очищена `RemovePerfsAtInactiveCells`
- `jobs_[1]` и `jobs_[2]` содержат перфорации — останутся
- Результат: скважина работает только в k=1 и k=2

## Чеклист подводных камней

- [x] **Блокирующие баги:** нет. BUG-009 (фильтр ActiveCells) исправлен. VAL-009, VAL-010, VAL-019, VAL-021 пройдены
- [x] **Фильтрация перфораций:** `RemovePerfsAtInactiveCells` итерирует `IsActiveCell.size() == Nz`. Для Nz=3: `ActiveCells = {false, true, true}`. `RawWellPerforationData[0]` будет очищен (перфорации в k=0 удалены). ✅ безопасно
- [x] **WellCompletionBuilder с Nz=3 и 3 open_layer:** `WellCompletionBuilder(3, hz)` создаёт `jobs_(3)`. `.open_layer(0).open_layer(1).open_layer(2)` — все три слоя имеют перфорации. `build()` возвращает вектор из 3 элементов. `WellScheduleBuilder::set_completions` копирует его в `completions_`. `build()` устанавливает `jpl = completions_` (3 элемента). ✅ безопасно
- [x] **Граничный случай k=0:** первый элемент массива `ActiveCells` = false. Цикл `RemovePerfsAtInactiveCells` начинает с `i=0` → `RawWellPerforationData[0]` очищается. Нет off-by-one. ✅
- [x] **Индексация GetPressureField:** вектор size = Nx×Ny×3. Слой k=0: offset 0. Слой k=1: offset Nx×Ny. Слой k=2: offset 2×Nx×Ny. ✅
- [x] **Z-связи отключены:** слои k=1 и k=2 физически изолированы друг от друга и от k=0. Каждый — независимая 2D-задача. ✅ для данного кейса
- [x] **RefPressure:** `make_uniform_horizon` задаёт `extPressure = P_init_Pa`. Конструктор ReservoirSimulator устанавливает `RefPressure = other_properties.extPressure`. ✅
- [x] **Баланс масс:** `GetOverallBalance` суммирует по `ActiveCellsNmbr`. При Nz=3, k=0 off — `ActiveCellsNmbr = 2 × Nx × Ny`. ✅
- [x] **Численная диффузия:** не применимо — self-consistency, не сравнение с аналитикой
- [x] **Симметрия:** кейс несимметричен (k=0 отключён, k=1 и k=2 работают). Не проверяем симметрию

## Шаги

---

### Шаг 1: Количественный тест — скважина перфорирована во всех слоях, k=0 неактивен

**Цель:** доказать self-consistency: при Nz=3 с деактивированным k=0, скважины с перфорациями во всех трёх слоях (включая неактивный) корректно работают — `RemovePerfsAtInactiveCells` отфильтровывает k=0, баланс масс соблюдается, рабочие слои k=1/k=2 дают ненулевой дебит

**Файлы:**
- `tests/test_inactive_cells.cpp` — добавить новый TEST_CASE в конец файла (после строки 1022)

**Контекст:**
Инвариант VAL-022: скважины с перфорациями во **всех** слоях (k=0, k=1, k=2) корректно работают при наличии неактивного верхнего слоя (k=0). Перфорация в k=0 должна быть автоматически удалена `RemovePerfsAtInactiveCells`. Z-связи закомментированы — слои не обмениваются жидкостью по z. Каждый рабочий слой — независимая 2D-задача.

Используем `WellCompletionBuilder(3, hz).open_layer(0, 0.0).open_layer(1, 0.0).open_layer(2, 0.0)` — перфорации во всех трёх слоях. Это ключевое отличие от VAL-019, где перфорации задаются только в активных слоях.

**Что сделать:**

1. Добавить `TEST_CASE("Inactive top layer - perf in all layers filtered",` `"[integration][inactive-cells][val-022]")` в конец `tests/test_inactive_cells.cpp`

2. Setup:
   ```cpp
   using Catch::Approx;
   constexpr size_t Nx = 5, Ny = 5, Nz = 3;
   constexpr double Lx = 50.0, Ly = 50.0, hz = 10.0;
   constexpr double P_init_atm = 200.0;
   constexpr double oil_sat = 0.8;

   auto horizon = test_helpers::make_uniform_horizon(
       Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

   // Деактивировать весь слой k=0 (верхний)
   for (size_t j = 0; j < Ny; ++j)
       for (size_t i = 0; i < Nx; ++i)
           horizon.active_cells[Nx * j + i] = false;
           // k=0: offset = Nx*Ny*0 + Nx*j + i = Nx*j + i

   auto numPrm = test_helpers::default_num_params();
   reservoir_simulator::ReservoirSimulator sim{
       numPrm, horizon, horizon.oil, horizon.water, horizon.other};
   ```

3. Скважины с WellCompletionBuilder — перфорации **во всех** 3 слоях:
   ```cpp
   double hx = Lx / Nx, hy = Ly / Ny;

   // INJ: закачка воды, перфорации в k=0, k=1, k=2
   auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
       .open_layer(0, 0.0).open_layer(1, 0.0).open_layer(2, 0.0);
   test_helpers::WellScheduleBuilder inj("INJ", hx * 0.5, hy * 2.5);
   inj.set_completions(c_inj)
      .inject_water(10.0).for_days(100.0);
   inj.add_to_sim(sim, horizon);

   // PROD: добыча нефти, перфорации в k=0, k=1, k=2
   auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
       .open_layer(0, 0.0).open_layer(1, 0.0).open_layer(2, 0.0);
   test_helpers::WellScheduleBuilder prod("PROD", hx * 4.5, hy * 2.5);
   prod.set_completions(c_prod)
       .produce_oil(8.0).for_days(100.0);
   prod.add_to_sim(sim, horizon);
   ```

4. Записать начальную массу **до** Solve:
   ```cpp
   double oil_mass_0 = sim.OilTotal();
   double water_mass_0 = sim.WaterTotal();
   ```

5. Solve и проверки:
   ```cpp
   sim.Solve({0.0, 100.0});

   auto P = sim.GetPressureField();
   auto Sw = sim.GetWaterSaturationField();

   double P_init_Pa = P_init_atm * 101325.0;
   double Sw_init = 1.0 - oil_sat;  // 0.2

   // Неактивный слой k=0: P и Sw не изменились
   for (size_t l = 0; l < Nx * Ny; ++l) {
       REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
       REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
   }

   // Рабочие слои k=1 и k=2: Sw изменилась вблизи INJ
   size_t inj_i = 0, inj_j = 2;
   size_t inj_cell_k1 = Nx * Ny * 1 + Nx * inj_j + inj_i;
   size_t inj_cell_k2 = Nx * Ny * 2 + Nx * inj_j + inj_i;
   CHECK(Sw[inj_cell_k1] > Sw_init);
   CHECK(Sw[inj_cell_k2] > Sw_init);

   // Баланс масс
   auto bal = sim.GetOverallBalance();
   double oil_residual   = bal[1] + bal[2] - bal[3];
   double water_residual = bal[4] + bal[5] - bal[6];
   REQUIRE(std::abs(oil_residual) / oil_mass_0 < 1e-3);
   if (water_mass_0 > 0)
       REQUIRE(std::abs(water_residual) / water_mass_0 < 1e-3);

   // Физическая корректность
   for (size_t i = 0; i < Nx * Ny * Nz; ++i) {
       REQUIRE(Sw[i] >= 0.0);
       REQUIRE(Sw[i] <= 1.0);
       REQUIRE(P[i] > 0.0);
   }
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все существующие 327 тестов зелёные + новый тест зелёный

**Подводные камни:**
- Индексация k=0: `horizon.active_cells[Nx*j + i]` — это первые Nx×Ny элементов массива (k=0). Убедись, что не перепутал с VAL-019, где `active_cells[Nx*Ny*1 + ...]` (k=1)
- `RemovePerfsAtInactiveCells` обрабатывает `RawWellPerforationData[0]` — перфорация в k=0 удаляется. Перфорации в k=1 и k=2 остаются
- Несбалансированные дебиты (10 vs 8) — намеренно, для проверки баланса при несимметричных условиях

**Зависимости:**
- Требует: ничего
- Блокирует: шаги 2, 3

**Оценка:** ~65 строк, ~15 минут

---

### Шаг 2: Визуальный тест — CSV-экспорт по слоям

**Цель:** CSV-файлы для визуального подтверждения: рабочие слои (k=1, k=2) изменились, неактивный (k=0) — нет

**Файлы:**
- `tests/test_inactive_cells.cpp` — добавить TEST_CASE с тегом `[.visual]` после теста из шага 1

**Контекст:**
Catch2 assert проверяет числа, но не показывает пространственную картину. Нужны CSV для Python-визуализации: карты P и Sw для каждого из 3 слоёв. Скрытый тест (`[.visual]`) запускается вручную: `build\Release\gdm_tests.exe "[.visual][val-022]"`.

Паттерн: аналогично `Inactive middle layer + multizone perf - CSV export` (тест VAL-019, строки 810–868).

**Что сделать:**

1. Добавить `TEST_CASE("Inactive top layer perf filtered - CSV export",` `"[.visual][inactive-cells][val-022]")` после теста из шага 1

2. Setup идентичен шагу 1, но с усиленной закачкой для наглядности фронта:
   ```cpp
   // inject_water(30.0) вместо 10.0
   // produce_oil(20.0) вместо 8.0
   // Solve({0.0, 200.0}) — длиннее, чтобы фронт дошёл
   ```

3. Экспортировать CSV:
   ```cpp
   std::filesystem::create_directories("results/val-022");

   auto P = sim.GetPressureField();
   auto Sw = sim.GetWaterSaturationField();

   for (size_t k = 0; k < Nz; ++k) {
       std::ofstream ofs("results/val-022/layer_" + std::to_string(k) + ".csv");
       ofs << "i,j,Sw,P_atm\n";
       for (size_t j = 0; j < Ny; ++j)
           for (size_t i = 0; i < Nx; ++i) {
               size_t idx = Nx * Ny * k + Nx * j + i;
               ofs << i << "," << j << ","
                   << std::setprecision(8) << Sw[idx] << ","
                   << P[idx] / 101325.0 << "\n";
           }
   }
   ```

4. Экспортировать баланс масс:
   ```cpp
   auto bal = sim.GetOverallBalance();
   std::ofstream bal_ofs("results/val-022/mass_balance.csv");
   bal_ofs << "oil_total,water_total,oil_residual,water_residual\n";
   bal_ofs << sim.OilTotal() << "," << sim.WaterTotal() << ","
           << (bal[1] + bal[2] - bal[3]) << ","
           << (bal[4] + bal[5] - bal[6]) << "\n";
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные
- Ручной запуск: `build\Release\gdm_tests.exe "[.visual][val-022]"`
- Файлы созданы: `results/val-022/layer_0.csv`, `layer_1.csv`, `layer_2.csv`, `mass_balance.csv`

**Подводные камни:**
- `std::filesystem::create_directories` — безопасна если каталог уже существует
- `[.visual]` тег скрывает тест от ctest по умолчанию

**Зависимости:**
- Требует: шаг 1 (setup подтверждён)
- Блокирует: шаг 3

**Оценка:** ~50 строк, ~10 минут

---

### Шаг 3: Python-скрипт визуализации

**Цель:** 3-панельная визуализация: P и Sw для каждого слоя (k=0, k=1, k=2). Неактивный слой k=0 должен быть однородным

**Файлы:**
- `scripts/plot_val022.py` — новый файл

**Контекст:**
Паттерн: `scripts/plot_val019.py` (2×3 subplots: строка 1 = P для k=0,1,2; строка 2 = Sw для k=0,1,2). Для VAL-022 — аналогичная компоновка, но неактивный слой — k=0 (а не k=1).

**Что сделать:**

1. Создать `scripts/plot_val022.py`:
   ```python
   """
   Визуализация VAL-022: 3 слоя (k=0 inactive, k=1 active, k=2 active).
   Перфорации заданы во всех 3 слоях — k=0 отфильтрован RemovePerfsAtInactiveCells.

   Использование:
       python scripts/plot_val022.py --results-dir results/val-022

   Зависимости: numpy, matplotlib
   """
   import argparse
   import os
   import numpy as np
   import matplotlib.pyplot as plt

   def main():
       parser = argparse.ArgumentParser()
       parser.add_argument("--results-dir", required=True)
       args = parser.parse_args()

       layers = {}
       for k in range(3):
           path = os.path.join(args.results_dir, f"layer_{k}.csv")
           layers[k] = np.genfromtxt(path, delimiter=",", skip_header=1)

       nx = int(layers[0][:, 0].max()) + 1
       ny = int(layers[0][:, 1].max()) + 1

       fig, axes = plt.subplots(2, 3, figsize=(18, 10))
       titles = ["k=0 (inactive)", "k=1 (active)", "k=2 (active)"]

       for col, k in enumerate(range(3)):
           Sw = layers[k][:, 2].reshape(ny, nx)
           P  = layers[k][:, 3].reshape(ny, nx)

           im = axes[0, col].imshow(P, origin="lower", cmap="viridis", aspect="auto")
           axes[0, col].set_title(f"P [atm] — {titles[col]}")
           plt.colorbar(im, ax=axes[0, col])

           im = axes[1, col].imshow(Sw, origin="lower", cmap="Blues", aspect="auto",
                                    vmin=0, vmax=1)
           axes[1, col].set_title(f"Sw — {titles[col]}")
           plt.colorbar(im, ax=axes[1, col])

       fig.suptitle("VAL-022: Inactive top layer — perf in all layers filtered", fontsize=14)
       plt.tight_layout()

       out_path = os.path.join(args.results_dir, "val022_comparison.png")
       plt.savefig(out_path, dpi=150)
       print(f"Saved: {out_path}")
       plt.show()

   if __name__ == "__main__":
       main()
   ```

**Проверка после этого шага:**
- `python scripts/plot_val022.py --results-dir results/val-022`
- Визуально: k=0 однороден (P ≈ P_init, Sw ≈ 0.2), k=1 и k=2 показывают фронт воды от INJ к PROD
- P в k=0 ≈ 200 atm, Sw в k=0 ≈ 0.2

**Зависимости:**
- Требует: шаг 2 (CSV-файлы)
- Блокирует: шаг 4

**Оценка:** ~50 строк, ~10 минут

---

### Шаг 4: Обновить vault и GitHub issue

**Цель:** зафиксировать результат валидации

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — обновить статус VAL-022
- `vault/GDM/knowledge/validation/val-022 inactive well cell perf.md` — создать заметку с результатами
- GitHub issue #55 — прокомментировать результатом

**Что сделать:**

1. Создать заметку `vault/GDM/knowledge/validation/val-022 inactive well cell perf.md`:
   ```markdown
   ---
   tags:
     - валидация
     - неактивные-ячейки
     - перфорации
   date: <дата выполнения>
   ---

   # VAL-022: неактивная ячейка скважины — перфорации фильтруются автоматически

   ## Результат: ✅ ПРОЙДЕН

   ## Суть
   Скважина перфорирована во всех 3 слоях (k=0, k=1, k=2). Слой k=0 неактивен.
   `RemovePerfsAtInactiveCells` автоматически удаляет перфорацию из k=0.

   ## Отличие от VAL-019
   - VAL-019: перфорации задаются *только* в активных слоях (через `WellCompletionBuilder`)
   - VAL-022: перфорации задаются *во всех* слоях, фильтрация — автоматическая

   ## Метрики
   - Неактивный слой: P = P_init (epsilon 1e-12), Sw = Sw_init (epsilon 1e-12)
   - Рабочие слои: Sw > Sw_init вблизи INJ
   - Баланс масс: < 1e-3

   ## Связанные заметки
   - [[val-019 inactive-layer-multizone-perf]]
   - [[val-021 barrier two independent reservoirs]]
   ```

2. В `валидационные кейсы.md`, секция VAL-022:
   - Добавить `- **План:** [[val-022 inactive-well-cell-perf]]`
   - Добавить `- **Подробности:** [[val-022 inactive well cell perf]]`
   - Обновить `- **Статус:** ✅ ПРОЙДЕН <дата>`

3. Прокомментировать issue #55:
   ```
   gh issue comment 55 --repo ArturSalamatin/GDM --body "VAL-022 пройден. Тесты: Inactive top layer - perf in all layers filtered ([integration], [.visual]). Метрики: неактивный слой P/Sw не изменился (epsilon 1e-12), рабочие слои k=1/k=2 дают ненулевой Sw вблизи INJ, баланс масс < 1e-3. Визуализация: results/val-022/"
   ```

4. Обновить `vault/GDM/00-home/index.md` — добавить ссылку на новую заметку в секцию validation

**Проверка после этого шага:**
- Статус обновлён
- Заметка создана
- Issue #55 прокомментирован

**Зависимости:**
- Требует: шаги 1–3 пройдены
- Блокирует: ничего

**Оценка:** ~5 минут

---

## Тесты

### Тест 1 (количественный)
- **Тест:** `Inactive top layer - perf in all layers filtered`
- **Тег:** `[integration][inactive-cells][val-022]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** Nz=3, слой k=0 деактивирован. INJ и PROD перфорированы во всех 3 слоях через WellCompletionBuilder. `RemovePerfsAtInactiveCells` фильтрует k=0. Проверка: неактивный слой хранит P_init/Sw_init, рабочие слои имеют Sw > Sw_init вблизи INJ, баланс масс < 1e-3
- **Setup:** Nx=5, Ny=5, Nz=3, Lx=50, Ly=50, hz=10, perm=100mD, poro=0.2, P=200atm, So=0.8. INJ inject_water(10.0), PROD produce_oil(8.0). Solve({0, 100})
- **Эталон:** self-consistency (P_init/Sw_init в k=0, ненулевые изменения в k=1/k=2, баланс масс)
- **Метрика:** Approx P/Sw в неактивном слое (epsilon 1e-12), CHECK Sw > Sw_init в рабочих слоях, relative balance < 1e-3
- **Tolerance:** 1e-12 для P/Sw в неактивном слое, 1e-3 для баланса масс
- **Предотвращает:** регрессию в `RemovePerfsAtInactiveCells` при k=0 (граничный случай — первый элемент массива), ошибки в фильтрации перфораций когда скважина перфорирована во всех слоях

### Тест 2 (визуальный)
- **Тест:** `Inactive top layer perf filtered - CSV export`
- **Тег:** `[.visual][inactive-cells][val-022]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** то же, но усиленная закачка (30.0), длительная симуляция (200 дней) для яркого фронта
- **Setup:** как тест 1, но inject_water(30.0), produce_oil(20.0), Solve({0, 200})
- **Эталон:** визуальный — 3-панельное сравнение слоёв
- **Метрика:** визуально: k=1 и k=2 показывают фронт, k=0 однороден
- **Предотвращает:** тонкие ошибки в пространственном распределении, невидимые в pointwise assert

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные (327 шт.)
- [ ] Новый тест `Inactive top layer - perf in all layers filtered` зелёный
- [ ] CSV-экспорт: файлы `layer_0.csv`, `layer_1.csv`, `layer_2.csv`, `mass_balance.csv` в `results/val-022/`
- [ ] Python-визуализация: 3-панельный plot показывает рабочие слои с фронтом и однородный неактивный слой k=0
- [ ] Неактивный слой k=0 хранит P_init и Sw_init (epsilon 1e-12)
- [ ] Оба рабочих слоя k=1/k=2 дают Sw > Sw_init вблизи INJ
- [ ] Баланс масс < 1e-3
- [ ] Vault обновлён: заметка в `knowledge/validation/`, статус в реестре
- [ ] GitHub issue #55 прокомментирован
- [ ] Заметка добавлена в `index.md`

## Связанные заметки

- [[валидационные кейсы]] — реестр VAL-NNN
- [[val-019 inactive-layer-multizone-perf]] — неактивный средний слой + перфорации в активных (предшественник)
- [[val-021 barrier-two-reservoirs]] — барьер из неактивных ячеек (предшественник)
- [[val-009 single-active-layer]] — один активный слой (предшественник)
- [[val-010 inactive-boundary-cell]] — неактивная угловая ячейка (предшественник)
- [[bug-009 active-cells-filter]] — фикс фильтра ActiveCells
