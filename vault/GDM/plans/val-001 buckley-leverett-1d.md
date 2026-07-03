---
tags:
  - план
  - валидация
  - аналитический
date: 2026-07-03
issue: VAL-001
github: 15
branch: val/val-001/buckley-leverett-1d
status: готов к реализации
audit:
  date: 2026-07-03
  findings: 0 / 2 / 2
  auto-fixed: 4
  manual-required: 0
---

# VAL-001: Buckley–Leverett 1D — валидация профиля насыщенности

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[задача Бакли-Леверетта — аналитический тест для одномерного вытеснения]]
   - [[относительные проницаемости задаются степенными моделями Кори]]
   - [[несжимаемость фаз упрощает уравнение неразрывности до дивергенции скорости]]
3. Прочитай существующие файлы:
   - `tests/buckley_leverett_analytical.h` — аналитическое решение
   - `tests/test_buckley_leverett.cpp` — существующие тесты
   - `tests/test_helpers.h` — хелперы `make_uniform_horizon()`, `add_simple_well()`
4. Создай ветку: `git checkout -b val/val-001/buckley-leverett-1d experimental`
5. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
6. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
7. Baseline: 290 тестов, 0 failed, ~51 сек
8. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание задачи

VAL-001 — количественная валидация GDM на задаче одномерного вытеснения нефти водой (Buckley–Leverett). Задача имеет аналитическое решение, что позволяет вычислить точную ошибку численной схемы.

**Подтип:** аналитический

**Зависимости:** BUG-001 (✅ исправлен)

---

## Эталонное решение

### Постановка

Одномерная задача вытеснения нефти водой в трубке тока длиной L:
- Две несжимаемые несмешивающиеся фазы
- Без капиллярного давления (p_c = 0)
- Без гравитации
- На входе (x=0) — закачка воды с постоянным расходом q_t
- Начальные условия: Sw = 0 (только нефть, Sw_init = 0 в нормированных координатах)

### Относительные проницаемости (Corey, n=3)

GDM использует степенную модель (hardcoded `permPower = 3` в `TwoPhaseFlowCell.h:18`):

```
kr_w(Sw) = Sw^3
kr_o(Sw) = (1 - Sw)^3
```

Остаточные насыщенности Sor = Swr = 0 (default в `PhaseFactory.hpp:15,19` — `resFrac = 0.0` для обеих фаз).

### Функция фракционного потока

```
f_w(Sw) = M * Sw^3 / (M * Sw^3 + (1-Sw)^3)
```

где M = μ_o / μ_w = 4.3 / 2.0 = 2.15 — отношение вязкостей (mobility ratio).

### Скорость фронта (Welge)

Фронт вытеснения (скачок насыщенности) определяется касательной из начала координат к кривой f_w(Sw). Точка касания Swf удовлетворяет:

```
f'_w(Swf) = f_w(Swf) / Swf
```

Скорость фронта:

```
v_front = (q_t / (φ * A)) * f'_w(Swf)
```

Положение фронта: x_front = v_front * t.

За фронтом (x > x_front): Sw = 0 (исходная нефть).
Перед фронтом (x < x_front): Sw(x,t) определяется из df_w/dSw = x * φ * A / (q_t * t).

### Реализация

Аналитическое решение **уже реализовано** в `tests/buckley_leverett_analytical.h`:
- `f_w(Sw, M)` — фракционный поток
- `df_w(Sw, M)` — производная
- `find_Swf(M)` — бисекция для Swf (конструкция Вэлджа)
- `analytical_profile(x_centers, qt, phi, A, t, M)` — полный профиль Sw(x,t)

### Связь аналитики с GDM

Аналитическое решение `analytical_profile()` возвращает **нормированную** насыщенность Sw_scaled (от 0 до 1), идентичную `SWater_Scaled()` в GDM при Sor = Swr = 0. `GetWaterSaturationField()` возвращает **физическую** насыщенность, но при Sor = Swr = 0 нормированная = физическая.

Важный момент: `analytical_profile()` считает начальную Sw = 0 (изначально только нефть). В GDM начальные условия задаются через `initial_oil_saturation`. При `So_init = 1.0` получаем `Sw_init = 0` — аналитика и GDM согласованы.

---

## Сценарий GDM

### Параметры

