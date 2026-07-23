---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-23
issue: VAL-019
github: 52
branch: val/val-019/inactive-layer-multizone-perf
status: готов к реализации
audit:
  date: 2026-07-23
  round: 2
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# VAL-019: Неактивные ячейки + многопластовые перфорации (Nz=3, k=1 off)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай `tests/test_inactive_cells.cpp` — паттерн setup для неактивных ячеек (тесты VAL-009, VAL-010)
3. Прочитай `tests/well_completion_builder.h` — API `WellCompletionBuilder::open_layer(layer_id, time)`
4. Прочитай `tests/well_schedule_builder.h` — API `WellScheduleBuilder::set_completions()`, `inject_water()`, `produce_oil()`, `add_to_sim()`
5. Прочитай `tests/test_helpers.h` — API `make_uniform_horizon`, `add_simple_well`, `default_num_params`
6. Создай ветку: `git checkout -b val/val-019/inactive-layer-multizone-perf experimental`
7. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
8. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
9. Запомни количество тестов и время — это baseline (на момент создания плана: 339 тестов)
10. Начни с шага 1. После каждого шага: сборка + тесты

## Подтип

**Инвариантный.** Проверяем self-consistency: при деактивации среднего слоя (k=1) в трёхслойной модели, скважины с многопластовыми перфорациями (слои 0+2) корректно работают — неактивный слой не участвует в потоках, баланс массы соблюдается, оба рабочих слоя вносят ненулевой вклад.

## Суть задачи

Сетка Nx × Ny × 3. Средний слой k=1 деактивирован. INJ (инжектор) и PROD (продюсер) перфорированы в слоях k=0 и k=2. Ожидание:

1. Перфорации в слое k=1 отфильтрованы `RemovePerfsAtInactiveCells` (или не созданы при использовании `WellCompletionBuilder`)
2. Скважины работают только в слоях k=0 и k=2
3. Неактивный слой сохраняет начальные значения P и Sw
4. Баланс массы соблюдается (≤ 1e-3 относительная невязка)
5. Оба рабочих слоя получают ненулевой вклад через перфорации

Отличие от VAL-009: VAL-009 проверяет инвариант «один активный слой ≡ 2D». VAL-019 проверяет корректную работу *нескольких* рабочих слоёв с *разрывом* посередине (слой k=1 неактивен). Это ближе к реальным моделям, где часть пропластков — глина.

## Физический контекст

GDM — двухфазный (нефть–вода) симулятор, IMPES-схема. Неактивные ячейки исключаются из линейной системы:
- `OilField::SetActiveCells` формирует массив только активных ячеек
- `cell_idx_Global2Local[global]` < 0 для неактивных
- `WellJobs::RemovePerfsAtInactiveCells` (`WellJobs.cpp:110–119`) очищает `RawWellPerforationData[i]` для слоёв, где `IsActiveCell[i] == false`
- `GetPressureField` / `GetWaterSaturationField` возвращают вектор размера Nx×Ny×Nz, где неактивные ячейки сохраняют начальные значения

**Критический нюанс:** Z-связи в графе связности *закомментированы* (`AbstractGrid.h:228–234, 263–269`). Слои не обмениваются жидкостью по z-направлению. Это означает:
- Слои k=0 и k=2 работают как две независимые 2D-задачи
- Деактивация слоя k=1 не влияет на потоки в k=0 и k=2 (они и так изолированы)
- Что именно проверяем: корректность *перфораций* и *индексации* при наличии неактивного слоя, а не взаимодействие слоёв

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

Для Nz=3 с k=1 неактивным: `ActiveCells = {true, false, true}`. `RemovePerfsAtInactiveCells` очистит `RawWellPerforationData[1]`.

При использовании `WellCompletionBuilder` с `open_layer(0, 0.0).open_layer(2, 0.0)` — слой 1 вообще не получит перфораций (пустой `JobsInLayer`), но `RemovePerfsAtInactiveCells` всё равно обработает массив размера Nz=3.

## Чеклист подводных камней

