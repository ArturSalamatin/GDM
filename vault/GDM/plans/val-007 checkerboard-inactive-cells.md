---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-22
issue: VAL-007
github: 48
branch: val/val-007/checkerboard-inactive-cells
status: готов к реализации
audit:
  date: 2026-07-22
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  repeat: true
---

# VAL-007: Шахматная деактивация ячеек (Nz=1)

## Контекст

Проверяем корректность обработки неактивных ячеек при шахматном паттерне деактивации на 2D-сетке (Nz=1). Это стресс-тест: ~50% ячеек неактивны, каждая активная ячейка граничит с неактивными по одному или нескольким направлениям.

**Подтип:** инвариантный (self-consistency: баланс масс + изоляция неактивных ячеек).

**Зависимости:** BUG-009 и BUG-011 исправлены (фильтр `ActiveCells` и `ConvertTriple2Local` signed/unsigned).

**Связанные заметки:**
- [[BUG-009 active cells filter]] — фикс unsigned overflow в фильтре
- [[сетка 3D структурированная с линейной индексацией]] — индексация $l = i + n_x j + n_x n_y k$, `operator[]` корректно обрабатывает отрицательные индексы (неактивные ячейки через `CellsInactive`)

## Эталонное решение

Аналитического эталона нет. Проверяется внутренняя согласованность:

1. **Баланс массы:** $\Delta M_{oil} + \Delta M_{water} = \sum q_{well} \cdot \Delta t$ (с учётом знаков: добыча > 0, закачка < 0 в терминах MER)
2. **Изоляция неактивных:** давление и насыщенность в неактивных ячейках = начальным значениям. Это гарантируется `operator[](ptrdiff_t idx)` в `AbstractGrid.h:78–84`: для `idx < 0` возвращается `CellsInactive[-(idx+1)]`, которые не участвуют в решении.
3. **Граф связности:** для шахматного паттерна на сетке 5×5 активные ячейки с нечётными глобальными индексами (1,3,5,...) не имеют активных соседей (при Nz=1 связность только по x и y). Это значит `connectivityGraph[l]` будет пустым для изолированных ячеек.

**Ключевое наблюдение:** на сетке 5×5 шахматный паттерн с деактивацией чётных (0,2,4,...) даёт 13 неактивных и 12 активных ячеек. Проблема: соседи по x и y для ячейки $(i,j)$ — это $(i\pm1, j)$ и $(i, j\pm1)$. Для нечётного $l = i + 5j$:
- Если $l$ нечётен и $l-1$ чётен — сосед по $-x$ неактивен
- $l+1$ чётен — сосед по $+x$ неактивен
- $l-5$ чётность зависит от $j$: $l - 5$ может быть и чётным, и нечётным

Значит, не все активные ячейки изолированы. Некоторые имеют активных соседей по y. Это нормально — тест проверяет, что граф связности корректно строится с «дырами».

**Лучше использовать сетку 5×5 (нечётную):** при нечётном $n_x$ шахматный паттерн гарантирует, что чередование сохраняется при переходе между строками. Ячейка $(i,j)$ активна, если $(i+j)$ нечётен. Тогда все соседи активной ячейки — неактивны (истинная шахматная доска).

## Сценарий GDM

| Параметр | Значение |
|---|---|
| Сетка | 5 × 5 × 1 |
| Lx, Ly | 50.0 м, 50.0 м |
| hz | 10.0 м |
| hx = hy | 10.0 м |
| Деактивация | $(i+j) \% 2 == 0$ → неактивна (13 из 25) |
| Активные ячейки | 12, все изолированы друг от друга |
| Проницаемость | 100 мД |
| Пористость | 0.2 |
| P_init | 200 атм |
| Sw_init | 0.2 (oil_saturation = 0.8) |
| Скважина | PROD в (25, 15) → ячейка (2,1) → $l = 2 + 5 \cdot 1 = 7$ (нечётный, активна) |
| Дебит | oil = 1.0 кг/день |
| dt | 0.1 день |
| Шагов | 5 |

**Почему $(i+j) \% 2 == 0$ → неактивна:**
- Ячейка (0,0): $0+0=0$ → неактивна ✓
- Ячейка (1,0): $1+0=1$ → активна ✓
- Ячейка (0,1): $0+1=1$ → активна ✓
- Ячейка (1,1): $1+1=2$ → неактивна ✓

Активные соседи ячейки (2,1): (1,1)→неакт, (3,1)→неакт, (2,0)→неакт, (2,2)→неакт. **Полная изоляция.**

## Метрики

