---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-22
issue: VAL-009
github: 50
branch: val/val-009/single-active-layer
status: в процессе
audit:
  date: 2026-07-22
  round: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# VAL-009: Один активный слой из многих (Nz=4, только k=2 активен)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай `tests/test_inactive_cells.cpp` — там паттерн setup для неактивных ячеек
3. Прочитай `tests/test_helpers.h` — API: `make_uniform_horizon`, `add_simple_well`, `default_num_params`
4. Прочитай `examples/example_runner.h` строки 35–50 — `write_layer_csv` для 3D CSV-экспорта
5. Создай ветку: `git checkout -b val/val-009/single-active-layer experimental`
6. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
7. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
8. Запомни количество тестов и время — это baseline (на момент создания плана: 320 тестов)
9. Начни с шага 1. После каждого шага: сборка + тесты

## Подтип

**Инвариантный.** Проверяем инвариант: 3D-задача с одним активным слоем ≡ 2D-задача.
Эталон — не аналитическое решение, а внутренняя самосогласованность симулятора.

## Суть задачи

Сетка Nx × Ny × 4. Слои k=0, 1, 3 деактивированы, слой k=2 — единственный активный. Скважина INJ перфорирована во всех 4 слоях (без PROD — чистая закачка воды, достаточно для проверки инварианта). Ожидание:

1. Перфорации в слоях 0, 1, 3 отфильтрованы `RemovePerfsAtInactiveCells`
2. Скважины работают только в слое k=2
3. Результат (давление, насыщенность, дебиты, масса) идентичен 2D-задаче (Nx × Ny × 1)

Это инвариант, а не приближение — совпадение до машинной точности.

## Физический контекст