| Параметр | Значение | Единицы | Обоснование |
|---|---|---|---|
| Nx | 100 | — | достаточно для L2, не слишком долго (~5с) |
| Ny, Nz | 1 | — | 1D задача |
| Lx | 100.0 | м | нормированная длина |
| Ly (= hy) | 1.0 | м | сечение трубки A = hy * hz = 1 м² |
| hz | 1.0 | м | |
| Perm | 100.0 | мД | стандартная для BL |
| Poro | 0.2 | — | стандартная |
| P_init | 200.0 | атм | достаточно высокое для стабильности |
| So_init | 1.0 | — | начально — только нефть (Sw_init = 0) |
| μ_o | 4.3 | мПа·с | default GDM (PhaseFactory) |
| μ_w | 2.0 | мПа·с | default GDM (PhaseFactory) |
| M = μ_o/μ_w | 2.15 | — | отношение подвижностей |
| Water injection rate | -1000.0 | кг/день | масса воды в скважину INJ (x=0.5м) |
| Oil production rate | 0.0 | кг/день | добычная скважина PROD (x=99.5м), rate=0 означает свободный переток |

### Эквивалент объёмного расхода

Для аналитического решения нужен q_t (м³/день):
```
q_t = |water_mass_rate| / ρ_w = 1000 / 1000 = 1.0 м³/день
```

Сечение трубки: A = hy * hz = 1.0 * 1.0 = 1.0 м².

### Время прогона

Для M = 2.15 и Corey n=3, Swf ≈ 0.46, f'(Swf) ≈ 1.57.
Скорость фронта: v_front = q_t * f'(Swf) / (φ * A) = 1.0 * 1.57 / (0.2 * 1.0) = 7.85 м/день.
Время прохождения ~60% длины (60 м): t ≈ 60 / 7.85 ≈ 7.6 дней.

Выберем t_final = 8.0 дней — фронт внутри домена, но далеко от правой границы.

### Скважины

В GDM 1D Buckley–Leverett реализуется через скважины:
- **INJ** (нагнетательная): x = hx/2 = 0.5 м (центр первой ячейки), закачка воды -1000 кг/день
- **PROD** (добывающая): x = Lx - hx/2 = 99.5 м (центр последней ячейки), oil_rate = 1000 кг/день (добыча нефти)

Использование `add_simple_well()` из `test_helpers.h` с параметрами:
- INJ: `oil_mass_rate = 0.0`, `water_mass_rate = -1000.0` (закачка)
- PROD: `oil_mass_rate = 1000.0`, `water_mass_rate = 0.0` (добыча)

### Нюанс: скважина vs boundary condition

Аналитика Buckley–Leverett предполагает постоянный расход на входе. Скважина в GDM — не идеальный boundary condition, а source term в уравнении. Для Nx=100 ячеек ячейка достаточно мала (hx=1м), чтобы скважинный source хорошо аппроксимировал boundary. Тем не менее, L2-норму считаем по ячейкам 2..Nx-2 (исключая скважинные ячейки), чтобы скважинный эффект не вносил систематической ошибки.

---

## Метрики сравнения

### 1. L2-норма ошибки профиля Sw(x)

```
L2 = sqrt(Σᵢ (Sw_GDM[i] - Sw_analytical[i])² * hx / Lx)
```

Суммирование по внутренним ячейкам (i = 1..Nx-2, исключая скважинные ячейки 0 и Nx-1).

**Tolerance:** L2 < 0.05 (5%). Upstream-схема даёт значительную численную диффузию на Nx=100. Для Corey n=3 с M=2.15 фронт размывается на ~10 ячеек.

### 2. Положение фронта

Фронт GDM: ячейка с максимальным |dSw/dx| (max grad).
Фронт аналитика: x_front = v_front * t.

**Tolerance:** |x_front_GDM - x_front_analytical| < 3*hx = 3.0 м.

### 3. Монотонность и физичность

- Sw монотонно убывает от входа к выходу (допуск: Sw[i+1] <= Sw[i] + 1e-6)
- 0 <= Sw <= 1 для всех ячеек

---

## Подводные камни

