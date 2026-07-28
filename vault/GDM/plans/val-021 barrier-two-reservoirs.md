---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-27
issue: VAL-021
github: 54
branch: val/val-021/barrier-two-reservoirs
status: реализован
audit:
  date: 2026-07-27
  round: 4
  findings: 1 / 1 / 0
  auto-fixed: 2
  manual-required: 0
---

# VAL-021: Барьер из неактивных ячеек + два независимых резервуара

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - `vault/GDM/knowledge/validation/val-010 неактивная граничная ячейка.md` — паттерн проверки изоляции
   - `vault/GDM/roadmap/стратегия тестирования GDM.md` — правила тестирования
3. Создай ветку: `git checkout -b val/val-021/barrier-two-reservoirs`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Baseline: ~68 тестов. Запомни количество и время
7. Начни с шага 1. После каждого шага: сборка + тесты

## Подтип

**Инвариантный** (self-consistency). Эталон — не аналитическое решение, а внутренняя согласованность: два резервуара, разделённых барьером из неактивных ячеек, не должны влиять друг на друга.

## Эталонное решение

Инварианты:
1. **Изоляция барьера:** `Sw` и `P` в ячейках барьера = начальные условия (tolerance 1e-12)
2. **Изоляция половин:** `Sw` и `P` в правой половине не зависят от скважин левой, и наоборот. За дальнюю зону от каждого INJ считаем ячейки противоположной половины
3. **Динамика:** каждая половина имеет ненулевую водонасыщенность вблизи своего инжектора — т.е. фильтрация происходит
4. **Баланс масс:** невязка баланса для всей системы (global) в пределах tolerance

Формального разделения баланса по половинам через API симулятора нет — `GetOverallBalance()` возвращает глобальный баланс. Поэтому баланс проверяем глобально, а изоляцию — поточечно.

## Сценарий GDM

```
Сетка:  Nx=21, Ny=5, Nz=1
Физика: Lx=210 м, Ly=50 м, hz=10 м
        k=100 мД, φ=0.2, P_init=200 атм, So_init=0.8
Барьер: столбец i=10 (все j) → active_cells[Nx*j + 10] = false

Левая половина (i=0..9):
  INJ_L:  x = hx*1.5,  y = hy*2.5  → ячейка (1, 2)
  PROD_L: x = hx*8.5,  y = hy*2.5  → ячейка (8, 2)

Правая половина (i=11..20):
  INJ_R:  x = hx*12.5, y = hy*2.5  → ячейка (12, 2)
  PROD_R: x = hx*19.5, y = hy*2.5  → ячейка (19, 2)

Дебиты: INJ_L, INJ_R — закачка воды 30 м³/сут
        PROD_L, PROD_R — добыча нефти 20 м³/сут
Время: 100 дней

hx = 210/21 = 10 м, hy = 50/5 = 10 м
```

Скважины расположены зеркально-симметрично относительно барьера: INJ_L(1,2)↔PROD_R(19,2) и PROD_L(8,2)↔INJ_R(12,2). Расстояние INJ→PROD одинаково в обеих половинах (7 ячеек). При зеркальной симметрии ожидаем Sw(i,j) ≈ Sw(20−i, j) с точностью roundoff — можно проверить как бонус.

## Метрики

| # | Метрика | Tolerance | Тип |
|---|---|---|---|
| 1 | Sw в ячейках барьера (i=10) = Sw_init | ε = 1e-12 | абсолютная |
| 2 | P в ячейках барьера (i=10) = P_init | ε = 1e-12 | абсолютная |
| 3 | Sw правой половины (i≥11) не изменилась из-за INJ_L | ε = 1e-12 | абсолютная (вариант B) |
| 4 | Sw левой половины (i≤9) не изменилась из-за INJ_R | ε = 1e-12 | абсолютная (вариант B) |
| 5 | Вблизи INJ_L есть ячейки с Sw > Sw_init + 1e-10 | — | булева |
| 6 | Вблизи INJ_R есть ячейки с Sw > Sw_init + 1e-10 | — | булева |

**Замечание к метрикам 3–4:** поскольку обе половины имеют свои скважины, «Sw не изменилась из-за чужого INJ» нельзя проверить напрямую. Вместо этого проверяем строго: ячейки **на другой стороне барьера** от данного INJ сохраняют начальные условия **только если** на той стороне нет своих скважин. Но у нас есть скважины с обеих сторон. Поэтому метрики 3–4 реализуются через **сравнение двух прогонов**:

**Вариант A (одна система, простой):** один прогон с 4 скважинами, проверяем только метрики 1, 2, 5, 6. Изоляция подтверждается тем, что барьер сохраняет init.

**Вариант B (две системы, строгий):** три прогона — (1) обе половины, (2) только левая, (3) только правая. Сравниваем поля: Sw левой половины в прогоне 1 == Sw в прогоне 2. Аналогично для правой.

Выбираем **Вариант A** как основной тест (достаточно для валидации) + **Вариант B** как optional visual-тест.

## Подводные камни

- [x] **Блокирующие баги:** BUG-009 (фильтр ActiveCells) исправлен ✅
- [x] **Согласованность единиц:** не актуально (self-consistency, нет внешнего эталона) ✅
- [x] **Граничные условия:** no-flow BC по умолчанию — барьер усиливает их, не конфликтует ✅
- [x] **Начальные условия:** единые, задаются через `make_uniform_horizon` ✅
- [x] **Временной шаг:** 100 дней достаточно для прохождения фронта на 8 ячеек (80 м) ✅
- [x] **Сетка:** нечётное Nx=21 даёт ровно один столбец-барьер i=10 ✅
- [x] **Численная диффузия:** upstream-схема размывает фронт — ожидаемо, не баг ✅
- [x] **Симметрия:** скважины симметричны → Sw(i,j) ≈ Sw(20-i, j) с точностью roundoff — можно проверить как бонус ✅

## Шаги

### Шаг 1: Количественный тест — барьер + две пары скважин

**Цель:** основной Catch2-тест, проверяющий что барьер полностью изолирует две половины сетки при наличии 4 скважин.

**Файлы:** `tests/test_inactive_cells.cpp` (существующий, ~869 строк)

**Контекст:**
Тест строится по паттерну существующих VAL-008 тестов (строки 175–232 в `test_inactive_cells.cpp`).
VAL-008 проверял барьер с одним INJ с одной стороны. VAL-021 усиливает: по обе стороны барьера — своя пара INJ+PROD. Если барьер работает корректно, ячейки барьера (i=10) сохраняют начальные Sw и P. Используем `WellScheduleBuilder` (как в VAL-019, строки 830–843) для удобства задания 4 скважин.

**Что сделать:**

Добавить в конец `test_inactive_cells.cpp` тест:

```cpp
TEST_CASE("Barrier isolates two independent reservoirs",
          "[integration][inactive-cells][val-021]") {
    using Catch::Approx;

    constexpr size_t Nx = 21, Ny = 5, Nz = 1;
    constexpr double Lx = 210.0, Ly = 50.0, hz = 10.0;
    constexpr double P_init_atm = 200.0;
    constexpr double oil_sat = 0.8;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    double Sw_init = 1.0 - oil_sat;
    double P_init_Pa = P_init_atm * 101325.0;

    // Барьер: столбец i=10
    constexpr size_t barrier_i = 10;
    for (size_t j = 0; j < Ny; ++j)
        horizon.active_cells[Nx * j + barrier_i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx;
    double hy = Ly / Ny;

    // Левая половина: INJ_L (1,2) + PROD_L (8,2)
    test_helpers::WellScheduleBuilder inj_l("INJ_L", hx * 1.5, hy * 2.5);
    inj_l.inject_water(30.0).for_days(100.0);
    inj_l.add_to_sim(sim, horizon);

    test_helpers::WellScheduleBuilder prod_l("PROD_L", hx * 8.5, hy * 2.5);
    prod_l.produce_oil(20.0).for_days(100.0);
    prod_l.add_to_sim(sim, horizon);

    // Правая половина: INJ_R (12,2) + PROD_R (19,2)
    test_helpers::WellScheduleBuilder inj_r("INJ_R", hx * 12.5, hy * 2.5);
    inj_r.inject_water(30.0).for_days(100.0);
    inj_r.add_to_sim(sim, horizon);

    test_helpers::WellScheduleBuilder prod_r("PROD_R", hx * 19.5, hy * 2.5);
    prod_r.produce_oil(20.0).for_days(100.0);
    prod_r.add_to_sim(sim, horizon);

    double oil_mass_0 = sim.OilTotal();
    double water_mass_0 = sim.WaterTotal();

    sim.Solve({0.0, 100.0});

    REQUIRE(std::isfinite(sim.OilTotal()));
    REQUIRE(std::isfinite(sim.WaterTotal()));

    auto Sw = sim.GetWaterSaturationField();
    auto P  = sim.GetPressureField();

    // Метрика 1–2: ячейки барьера сохраняют init
    for (size_t j = 0; j < Ny; ++j) {
        size_t l = Nx * j + barrier_i;
        REQUIRE(Sw[l] == Approx(Sw_init).epsilon(1e-12));
        REQUIRE(P[l]  == Approx(P_init_Pa).epsilon(1e-12));
    }

    // Метрика 5: левая половина имеет динамику вблизи INJ_L
    bool left_has_water = false;
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < barrier_i; ++i) {
            size_t l = Nx * j + i;
            if (Sw[l] > Sw_init + 1e-10)
                left_has_water = true;
        }
    REQUIRE(left_has_water);

    // Метрика 6: правая половина имеет динамику вблизи INJ_R
    bool right_has_water = false;
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = barrier_i + 1; i < Nx; ++i) {
            size_t l = Nx * j + i;
            if (Sw[l] > Sw_init + 1e-10)
                right_has_water = true;
        }
    REQUIRE(right_has_water);

    // Инвариант 4: глобальный баланс масс
    // Паттерн из VAL-019 (test_inactive_cells.cpp:799): нормировка на начальную массу
    auto bal = sim.GetOverallBalance();
    double oil_residual   = bal[1] + bal[2] - bal[3];
    double water_residual = bal[4] + bal[5] - bal[6];
    REQUIRE(std::abs(oil_residual) / oil_mass_0 < 1e-3);
    if (water_mass_0 > 0)
        REQUIRE(std::abs(water_residual) / water_mass_0 < 1e-3);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все существующие тесты зелёные + новый тест зелёный

**Подводные камни:**
- `WellScheduleBuilder` по умолчанию создаёт 1 слой перфорации (hz=10), Nz=1 — совпадает со сценарием
- Барьер i=10 при Nx=21 — ровно серединный столбец

**Зависимости:**
- Требует: ничего (первый шаг)
- Блокирует: шаги 2, 3

**Оценка:** ~80 строк, ~15 минут

---

### Шаг 2: Visual-тест — CSV export с картой обеих половин

**Цель:** CSV-экспорт полей Sw, P + карта активности ячеек для визуальной верификации в Python.

**Файлы:** `tests/test_inactive_cells.cpp` (существующий)

**Контекст:**
Паттерн — VAL-008 CSV export (строки 284–325 в `test_inactive_cells.cpp`). Тег `[.visual]` — тест скрыт от ctest (Catch2 скрывает теги с точкой). Запуск только напрямую через exe: `build\Release\gdm_tests.exe "[.visual][val-021]"`. Результаты в `results/val-021/`.

**Что сделать:**

Добавить в конец `test_inactive_cells.cpp`:

```cpp
TEST_CASE("Two independent reservoirs - CSV export",
          "[.visual][inactive-cells][val-021]") {
    constexpr size_t Nx = 21, Ny = 5, Nz = 1;
    constexpr double Lx = 210.0, Ly = 50.0, hz = 10.0;
    constexpr double P_init_atm = 200.0;
    constexpr double oil_sat = 0.8;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, P_init_atm, oil_sat);

    constexpr size_t barrier_i = 10;
    std::vector<bool> is_inactive(Nx * Ny, false);
    for (size_t j = 0; j < Ny; ++j) {
        horizon.active_cells[Nx * j + barrier_i] = false;
        is_inactive[Nx * j + barrier_i] = true;
    }

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    double hx = Lx / Nx, hy = Ly / Ny;

    test_helpers::WellScheduleBuilder inj_l("INJ_L", hx * 1.5, hy * 2.5);
    inj_l.inject_water(30.0).for_days(100.0);
    inj_l.add_to_sim(sim, horizon);

    test_helpers::WellScheduleBuilder prod_l("PROD_L", hx * 8.5, hy * 2.5);
    prod_l.produce_oil(20.0).for_days(100.0);
    prod_l.add_to_sim(sim, horizon);

    test_helpers::WellScheduleBuilder inj_r("INJ_R", hx * 12.5, hy * 2.5);
    inj_r.inject_water(30.0).for_days(100.0);
    inj_r.add_to_sim(sim, horizon);

    test_helpers::WellScheduleBuilder prod_r("PROD_R", hx * 19.5, hy * 2.5);
    prod_r.produce_oil(20.0).for_days(100.0);
    prod_r.add_to_sim(sim, horizon);

    sim.Solve({0.0, 100.0});

    std::filesystem::create_directories("results/val-021");
    std::ofstream ofs("results/val-021/two_reservoirs.csv");
    ofs << "i,j,active,half,P_atm,Sw\n";

    auto P  = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();

    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i) {
            size_t l = Nx * j + i;
            const char* half = (i < barrier_i) ? "left"
                             : (i == barrier_i) ? "barrier"
                             : "right";
            ofs << i << "," << j << ","
                << (is_inactive[l] ? 0 : 1) << ","
                << half << ","
                << std::setprecision(8) << P[l] / 101325.0 << ","
                << Sw[l] << "\n";
        }

    auto bal = sim.GetOverallBalance();
    std::ofstream bal_ofs("results/val-021/mass_balance.csv");
    bal_ofs << "oil_total,water_total,oil_residual,water_residual\n";
    bal_ofs << sim.OilTotal() << "," << sim.WaterTotal() << ","
            << (bal[1] + bal[2] - bal[3]) << ","
            << (bal[4] + bal[5] - bal[6]) << "\n";
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Visual-тест отдельно: `build\Release\gdm_tests.exe "[.visual][val-021]"`
- Проверить что файлы `results/val-021/two_reservoirs.csv` и `results/val-021/mass_balance.csv` созданы

