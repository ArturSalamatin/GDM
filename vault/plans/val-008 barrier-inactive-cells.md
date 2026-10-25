---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-22
issue: VAL-008
github: 49
branch: val/val-008/barrier-inactive-cells
status: реализован
audit:
  date: 2026-07-22
  round: 3
  findings: 0 / 0 / 4
  auto-fixed: 4
  manual-required: 0
---

# VAL-008: Барьер из неактивных ячеек между INJ и PROD

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[математическая модель двухфазной фильтрации]] — `vault/GDM/atlas/математическая модель двухфазной фильтрации.md`
3. Создай ветку: `git checkout -b val/val-008/barrier-inactive-cells`
4. Собери:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline):
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
6. Baseline: **318 тестов**, все зелёные
7. Начни с шага 1. После каждого шага: сборка + тесты

## Связанные заметки

- [[валидационные кейсы]] — запись VAL-008
- GitHub issue: [#49](https://github.com/ArturSalamatin/GDM/issues/49)
- Связанный кейс: VAL-007 ([#48](https://github.com/ArturSalamatin/GDM/issues/48)) — шахматная деактивация
- Существующие тесты: `tests/test_inactive_cells.cpp` (5 тестов VAL-007)
- Визуализация-шаблон: `scripts/plot_checkerboard.py`

## Подтип

**Инвариантный** — проверка self-consistency: полоса неактивных ячеек поперёк пласта полностью блокирует перенос насыщенности.

## Эталонное решение

Аналитический эталон не нужен. Проверяем инвариант:

1. **Изоляция по Sw:** если INJ и PROD разделены барьером из неактивных ячеек, то Sw в области PROD = Sw₀ (начальная) на протяжении всей симуляции. Закачанная вода не может пройти через барьер.
2. **Накопление в области INJ:** Sw в области INJ > Sw₀ — закачанная вода остаётся в замкнутой области.
3. **Баланс массы:** общий баланс воды и нефти в системе должен сходиться.

Механизм: `SetConnectivityGraph_3D` в [AbstractGrid.h:209-276](HydroSolver/Solver/Grids/AbstractGrid.h#L209-L276) строит граф связности только между активными ячейками. Условие `active_cells[l ± offset]` гарантирует, что неактивные ячейки не попадают в граф → нет потока через барьер. Барьер разрывает connectivityGraph на два изолированных компонента связности.

## Сценарий GDM

- **Сетка:** 2D, Nx=20, Ny=5, Nz=1
- **Размеры:** Lx=200 м, Ly=50 м, hz=10 м (ячейка 10×10×10)
- **Барьер:** столбец i=10 полностью неактивен (5 ячеек), разделяет сетку на:
  - Область INJ: i ∈ [0, 9] — 50 ячеек
  - Барьер: i = 10 — 5 ячеек (неактивны)
  - Область PROD: i ∈ [11, 19] — 45 ячеек
- **Скважины:**
  - INJ: x=5.0, y=25.0 (ячейка i=0, j=2), water_mass_rate=-1000 кг/день (закачка воды)
  - PROD **не используется**: её наличие вызывает падение давления в замкнутой области → контурный поток закачивает воду через границу → Sw растёт (ложное срабатывание теста). См. чеклист «Контурный поток».
- **Физика:**
  - Пористость: 0.2
  - Проницаемость: 100 мД → 100 × 9.869233e-16 м² (SI)
  - P_init: 200 атм
  - So_init: 0.8 → Sw_init: 0.2
  - Фазы: default oil + water (из PhaseFactory)
- **Временной шаг:** dt = 0.1 день, 20 шагов (2 дня модельного времени)
- **Солвер:** Newton tol = 1e-6, max iter = 65, dp_tol = 1e-5, ds_tol = 1e-5

## Метрики

| # | Метрика | Проверка | Tolerance |
|---|---|---|---|
| 1 | Sw за барьером (i > 10) | Sw[l] == Sw₀ для всех l с i ∈ [11, 19] | epsilon 1e-12 |
| 2 | Sw в области INJ | хотя бы одна ячейка с Sw > Sw₀ | строгое неравенство |
| 3 | P и Sw в барьере | P[l] == P_init, Sw[l] == Sw₀ для i = 10 | epsilon 1e-12 |
| 4 | Стабильность | OilTotal() finite, WaterTotal() finite | isfinite |
| 5 | CSV export | файл записан, Sw-карта показывает барьер | visual |

## Чеклист подводных камней

- [x] **Блокирующие баги:** нет — VAL-007 пройден, деактивация работает
- [x] **Граф связности:** `SetConnectivityGraph_3D` проверяет `active_cells[l ± offset]` — барьер разрывает граф. Безопасно.
- [x] **Скважина в неактивной ячейке:** INJ (i=0) далеко от барьера (i=10). PROD не используется (см. «Контурный поток»). Безопасно.
- [x] **GetPressureField/GetWaterSaturationField:** возвращают вектор размера `TotalCellsNmbr` (глобальная индексация). Можно проверять Sw[l] по глобальному индексу. Для неактивных ячеек обращение через `Grid[ConvertGlobal2Local(l)]` — это `CellsInactive[-(idx+1)]`, возвращает начальное состояние.
- [x] **Начальные условия:** Sw₀ = 0.2 одинаковое по всей сетке (horizon.initial_oil_saturation = oil_saturation). Безопасно.
- [x] **RemovePerfsAtInactiveCells:** вызывается в конструкторе WellJobs — перфорации в неактивных ячейках автоматически удаляются. Скважины в активных ячейках — безопасно.
- [x] **Численная диффузия:** upstream-схема может размывать фронт внутри области INJ, но через барьер поток невозможен. Не влияет на тест.
- [x] **Контурный поток (аудит):** `accountForBoundaryConditions` (JacobianAssembler.cpp:145) применяет граничное условие 3-го рода к ячейкам на границе сетки. Если давление в ячейке падает ниже `refPressure` (= P_init), через контур втекает **чистая вода** (f_water=1.0, строка 192). Это означает: если в области PROD есть добывающая скважина → давление падает → через контур втекает вода → Sw растёт → assert Sw == Sw₀ **упадёт**. Решение: убрать PROD из всех тестов. Тест проверяет инвариант изоляции барьером, а не поведение PROD в замкнутой области. Без скважины в области PROD давление не меняется → контурный поток нулевой → Sw = Sw₀.

## Шаги реализации

---

### Шаг 1: Количественный тест — изоляция барьером

**Цель:** добавить Catch2-тест, проверяющий, что Sw за барьером остаётся начальной при наличии полосы неактивных ячеек.

**Файлы:** `tests/test_inactive_cells.cpp` (существующий)

**Контекст:**
Сетка 20×5, столбец i=10 неактивен. INJ (i=0, j=2) закачивает воду. PROD не используется (контурный поток — см. чеклист). Если барьер работает, вода не может пройти на сторону за барьером (i ∈ [11,19]). `GetWaterSaturationField()` возвращает вектор размера Nx×Ny в глобальной индексации — индекс l = Nx*j + i.

**Что сделать:**

1. Добавить тест после строки 171 (конец файла):

```cpp
TEST_CASE("Barrier of inactive cells blocks transport",
          "[integration][inactive-cells][val-008]") {
    using Catch::Approx;

    size_t Nx = 20, Ny = 5, Nz = 1;
    double Lx = 200.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double Sw_init = 1.0 - oil_sat;
    double P_init_Pa = P_init_atm * 101325.0;

    // Барьер: столбец i=10 полностью неактивен
    size_t barrier_i = 10;
    for (size_t j = 0; j < Ny; ++j)
        horizon.active_cells[Nx * j + barrier_i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;

    // INJ слева от барьера (i=0, j=2): закачка воды
    // PROD не добавляем: при добыче из замкнутой области давление падает ниже
    // refPressure → accountForBoundaryConditions закачивает воду через контур → Sw растёт
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1000.0);

    double dt = 0.1;
    for (int step = 0; step < 20; ++step)
        sim.SingleIteration(dt, dt);

    REQUIRE(std::isfinite(sim.OilTotal()));
    REQUIRE(std::isfinite(sim.WaterTotal()));

    auto Sw = sim.GetWaterSaturationField();
    auto P = sim.GetPressureField();

    // Область PROD (i > barrier_i): Sw не изменилась
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = barrier_i + 1; i < Nx; ++i) {
            size_t l = Nx * j + i;
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        }

    // Барьер (i == barrier_i): состояние начальное
    for (size_t j = 0; j < Ny; ++j) {
        size_t l = Nx * j + barrier_i;
        REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
    }

    // Область INJ (i < barrier_i): хотя бы одна ячейка с Sw > Sw_init
    bool inj_has_water = false;
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < barrier_i; ++i) {
            size_t l = Nx * j + i;
            if (Sw[l] > Sw_init + 1e-10)
                inj_has_water = true;
        }
    REQUIRE(inj_has_water);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 319 тестов зелёные (318 existing + 1 новый)

**Подводные камни:**
- PROD не добавляется: при добыче из замкнутой области давление падает ниже refPressure → accountForBoundaryConditions (JacobianAssembler.cpp:192) закачивает чистую воду через контур → Sw растёт → ложное падение assert.
- Скважина INJ с water_mass_rate=-1000 закачивает воду в замкнутую область INJ. Давление растёт → через контур вытекает нефть (dp > 0, f_oil = cell.F_Oil()). Sw в области INJ растёт. Это не влияет на assert в области PROD.

**Зависимости:** нет (первый шаг)

**Оценка:** ~60 строк, ~15 минут

---

### Шаг 2: Тест — состояние сохраняется при полном барьере по Y

**Цель:** добавить тест с барьером по оси Y (строка j=const), чтобы убедиться, что изоляция работает в обоих направлениях.

**Файлы:** `tests/test_inactive_cells.cpp` (существующий)

**Контекст:**
Дополнительная проверка: барьер — строка j=2 (неактивна), INJ при j=0. Сетка 5×5. Если барьер работает только по X, этот тест поймает ошибку.

**Что сделать:**

1. Добавить тест после теста из шага 1:

```cpp
TEST_CASE("Y-direction barrier blocks transport",
          "[integration][inactive-cells][val-008]") {
    using Catch::Approx;

    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double Sw_init = 1.0 - oil_sat;

    // Барьер: строка j=2 полностью неактивна
    size_t barrier_j = 2;
    for (size_t i = 0; i < Nx; ++i)
        horizon.active_cells[Nx * barrier_j + i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;

    // INJ внизу (i=2, j=0): закачка воды
    // PROD не добавляем (см. шаг 1 — контурный поток при dp < 0)
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 2.5, hy * 0.5,
        0.0, -1000.0);

    double dt = 0.1;
    for (int step = 0; step < 20; ++step)
        sim.SingleIteration(dt, dt);

    REQUIRE(std::isfinite(sim.OilTotal()));

    auto Sw = sim.GetWaterSaturationField();

    // Область PROD (j > barrier_j): Sw не изменилась
    for (size_t j = barrier_j + 1; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        }

    // Область INJ: хотя бы одна ячейка с Sw > Sw_init
    bool inj_has_water = false;
    for (size_t j = 0; j < barrier_j; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            if (Sw[l] > Sw_init + 1e-10)
                inj_has_water = true;
        }
    REQUIRE(inj_has_water);
}
```

**Проверка после этого шага:**
- Сборка + тесты: 320 тестов зелёные

**Зависимости:** шаг 1 (компиляция)

**Оценка:** ~55 строк, ~10 минут

---

### Шаг 3: CSV-экспорт и Python-визуализация

**Цель:** добавить visual-тест с CSV-экспортом Sw-поля + Python-скрипт для построения карты с барьером.

**Файлы:**
- `tests/test_inactive_cells.cpp` (существующий) — visual-тест
- `scripts/plot_barrier.py` (новый) — визуализация

**Контекст:**
Шаблон — существующий `plot_checkerboard.py` и тест "Checkerboard inactive cells - CSV export" (строки 133-171). Формат CSV тот же: `i,j,active,P_atm,Sw`. На карте должна быть видна полоса барьера и градиент Sw слева от него.

**Что сделать:**

1. Добавить visual-тест в `test_inactive_cells.cpp`:

```cpp
TEST_CASE("Barrier inactive cells - CSV export",
          "[.visual][inactive-cells][val-008]") {
    size_t Nx = 20, Ny = 5, Nz = 1;
    double Lx = 200.0, Ly = 50.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    size_t barrier_i = 10;
    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j) {
        horizon.active_cells[Nx * j + barrier_i] = false;
        is_inactive[Nx * j + barrier_i] = true;
    }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy * 2.5,
        0.0, -1000.0);

    double dt = 0.1;
    for (int step = 0; step < 20; ++step)
        sim.SingleIteration(dt, dt);

    std::filesystem::create_directories("results/val-008");
    std::ofstream ofs("results/val-008/barrier.csv");
    ofs << "i,j,active,P_atm,Sw\n";

    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();

    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            ofs << i << "," << j << ","
                << (is_inactive[l] ? 0 : 1) << ","
                << std::setprecision(8) << P[l] / 101325.0 << ","
                << Sw[l] << "\n";
        }
}
```

2. Создать `scripts/plot_barrier.py`:

```python
"""
Визуализация барьера из неактивных ячеек (VAL-008).

Использование:
    python scripts/plot_barrier.py --results-dir results/val-008

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

    data = np.genfromtxt(
        os.path.join(args.results_dir, "barrier.csv"),
        delimiter=",", skip_header=1)

    i = data[:, 0].astype(int)
    j = data[:, 1].astype(int)
    active = data[:, 2].astype(int)
    P_atm = data[:, 3]
    Sw = data[:, 4]

    nx = i.max() + 1
    ny = j.max() + 1

    active_map = active.reshape(ny, nx)
    P_map = P_atm.reshape(ny, nx)
    Sw_map = Sw.reshape(ny, nx)

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    ax = axes[0]
    im = ax.imshow(active_map, origin="lower", cmap="RdYlGn",
                   vmin=0, vmax=1, aspect="auto")
    ax.set_title("Active cells (1=active, 0=inactive)")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    ax = axes[1]
    im = ax.imshow(P_map, origin="lower", cmap="viridis", aspect="auto")
    ax.set_title("Pressure [atm]")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    ax = axes[2]
    im = ax.imshow(Sw_map, origin="lower", cmap="Blues",
                   vmin=0, vmax=1, aspect="auto")
    ax.set_title("Water saturation Sw")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    # Отметить скважину INJ
    axes[2].plot(0, 2, "v", color="blue", markersize=10, label="INJ")
    axes[2].legend(loc="upper right")

    plt.suptitle("VAL-008: Barrier of inactive cells", fontsize=14)
    plt.tight_layout()
    out = os.path.join(args.results_dir, "barrier.png")
    plt.savefig(out, dpi=150)
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
```

**Проверка после этого шага:**
- Сборка + тесты: 320 тестов зелёные (visual-тест не входит в обычный прогон — тег `[.visual]`)
- Ручной прогон visual-теста:
  ```powershell
  .\build\tests\Release\GDM_Tests.exe "[.visual][val-008]"
  python scripts/plot_barrier.py --results-dir results/val-008
  ```
- Визуально: на карте Sw видна полоса барьера (столбец i=10), слева Sw > 0.2 (зона INJ), справа Sw = 0.2 (изолированная область)

**Зависимости:** шаг 1

**Оценка:** ~90 строк C++ + ~70 строк Python, ~20 минут

---

### Шаг 4: Обновление vault

**Цель:** обновить статус VAL-008 в реестре, добавить ссылку на план, прокомментировать issue.

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md`
- `vault/GDM/00-home/index.md`

**Что сделать:**

1. В `валидационные кейсы.md`: изменить статус VAL-008 на `🟢 ПРОЙДЕН <дата>`
2. Прокомментировать GitHub issue #49:
   ```powershell
   gh issue comment 49 --repo ArturSalamatin/GDM --body "VAL-008 пройден. 3 теста: barrier X, barrier Y, CSV export. Визуализация: results/val-008/barrier.png"
   ```
3. Если в `index.md` нет ссылки на план — добавить

**Зависимости:** шаги 1–3 (все тесты зелёные + визуальная верификация)

**Оценка:** ~5 минут

---

## Тесты

### Тест 1 (шаг 1)
- **Тест:** "Barrier of inactive cells blocks transport"
- **Тег:** `[integration][inactive-cells][val-008]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** сетка 20×5, столбец i=10 неактивен, только INJ слева от барьера
- **Эталон:** self-consistency (Sw за барьером = Sw₀)
- **Метрика:** Sw за барьером, Sw/P в барьере, Sw > Sw₀ в области INJ
- **Tolerance:** epsilon 1e-12
- **Предотвращает:** некорректную деактивацию, утечку потока через неактивные ячейки

### Тест 2 (шаг 2)
- **Тест:** "Y-direction barrier blocks transport"
- **Тег:** `[integration][inactive-cells][val-008]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** сетка 5×5, строка j=2 неактивна, только INJ внизу
- **Эталон:** self-consistency (Sw за барьером = Sw₀)
- **Метрика:** Sw за барьером, Sw > Sw₀ в области INJ
- **Tolerance:** epsilon 1e-12
- **Предотвращает:** анизотропию деактивации (работает по X, но не по Y)

### Тест 3 (шаг 3)
- **Тест:** "Barrier inactive cells - CSV export"
- **Тег:** `[.visual][inactive-cells][val-008]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** тот же что тест 1, с CSV-экспортом
- **Метрика:** визуальная — карта Sw с барьером
- **Предотвращает:** ошибки в выводе, невидимые для числовых assert

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные (318)
- [ ] 2 новых количественных теста зелёные (320 total)
- [ ] Visual-тест создаёт CSV, Python-скрипт строит карту
- [ ] На карте: барьер виден, Sw слева > 0.2, справа = 0.2
- [ ] Vault обновлён: статус VAL-008 = ПРОЙДЕН
- [ ] GitHub issue #49 прокомментирован