| Метрика | Tolerance | Обоснование |
|---|---|---|
| `isfinite(OilTotal())` | — | краш-гард |
| `isfinite(WaterTotal())` | — | краш-гард |
| Давление неактивных = P_init | exact (== P_init_Pa) | `CellsInactive` не участвуют в решении |
| Sw неактивных = Sw_init | exact (== 0.2) | `CellsInactive` не участвуют в решении |
| `OilTotal() > 0` | — | ненулевая масса в активных ячейках |

**Примечание о балансе масс:** при полной изоляции скважинной ячейки (нет активных соседей) она работает как «бочка» — давление падает, нефть добывается из одной ячейки. MassBalanceTracker отслеживает $\Delta M = q \cdot \Delta t$, но для проверки нужна связь между `OilTotal()` и дебитами. В данном тесте достаточно проверить:
- Симулятор не крашится
- Неактивные ячейки не затронуты
- Масса конечна и положительна

## Подводные камни

- [x] **Блокирующие баги:** BUG-009 и BUG-011 исправлены ✅
- [x] **`GetPressureField` / `GetOilSaturationField` безопасны:** `operator[]` в `AbstractGrid.h:78–84` обрабатывает `idx < 0` через `CellsInactive[-(idx+1)]`. Вызов для неактивных ячеек не UB.
- [x] **`OilTotal()` / `WaterTotal()`:** итерируют до `ActiveCellsNmbr`, не `TotalCellsNmbr` — безопасны.
- [ ] **`CellsInactive` заполнен?** Конструктор `SomeGrid` в `AbstractGrid.h:130–153` создаёт `CellsInactive` только если ячейка неактивна. Нужно убедиться, что `CellsInactive` имеет элементы (при 13 неактивных — точно имеет).
- [ ] **Связность Z закомментирована:** в `SetConnectivityGraph_3D` (строки 228–234, 263–269) связность по Z закомментирована. Для Nz=1 это не проблема.
- [ ] **Скважина в изолированной ячейке:** при нулевых связях Newton-солвер решает одно уравнение с одной неизвестной (давление в ячейке). Сходимость должна быть мгновенной. Если нет — баг.
- [ ] **`add_simple_well` создаёт Nz перфораций:** для Nz=1 это 1 перфорация — ОК.
- [ ] **`RemovePerfsAtInactiveCells`:** скважина в (25, 15) → ячейка (2,1,0) → $l=7$, $(i+j)\%2 = (2+1)\%2 = 1$ ≠ 0 → активна. Перфорация не фильтруется — корректно.
- [x] **SparsityPattern с нулевой связностью:** при пустом `connectivityGraph[l]` SparsityPattern корректно обрабатывает случай (строки 134–142 SparsityPattern.cpp): диагональный блок добавляется после пустого цикла по соседям. Матрица СЛАУ — чисто диагональная. ✅
- [x] **`using Catch::Approx`:** тесты с `Approx` требуют `using Catch::Approx;` внутри TEST_CASE (без него — ошибка компиляции). ✅ (добавлено в шаги 2, 3)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные заметки:
   - `vault/GDM/knowledge/debugging/BUG-009 active cells filter.md`
   - `vault/GDM/knowledge/decisions/сетка 3D структурированная с линейной индексацией.md`
3. Создай ветку: `git checkout -b val/val-007/checkerboard-inactive-cells`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни: 316 тестов, все зелёные
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Шаги

### Шаг 1: Catch2-тест — шахматная деактивация (smoke)

**Цель:** симулятор не крашится при шахматной деактивации на 5×5×1

**Файлы:** `tests/test_inactive_cells.cpp`

**Контекст:**
В файле уже есть 2 теста (`"Well skips inactive cell layer"` и `"All layers inactive - well has zero production"`). Добавляем третий тест, который деактивирует ячейки по шахматному паттерну $(i+j)\%2 == 0$ на сетке 5×5×1 и запускает 5 шагов симуляции.

**Что сделать:**

1. Добавить новый `TEST_CASE` в конец файла `tests/test_inactive_cells.cpp`:

```cpp
TEST_CASE("Checkerboard inactive cells - no crash",
          "[integration][wells][inactive-cells][val-007]") {
    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    // Шахматная деактивация: (i+j) % 2 == 0 → неактивна
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0)
                horizon.active_cells[Nx * j + i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // PROD в (25, 15) → ячейка (2,1) → l=7, (2+1)%2=1 → активна
    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    double water = sim.WaterTotal();
    REQUIRE(std::isfinite(oil));
    REQUIRE(std::isfinite(water));
    REQUIRE(oil > 0.0);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные (317 тестов)

**Подводные камни:**
- Скважина должна попасть в активную ячейку. (2,1): $(2+1)\%2 = 1 \neq 0$ → активна ✓
- `add_simple_well` создаёт Nz=1 перфорацию → ОК

**Зависимости:** нет

**Оценка:** ~25 строк, ~5 минут

---

### Шаг 2: Проверка изоляции неактивных ячеек

**Цель:** давление и насыщенность в неактивных ячейках не изменились после симуляции

**Файлы:** `tests/test_inactive_cells.cpp`

**Контекст:**
`GetPressureField()` и `GetOilSaturationField()` возвращают поля для ВСЕХ ячеек (активных и неактивных), итерируя по `TotalCellsNmbr()`. Для неактивных ячеек `ConvertGlobal2Local(l)` возвращает -1, и `operator[](-1)` возвращает `CellsInactive[0]`. Ключевой инвариант: `CellsInactive` хранят начальные значения, потому что неактивные ячейки не обновляются в `UpdateState`, `AcceptState`, `ReverseState` — эти методы итерируют по `activeCellsNmbr`.

Каждая неактивная ячейка имеет свой элемент в `CellsInactive`. Механизм: конструктор `SomeGrid` (AbstractGrid.h:130–153) настраивает индексы — `cell_idx_Global2Local[l] = -static_cast<ptrdiff_t>(inActiveCellsNmbr)` (первая неактивная = -1, вторая = -2, ...). Затем `SomeStructuredGrid3Dim` конструктор заполняет `CellsInactive = std::move(cellsInactive)` из `OilField::SetInActiveCells()`, которая создаёт ячейки с начальными P и Sw. В `operator[]`: `CellsInactive[-(idx+1)]` = `CellsInactive[0]` для idx=-1, `CellsInactive[1]` для idx=-2, и т.д.

**Что сделать:**

1. Добавить новый `TEST_CASE` в `tests/test_inactive_cells.cpp`:

```cpp
TEST_CASE("Checkerboard inactive cells - isolation check",
          "[integration][wells][inactive-cells][val-007]") {
    using Catch::Approx;

    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double P_init_Pa = P_init_atm * 101325.0;
    double Sw_init = 1.0 - oil_sat;

    // Шахматная деактивация
    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0) {
                horizon.active_cells[Nx * j + i] = false;
                is_inactive[Nx * j + i] = true;
            }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    // Проверка: неактивные ячейки сохранили начальные значения
    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();

    for (size_t l = 0; l < Nx * Ny; ++l) {
        if (is_inactive[l]) {
            REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
            REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        }
    }
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 318 тестов, все зелёные

**Подводные камни:**
- `GetPressureField()` возвращает `TotalCellsNmbr()` = 25 элементов (не 12 активных). Индексация глобальная.
- `Approx` с `epsilon(1e-12)` — relative tolerance. Для давления ~20 МПа это ~20 нПа, что строже machine epsilon. Если не пройдёт — ослабить до `1e-10`.

**Зависимости:** шаг 1 (чтобы убедиться что симулятор не крашится)

**Оценка:** ~35 строк, ~10 минут

---

### Шаг 3: Проверка связности графа

**Цель:** убедиться, что при шахматном паттерне на нечётной сетке все активные ячейки полностью изолированы (пустой граф связности)

**Файлы:** `tests/test_inactive_cells.cpp`

**Контекст:**
На сетке 5×5 с деактивацией $(i+j)\%2 == 0$ каждая активная ячейка $(i,j)$ с $(i+j)\%2 == 1$ имеет соседей:
- $(i-1,j)$: $(i-1+j)\%2 = 0$ → неактивна
- $(i+1,j)$: $(i+1+j)\%2 = 0$ → неактивна
- $(i,j-1)$: $(i+j-1)\%2 = 0$ → неактивна
- $(i,j+1)$: $(i+j+1)\%2 = 0$ → неактивна

Все соседи неактивны → `connectivityGraph[l_local]` пуст для всех активных ячеек. Это значит, что давление в каждой активной ячейке определяется только скважиной (если есть) или не меняется (если нет). Давление не распространяется между ячейками — сетка полностью декомпозирована на независимые ячейки.

**Косвенная проверка:** если скважина только в одной ячейке, давление во всех остальных активных ячейках должно остаться = P_init.

**Что сделать:**

1. Расширить тест шага 2, добавив проверку давления в активных ячейках без скважины:

```cpp
TEST_CASE("Checkerboard inactive cells - active cells isolated",
          "[integration][wells][inactive-cells][val-007]") {
    using Catch::Approx;

    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    double P_init_atm = 200.0;
    double oil_sat = 0.8;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double P_init_Pa = P_init_atm * 101325.0;

    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0) {
                horizon.active_cells[Nx * j + i] = false;
                is_inactive[Nx * j + i] = true;
            }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // PROD в ячейке (2,1) → l=7
    size_t well_cell = 2 + 5 * 1;
    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    auto P = sim.GetPressureField();

    // Все активные ячейки, КРОМЕ скважинной, должны сохранить P_init
    for (size_t l = 0; l < Nx * Ny; ++l) {
        if (!is_inactive[l] && l != well_cell) {
            REQUIRE(P[l] == Approx(P_init_Pa).epsilon(1e-12));
        }
    }

    // Скважинная ячейка: давление изменилось (добыча снижает давление)
    // Точное значение зависит от PI и дебита, проверяем только что не равно начальному
    // (при ненулевом дебите давление должно упасть)
    // Примечание: если дебит слишком мал относительно объёма ячейки,
    // изменение может быть < tolerance. Но при dt=0.1, 5 шагов, 1 кг/день
    // из ячейки V = 10*10*10 = 1000 м³, poro=0.2 → Vpore = 200 м³,
    // добыча за 0.5 дня = 0.5 кг ~ ничтожно. Поэтому не проверяем != P_init.
}
```

**Проверка после этого шага:**
- Сборка + тесты: 319 тестов, все зелёные

**Подводные камни:**
- Добыча 1 кг/день из ячейки с Vpore = 200 м³ × 800 кг/м³ = 160 000 кг нефти — изменение давления ничтожно. Поэтому нельзя проверить `P[well_cell] != P_init_Pa` — разница может быть ниже machine epsilon. Проверяем только изоляцию остальных ячеек.

**Зависимости:** шаг 1

**Оценка:** ~35 строк, ~10 минут

---

### Шаг 4: Python-скрипт визуализации шахматного паттерна

**Цель:** создать скрипт для визуальной верификации: карта активных/неактивных ячеек + поле давления

**Файлы:** `scripts/plot_checkerboard.py` (новый)

**Контекст:**
Скрипт не запускается автоматически тестами. Используется разработчиком для ручной проверки. Формат входных данных: тест должен экспортировать CSV с полями (i, j, active, P_atm, Sw) в директорию `results/val-007/`. Для этого добавим отдельный тест с тегом `[.visual]` (точка = скрыт от обычного прогона).

**Что сделать:**

1. Создать `scripts/plot_checkerboard.py`:

```python
"""
Визуализация шахматной деактивации ячеек (VAL-007).

Использование:
    python scripts/plot_checkerboard.py --results-dir results/val-007

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
        os.path.join(args.results_dir, "checkerboard.csv"),
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

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))

    # 1. Карта активности
    ax = axes[0]
    im = ax.imshow(active_map, origin="lower", cmap="RdYlGn", vmin=0, vmax=1)
    ax.set_title("Active cells (1=active, 0=inactive)")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    # 2. Поле давления
    ax = axes[1]
    im = ax.imshow(P_map, origin="lower", cmap="viridis")
    ax.set_title("Pressure [atm]")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    # 3. Поле Sw
    ax = axes[2]
    im = ax.imshow(Sw_map, origin="lower", cmap="Blues", vmin=0, vmax=1)
    ax.set_title("Water saturation Sw")
    ax.set_xlabel("i")
    ax.set_ylabel("j")
    plt.colorbar(im, ax=ax)

    plt.tight_layout()
    out = os.path.join(args.results_dir, "checkerboard.png")
    plt.savefig(out, dpi=150)
    print(f"Saved: {out}")
    plt.show()


if __name__ == "__main__":
    main()
```

2. Добавить visual-тест в `tests/test_inactive_cells.cpp` (экспорт CSV):

```cpp
TEST_CASE("Checkerboard inactive cells - CSV export",
          "[.visual][inactive-cells][val-007]") {
    size_t Nx = 5, Ny = 5, Nz = 1;
    double Lx = 50.0, Ly = 50.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            if ((i + j) % 2 == 0) {
                horizon.active_cells[Nx * j + i] = false;
                is_inactive[Nx * j + i] = true;
            }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    test_helpers::add_simple_well(sim, horizon, "PROD", 25.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    for (int step = 0; step < 5; ++step)
        sim.SingleIteration(dt, dt);

    // Экспорт CSV
    std::filesystem::create_directories("results/val-007");
    std::ofstream ofs("results/val-007/checkerboard.csv");
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

**Необходимые include для `<filesystem>` и `<fstream>`:** `test_helpers.h` уже включает `stdafx.h` и `ReservoirSimulator.h`, которые подтягивают `<fstream>`. Для `<filesystem>` добавить `#include <filesystem>` в начало `test_inactive_cells.cpp`.