- [x] ✅ **Блокирующие баги:** BUG-001 исправлен, Newton сходится при закачке воды
- [x] ✅ **Согласованность единиц:** вязкость мПа·с → Па·день (CustomUnitConverter), расход кг/день, всё внутренне согласовано
- [x] ✅ **Граничные условия:** GDM использует закрытые границы + скважины; аналитика — постоянный расход на входе. Различие адресовано исключением скважинных ячеек из L2
- [x] ✅ **Начальные условия:** So_init = 1.0 (Sw_init = 0) — совпадает с аналитикой
- [x] ✅ **Численная диффузия:** upstream-схема размывает фронт — это ожидаемо, не баг. Tolerance учитывает
- [ ] ⚠️ **Временной шаг:** GDM использует адаптивный шаг (PI-контроллер). При large CFL фронт может смещаться. Решение: использовать initial dt = 0.01 дней, solver сам подберёт
- [ ] ⚠️ **Добывающая скважина:** если PROD стоит в последней ячейке, при достижении фронта скважина начнёт добывать воду, что изменит баланс. Решение: t_final < t_breakthrough, фронт не достигает PROD
- [ ] ⚠️ **GetWaterSaturationField() vs SWater_Scaled():** при Sor=Swr=0 они идентичны. Проверить, что GetWaterSaturationField() возвращает вектор длины Nx*Ny*Nz

---

## Обнаруженные проблемы

Нет новых проблем.

---

## Затронутые файлы

| Файл | Действие | Роль |
|---|---|---|
| `tests/test_buckley_leverett.cpp` | модифицировать | добавить новые TEST_CASE |
| `tests/buckley_leverett_analytical.h` | не менять | используется as-is |
| `tests/test_helpers.h` | не менять | используется as-is |

---

## Тестовая стратегия

### Тест 1: L2-норма профиля Sw

**Тест:** `"BL validation: Sw profile L2 error vs analytical"`
**Тег:** `[buckley-leverett][validation]`
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Сценарий:** Nx=100 1D вытеснение, сравнение Sw(x) с аналитикой при t=8 дней
**Setup:** Nx=100, Lx=100, hy=hz=1, perm=100мД, poro=0.2, P_init=200атм, So=1.0; INJ в x=0.5 (-1000 кг/день воды), PROD в x=99.5 (+1000 кг/день нефти)
**Эталон:** `buckley_leverett::analytical_profile()` с qt=1.0 м³/день, phi=0.2, A=1.0, t=8.0, M=2.15
**Метрика:** L2-норма по внутренним ячейкам (1..Nx-2)
**Tolerance:** L2 < 0.05
**Предотвращает:** регрессию transport solver, ошибки в upstream-взвешивании, ошибки в знаках/единицах

### Тест 2: положение фронта

**Тест:** `"BL validation: front position within 3 cells of analytical"`
**Тег:** `[buckley-leverett][validation]`
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Сценарий:** тот же прогон, проверка x_front_GDM vs x_front_analytical
**Метрика:** |x_front_GDM - x_front_analytical| < 3*hx
**Предотвращает:** грубые ошибки в скорости распространения фронта

### Тест 3: монотонность и физичность

**Тест:** `"BL validation: monotonicity and bounds"`
**Тег:** `[buckley-leverett][validation]`
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Сценарий:** тот же прогон, проверка Sw[i] ∈ [0,1] и монотонность
**Предотвращает:** осцилляции, отрицательные насыщенности

### Тест 4: CSV-экспорт для визуальной верификации

**Тест:** `"BL validation: CSV export for visual check"`
**Тег:** `[buckley-leverett][validation][.]`
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Сценарий:** запись GDM и аналитического профиля в CSV
**Предотвращает:** отсутствие данных для визуальной проверки
**Примечание:** тег `[.]` — тест запускается только вручную (генерирует файл)

### Regression: существующие тесты, которые не должны сломаться

- `"BL analytical: fractional flow properties"` — тест аналитических формул
- `"BL analytical: derivative vs finite difference"` — df_w vs конечные разности
- `"BL analytical: shock front Welge construction"` — конструкция Вэлджа
- `"Well injection: basic well works without crash"` — INJ на Nx=10
- `"Well injection: Sw increases with water injection"` — INJ + Sw растёт

---

## Критерии завершения

- [ ] L2-тест зелёный (L2 < 0.05)
- [ ] Фронт-тест зелёный (ΔxF < 3*hx)
- [ ] Монотонность-тест зелёный
- [ ] CSV-export тест работает при ручном запуске
- [ ] Все существующие 290 тестов зелёные
- [ ] Визуальная верификация пройдена (график Sw_GDM vs Sw_analytical)
- [ ] Vault обновлён: заметка в `knowledge/validation/`, статус в реестре `валидационные кейсы.md`
- [ ] Roadmap обновлён

---

## Шаги реализации

### Шаг 1: Добавить количественный L2-тест

**Цель:** создать тест, который запускает 1D Buckley–Leverett в GDM и сравнивает профиль Sw(x) с аналитическим решением по L2-норме.