- [x] **Блокирующие баги:** нет. BUG-009 (фильтр ActiveCells) исправлен. VAL-009, VAL-010 пройдены
- [x] **Фильтрация перфораций:** `RemovePerfsAtInactiveCells` итерирует по `IsActiveCell.size() == Nz`. Для Nz=3: `ActiveCells = {true, false, true}`. Перфорации в слое 1 будут очищены. ✅ безопасно
- [x] **WellCompletionBuilder с Nz=3:** `WellCompletionBuilder(3, hz)` создаёт `jobs_(3)`. `open_layer(0, 0.0).open_layer(2, 0.0)` — `jobs_[1]` остаётся пустым. `build()` возвращает вектор из 3 элементов. `WellScheduleBuilder::set_completions` копирует его в `completions_`. `build()` устанавливает `jpl = completions_` (3 элемента). `WellJobs(name, jobs_per_layer)` — OK. ✅ безопасно
- [x] **Индексация GetPressureField:** вектор size = Nx×Ny×3. Слой k=0: offset 0. Слой k=1: offset Nx×Ny. Слой k=2: offset 2×Nx×Ny. ✅
- [x] **Z-связи отключены:** слои k=0 и k=2 физически изолированы. Каждый — независимая 2D-задача с INJ+PROD. ✅ для данного кейса
- [x] **RefPressure:** `make_uniform_horizon` задаёт `extPressure = P_init_Pa`. Конструктор ReservoirSimulator устанавливает `RefPressure = other_properties.extPressure`. ✅
- [x] **Баланс масс:** `GetOverallBalance` суммирует по `ActiveCellsNmbr`. При Nz=3, k=1 off — `ActiveCellsNmbr = 2 × Nx × Ny`. ✅
- [x] **Численная диффузия:** не применимо — self-consistency, не сравнение с аналитикой
- [x] **add_simple_well vs WellCompletionBuilder:** `add_simple_well` создаёт Nz перфораций (все слои). При деактивации k=1 фильтрация уберёт перфорацию из слоя 1. `WellCompletionBuilder` создаёт перфорации только в указанных слоях. Оба подхода корректны, но WellCompletionBuilder точнее моделирует реальный сценарий (скважина перфорирована *только* в рабочих пластах). ✅

## Шаги

---

### Шаг 1: Количественный тест — INJ+PROD в двух рабочих слоях с неактивным средним

**Цель:** доказать self-consistency: при Nz=3 с деактивированным k=1, INJ и PROD с completions в k=0 и k=2 корректно работают — баланс масс, неактивный слой не затронут, оба рабочих слоя дают ненулевой дебит

**Файлы:**
- `tests/test_inactive_cells.cpp` — добавить новый TEST_CASE в конец файла

**Контекст:**
Инвариант VAL-019: скважины с многопластовыми перфорациями (k=0, k=2) корректно работают при наличии неактивного среднего слоя (k=1). Z-связи закомментированы — слои не обмениваются жидкостью по z. Поэтому каждый рабочий слой — независимая 2D-задача. Неактивный слой лишь увеличивает массив, но не участвует в потоках.

Используем `WellCompletionBuilder` + `WellScheduleBuilder` для явного задания перфораций (паттерн из `test_3d_completions.cpp:396–417`).

**Что сделать:**

1. Добавить `#include "well_completion_builder.h"` и `#include "well_schedule_builder.h"` в начало `tests/test_inactive_cells.cpp` (после существующих `#include`)

2. Добавить `TEST_CASE("Inactive middle layer + multizone perforation", "[integration][inactive-cells][val-019]")` в конец `tests/test_inactive_cells.cpp`

3. Setup:
   ```cpp
   using Catch::Approx;
   constexpr size_t Nx = 5, Ny = 5, Nz = 3;
   constexpr double Lx = 50.0, Ly = 50.0, hz = 10.0;
   constexpr double P_init_atm = 200.0;
   constexpr double oil_sat = 0.8;

   auto horizon = test_helpers::make_uniform_horizon(
       Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

   // Деактивировать весь слой k=1
   for (size_t j = 0; j < Ny; ++j)
       for (size_t i = 0; i < Nx; ++i)
           horizon.active_cells[Nx * Ny * 1 + Nx * j + i] = false;

   auto numPrm = test_helpers::default_num_params();
   reservoir_simulator::ReservoirSimulator sim{
       numPrm, horizon, horizon.oil, horizon.water, horizon.other};
   ```