GDM — двухфазный (нефть–вода) симулятор. Неактивные ячейки исключаются из линейной системы:
- `OilField::SetActiveCells` формирует массив только активных ячеек
- `cell_idx_Global2Local[global]` = -1 для неактивных
- `ConvertTriple2Local` → `ConvertGlobal2Local` → ptrdiff_t < 0 для неактивных
- `WellJobs::RemovePerfsAtInactiveCells` очищает перфорации в неактивных слоях
- `GetPressureField` / `GetWaterSaturationField` возвращают вектор размера Nx×Ny×Nz, где неактивные ячейки сохраняют начальные значения

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
    ...
}
perforationsOfWell.AccumulatePerforations(ActiveCells);
```

`AccumulatePerforations` вызывает `RemovePerfsAtInactiveCells(ActiveCells)` — очищает `RawWellPerforationData[i]` для слоёв, где `IsActiveCell[i] == false`.

## Чеклист подводных камней

- [x] **Блокирующие баги:** нет. VAL-007, VAL-008 пройдены — inactive cells работают в 2D
- [x] **Фильтрация перфораций:** `RemovePerfsAtInactiveCells` итерирует по `IsActiveCell.size() == Nz`. Для Nz=4 с 3 неактивными слоями: `ActiveCells = {false, false, true, false}`. Перфорации в слоях 0, 1, 3 будут очищены. ✅ безопасно
- [x] **Индексация GetPressureField:** возвращает вектор size = Nx×Ny×Nz. Для активного слоя k=2 offset = Nx×Ny×2. Сравнивать с 2D-эталоном (offset=0). ✅
- [x] **GetOverallBalance:** суммирует по `ActiveCellsNmbr`. В 3D это Nx×Ny (только 1 активный слой), как в 2D. ✅
- [x] **OilTotal/WaterTotal:** суммируют по `ActiveCellsNmbr` — одинаково для обоих случаев. ✅
- [x] **Численная диффузия:** не применимо — сравниваем GDM с GDM
- [x] **Симметрия:** нет — кейс не симметричен (INJ ≠ PROD)
- [ ] **Неактивные ячейки в GetPressureField:** должны хранить P_init и Sw_init. Стоит проверить отдельным assert
- [x] **Z-связи в графе связности:** `SetConnectivityGraph_3D` (`AbstractGrid.h:228–234, 263–269`) — z-соединения закомментированы. Слои не обмениваются жидкостью по z. Для VAL-009 это нейтрально (один активный слой не имеет z-соседей), но инвариант выполняется не благодаря фильтрации неактивных z-соседей, а из-за отсутствия z-связей вообще. ✅ для данного кейса
- [x] **Z-граничные условия:** `accountForBoundaryConditions` (`JacobianAssembler.cpp:274`) — XY-plane BC отключены (`if (false && (nz > 1))`). Для обоих случаев Z-BC не применяются. ✅
- [x] **RefPressure:** конструктор `ReservoirSimulator` устанавливает `RefPressure = other_properties.extPressure` (строка 40 ReservoirSimulator.cpp), а не дефолт 100.0 Па. `make_uniform_horizon` задаёт `extPressure = P_init_Pa`. Обе задачи используют одинаковое значение. ✅

## Шаги

---

### Шаг 1: Количественный тест — 3D vs 2D эквивалентность

**Цель:** доказать, что 3D-задача (Nz=4, только k=2 активен) даёт идентичные результаты 2D-задаче (Nz=1)

**Файлы:**
- `tests/test_inactive_cells.cpp` — добавить новый TEST_CASE

**Контекст:**
Инвариант VAL-009: при деактивации 3 из 4 слоёв результат должен совпасть с 2D-задачей до машинной точности. Оба сценария используют одинаковые Nx, Ny, физические параметры, скважины. Единственное отличие — Nz (1 vs 4) и деактивация слоёв.

Паттерн: берём существующий тест VAL-008 "Barrier of inactive cells blocks transport" (строки 173–230) как шаблон setup, но без барьера — вместо этого создаём 3D-сетку с деактивированными слоями.

**Что сделать:**

1. Добавить `TEST_CASE("Single active layer equals 2D", "[integration][inactive-cells][val-009]")` в конец `tests/test_inactive_cells.cpp` (перед закрывающей скобкой, если есть, или в конец файла)

2. Тест создаёт два сценария:
   - **2D-эталон:** `make_uniform_horizon(Nx=10, Ny=5, Nz=1, Lx=100, Ly=50, hz=10, perm=100, poro=0.2, P_init=200, oil_sat=0.8)`. INJ в (5, 25), PROD нет — только закачка, чтобы увидеть фронт. `water_mass_rate = -1000.0` (закачка воды). `sim_2d.Solve({0.0, 2.0})`
   - **3D-сценарий:** `make_uniform_horizon(Nx=10, Ny=5, Nz=4, ...)` — те же параметры. Деактивировать слои k=0, 1, 3:
     ```cpp
     for (size_t k = 0; k < 4; ++k)
         if (k != 2)
             for (size_t j = 0; j < Ny; ++j)
                 for (size_t i = 0; i < Nx; ++i)
                     horizon_3d.active_cells[Nx * Ny * k + Nx * j + i] = false;
     ```
     INJ на тех же координатах, перфорация через все 4 слоя (add_simple_well создаёт Nz перфораций автоматически). `sim_3d.Solve({0.0, 2.0})`

3. Сравнить:
   ```cpp
   auto P_2d = sim_2d.GetPressureField();    // size = Nx*Ny*1
   auto Sw_2d = sim_2d.GetWaterSaturationField();
   auto P_3d = sim_3d.GetPressureField();    // size = Nx*Ny*4
   auto Sw_3d = sim_3d.GetWaterSaturationField();

   size_t layer2_offset = Nx * Ny * 2;
   for (size_t l = 0; l < Nx * Ny; ++l) {
       REQUIRE(P_3d[layer2_offset + l] == Approx(P_2d[l]).epsilon(1e-12));
       REQUIRE(Sw_3d[layer2_offset + l] == Approx(Sw_2d[l]).epsilon(1e-12));
   }
   ```

4. Проверить неактивные слои — сохраняют начальные значения:
   ```cpp
   double P_init_Pa = 200.0 * 101325.0;
   double Sw_init = 1.0 - 0.8;  // = 0.2
   for (size_t k : {size_t(0), size_t(1), size_t(3)})
       for (size_t l = 0; l < Nx * Ny; ++l) {
           size_t idx = Nx * Ny * k + l;
           REQUIRE(P_3d[idx] == Approx(P_init_Pa).epsilon(1e-12));
           REQUIRE(Sw_3d[idx] == Approx(Sw_init).epsilon(1e-12));
       }
   ```

5. Проверить массу:
   ```cpp
   REQUIRE(sim_3d.OilTotal() == Approx(sim_2d.OilTotal()).epsilon(1e-12));
   REQUIRE(sim_3d.WaterTotal() == Approx(sim_2d.WaterTotal()).epsilon(1e-12));
   ```

6. Проверить баланс:
   ```cpp
   auto bal_2d = sim_2d.GetOverallBalance();
   auto bal_3d = sim_3d.GetOverallBalance();
   for (size_t i = 0; i < bal_2d.size(); ++i)
       REQUIRE(bal_3d[i] == Approx(bal_2d[i]).epsilon(1e-12));
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все существующие тесты зелёные + новый тест `Single active layer equals 2D` зелёный