**Файлы:** `tests/test_buckley_leverett.cpp`

**Контекст:**
Задача Бакли–Леверетта — базовый аналитический тест для двухфазного транспорта. Аналитическое решение уже реализовано в `tests/buckley_leverett_analytical.h` (функции `f_w`, `df_w`, `find_Swf`, `analytical_profile`). Существующие well-тесты (строки 78–141) проверяют только bounds (Sw >= 0, Sw <= 1) на грубой сетке Nx=10, без количественного сравнения с аналитикой.

GDM использует Corey n=3 (`TwoPhaseFlowCell.h:18`, `permPower = 3`), Sor = Swr = 0 (`PhaseFactory.hpp:15,19`, `resFrac = 0.0`), μ_o = 4.3 мПа·с, μ_w = 2.0 мПа·с (`PhaseFactory.hpp:15,19`). Эти параметры полностью совпадают с аналитикой.

Аналитическое решение `analytical_profile()` принимает qt (объёмный расход, м³/день), phi (пористость), A (сечение, м²), t (время, дни), M (отношение вязкостей). Расход qt связан с массовым расходом: qt = |water_mass_rate| / ρ_w = 1000/1000 = 1.0 м³/день.

**Что сделать:**

1. В файле `tests/test_buckley_leverett.cpp`, после последнего TEST_CASE (строка ~141), добавить новый тест:

**Изменения (новый код после строки 141):**