**Подводные камни:**
- Столбец `half` (left/barrier/right) упрощает фильтрацию в Python-скрипте

**Зависимости:**
- Требует: шаг 1 (тот же сценарий)
- Блокирует: шаг 3

**Оценка:** ~60 строк, ~10 минут

---

### Шаг 3: Python-скрипт визуализации

**Цель:** heatmap Sw + overlay давления, показывающий два изолированных резервуара. Аналог `scripts/plot_val010.py`.

**Файлы:** `scripts/plot_val021.py` (новый)

**Контекст:**
Python-скрипт читает `results/val-021/two_reservoirs.csv` и строит:
1. Heatmap Sw с барьером (штриховкой или серым цветом для неактивных ячеек)
2. Heatmap P
3. Маркеры скважин (INJ синий, PROD красный)

**Что сделать:**

Создать `scripts/plot_val021.py`:

```python
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv("results/val-021/two_reservoirs.csv")
Nx, Ny = df['i'].max() + 1, df['j'].max() + 1

Sw = df.pivot(index='j', columns='i', values='Sw').values
P  = df.pivot(index='j', columns='i', values='P_atm').values
active = df.pivot(index='j', columns='i', values='active').values

wells = {
    'INJ_L':  (1, 2),  'PROD_L': (8, 2),
    'INJ_R':  (12, 2), 'PROD_R': (19, 2),
}

fig, axes = plt.subplots(1, 2, figsize=(14, 4))

for ax, field, title, cmap in [
    (axes[0], Sw, 'Sw', 'RdYlBu_r'),
    (axes[1], P,  'P (атм)', 'viridis'),
]:
    masked = np.ma.masked_where(active == 0, field)
    im = ax.pcolormesh(masked, cmap=cmap, edgecolors='gray', linewidth=0.3)
    # Барьер — серые ячейки
    barrier = np.ma.masked_where(active == 1, np.ones_like(field))
    ax.pcolormesh(barrier, cmap='Greys', vmin=0, vmax=2, alpha=0.5)
    fig.colorbar(im, ax=ax, shrink=0.8)
    ax.set_title(title)
    ax.set_xlabel('i')
    ax.set_ylabel('j')
    ax.set_aspect('equal')
    for name, (wi, wj) in wells.items():
        color = 'blue' if 'INJ' in name else 'red'
        ax.plot(wi + 0.5, wj + 0.5, 'o', color=color, markersize=8)
        ax.annotate(name, (wi + 0.5, wj + 0.5), fontsize=7,
                    ha='center', va='bottom', color=color)

fig.suptitle('VAL-021: два независимых резервуара с барьером')
plt.tight_layout()
plt.savefig('results/val-021/two_reservoirs.png', dpi=150)
plt.show()
```