**Подводные камни:**
- `add_simple_well` создаёт `WellJobsPerLayer` с Nz элементами (строка 112 test_helpers.h). Для 3D (Nz=4) это 4 элемента. `RemovePerfsAtInactiveCells` очистит 3 из 4. Если Nz различается, well setup различается — но конечный результат (после фильтрации) должен быть идентичен
- `Solve({0.0, 2.0})` — достаточно короткий интервал чтобы фронт не дошёл до границ, но достаточный чтобы Sw изменилась вблизи INJ

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2, 3

**Оценка:** ~60 строк, ~15 минут

---

### Шаг 2: Визуальный тест — CSV-экспорт для сравнения 2D vs 3D

**Цель:** CSV-файлы для визуального подтверждения эквивалентности (Python-скрипт строит overlay plot)

**Файлы:**
- `tests/test_inactive_cells.cpp` — добавить TEST_CASE с тегом `[.visual]`

**Контекст:**
Catch2 assert проверяет числа, но не показывает картину: человек должен увидеть, что поля давления и насыщенности совпадают визуально. Скрытый тест (`[.visual]`) запускается вручную: `build\Release\gdm_tests.exe "[.visual][val-009]"` (ctest не находит `[.visual]` тесты — Catch2 скрывает теги с точкой).

Паттерн: аналогично `Barrier inactive cells - CSV export` (строки 282–323 test_inactive_cells.cpp).

**Что сделать:**

1. Добавить `TEST_CASE("Single active layer - CSV export", "[.visual][inactive-cells][val-009]")` после теста из шага 1

2. Setup идентичен шагу 1, но с более длительной симуляцией (`Solve({0.0, 30.0})`) и усиленной закачкой (`water_mass_rate = -1e6`) для наглядности фронта