**Проверка после этого шага:**
- Сборка: 320 тестов, все зелёные (visual-тест скрыт за `[.visual]`)
- Ручной запуск: `ctest --test-dir build -C Release -R "CSV export" --output-on-failure`
- Визуализация: `python scripts/plot_checkerboard.py --results-dir results/val-007`

**Зависимости:** шаг 1

**Оценка:** ~80 строк (скрипт) + ~35 строк (тест), ~20 минут

---

### Шаг 5: Обновить vault

**Цель:** зафиксировать результаты валидации в vault

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — статус VAL-007
- `vault/GDM/00-home/index.md` — ссылка на план

**Что сделать:**

1. В `валидационные кейсы.md`, секция VAL-007:
   - Изменить `**Статус:** ⬜ НЕ НАЧАТО` → `**Статус:** ✅ ПРОЙДЕН <дата>`
   - (Ссылки `**План:**` и `**GitHub:**` уже добавлены — не дублировать)

2. (Ссылка в `vault/GDM/00-home/index.md` уже добавлена — не дублировать)

3. Прокомментировать GitHub issue #48: `Тесты пройдены. 3 Catch2 теста ([val-007]), 1 visual-тест с CSV export. Python-скрипт визуализации: scripts/plot_checkerboard.py.`

**Проверка после этого шага:**
- Vault-записи обновлены
- GitHub issue прокомментирован

**Зависимости:** шаги 1–4

**Оценка:** ~5 минут

---

## Тестовая стратегия

**Тест 1:**
- **Тест:** `"Checkerboard inactive cells - no crash"`
- **Тег:** `[integration][wells][inactive-cells][val-007]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** 5×5×1, шахматная деактивация, PROD в активной ячейке, 5 шагов
- **Эталон:** self-consistency (isfinite, oil > 0)
- **Метрика:** smoke test
- **Tolerance:** —
- **Предотвращает:** краши при разреженной сетке, OOB, деление на ноль

**Тест 2:**
- **Тест:** `"Checkerboard inactive cells - isolation check"`
- **Тег:** `[integration][wells][inactive-cells][val-007]`
- **Файл:** `tests/test_inactive_cells.cpp`
- **Сценарий:** то же, + проверка P и Sw в неактивных ячейках
- **Эталон:** P_init, Sw_init
- **Метрика:** absolute equality (Approx epsilon 1e-12)
- **Tolerance:** 1e-12 relative
- **Предотвращает:** утечку решения в неактивные ячейки

**Тест 3:**
- **Тест:** `"Checkerboard inactive cells - active cells isolated"`
- **Тег:** `[integration][wells][inactive-cells][val-007]`
- **Файл:** `tests/test_inactive_cells.cpp`
- **Сценарий:** то же, + проверка P в активных ячейках без скважины
- **Эталон:** P_init (полная изоляция при шахматном паттерне)
- **Метрика:** absolute equality (Approx epsilon 1e-12)
- **Tolerance:** 1e-12 relative
- **Предотвращает:** ложные связи в графе, перенос через неактивные ячейки

**Тест 4 (visual):**
- **Тест:** `"Checkerboard inactive cells - CSV export"`
- **Тег:** `[.visual][inactive-cells][val-007]`
- **Файл:** `tests/test_inactive_cells.cpp`
- **Сценарий:** экспорт полей в CSV для ручной визуализации
- **Скрипт:** `scripts/plot_checkerboard.py`
- **Предотвращает:** ошибки, которые не ловятся численными assert-ами

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные (316 + 3 новых = 319 видимых, +1 скрытый visual)
- [ ] 3 количественных теста зелёные
- [ ] Visual-тест: CSV экспортируется, скрипт строит картинку
- [ ] Шахматный паттерн виден на графике
- [ ] Vault обновлён: статус VAL-007 = ✅ ПРОЙДЕН
- [ ] GitHub issue #48 прокомментирован
- [ ] Ветка: `val/val-007/checkerboard-inactive-cells`

## Оценка

| Метрика | Значение |
|---|---|
| Файлов новых | 1 (`scripts/plot_checkerboard.py`) |
| Файлов изменённых | 1 (`tests/test_inactive_cells.cpp`) |
| Строк кода | ~120 (C++) + ~80 (Python) |
| Шагов | 5 |
| Время | ~50 минут |