**Проверка после этого шага:**
- Запустить visual-тест: `build\Release\gdm_tests.exe "[.visual][val-021]"`
- Запустить скрипт: `python scripts/plot_val021.py`
- Визуально убедиться: фронт Sw не пересекает барьер, обе половины имеют динамику

**Зависимости:**
- Требует: шаг 2 (CSV-файл)
- Блокирует: шаг 4

**Оценка:** ~45 строк, ~10 минут

---

### Шаг 4: Обновление vault

**Цель:** зафиксировать результат в vault — заметка, статус, ссылки.

**Файлы:**
- `vault/GDM/knowledge/validation/val-021 barrier two independent reservoirs.md` (новый)
- `vault/GDM/roadmap/валидационные кейсы.md` (обновить статус)
- `vault/GDM/00-home/index.md` (добавить ссылку)

**Что сделать:**

1. Создать `vault/GDM/knowledge/validation/val-021 barrier two independent reservoirs.md`:

```markdown
---
tags:
  - валидация
  - неактивные ячейки
  - барьер
  - изоляция
date: <дата прогона>
---

# VAL-021: барьер из неактивных ячеек + два независимых резервуара

**Подтип:** инвариантный (self-consistency).

**Проверяем:** полная изоляция двух резервуаров, разделённых столбцом неактивных ячеек. По обе стороны барьера — своя пара INJ+PROD. Фронт Sw не пересекает барьер.

## Сценарий

21×5×1, барьер i=10. Левая: INJ_L(1,2) + PROD_L(8,2). Правая: INJ_R(12,2) + PROD_R(19,2).
Закачка воды 30 м³/сут, добыча нефти 20 м³/сут, время 100 дней.

## Результат

<заполнить после прогона>

## Связанные

- [[val-010 неактивная граничная ячейка]] — VAL-010: деактивация граничных ячеек
- [[val-008 barrier-inactive-cells]] — VAL-008: барьер блокирует перенос (одна пара скважин)
- [[BUG-009 active cells filter]] — баг фильтрации неактивных ячеек (исправлен)
```

2. Обновить статус в `валидационные кейсы.md`:
   - `- **Статус:** ⬜ НЕ НАЧАТО` → `- **Статус:** ✅ ПРОЙДЕН <дата>`
   - Добавить `- **Подробности:** [[val-021 barrier two independent reservoirs]]`
   - (Поля **План**, **Ветка**, **GitHub** уже добавлены при создании плана)

3. `vault/GDM/00-home/index.md` — ссылки на план и заметку уже добавлены при создании плана. Проверить, что ссылка на knowledge-заметку также присутствует.

4. Прокомментировать GitHub issue #54: результат, ссылка на заметку.

**Проверка после этого шага:**
- Wiki-ссылки корректны
- Frontmatter заполнен

**Зависимости:**
- Требует: шаги 1–3 (результаты прогона)
- Блокирует: ничего

**Оценка:** ~15 минут

---

## Тестовая стратегия

**Тест 1:**
- **Тест:** Barrier isolates two independent reservoirs
- **Тег:** `[integration][inactive-cells][val-021]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** 21×5 сетка, барьер i=10, 4 скважины (2 пары INJ+PROD)
- **Эталон:** self-consistency (ячейки барьера = init, обе половины с динамикой)
- **Метрика:** абсолютная (Sw, P в барьере), булева (динамика в каждой половине), относительная (баланс масс)
- **Tolerance:** 1e-12 (барьер), 1e-10 (динамика), 0.1% (баланс масс, как в VAL-019)
- **Предотвращает:** утечка потока через барьер неактивных ячеек при наличии скважин с обеих сторон

**Тест 2:**
- **Тест:** Two independent reservoirs - CSV export
- **Тег:** `[.visual][inactive-cells][val-021]`
- **Файл:** `tests/test_inactive_cells.cpp` (существующий)
- **Сценарий:** тот же
- **Эталон:** визуальный (Python heatmap)
- **Предотвращает:** неправильная физика, которую Catch2 assert не ловит

## Критерии завершения

- [ ] Все шаги выполнены
- [ ] Все существующие тесты зелёные
- [ ] Новый количественный тест зелёный
- [ ] Visual-тест генерирует CSV
- [ ] Python-скрипт строит heatmap — фронт не пересекает барьер
- [ ] Vault обновлён: заметка + статус + index
- [ ] GitHub issue #54 прокомментирован