3. Экспортировать 3 CSV-файла:
   ```cpp
   std::filesystem::create_directories("results/val-009");

   // 2D-эталон
   std::ofstream ofs_2d("results/val-009/reference_2d.csv");
   ofs_2d << "i,j,P_atm,Sw\n";
   auto P_2d = sim_2d.GetPressureField();
   auto Sw_2d = sim_2d.GetWaterSaturationField();
   for (size_t j = 0; j < Ny; ++j)
       for (size_t i = 0; i < Nx; ++i) {
           size_t l = Nx * j + i;
           ofs_2d << i << "," << j << ","
                  << std::setprecision(8) << P_2d[l] / 101325.0 << ","
                  << Sw_2d[l] << "\n";
       }

   // 3D слой k=2 (активный)
   std::ofstream ofs_3d("results/val-009/layer2_3d.csv");
   ofs_3d << "i,j,P_atm,Sw\n";
   auto P_3d = sim_3d.GetPressureField();
   auto Sw_3d = sim_3d.GetWaterSaturationField();
   size_t offset = Nx * Ny * 2;
   for (size_t j = 0; j < Ny; ++j)
       for (size_t i = 0; i < Nx; ++i) {
           size_t l = Nx * j + i;
           ofs_3d << i << "," << j << ","
                  << std::setprecision(8) << P_3d[offset + l] / 101325.0 << ","
                  << Sw_3d[offset + l] << "\n";
       }

   // Разность
   std::ofstream ofs_diff("results/val-009/difference.csv");
   ofs_diff << "i,j,dP_atm,dSw\n";
   for (size_t j = 0; j < Ny; ++j)
       for (size_t i = 0; i < Nx; ++i) {
           size_t l = Nx * j + i;
           ofs_diff << i << "," << j << ","
                    << std::setprecision(12)
                    << (P_3d[offset + l] - P_2d[l]) / 101325.0 << ","
                    << (Sw_3d[offset + l] - Sw_2d[l]) << "\n";
       }
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные (визуальный тест скрыт по тегу `[.visual]`)
- Ручной прогон визуального теста (Catch2 `[.visual]` скрыт от ctest): `build\Release\gdm_tests.exe "[.visual][val-009]"`
- Файлы `results/val-009/reference_2d.csv`, `layer2_3d.csv`, `difference.csv` созданы

**Зависимости:**
- Требует: шаг 1 (общий setup)
- Блокирует: шаг 3

**Оценка:** ~50 строк, ~10 минут

---

### Шаг 3: Python-скрипт визуализации

**Цель:** overlay plot 2D-эталона и 3D-слоя k=2, карта разностей

**Файлы:**
- `scripts/plot_val009.py` — новый файл

**Контекст:**
Скрипт `scripts/plot_barrier.py` — шаблон (3 subplots: active, pressure, Sw). Для VAL-009 нужны другие subplots: 2D vs 3D overlay для P и Sw, + карта разностей.

**Что сделать:**

1. Создать `scripts/plot_val009.py`:
   ```python
   """
   Визуализация VAL-009: 2D-эталон vs 3D (один активный слой).

   Использование:
       python scripts/plot_val009.py --results-dir results/val-009

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

       ref = np.genfromtxt(os.path.join(args.results_dir, "reference_2d.csv"),
                           delimiter=",", skip_header=1)
       l3d = np.genfromtxt(os.path.join(args.results_dir, "layer2_3d.csv"),
                           delimiter=",", skip_header=1)
       diff = np.genfromtxt(os.path.join(args.results_dir, "difference.csv"),
                            delimiter=",", skip_header=1)

       nx = int(ref[:, 0].max()) + 1
       ny = int(ref[:, 1].max()) + 1

       P_2d = ref[:, 2].reshape(ny, nx)
       Sw_2d = ref[:, 3].reshape(ny, nx)
       P_3d = l3d[:, 2].reshape(ny, nx)
       Sw_3d = l3d[:, 3].reshape(ny, nx)
       dP = diff[:, 2].reshape(ny, nx)
       dSw = diff[:, 3].reshape(ny, nx)

       fig, axes = plt.subplots(2, 3, figsize=(18, 10))

       # Row 1: Pressure
       im = axes[0, 0].imshow(P_2d, origin="lower", cmap="viridis", aspect="auto")
       axes[0, 0].set_title("P [atm] — 2D reference")
       plt.colorbar(im, ax=axes[0, 0])

       im = axes[0, 1].imshow(P_3d, origin="lower", cmap="viridis", aspect="auto")
       axes[0, 1].set_title("P [atm] — 3D layer k=2")
       plt.colorbar(im, ax=axes[0, 1])

       im = axes[0, 2].imshow(dP, origin="lower", cmap="RdBu_r", aspect="auto")
       axes[0, 2].set_title("ΔP [atm] (3D − 2D)")
       plt.colorbar(im, ax=axes[0, 2], format="%.2e")

       # Row 2: Saturation
       im = axes[1, 0].imshow(Sw_2d, origin="lower", cmap="Blues", aspect="auto")
       axes[1, 0].set_title("Sw — 2D reference")
       plt.colorbar(im, ax=axes[1, 0])

       im = axes[1, 1].imshow(Sw_3d, origin="lower", cmap="Blues", aspect="auto")
       axes[1, 1].set_title("Sw — 3D layer k=2")
       plt.colorbar(im, ax=axes[1, 1])

       im = axes[1, 2].imshow(dSw, origin="lower", cmap="RdBu_r", aspect="auto")
       axes[1, 2].set_title("ΔSw (3D − 2D)")
       plt.colorbar(im, ax=axes[1, 2], format="%.2e")

       fig.suptitle("VAL-009: Single active layer (k=2) vs 2D reference", fontsize=14)
       plt.tight_layout()

       out_path = os.path.join(args.results_dir, "val009_comparison.png")
       plt.savefig(out_path, dpi=150)
       print(f"Saved: {out_path}")
       plt.show()

   if __name__ == "__main__":
       main()
   ```

**Проверка после этого шага:**
- `python scripts/plot_val009.py --results-dir results/val-009`
- Визуально: P и Sw первых двух столбцов идентичны, столбец разностей — ≈0 (однородный цвет)

**Зависимости:**
- Требует: шаг 2 (CSV-файлы)
- Блокирует: шаг 4

**Оценка:** ~70 строк, ~10 минут

---

### Шаг 4: Обновить vault и GitHub issue

**Цель:** зафиксировать результат валидации

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — обновить статус VAL-009
- GitHub issue #50 — прокомментировать результатом

**Что сделать:**

Ветка, план и index уже добавлены в vault (при создании плана). Остаётся:

1. В `валидационные кейсы.md`, секция VAL-009:
   - Обновить `- **Статус:** ✅ ПРОЙДЕН <дата>`

2. Прокомментировать issue #50 результатом валидации:
   ```
   gh issue comment 50 --repo ArturSalamatin/GDM --body "VAL-009 пройден. Тест: Single active layer equals 2D. Все метрики совпадают с 2D-эталоном (epsilon 1e-12). Баланс масс ОК. Визуализация: results/val-009/"
   ```

**Проверка после этого шага:**
- Статус обновлён
- Issue прокомментирован

**Зависимости:**
- Требует: шаги 1–3 пройдены
- Блокирует: ничего

**Оценка:** ~5 минут

---

## Тесты

### Тест 1 (количественный)
- **Тест:** `Single active layer equals 2D`
- **Тег:** `[integration][inactive-cells][val-009]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** Nz=4, слои 0/1/3 деактивированы. Скважина INJ перфорирована во всех слоях. Сравнение P, Sw, масс, баланса с 2D-эталоном
- **Setup:** Nx=10, Ny=5, Lx=100, Ly=50, hz=10, perm=100mD, poro=0.2, P=200atm, So=0.8. INJ water_mass_rate=-1000. Solve({0, 2})
- **Эталон:** 2D-задача (Nx=10, Ny=5, Nz=1) с теми же параметрами
- **Метрика:** pointwise Approx (P, Sw), OilTotal, WaterTotal, GetOverallBalance
- **Tolerance:** epsilon 1e-12 (машинная точность)
- **Предотвращает:** регрессию в фильтрации перфораций, ошибки индексации 3D, утечку массы через неактивные слои

### Тест 2 (визуальный)
- **Тест:** `Single active layer - CSV export`
- **Тег:** `[.visual][inactive-cells][val-009]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** то же, но Solve({0, 30}), water_mass_rate=-1e6 для наглядности
- **Setup:** как тест 1, но усиленная закачка и длительная симуляция
- **Эталон:** визуальный — overlay plot
- **Метрика:** визуальное совпадение P и Sw карт, карта разностей ≈ 0
- **Предотвращает:** тонкие ошибки, невидимые в pointwise сравнении (паттерн потока, форма фронта)

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные
- [ ] Новый тест `Single active layer equals 2D` зелёный
- [ ] CSV-экспорт: 3 файла в `results/val-009/`
- [ ] Python-визуализация: overlay plot показывает совпадение
- [ ] Карта разностей ≈ 0 (однородный цвет)
- [ ] Неактивные слои хранят P_init и Sw_init
- [ ] Баланс масс совпадает
- [ ] Vault обновлён: статус, план, index
- [ ] GitHub issue #50 прокомментирован

## Связанные заметки

- [[валидационные кейсы]] — реестр VAL-NNN
- [[val-007 checkerboard-inactive-cells]] — шахматка неактивных ячеек (предшественник)
- [[val-008 barrier-inactive-cells]] — барьер неактивных ячеек (прямой предшественник)
- [[стратегия тестирования GDM]] — пирамида тестов, правило визуальной верификации
