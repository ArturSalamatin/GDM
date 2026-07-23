---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-23
issue: VAL-020
github: 53
branch: val/val-020/shutin-restart-close-layer
status: готов к реализации
audit:
  date: 2026-07-23
  round: 5
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# VAL-020: Закрытие перфорации + shut-in + restart

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай `tests/test_3d_completions.cpp` — паттерн: `MultiLayerCase` + `run_case_3d` + `WellScheduleBuilder`
3. Прочитай `tests/well_completion_builder.h` — API `open_layer(layer_id, time)`, `close_layer(layer_id, time)`
4. Прочитай `tests/well_schedule_builder.h` — API `inject_water()`, `produce_oil()`, `shut_in()`, `for_days()`, `set_completions()`, `add_to_sim()`
5. Прочитай `tests/simulation_cases/MultiLayerCase.h` — обёртка над `ReservoirSimulator` с экспортом CSV
6. Создай ветку: `git checkout -b val/val-020/shutin-restart-close-layer experimental`
7. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
8. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
9. Запомни количество тестов и время — это baseline (на момент аудита: 325 тестов)
10. Начни с шага 1. После каждого шага: сборка + тесты

## Подтип

**Инвариантный.** Self-consistency: комбинация трёх динамических событий (работа → shut-in → restart с закрытой перфорацией) должна сохранять баланс масс на каждой фазе. После restart поток идёт только через открытые слои.

## Суть задачи

Сценарий в три фазы на многослойной сетке (Nz=2):

| Фаза | Время (дни) | INJ | PROD |
|---|---|---|---|
| 1 | 0–100 | inject_water через оба слоя (k=0, k=1) | produce_oil через оба слоя |
| 2 | 100–150 | shut_in | shut_in |
| 3 | 150–300 | inject_water, но k=0 закрыт (`close_layer(0, 150.0)`) | produce_oil, k=0 закрыт |

Ожидание:
1. Фаза 1: нормальная закачка/добыча, баланс масс ≤ 1e-3
2. Фаза 2 (shut-in): расходы = 0, давление/Sw стабилизируются, баланс масс соблюдается
3. Фаза 3 (restart с закрытым k=0): поток идёт только через k=1, в k=0 Sw не меняется (по сравнению с концом фазы 2), баланс масс ≤ 1e-3

## Физический контекст

GDM — двухфазный (нефть–вода) симулятор, **полностью неявная схема (Fully Implicit Method)** — P и Sw решаются одновременно, блочная система 2×2. Z-связи между слоями **закомментированы** (`HydroSolver/Solver/Grids/AbstractGrid.h:228–234, 263–269`). Это означает:
- Слои k=0 и k=1 — две независимые 2D-задачи
- `close_layer(0, 150.0)` убирает перфорацию скважины из слоя k=0 начиная с t=150 дней
- После закрытия перфорации в k=0: скважина продолжает работать только через k=1
- Слой k=0 «замораживается» — в нём нет ни источников, ни стоков, Sw/P не должны меняться

Механизм перфорации:
- `WellCompletionBuilder::close_layer(layer_id, time)` добавляет запись `(0.0, hz_, false, time)` в `jobs_[layer_id]`
- При `t ≥ time` перфорация в этом слое выключена (`is_open = false`)
- `WellJobs::AccumulatePerforations` обрабатывает расписание перфораций по времени

Существующий тест «layer closure mid-simulation» (`test_3d_completions.cpp:349–377`) проверяет закрытие слоя на ходу, но **без shut-in и restart**. VAL-020 добавляет shut-in + restart как комбинированный сценарий.

## Чеклист подводных камней