4. Скважины с WellCompletionBuilder — перфорации в k=0 и k=2:
   ```cpp
   double hx = Lx / Nx, hy = Ly / Ny;

   // INJ: закачка воды, перфорации в k=0 и k=2
   auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
       .open_layer(0, 0.0).open_layer(2, 0.0);
   test_helpers::WellScheduleBuilder inj("INJ", hx * 0.5, hy * 2.5);
   inj.set_completions(c_inj)
      .inject_water(10.0).for_days(100.0);
   inj.add_to_sim(sim, horizon);

   // PROD: добыча нефти, перфорации в k=0 и k=2
   auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
       .open_layer(0, 0.0).open_layer(2, 0.0);
   test_helpers::WellScheduleBuilder prod("PROD", hx * 4.5, hy * 2.5);
   prod.set_completions(c_prod)
       .produce_oil(8.0).for_days(100.0);
   prod.add_to_sim(sim, horizon);
   ```

5. Записать начальную массу **до** Solve:
   ```cpp
   double oil_mass_0 = sim.OilTotal();
   double water_mass_0 = sim.WaterTotal();
   ```

6. Solve и проверки:
   ```cpp
   sim.Solve({0.0, 100.0});

   auto P = sim.GetPressureField();
   auto Sw = sim.GetWaterSaturationField();

   double P_init_Pa = P_init_atm * 101325.0;
   double Sw_init = 1.0 - oil_sat;  // 0.2

   // Неактивный слой k=1: P и Sw не изменились
   for (size_t l = 0; l < Nx * Ny; ++l) {
       size_t idx = Nx * Ny * 1 + l;
       REQUIRE(P[idx] == Approx(P_init_Pa).epsilon(1e-12));
       REQUIRE(Sw[idx] == Approx(Sw_init).epsilon(1e-12));
   }

   // Рабочие слои: Sw изменилась вблизи INJ
   size_t inj_i = 0, inj_j = 2;
   size_t inj_cell_k0 = Nx * inj_j + inj_i;
   size_t inj_cell_k2 = Nx * Ny * 2 + Nx * inj_j + inj_i;
   CHECK(Sw[inj_cell_k0] > Sw_init);
   CHECK(Sw[inj_cell_k2] > Sw_init);

   // Баланс масс (oil_mass_0 записан до Solve — см. пункт 5)
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
- Все существующие 339 тестов зелёные + новый тест `Inactive middle layer + multizone perforation` зелёный

**Подводные камни:**
- `WellCompletionBuilder(Nz=3, hz)` создаёт `jobs_(3)`. `open_layer(0, 0.0).open_layer(2, 0.0)` — `jobs_[1]` пуст. `RemovePerfsAtInactiveCells` обработает его (безопасно — уже пуст)
- Дебиты (`inject_water(10.0)`, `produce_oil(8.0)`) — умеренные для сетки 5×5. Несбалансированность (10 vs 8) — намеренная, чтобы проверить баланс при несимметричных условиях
- `Solve({0.0, 100.0})` — достаточно для изменения Sw вблизи скважин

**Зависимости:**
- Требует: ничего
- Блокирует: шаги 2, 3

**Оценка:** ~60 строк, ~15 минут

---

### Шаг 2: Визуальный тест — CSV-экспорт по слоям

**Цель:** CSV-файлы для визуального подтверждения: рабочие слои изменились, неактивный — нет

**Файлы:**
- `tests/test_inactive_cells.cpp` — добавить TEST_CASE с тегом `[.visual]`

**Контекст:**
Catch2 assert проверяет числа, но не показывает картину. Нужны CSV для Python-визуализации: карты P и Sw для каждого из 3 слоёв. Скрытый тест (`[.visual]`) запускается вручную: `build\Release\gdm_tests.exe "[.visual][val-019]"`.

Паттерн: аналогично `Single active layer - CSV export` (строки 398–477 test_inactive_cells.cpp) и `Corner cell inactive - CSV export` (строки 661+ test_inactive_cells.cpp).

**Что сделать:**

1. Добавить `TEST_CASE("Inactive middle layer + multizone perf - CSV export", "[.visual][inactive-cells][val-019]")` после теста из шага 1

2. Setup идентичен шагу 1, но с усиленной закачкой для наглядности фронта:
   ```cpp
   // inject_water(30.0) вместо 10.0 — для яркого фронта
   // produce_oil(20.0) вместо 8.0
   // Solve({0.0, 200.0}) — длиннее, чтобы фронт дошёл
   ```

3. Экспортировать CSV:
   ```cpp
   std::filesystem::create_directories("results/val-019");
   // Для каждого слоя: i, j, Sw, P_atm
   auto P = sim.GetPressureField();
   auto Sw = sim.GetWaterSaturationField();

   for (size_t k = 0; k < Nz; ++k) {
       std::ofstream ofs("results/val-019/layer_" + std::to_string(k) + ".csv");
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
   std::ofstream bal_ofs("results/val-019/mass_balance.csv");
   bal_ofs << "oil_total,water_total,oil_residual,water_residual\n";
   auto bal = sim.GetOverallBalance();
   bal_ofs << sim.OilTotal() << "," << sim.WaterTotal() << ","
           << (bal[1] + bal[2] - bal[3]) << ","
           << (bal[4] + bal[5] - bal[6]) << "\n";
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные
- Ручной запуск: `build\Release\gdm_tests.exe "[.visual][val-019]"`
- Файлы созданы: `results/val-019/layer_0.csv`, `layer_1.csv`, `layer_2.csv`, `mass_balance.csv`

**Подводные камни:**
- Каталог `results/` не в git (нет в `.gitignore`, но нет и в дереве) — убедиться, что `std::filesystem::create_directories` не падает
- `[.visual]` тег скрывает тест от `ctest` по дефолту

**Зависимости:**
- Требует: шаг 1 (setup подтверждён)
- Блокирует: шаг 3

**Оценка:** ~50 строк, ~10 минут

---

### Шаг 3: Python-скрипт визуализации

**Цель:** 3-панельная визуализация: P и Sw для каждого слоя (k=0, k=1, k=2). Неактивный слой должен быть однородным

**Файлы:**
- `scripts/plot_val019.py` — новый файл

**Контекст:**
Паттерн: `scripts/plot_val009.py` (overlay 2D vs 3D, 2×3 subplots). Для VAL-019 нужна другая компоновка: 2×3 subplots — строка 1: P для k=0, k=1, k=2; строка 2: Sw для k=0, k=1, k=2. Неактивный слой k=1 должен выглядеть однородным (P = P_init, Sw = Sw_init).

**Что сделать:**

1. Создать `scripts/plot_val019.py`:
   ```python
   """
   Визуализация VAL-019: 3 слоя (k=0 active, k=1 inactive, k=2 active).

   Использование:
       python scripts/plot_val019.py --results-dir results/val-019

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
       titles = ["k=0 (active)", "k=1 (inactive)", "k=2 (active)"]

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

       fig.suptitle("VAL-019: Inactive middle layer + multizone perforation", fontsize=14)
       plt.tight_layout()

       out_path = os.path.join(args.results_dir, "val019_comparison.png")
       plt.savefig(out_path, dpi=150)
       print(f"Saved: {out_path}")
       plt.show()

   if __name__ == "__main__":
       main()
   ```

**Проверка после этого шага:**
- `python scripts/plot_val019.py --results-dir results/val-019`
- Визуально: k=0 и k=2 показывают фронт воды от INJ к PROD, k=1 — однородный (неизменён)
- P в k=1 ≈ P_init, Sw в k=1 ≈ 0.2

**Зависимости:**
- Требует: шаг 2 (CSV-файлы)
- Блокирует: шаг 4

**Оценка:** ~50 строк, ~10 минут

---

### Шаг 4: Обновить vault и GitHub issue

**Цель:** зафиксировать результат валидации

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — обновить статус VAL-019
- GitHub issue #52 — прокомментировать результатом

**Что сделать:**

Ветка и план уже добавлены в vault (при создании плана). Остаётся:

1. В `валидационные кейсы.md`, секция VAL-019:
   - Обновить `- **Статус:** ✅ ПРОЙДЕН <дата>`

2. Прокомментировать issue #52 результатом:
   ```
   gh issue comment 52 --repo ArturSalamatin/GDM --body "VAL-019 пройден. Тесты: Inactive middle layer + multizone perforation ([integration], [.visual]). Все метрики: баланс масс < 1e-3, неактивный слой P/Sw не изменился, оба рабочих слоя дают ненулевой Sw вблизи INJ. Визуализация: results/val-019/"
   ```

**Проверка после этого шага:**
- Статус обновлён
- Issue #52 прокомментирован

**Зависимости:**
- Требует: шаги 1–3 пройдены
- Блокирует: ничего

**Оценка:** ~5 минут

---

## Тесты

### Тест 1 (количественный)
- **Тест:** `Inactive middle layer + multizone perforation`
- **Тег:** `[integration][inactive-cells][val-019]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** Nz=3, слой k=1 деактивирован. INJ и PROD перфорированы в слоях k=0 и k=2 через WellCompletionBuilder. Проверка: неактивный слой хранит P_init/Sw_init, рабочие слои имеют Sw > Sw_init вблизи INJ, баланс масс < 1e-3
- **Setup:** Nx=5, Ny=5, Lx=50, Ly=50, hz=10, perm=100mD, poro=0.2, P=200atm, So=0.8. INJ inject_water(10.0), PROD produce_oil(8.0). Solve({0, 100})
- **Эталон:** self-consistency (P_init/Sw_init в k=1, ненулевые изменения в k=0/k=2, баланс масс)
- **Метрика:** Approx P/Sw в неактивном слое (epsilon 1e-12), CHECK Sw > Sw_init в рабочих слоях, relative balance < 1e-3
- **Tolerance:** 1e-12 для P/Sw в неактивном слое, 1e-3 для баланса масс
- **Предотвращает:** регрессию в фильтрации перфораций при Nz=3, ошибки индексации в multizone completions, утечку массы через неактивный слой

### Тест 2 (визуальный)
- **Тест:** `Inactive middle layer + multizone perf - CSV export`
- **Тег:** `[.visual][inactive-cells][val-019]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** то же, но усиленная закачка (30.0), длительная симуляция (200 дней) для яркого фронта
- **Setup:** как тест 1, но inject_water(30.0), produce_oil(20.0), Solve({0, 200})
- **Эталон:** визуальный — 3-панельное сравнение слоёв
- **Метрика:** визуально: k=0 и k=2 показывают фронт, k=1 однороден
- **Предотвращает:** тонкие ошибки в пространственном распределении, невидимые в pointwise assert

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные (339 шт.)
- [ ] Новый тест `Inactive middle layer + multizone perforation` зелёный
- [ ] CSV-экспорт: файлы `layer_0.csv`, `layer_1.csv`, `layer_2.csv`, `mass_balance.csv` в `results/val-019/`
- [ ] Python-визуализация: 3-панельный plot показывает рабочие слои с фронтом и однородный неактивный слой
- [ ] Неактивный слой k=1 хранит P_init и Sw_init (epsilon 1e-12)
- [ ] Оба рабочих слоя дают Sw > Sw_init вблизи INJ
- [ ] Баланс масс < 1e-3
- [ ] Vault обновлён: статус VAL-019
- [ ] GitHub issue #52 прокомментирован

## Связанные заметки

- [[валидационные кейсы]] — реестр VAL-NNN
- [[val-009 single-active-layer]] — один активный слой (предшественник)
- [[val-010 inactive-boundary-cell]] — неактивная угловая ячейка (предшественник)
- [[bug-009 active-cells-filter]] — фикс фильтра ActiveCells