```cpp
TEST_CASE("BL validation: Sw profile L2 error vs analytical",
          "[buckley-leverett][validation]") {
    constexpr size_t Nx = 100;
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 1.0;
    constexpr double water_inject_rate = -1000.0; // кг/день, закачка
    constexpr double oil_prod_rate = 1000.0;      // кг/день, добыча
    constexpr double t_final = 8.0;               // дни

    // GDM setup
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    test_helpers::add_simple_well(sim, horizon,
        L"INJ", hx * 0.5, hy * 0.5, 0.0, water_inject_rate);
    test_helpers::add_simple_well(sim, horizon,
        L"PROD", Lx - hx * 0.5, hy * 0.5, oil_prod_rate, 0.0);

    sim.Solve({0.0, t_final});

    auto Sw_gdm = sim.GetWaterSaturationField();
    REQUIRE(Sw_gdm.size() == Nx);

    // Analytical solution
    constexpr double M = 4.3 / 2.0;
    constexpr double qt = 1.0; // м³/день = |water_inject_rate| / rho_w
    constexpr double A = hy * hz;
    std::vector<double> x_centers(Nx);
    for (size_t i = 0; i < Nx; ++i) x_centers[i] = (i + 0.5) * hx;
    auto Sw_analytical = buckley_leverett::analytical_profile(
        x_centers, qt, poro, A, t_final, M);

    // L2 norm over interior cells (exclude well cells 0 and Nx-1)
    double sum_sq = 0.0;
    size_t count = 0;
    for (size_t i = 1; i + 1 < Nx; ++i) {
        double diff = Sw_gdm[i] - Sw_analytical[i];
        sum_sq += diff * diff;
        ++count;
    }
    double L2 = std::sqrt(sum_sq * hx / Lx);

    INFO("L2 error = " << L2);
    CHECK(L2 < 0.05);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: 291 тестов (290 + 1 новый), все зелёные, L2 < 0.05

**Подводные камни:**
- ⚠️ Если `Solve()` с `t_final = 8.0` прогоняет слишком долго — уменьшить `t_final` до 4.0 (фронт при ~31 м, достаточно далеко от границ)
- ⚠️ Если L2 > 0.05 — это может означать, что upstream-диффузия на Nx=100 сильнее ожидаемого. Решение: увеличить tolerance до 0.08 или увеличить Nx до 200
- ⚠️ `add_simple_well` с `oil_mass_rate = 0.0, water_mass_rate = -1000.0` создаёт чистую закачку воды. Проверить что MER-record c `oil_m = 0` не вызывает проблем
- ⚠️ `GetWaterSaturationField()` — проверить, что это физическая насыщенность (при Sor=Swr=0 совпадает с нормированной)

**Зависимости:**
- Требует: нет (первый шаг)
- Блокирует: шаг 2, шаг 3

**Оценка:** ~40 строк, ~20 минут (включая время прогона тестов)

---

### Шаг 2: Добавить тест положения фронта и монотонности

**Цель:** проверить, что фронт вытеснения находится в правильном месте и профиль монотонен.

**Файлы:** `tests/test_buckley_leverett.cpp`

**Контекст:**
Шаг 1 проверяет L2-норму профиля. Этот шаг добавляет два дополнительных теста: (1) положение фронта — ячейка с максимальным градиентом Sw совпадает с аналитическим x_front, (2) монотонность — Sw убывает от входа к выходу, без осцилляций.

Фронт GDM определяется как ячейка с максимальным |Sw[i] - Sw[i+1]| (максимальный скачок). Аналитический фронт: x_front = v_front * t, где v_front = qt * f'(Swf) / (φ * A).

**Что сделать:**

1. В файле `tests/test_buckley_leverett.cpp`, после тест-кейса из шага 1, добавить:

```cpp
TEST_CASE("BL validation: front position and monotonicity",
          "[buckley-leverett][validation]") {
    constexpr size_t Nx = 100;
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 1.0;
    constexpr double t_final = 8.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    test_helpers::add_simple_well(sim, horizon,
        L"INJ", hx * 0.5, hy * 0.5, 0.0, -1000.0);
    test_helpers::add_simple_well(sim, horizon,
        L"PROD", Lx - hx * 0.5, hy * 0.5, 1000.0, 0.0);

    sim.Solve({0.0, t_final});

    auto Sw = sim.GetWaterSaturationField();
    REQUIRE(Sw.size() == Nx);

    // Front position: cell with max |Sw[i] - Sw[i+1]|
    double max_grad = 0.0;
    size_t front_cell = 0;
    for (size_t i = 1; i + 2 < Nx; ++i) {
        double grad = std::abs(Sw[i] - Sw[i + 1]);
        if (grad > max_grad) {
            max_grad = grad;
            front_cell = i;
        }
    }
    double x_front_gdm = (front_cell + 0.5) * hx;

    constexpr double M = 4.3 / 2.0;
    double Swf = buckley_leverett::find_Swf(M);
    double v_front = 1.0 * buckley_leverett::df_w(Swf, M) / (poro * hy * hz);
    double x_front_analytical = v_front * t_final;

    INFO("GDM front at x = " << x_front_gdm);
    INFO("Analytical front at x = " << x_front_analytical);
    CHECK(std::abs(x_front_gdm - x_front_analytical) < 3.0 * hx);

    // Monotonicity: Sw should decrease from inlet to outlet
    SECTION("monotonicity and bounds") {
        for (size_t i = 0; i < Nx; ++i) {
            CHECK(Sw[i] >= -1e-6);
            CHECK(Sw[i] <= 1.0 + 1e-6);
        }
        for (size_t i = 2; i + 2 < Nx; ++i) {
            CHECK(Sw[i] <= Sw[i - 1] + 1e-6);
        }
    }
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: 293 теста (291 + 1 тест + 1 SECTION = 2), все зелёные

**Подводные камни:**
- ⚠️ SECTION создаёт отдельный test case в ctest, поэтому +2 теста, не +1
- ⚠️ Монотонность проверяется с i=2, чтобы исключить скважинную ячейку INJ (i=0) и её соседа
- ⚠️ Определение фронта через max_grad может дать неточность при сильной диффузии. 3*hx — разумный допуск

**Зависимости:**
- Требует: шаг 1 (общая setup-логика, но шаг 2 самостоятелен — дублирует setup)
- Блокирует: шаг 3

**Оценка:** ~45 строк, ~15 минут

---

### Шаг 3: CSV-export тест для визуальной верификации

**Цель:** создать скрытый тест (`[.]`), генерирующий CSV с GDM и аналитическим профилем для построения графика.

**Файлы:** `tests/test_buckley_leverett.cpp`

**Контекст:**
Визуальная верификация — обязательная часть валидации. CSV-файл позволяет построить overlay-график Sw_GDM vs Sw_analytical. Тест помечен тегом `[.]` — не запускается автоматически ctest, только вручную (`ctest -R "BL validation: CSV"`).

**Что сделать:**

1. Добавить `#include <fstream>` в начало файла `tests/test_buckley_leverett.cpp` (если ещё нет).

2. После тестов из шагов 1–2, добавить:

```cpp
TEST_CASE("BL validation: CSV export for visual check",
          "[buckley-leverett][validation][.]") {
    constexpr size_t Nx = 200;
    constexpr double Lx = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double So_init = 1.0;
    constexpr double t_final = 8.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, Lx, hy, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(0.01);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / Nx;
    test_helpers::add_simple_well(sim, horizon,
        L"INJ", hx * 0.5, hy * 0.5, 0.0, -1000.0);
    test_helpers::add_simple_well(sim, horizon,
        L"PROD", Lx - hx * 0.5, hy * 0.5, 1000.0, 0.0);

    sim.Solve({0.0, t_final});

    auto Sw_gdm = sim.GetWaterSaturationField();

    constexpr double M = 4.3 / 2.0;
    constexpr double qt = 1.0;
    constexpr double A = hy * hz;
    std::vector<double> x_centers(Nx);
    for (size_t i = 0; i < Nx; ++i) x_centers[i] = (i + 0.5) * hx;
    auto Sw_analytical = buckley_leverett::analytical_profile(
        x_centers, qt, poro, A, t_final, M);

    std::ofstream csv("bl_validation_profile.csv");
    csv << "x,Sw_GDM,Sw_analytical\n";
    for (size_t i = 0; i < Nx; ++i) {
        csv << x_centers[i] << "," << Sw_gdm[i] << "," << Sw_analytical[i] << "\n";
    }
    csv.close();

    // L2 for reference
    double sum_sq = 0.0;
    for (size_t i = 1; i + 1 < Nx; ++i) {
        double diff = Sw_gdm[i] - Sw_analytical[i];
        sum_sq += diff * diff;
    }
    double L2 = std::sqrt(sum_sq * hx / Lx);
    INFO("CSV written to bl_validation_profile.csv, L2 = " << L2);
    CHECK(true);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — новый тест НЕ должен появиться в автоматическом прогоне (тег `[.]`)
- Ожидаемый результат: 293 теста (как после шага 2 — тест с `[.]` не считается)
- Ручная проверка: `ctest --test-dir build -C Release -R "BL validation: CSV"` — должен пройти и создать `bl_validation_profile.csv`

**Подводные камни:**
- ⚠️ CSV-тест использует Nx=200 (мельче, чем L2-тест) для более красивого графика
- ⚠️ Файл `bl_validation_profile.csv` создаётся в текущей директории ctest (`build/`). Это нормально — файл не коммитится

**Зависимости:**
- Требует: шаг 1 (подтверждает работоспособность setup)
- Блокирует: нет

**Оценка:** ~40 строк, ~10 минут

---

### Шаг 4: Визуальная верификация и обновление vault

**Цель:** запустить CSV-тест, построить график, обновить vault.

**Файлы:**
- `vault/GDM/knowledge/validation/задача Бакли-Леверетта — аналитический тест для одномерного вытеснения.md`
- `vault/GDM/roadmap/валидационные кейсы.md`
- `vault/GDM/00-home/index.md`

**Контекст:**
После прохождения всех тестов — визуальная верификация (overlay-график) и обновление vault (статус, заметка).

**Что сделать:**

1. Запустить CSV-тест:
   ```
   ctest --test-dir build -C Release -R "BL validation: CSV"
   ```
2. Проверить наличие `build/bl_validation_profile.csv`
3. Построить overlay-график через HTML-артефакт или Python:
   - Прочитать CSV: три колонки x, Sw_GDM, Sw_analytical
   - Построить два графика на одних осях: Sw_GDM (точки или линия) vs Sw_analytical (линия)
   - Заголовок: «VAL-001: Buckley–Leverett 1D, Nx=200, t=8 дней»
   - Оси: x (м), Sw (доля)
4. Убедиться что:
   - Фронт GDM расплывчатый (upstream-диффузия), но в правильном месте
   - За фронтом Sw ≈ 0
   - Перед фронтом Sw монотонно убывает от ~1 до Swf

5. Обновить `vault/GDM/roadmap/валидационные кейсы.md`:
   - VAL-001: изменить статус на `✅ ПРОЙДЕН <дата>`, добавить поле `План:`

6. Обновить `vault/GDM/knowledge/validation/задача Бакли-Леверетта — аналитический тест для одномерного вытеснения.md`:
   - Добавить раздел «Результаты валидации» с L2-нормой и описанием

7. Обновить `vault/GDM/00-home/index.md` — если создана новая заметка (не нужно, обновляем существующую)

8. Обновить frontmatter этого плана: `status: реализован`

**Проверка после этого шага:**
- CSV-файл существует и содержит данные
- График визуально корректен
- Vault обновлён

**Зависимости:**
- Требует: шаги 1, 2, 3
- Блокирует: нет

**Оценка:** ~30 строк vault-правок, ~15 минут