- [x] **Блокирующие баги:** нет. Тест «layer closure mid-simulation» проходит, `close_layer` работает
- [x] **shut_in + restart:** `WellScheduleBuilder::shut_in()` устанавливает `is_shut_ = true`, `for_days()` записывает `is_work = 0.0`. Следующий `inject_water()` или `produce_oil()` сбрасывает `is_shut_ = false` (строка 74 well_schedule_builder.h). Restart — просто следующий вызов `inject_water`/`produce_oil` после `shut_in().for_days()`. ✅ безопасно
- [x] **close_layer timing:** `close_layer(0, 150.0)` задаётся в `WellCompletionBuilder` **до** старта симуляции. Время t=150 — момент, когда перфорация закрывается. Это не зависит от shut_in/restart — completions и MER-расписание задаются независимо. ✅
- [x] **Баланс масс при shut_in:** `GetOverallBalance()` суммирует по `ActiveCellsNmbr`. При shut_in расходы = 0, поэтому `accumOilDebet` и `accumWaterDebet` не изменяются за фазу 2. Невязка должна оставаться 0 (или машинная точность). ✅
- [x] **Проверка Sw в k=0 после restart:** Z-связи отключены. Без перфорации в слое k=0 нет источников/стоков. Sw и P не должны меняться. FIM решает P и Sw одновременно, но без источников в изолированном слое солвер не должен менять поля. Ожидаемая погрешность ∼ машинная точность. ✅
- [x] **Численная диффузия:** не применимо — self-consistency, не сравнение с аналитикой
- [x] **Два слоя достаточно:** Nz=2. Закрываем k=0, работаем через k=1. Проще и дешевле, чем 3 слоя. ✅
- [x] **metadata.json ключи:** `write_metadata_json` пишет `"Nx"`, `"Ny"`, `"Nz"` (с заглавной). Python-скрипт должен использовать `meta["Nx"]`, не `meta["nx"]`. ✅ исправлено при аудите

## Шаги

---

### Шаг 1: Количественный тест — shut-in + restart с закрытой перфорацией

**Цель:** доказать self-consistency: баланс масс на каждой фазе, дебит через k=0 = 0 после restart, Sw в k=0 стабильна после закрытия

**Файлы:**
- `tests/test_3d_completions.cpp` — добавить новый TEST_CASE в конец файла (после строки 630, после последнего TEST_CASE; анонимный namespace закрывается на строке 267, TEST_CASE-ы находятся ВНЕ namespace)

**Контекст:**
Инфраструктура `MultiLayerCase` + `run_case_3d` выполняет: создание сетки, добавление скважин, пошаговый Solve, экспорт CSV, проверку баланса масс. Результат `RunResult` содержит `max_oil_balance_rel`, `max_water_balance_rel`, финальные `Sw` и `P`.

Проблема: `run_case_3d` считает баланс масс **суммарно** за весь прогон, а не по фазам. Для VAL-020 нужна проверка по фазам. Поэтому для количественного теста потребуется частичное дублирование логики — три вызова `run_case_3d` с разными `MultiLayerCase` для каждой фазы.

Альтернатива: одна сквозная симуляция через `MultiLayerCase`, а проверку Sw в k=0 делать по финальным полям. `run_case_3d` гарантирует `Sw[i] >= 0, Sw[i] <= 1, P[i] > 0` на каждом шаге и считает max баланса. Этого достаточно для основного инварианта. Дополнительно: сравнить Sw в k=0 в конце (t=300) с начальным значением — если close_layer работает, Sw в k=0 за фазу 3 не должна значительно измениться.

Выбираем **одну сквозную симуляцию** — проще, использует стандартный `run_case_3d`, инвариант проверяется через финальные поля.

**Что сделать:**

1. Добавить `#include <catch2/catch_approx.hpp>` после строки 2 (`#include <catch2/matchers/catch_matchers_floating_point.hpp>`) — нужен для `Catch::Approx`

2. Добавить TEST_CASE после последнего существующего в `test_3d_completions.cpp`:

```cpp
TEST_CASE("3D completions: shut-in + restart with closed layer",
          "[3d][completions][shutin-restart][val-020]")
{
    constexpr size_t Nz = 2;

    simulation_cases::MultiLayerCase sc(
        "3d_shutin_restart_close", Nx, Ny, Nz, Lx, Ly, hz,
        300.0, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            // INJ: оба слоя открыты с t=0, k=0 закрывается при t=150
            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(0, 150.0);
            builders.emplace_back("INJ", 125.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(100.0)    // фаза 1
                .shut_in().for_days(50.0)               // фаза 2
                .inject_water(30.0).for_days(150.0);    // фаза 3 (restart, k=0 закрыт)

            // PROD: оба слоя открыты с t=0, k=0 закрывается при t=150
            auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(0, 150.0);
            builders.emplace_back("PROD", 375.0, 250.0);
            builders.back()
                .set_completions(c_prod)
                .produce_oil(20.0).for_days(100.0)     // фаза 1
                .shut_in().for_days(50.0)               // фаза 2
                .produce_oil(20.0).for_days(150.0);     // фаза 3

            return builders;
        },
        {{"INJ",  "injector", 125.0, 250.0},
         {"PROD", "producer", 375.0, 250.0}}
    );

    auto result = run_case_3d(sc, true);

    // Баланс масс через весь прогон (все 3 фазы)
    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    // После restart (t=150→300) слой k=0 закрыт для обеих скважин.
    // Z-связи отключены → в k=0 нет источников/стоков → Sw близка к начальной.
    // Начальная Sw_water = 1 - oil_saturation = 0.2.
    // За фазу 1 (0–100д) Sw в k=0 изменилась (инжекция воды).
    // За фазу 3 (150–300д) Sw в k=0 не должна измениться (перфорации закрыты).
    // Проверяем инвариант: k=0 не имеет источников/стоков в фазе 3 → поля стабильны.
    //
    // Косвенная проверка: Sw в k=1 вблизи INJ должна быть > Sw в k=0 вблизи INJ,
    // потому что k=1 получала закачку все 300 дней, а k=0 — только 100.
    size_t inj_i = static_cast<size_t>(125.0 / (Lx / Nx));
    size_t inj_j = static_cast<size_t>(250.0 / (Ly / Ny));
    size_t cell_k0 = Nx * inj_j + inj_i;
    size_t cell_k1 = Nx * Ny + Nx * inj_j + inj_i;

    CHECK(result.Sw[cell_k1] > result.Sw[cell_k0]);

    // Прямая проверка «заморозки» k=0: ячейки в k=0, далёкие от INJ,
    // не должны получить воду за фазу 3 (перфорация закрыта).
    // За 100 дней (фаза 1) фронт на сетке 11×11 (hx≈45м) проходит ~2–3 ячейки.
    // Дальний угол (i=Nx-1, j=0) в k=0 должен остаться при Sw_init = 0.2.
    // Если close_layer НЕ работает, 150 дней фазы 3 продвинут фронт дальше.
    constexpr double Sw_init = 1.0 - 0.8;  // = 0.2
    size_t far_cell_k0 = Nx * 0 + (Nx - 1);  // (Nx-1, 0) в слое k=0
    CHECK(result.Sw[far_cell_k0] == Catch::Approx(Sw_init).epsilon(1e-6));

    // Для сравнения: та же ячейка в k=1 — если close_layer работает,
    // k=1 получала закачку 250 дней (100 + 150), и фронт мог уйти дальше.
    // Но мы не проверяем это строго — только что k=0 стабильна.
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 325 существующих тестов зелёные + новый тест зелёный (326 всего)

**Подводные камни:**
- `close_layer(0, 150.0)` — момент закрытия совпадает с концом shut-in. Перфорация закрывается одновременно с restart. Это штатный сценарий: completions-расписание задаётся по абсолютному времени, не по фазам
- Дебиты: inject_water(30.0) vs produce_oil(20.0) — несбалансированные, намеренно (как в существующем тесте «layer closure mid-simulation»)
- `snapshot_dt = 10.0` — 31 snapshot за 300 дней. Достаточно для визуализации фаз

**Зависимости:**
- Требует: ничего
- Блокирует: шаги 2, 3

**Оценка:** ~50 строк, ~15 минут

---

### Шаг 2: Визуальный тест — CSV-экспорт с фазовой разметкой

**Цель:** CSV-файлы для визуальной верификации: (1) snapshot-ы Sw по слоям видны через `run_case_3d(sc, true)` — уже экспортируются в шаге 1, (2) дополнительный тест с тегом `[.visual]` для экспорта разницы Sw между фазами

**Файлы:**
- `tests/test_3d_completions.cpp` — добавить TEST_CASE с тегом `[.visual]`

**Контекст:**
`run_case_3d` уже экспортирует `results/3d_shutin_restart_close/snapshot_NNN_layer_K.csv` и `mass_balance.csv`. Этого достаточно для визуализации. Но для удобства Python-скрипта полезно экспортировать snapshot-ы в более частые моменты и записать метаданные фаз.

На самом деле, `run_case_3d` с `snapshot_dt = 10.0` уже экспортирует snapshot-ы каждые 10 дней. Это достаточно. Дополнительный `[.visual]` тест не нужен — `run_case_3d` в шаге 1 экспортирует всё необходимое при запуске с `export_snapshots = true`.

Вместо этого можно добавить отдельный `[.visual]` тест, который запускается вручную с **увеличенной сеткой** (21×21) для более красивой визуализации. Основной количественный тест (шаг 1) работает на сетке 11×11 для скорости.

**Что сделать:**

1. Добавить TEST_CASE после теста из шага 1:

```cpp
TEST_CASE("3D completions: shut-in + restart - visual",
          "[.visual][3d][completions][shutin-restart][val-020]")
{
    constexpr size_t Nz = 2;
    constexpr size_t Nx_v = 21, Ny_v = 21;
    constexpr double Lx_v = 500.0, Ly_v = 500.0;

    simulation_cases::MultiLayerCase sc(
        "val-020", Nx_v, Ny_v, Nz, Lx_v, Ly_v, hz,
        300.0, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(0, 150.0);
            builders.emplace_back("INJ", 125.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(100.0)
                .shut_in().for_days(50.0)
                .inject_water(30.0).for_days(150.0);

            auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(0, 150.0);
            builders.emplace_back("PROD", 375.0, 250.0);
            builders.back()
                .set_completions(c_prod)
                .produce_oil(20.0).for_days(100.0)
                .shut_in().for_days(50.0)
                .produce_oil(20.0).for_days(150.0);

            return builders;
        },
        {{"INJ",  "injector", 125.0, 250.0},
         {"PROD", "producer", 375.0, 250.0}}
    );

    auto result = run_case_3d(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);
}
```

2. Ручной запуск: `build\Release\gdm_tests.exe "[.visual][val-020]"`
3. Результаты: `results/val-020/snapshot_NNN_layer_K.csv`, `mass_balance.csv`, `metadata.json`

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные (скрытый `[.visual]` не запускается через ctest)
- Ручной запуск: `build\Release\gdm_tests.exe "[.visual][val-020]"` — создаёт CSV файлы

**Подводные камни:**
- Сетка 21×21 × 2 слоя = 882 ячейки. Прогон ~30 секунд — допустимо для `[.visual]` теста
- Каталог `results/val-020/` создаётся `run_case_3d` автоматически через `fs::create_directories`

**Зависимости:**
- Требует: шаг 1 (подтверждённый setup)
- Блокирует: шаг 3

**Оценка:** ~40 строк, ~10 минут

---

### Шаг 3: Python-скрипт визуализации

**Цель:** визуализация трёх фаз: карты Sw по слоям (k=0 и k=1) на моменты t=100, t=150, t=300. Дополнительно: график баланса масс по времени.

**Файлы:**
- `scripts/plot_val020.py` — новый файл

**Контекст:**
Паттерн: `scripts/plot_val019.py`. Считывает CSV-snapshot-ы из `results/val-020/`, строит визуализацию. Отличие от val-019: здесь 2 слоя вместо 3, и нужна визуализация по времени (3 фазы), а не только финальная карта.

Формат CSV (создаётся `write_layer_csv` в `test_3d_completions.cpp`):
```
i,j,Sw,P_atm
0,0,0.20000000,200.00000000
...
```

Формат `mass_balance.csv`:
```
t,oil_mass,water_mass,accumOil,accumOilOutFlux,accumOilDebet,oil_residual,accumWater,accumWaterOutFlux,accumWaterDebet,water_residual
```

Формат `metadata.json`:
```json
{"case": "val-020", "Nx": 21, "Ny": 21, "Nz": 2, "Lx": 500, "Ly": 500, "hz": 10, "wells": [...], "save_times": [...]}
```

**Что сделать:**

1. Создать `scripts/plot_val020.py`:

```python
"""
Визуализация VAL-020: shut-in + restart с закрытой перфорацией (2 слоя).

Использование:
    python scripts/plot_val020.py --results-dir results/val-020

Зависимости: numpy, matplotlib
"""
import argparse
import json
import os
import numpy as np
import matplotlib.pyplot as plt


def load_snapshot(results_dir, step, layer):
    path = os.path.join(results_dir, f"snapshot_{step:03d}_layer_{layer}.csv")
    return np.genfromtxt(path, delimiter=",", skip_header=1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--results-dir", required=True)
    args = parser.parse_args()

    with open(os.path.join(args.results_dir, "metadata.json")) as f:
        meta = json.load(f)

    nx, ny = meta["Nx"], meta["Ny"]
    times = meta["save_times"]

    # Найти snapshot-ы для ключевых моментов: t=100 (конец фазы 1),
    # t=150 (конец shut-in), t=300 (конец фазы 3)
    targets = [100.0, 150.0, times[-1]]
    indices = []
    for t in targets:
        idx = min(range(len(times)), key=lambda i: abs(times[i] - t))
        indices.append(idx)

    phase_labels = [
        f"Фаза 1 (t={times[indices[0]]:.0f}д)",
        f"Shut-in (t={times[indices[1]]:.0f}д)",
        f"Restart (t={times[indices[2]]:.0f}д)",
    ]

    # --- Карты Sw: 2 слоя × 3 фазы ---
    fig, axes = plt.subplots(2, 3, figsize=(18, 10))
    layer_titles = ["k=0 (закрыт при t=150)", "k=1 (работает весь период)"]

    for row, k in enumerate(range(2)):
        for col, (step_idx, label) in enumerate(zip(indices, phase_labels)):
            data = load_snapshot(args.results_dir, step_idx, k)
            Sw = data[:, 2].reshape(ny, nx)

            im = axes[row, col].imshow(
                Sw, origin="lower", cmap="Blues", aspect="auto", vmin=0, vmax=1)
            axes[row, col].set_title(f"Sw — {layer_titles[row]}\n{label}")
            plt.colorbar(im, ax=axes[row, col])

    fig.suptitle("VAL-020: shut-in + restart с закрытой перфорацией", fontsize=14)
    fig.tight_layout()
    fig.savefig(os.path.join(args.results_dir, "sw_phases.png"), dpi=150)
    print(f"Сохранено: {os.path.join(args.results_dir, 'sw_phases.png')}")

    # --- Баланс масс ---
    bal = np.genfromtxt(
        os.path.join(args.results_dir, "mass_balance.csv"),
        delimiter=",", skip_header=1)
    t = bal[:, 0]
    oil_res = bal[:, 6]
    water_res = bal[:, 10]

    fig2, ax = plt.subplots(figsize=(10, 5))
    ax.plot(t, oil_res, "o-", label="Oil residual", markersize=3)
    ax.plot(t, water_res, "s-", label="Water residual", markersize=3)
    ax.axvline(100, color="gray", linestyle="--", alpha=0.5, label="shut-in start")
    ax.axvline(150, color="red", linestyle="--", alpha=0.5, label="restart (k=0 closed)")
    ax.set_xlabel("Время [дни]")
    ax.set_ylabel("Невязка баланса масс")
    ax.legend()
    ax.set_title("VAL-020: баланс масс по фазам")
    fig2.tight_layout()
    fig2.savefig(os.path.join(args.results_dir, "mass_balance.png"), dpi=150)
    print(f"Сохранено: {os.path.join(args.results_dir, 'mass_balance.png')}")

    # --- Разность Sw в k=0 между shut-in и restart ---
    data_shutin = load_snapshot(args.results_dir, indices[1], 0)
    data_restart = load_snapshot(args.results_dir, indices[2], 0)
    dSw = (data_restart[:, 2] - data_shutin[:, 2]).reshape(ny, nx)

    fig3, ax3 = plt.subplots(figsize=(7, 6))
    im3 = ax3.imshow(dSw, origin="lower", cmap="RdBu_r", aspect="auto",
                     vmin=-0.01, vmax=0.01)
    ax3.set_title("ΔSw в k=0 (restart − shut-in)\nОжидание: ≈ 0 (перфорация закрыта)")
    plt.colorbar(im3, ax=ax3, label="ΔSw")
    fig3.tight_layout()
    fig3.savefig(os.path.join(args.results_dir, "delta_sw_k0.png"), dpi=150)
    print(f"Сохранено: {os.path.join(args.results_dir, 'delta_sw_k0.png')}")

    plt.show()


if __name__ == "__main__":
    main()
```

**Проверка после этого шага:**
- Прогнать визуальный тест: `build\Release\gdm_tests.exe "[.visual][val-020]"`
- Прогнать скрипт: `python scripts/plot_val020.py --results-dir results/val-020`
- Визуально проверить:
  - `sw_phases.png`: в k=0 карта Sw одинакова для shut-in и restart; в k=1 фронт продвинулся за restart
  - `mass_balance.png`: невязка < 1e-3 на всех фазах, нет скачков при shut-in/restart
  - `delta_sw_k0.png`: значения ΔSw ≈ 0 (в пределах цветовой шкалы ±0.01)

**Подводные камни:**
- Формат snapshot-файлов: `snapshot_NNN_layer_K.csv` — NNN с ведущими нулями (3 цифры), создаётся `std::snprintf` в `run_case_3d`
- Если Python не находит snapshot для нужного t — `min()` найдёт ближайший по времени

**Зависимости:**
- Требует: шаг 2 (CSV-файлы от визуального теста)
- Блокирует: ничего

**Оценка:** ~100 строк Python, ~15 минут

---

### Шаг 4: Документация — vault и GitHub issue

**Цель:** обновить vault и issue с результатами валидации

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — обновить статус VAL-020
- `vault/GDM/knowledge/validation/val-020 shutin restart close layer.md` — новая заметка с результатами

**Что сделать:**

1. Создать заметку `vault/GDM/knowledge/validation/val-020 shutin restart close layer.md`:

```markdown
---
tags:
  - валидация
  - self-consistency
  - перфорации
  - shut-in
date: 2026-07-XX
---

# VAL-020: закрытие перфорации + shut-in + restart — self-consistency

## Результат

[ПРОЙДЕН/НЕ ПРОЙДЕН]

## Сценарий

Nz=2, INJ+PROD с перфорациями в обоих слоях. Три фазы:
1. Работа 100 дней
2. Shut-in 50 дней
3. Restart с закрытым k=0, 150 дней

## Метрики

| Метрика | Значение | Критерий |
|---|---|---|
| max oil balance rel | ... | < 1e-3 |
| max water balance rel | ... | < 1e-3 |
| ΔSw в k=0 (restart − shut-in) | ... | ≈ 0 |

## Связанные

- [[val-019 inactive-layer-multizone-perf]] — close_layer + неактивные ячейки
- [[схема дискретизации полностью неявная а не IMPES]]
```

2. Обновить статус в `валидационные кейсы.md`: `⬜ НЕ НАЧАТО` → `✅ ПРОЙДЕН YYYY-MM-DD` (или `❌ НЕ ПРОЙДЕН`)

3. Добавить `- **План:** [[val-020 shutin-restart-close-layer]]` в запись VAL-020 в реестре

4. Прокомментировать issue #53: `gh issue comment 53 --repo ArturSalamatin/GDM --body "Результат: ..."` (с метриками и ссылкой на скриншоты)

5. Обновить `vault/GDM/00-home/index.md` — добавить ссылку на новую заметку

**Проверка после этого шага:**
- Заметка создана
- Реестр обновлён
- Issue прокомментирован

**Зависимости:**
- Требует: шаги 1, 2, 3 (результаты прогонов)
- Блокирует: ничего

**Оценка:** ~30 строк markdown, ~10 минут

## Тестовая стратегия

**Тест 1:** `3D completions: shut-in + restart with closed layer`
- **Тег:** `[3d][completions][shutin-restart][val-020]`
- **Файл:** `tests/test_3d_completions.cpp` (существующий)
- **Сценарий:** INJ+PROD, оба слоя → shut-in 50д → restart с k=0 закрытым
- **Setup:** 11×11×2, perm=100 mD, P_init=200 atm, Sw_init=0.2
- **Эталон:** self-consistency
- **Метрика:** max_oil_balance_rel, max_water_balance_rel, Sw[k=1] > Sw[k=0] вблизи INJ
- **Tolerance:** 1e-3 (баланс масс)
- **Предотвращает:** регрессию в обработке shut-in/restart + close_layer

**Тест 2:** `3D completions: shut-in + restart - visual`
- **Тег:** `[.visual][3d][completions][shutin-restart][val-020]`
- **Файл:** `tests/test_3d_completions.cpp` (существующий)
- **Сценарий:** то же, но сетка 21×21 для наглядности
- **Предотвращает:** визуальные аномалии, которые числовые проверки не ловят

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все 325 существующих тестов зелёные
- [ ] Новый количественный тест зелёный (баланс масс < 1e-3)
- [ ] Визуальная верификация пройдена (3 графика в `results/val-020/`)
- [ ] Vault обновлён: заметка в `knowledge/validation/`, статус в реестре
- [ ] GitHub issue #53 прокомментирован с результатом
- [ ] План помечен как `status: реализован`
