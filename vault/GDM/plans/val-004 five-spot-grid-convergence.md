---
tags:
  - план
  - валидация
  - сходимость
date: 2026-07-21
issue: VAL-004
github: 47
branch: val/val-004/five-spot-grid-convergence
status: готов к реализации
audit:
  date: 2026-07-21
  round: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# VAL-004: Five-spot сетчатая сходимость

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[five-spot сравнение с MRST]] — параметры сценария, скважины, начальные условия
   - [[val-002 grid-convergence-1d]] — образец реализации grid convergence (1D/radial)
3. Прочитай существующие файлы:
   - `tests/test_five_spot.cpp` — текущий five-spot тест (21×21, 5 скважин, симметрия)
   - `tests/test_buckley_leverett.cpp` строки 159–190 (`run_radial_simulation`) и 231–254 (`radial_L2`) — образец helper-функции для 2D grid convergence
   - `tests/test_helpers.h` — хелперы `make_uniform_horizon()`, `add_simple_well()`
4. Создай ветку: `git checkout -b val/val-004/five-spot-grid-convergence experimental`
5. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
6. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
7. Запомни количество тестов и время — это baseline (на момент аудита: 316 тестов)
8. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание задачи

VAL-004 — проверка сходимости решения five-spot на серии сеток (self-convergence). Аналитического решения для five-spot в замкнутой форме не существует — эталоном служит собственное решение GDM на мелкой сетке.

**Подтип:** сетчатая сходимость (self-convergence)

**Зависимости:**
- VAL-003 (🟡 частично) — тест five-spot работает, Newton сходится, симметрия подтверждена. Количественное сравнение с MRST не завершено, но не блокирует self-convergence
- VAL-002 (✅ пройден) — образец реализации grid convergence для 2D (radial BL)

---

## Текущее состояние

Тест `test_five_spot.cpp` проверяет:
- Newton сходится, bounds [0, 1] выполняются
- 4-кратная симметрия Sw (8 знаков)
- CSV-экспорт для MRST-сравнения

Но всё на одной сетке 21×21. Серии сеток нет.

Тест `test_buckley_leverett.cpp` содержит полный grid convergence для радиального BL (строки 306–347) — аналогичная задача в 2D, но с осевой симметрией. В VAL-004 осевой симметрии нет — нужно pointwise сравнение на 2D-сетке.

---

## Эталонное решение

**Тип:** fine-grid self-convergence.

Эталон — решение GDM на мелкой сетке N_ref = 161 (161×161 = 25921 ячейка). Это не аналитическое решение — ошибка O(h_ref). Для оценки порядка сходимости это допустимо, пока h_ref ≪ h_coarse (Roache, "Verification and Validation in Computational Science and Engineering", 1998, Ch. 5).

Для сеток {21, 41, 81} vs ref=161:
- h_ref = 500/161 ≈ 3.1 м
- h_81 = 500/81 ≈ 6.2 м — отношение h_81/h_ref ≈ 2.0
- h_41 = 500/41 ≈ 12.2 м — отношение h_41/h_ref ≈ 3.9
- h_21 = 500/21 ≈ 23.8 м — отношение h_21/h_ref ≈ 7.7

Для корректной оценки порядка нужно, чтобы ошибка e(h_coarse) ≫ e(h_ref). Первый порядок (upstream): e ~ h, значит e_81/e_ref ≈ 2 — допустимо, но не идеально. Если прогон 161×161 окажется слишком долгим (>5 мин) — оставить 161. Если быстро — рассмотреть N_ref=321, но это 103K ячеек и может быть дорого.

### Почему удвоение сеток

Серия {21, 41, 81, 161} — каждый раз N удваивается (≈×2). Это позволяет использовать стандартную формулу:

```
p = log₂(L2(N) / L2(2N))
```

В отличие от произвольных сеток {21, 51, 101}, где нужна общая формула log(e₁/e₂)/log(h₁/h₂). Удвоение — стандарт для grid convergence studies.

**Важно:** issue #47 указывает сетки {21, 51, 101}. План использует {21, 41, 81} + ref=161 — удвоение корректнее. Порядок сходимости не зависит от выбора точек (если все в asymptotic range), но log₂ проще и стандартнее.

**Замечание (аудит):** отношение N_{k+1}/N_k для серии {21, 41, 81} не точно 2 (41/21 ≈ 1.95). Формула log₂ вносит систематическую ошибку ~3.5% в оценку порядка. Точная формула: `p = log(L2_1/L2_2) / log(N_2/N_1)`. Однако VAL-002 использует ту же log₂ для аналогичных сеток — сохраняем consistency. При p ≈ 0.5 ошибка ≈ 0.017, tolerance p > 0.3 поглощает.

### Порядок сходимости

Для upstream-схемы (1-го порядка):
- В гладких областях: p ≈ 1
- На фронте насыщенности (разрыв): upstream размывает фронт на O(√h) ячеек, L2-ошибка O(√h), порядок p ≈ 0.5 (LeVeque, "Finite Volume Methods for Hyperbolic Problems", §8.6)
- Для five-spot: фронт кривой формы, не плоский — размытие зависит от ориентации сетки (grid orientation effect). Ожидание: p ≈ 0.3–1.0

**Tolerance:** p > 0.3 (как в VAL-002 radial, строка 345). Ниже 0.3 — сигнал о проблеме в схеме. Выше 1.0 — суперсходимость, тоже подозрительно (но не ошибка).

---

## Сценарий GDM

Параметры из [[five-spot сравнение с MRST]]:

| Параметр | Значение | Единицы |
|---|---|---|
| Lx × Ly | 500 × 500 | м |
| hz | 10 | м |
| k | 100 | мД |
| φ | 0.2 | — |
| μ_w / μ_o | 2.0 / 4.3 | мПа·с |
| ρ_w / ρ_o | 1000 / 800 | кг/м³ |
| k_rw / k_ro | S_w³ / (1-S_w)³ | Corey n=3 (default GDM) |
| P_init | 200 | атм |
| So_init | 0.999 | — (Sw_init = 0.001, CPR требует) |
| INJ | центр сетки | 50 м³/день воды (= -50000 кг/день) |
| P1..P4 | 4 угла | 12.5 м³/день нефти каждый (= 10000 кг/день) |
| T_end | 500 | дней |
| dt_initial | 5.0 | дней |

### Серия сеток

| Уровень | N | h, м | Ячеек | Роль |
|---|---|---|---|---|
| 0 | 21 | 23.8 | 441 | грубая |
| 1 | 41 | 12.2 | 1681 | средняя |
| 2 | 81 | 6.2 | 6561 | мелкая |
| ref | 161 | 3.1 | 25921 | эталон |

### Расположение скважин

Для сетки N×N:
- INJ: центр — `((N/2) + 0.5) * h, ((N/2) + 0.5) * h` (аналогично radial BL, строка 180)
- P1: `(0.5 * h, 0.5 * h)` — угол (0,0)
- P2: `(0.5 * h, (N-0.5) * h)` — угол (0,N-1)
- P3: `((N-0.5) * h, 0.5 * h)` — угол (N-1,0)
- P4: `((N-0.5) * h, (N-0.5) * h)` — угол (N-1,N-1)

Координаты скважин = центры угловых/центральной ячеек. При масштабировании сетки скважины всегда в тех же физических позициях.

---

## Метрики сравнения

### 1. L2-норма Sw на 2D-сетке

Для грубого решения Sw_coarse (N×N) vs эталона Sw_ref (N_ref×N_ref):

```
L2 = sqrt( (1 / A_total) * Σ_{i,j ∈ ref} (Sw_interp(i,j) - Sw_ref(i,j))² * h_ref² )
```

где `Sw_interp(i,j)` — значение грубого решения в точке (i,j) мелкой сетки. Интерполяция: nearest-neighbor (значение ближайшей ячейки грубой сетки). A_total = Lx × Ly.

Nearest-neighbor вносит ошибку O(h_coarse), что для first-order scheme не портит порядок. Bilinear интерполяция — опциональное улучшение, но для upstream на разрыве разница незначительна.

### 2. Порядок сходимости

```
p_k = log₂(L2[k-1] / L2[k])   для k = 1, 2
```

(3 грубые сетки → 2 пары: 21→41, 41→81)

**Tolerance:**
- L2 монотонно убывает: `L2[k] < L2[k-1]`
- Порядок: `p > 0.3` (upstream с разрывом + grid orientation effect)

### 3. Симметрия (sanity check)

Для каждой сетки проверить 4-кратную симметрию Sw. Если нарушена — проблема в solver или скважинах, а не в сходимости.

---

## Подводные камни

- [x] ✅ **Блокирующие баги:** Newton сходится для 21×21 (test_five_spot.cpp), нужно проверить для крупных сеток
- [x] ✅ **Начальные условия:** So_init=0.999 (Sw_init=0.001) — CPR требует ненулевую Sw. Одинаково для всех сеток
- [x] ✅ **Граничные условия:** нет BC — замкнутый пласт, все потоки через скважины. Баланс: Q_inj = 50 м³/день = 4 × 12.5 м³/день = Q_prod_total
- [x] ✅ **Единицы:** массовые расходы в кг/день. Q_inj_water = -50 × 1000 = -50000 кг/день. Q_prod_oil = 12.5 × 800 = 10000 кг/день на каждый продюсер
- [ ] ⚠️ **Время прогона N_ref=161:** ~26K ячеек, 500 дней. VAL-002 radial N_ref=321 (~103K) считает ~2–3 мин. 161×161 должен быть быстрее (~1–2 мин). Но 4 прогона суммарно — ~5–8 мин. Тест скрытый `[.]`, это допустимо
- [ ] ⚠️ **Grid orientation effect:** upstream-схема на прямоугольной сетке создаёт артефакты вдоль осей сетки. Five-spot фронт вытеснения зависит от ориентации. Это ожидаемо, не баг — но может влиять на порядок сходимости (p может быть ниже 0.5 в отдельных направлениях). Tolerance p > 0.3 это учитывает
- [ ] ⚠️ **r_app зависит от сетки:** `r_app = max(0.2*h, 0.2)`. Для N=21 r_app=4.76, для N=161 r_app=0.62. PI Писмана разный на разных сетках. Для self-convergence это допустимо — мы сравниваем решения GDM между собой, а не с аналитикой
- [x] ✅ **Численная диффузия:** upstream размывает фронт — ожидаемо. На мелких сетках фронт острее
- [x] ✅ **Баланс масс:** Q_inj = Q_prod_total → давление стационарно. Sw эволюционирует

---

## Обнаруженные проблемы

Нет новых проблем.

---

## Затронутые файлы

| Файл | Действие | Роль |
|---|---|---|
| `tests/test_five_spot.cpp` | модифицировать | добавить helper + grid convergence тесты |
| `tests/test_helpers.h` | не менять | используется as-is |
| `scripts/plot_five_spot_convergence.py` | создать | log-log convergence plot + field comparison |
| `vault/GDM/roadmap/валидационные кейсы.md` | модифицировать | обновить статус VAL-004 |

---

## Шаги реализации

### Шаг 1: Helper-функция `run_five_spot`

**Цель:** вынести настройку и прогон five-spot в переиспользуемую функцию (аналог `run_radial_simulation` из `test_buckley_leverett.cpp:159`).

**Файлы:** `tests/test_five_spot.cpp`

**Контекст:**
Текущий `test_five_spot.cpp` содержит inline-код создания симулятора и 5 скважин. Для grid convergence нужно прогонять тот же сценарий на разных сетках N. Вместо копирования 30 строк 4 раза — helper-функция. Функция в анонимном namespace файла (не в test_helpers.h, т.к. специфична для five-spot).

Существующий `run_radial_simulation` (test_buckley_leverett.cpp:159) — образец: принимает N и параметры, возвращает `std::vector<double>` (поле Sw). Для five-spot аналогично, но 5 скважин вместо одной.

**Что сделать:**
1. В `test_five_spot.cpp`, внутри анонимного namespace (после строки 25), добавить функцию:

```cpp
std::vector<double> run_five_spot(
    size_t N,
    double Lx, double Ly, double hz,
    double perm_mD, double poro,
    double P_init_atm, double So_init,
    double Q_inj_water_mass, double Q_prod_oil_mass,
    double T_end, double dt_save = 50.0) {

    auto horizon = test_helpers::make_uniform_horizon(
        N, N, 1, Lx, Ly, hz, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(5.0);
    sim.numPrm.set_currentMoment(0.0);

    double hx = Lx / N, hy = Ly / N;
    double r_app = std::max(0.2 * hx, 0.2);

    test_helpers::add_simple_well(sim, horizon,
        "INJ", (N / 2 + 0.5) * hx, (N / 2 + 0.5) * hy,
        0.0, Q_inj_water_mass, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P1", 0.5 * hx, 0.5 * hy,
        Q_prod_oil_mass, 0.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P2", 0.5 * hx, (N - 0.5) * hy,
        Q_prod_oil_mass, 0.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P3", (N - 0.5) * hx, 0.5 * hy,
        Q_prod_oil_mass, 0.0, r_app);
    test_helpers::add_simple_well(sim, horizon,
        "P4", (N - 0.5) * hx, (N - 0.5) * hy,
        Q_prod_oil_mass, 0.0, r_app);

    std::vector<double> timeMoments = {0.0};
    for (double t = dt_save; t < T_end; t += dt_save)
        timeMoments.push_back(t);
    timeMoments.push_back(T_end);

    sim.Solve(timeMoments);
    return sim.GetWaterSaturationField();
}
```

2. Рефакторинг существующих тестов НЕ делать — они работают, трогать не нужно. Helper используется только в новых тестах.

**Примечание (аудит):** `std::max`, `std::log2`, `std::sqrt`, `std::filesystem`, `std::string` доступны транзитивно через `test_helpers.h` → `stdafx.h`. Явные `#include` не нужны, но если при сборке возникнет ошибка — добавить `#include <cmath>`, `#include <filesystem>`, `#include <string>` в начало файла.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все существующие тесты зелёные (helper не вызывается, но должен скомпилироваться)

**Подводные камни:**
- `add_simple_well` с `r_app` — 7-й параметр, по умолчанию 0.0 (строка 80 test_helpers.h). Явно передавать r_app, т.к. при r_app=0 используется default формула из симулятора, которая может отличаться
- Позиции скважин: `(N/2 + 0.5) * hx` — целочисленное деление N/2. Для N=21: 10.5*hx. Для N=41: 20.5*hx. Для N=161: 80.5*hx. Всё в центре домена — корректно

**Зависимости:**
- Требует: ничего
- Блокирует: шаги 2, 3, 4

**Оценка:** ~40 строк, ~5 минут

---

### Шаг 2: L2-норма для 2D-поля (nearest-neighbor)

**Цель:** функция `five_spot_L2` — вычисление L2-нормы разности между грубым и эталонным полями Sw на 2D-сетке.

**Файлы:** `tests/test_five_spot.cpp`

**Контекст:**
В radial BL (test_buckley_leverett.cpp) используется `azimuthal_average` + `radial_L2` — усреднение по углу, сравнение 1D-профилей. Для five-spot осевой симметрии нет — нужно pointwise сравнение в 2D.

Подход: для каждой ячейки мелкой сетки (i_ref, j_ref) найти ближайшую ячейку грубой сетки (i_coarse, j_coarse) = nearest-neighbor. Сравнить Sw.

Nearest-neighbor: координата центра ячейки ref = ((i_ref+0.5)*h_ref, (j_ref+0.5)*h_ref). Индекс грубой ячейки: i_coarse = floor(x / h_coarse), j_coarse = floor(y / h_coarse), с clamp к [0, N_coarse-1].

**Что сделать:**
1. После `run_five_spot`, в том же анонимном namespace, добавить:

```cpp
double five_spot_L2(
    const std::vector<double>& Sw_coarse, size_t N_coarse,
    const std::vector<double>& Sw_ref, size_t N_ref,
    double Lx, double Ly) {

    double h_ref_x = Lx / N_ref;
    double h_ref_y = Ly / N_ref;
    double h_coarse_x = Lx / N_coarse;
    double h_coarse_y = Ly / N_coarse;
    double A_total = Lx * Ly;

    double sum_sq = 0.0;
    for (size_t j = 0; j < N_ref; ++j) {
        for (size_t i = 0; i < N_ref; ++i) {
            double x = (i + 0.5) * h_ref_x;
            double y = (j + 0.5) * h_ref_y;

            size_t ic = std::min(static_cast<size_t>(x / h_coarse_x),
                                 N_coarse - 1);
            size_t jc = std::min(static_cast<size_t>(y / h_coarse_y),
                                 N_coarse - 1);

            double Sw_interp = Sw_coarse[jc * N_coarse + ic];
            double diff = Sw_interp - Sw_ref[j * N_ref + i];
            sum_sq += diff * diff * h_ref_x * h_ref_y;
        }
    }
    return std::sqrt(sum_sq / A_total);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все существующие тесты зелёные

**Подводные камни:**
- Индексация Sw[j * N + i] — row-major (j = строка, i = столбец). Совпадает с `make_uniform_horizon` (строка 43 test_helpers.h) и `test_five_spot.cpp` (строка 83)
- `std::min` для clamp — предотвращает выход за границу при x = Lx (правый край)
- Lx = Ly для five-spot, но функция общая

**Зависимости:**
- Требует: ничего
- Блокирует: шаги 3, 4

**Оценка:** ~25 строк, ~5 минут

---

### Шаг 3: Тест grid convergence

**Цель:** основной тест — серия сеток, вычисление L2 и порядка сходимости.

**Файлы:** `tests/test_five_spot.cpp`

**Контекст:**
Аналог `"Radial BL: grid convergence of Sw(r) profile"` (test_buckley_leverett.cpp:306). Тест скрытый `[.]` — не запускается автоматически (долгий: ~5–8 мин). Запуск вручную: `ctest -R "Five-spot.*grid convergence"`.

Серия: {21, 41, 81} + ref=161. Для каждой грубой сетки: прогнать, интерполировать на ref-сетку, вычислить L2. Потом вычислить порядок p между парами.

**Что сделать:**
1. После существующих TEST_CASE (после строки 147), добавить:

```cpp
TEST_CASE("Five-spot: grid convergence of Sw field",
          "[five-spot][2d][validation][convergence][.]") {
    constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.999;
    constexpr double Q_inj_water = -50000.0;
    constexpr double Q_prod_oil = 10000.0;
    constexpr double T_end = 500.0;

    constexpr size_t N_ref = 161;
    constexpr size_t grids[] = {21, 41, 81};
    constexpr size_t NG = sizeof(grids) / sizeof(grids[0]);

    auto Sw_ref = run_five_spot(
        N_ref, Lx, Ly, hz, perm_mD, poro, P_init_atm, So_init,
        Q_inj_water, Q_prod_oil, T_end);
    REQUIRE(Sw_ref.size() == N_ref * N_ref);

    double L2[NG];
    for (size_t g = 0; g < NG; ++g) {
        auto Sw_coarse = run_five_spot(
            grids[g], Lx, Ly, hz, perm_mD, poro, P_init_atm, So_init,
            Q_inj_water, Q_prod_oil, T_end);
        REQUIRE(Sw_coarse.size() == grids[g] * grids[g]);
        L2[g] = five_spot_L2(Sw_coarse, grids[g], Sw_ref, N_ref, Lx, Ly);

        double h = Lx / grids[g];
        WARN("N=" << grids[g] << " h=" << h << " L2=" << L2[g]);
        REQUIRE(L2[g] >= 0.0);
    }

    for (size_t g = 1; g < NG; ++g) {
        double p = std::log2(L2[g - 1] / L2[g]);
        WARN("N=" << grids[g-1] << "->" << grids[g]
             << ": L2 " << L2[g-1] << " -> " << L2[g] << ", order p=" << p);
        CHECK(L2[g] < L2[g - 1]);
        CHECK(p > 0.3);
    }

    // Sanity check: симметрия на эталонной сетке (если нарушена — проблема не в сходимости)
    for (size_t i = 0; i < N_ref / 2; ++i) {
        for (size_t j = 0; j < N_ref / 2; ++j) {
            double s00 = Sw_ref[j * N_ref + i];
            double s10 = Sw_ref[j * N_ref + (N_ref - 1 - i)];
            double s01 = Sw_ref[(N_ref - 1 - j) * N_ref + i];
            double s11 = Sw_ref[(N_ref - 1 - j) * N_ref + (N_ref - 1 - i)];
            CHECK(std::abs(s00 - s10) < 1e-6);
            CHECK(std::abs(s00 - s01) < 1e-6);
            CHECK(std::abs(s00 - s11) < 1e-6);
        }
    }
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — скрытый тест не запустится
- Ручная проверка: `ctest --test-dir build -C Release -R "Five-spot.*grid convergence" --output-on-failure`
- Ожидание: L2 монотонно убывает, p > 0.3 для обеих пар
- Время: ~5–8 мин (4 прогона, максимальный 161×161 ~2 мин)

**Подводные камни:**
- N_ref=161 может быть медленным. Если >5 мин — OK для скрытого теста
- `run_five_spot` с dt_save=50 по умолчанию — для convergence нужен только финальный snapshot. Можно передать dt_save=T_end, тогда Solve получит {0, 500} — быстрее (меньше snapshotов). Но dt_save=50 безопаснее (адаптивный шаг видит промежуточные точки)
- Grid orientation effect: p может быть < 0.5 для five-spot на прямоугольной сетке. Tolerance p > 0.3 это учитывает
- Newton divergence на крупных сетках (81×81, 161×161): если `Solve()` бросит исключение, Catch2 перехватит и покажет сообщение. Специальная обработка не нужна — REQUIRE после Solve не выполнится, тест упадёт информативно

**Зависимости:**
- Требует: шаги 1, 2
- Блокирует: шаг 5

**Оценка:** ~35 строк, ~10 минут (включая прогон)

---

### Шаг 4: CSV-экспорт convergence-таблицы и Sw-полей

**Цель:** экспортировать L2-таблицу и 2D Sw-поля для визуализации.

**Файлы:** `tests/test_five_spot.cpp`

**Контекст:**
Два CSV:
1. `results/validation/five_spot_convergence.csv` — таблица (N, h, L2, p)
2. `results/validation/five_spot_Sw_N{21,41,81,161}.csv` — 2D-поля Sw (i, j, Sw) для каждой сетки

Формат Sw-полей: `i,j,Sw` — тот же, что в существующем экспорте (test_five_spot.cpp:139). Python-скрипт `plot_convergence.py` уже работает с этим форматом.

Один скрытый TEST_CASE, который и считает, и экспортирует (как VAL-002 convergence CSV export).

**Что сделать:**
1. После теста из шага 3, добавить:

```cpp
TEST_CASE("Five-spot: convergence CSV export",
          "[five-spot][2d][validation][convergence][.export]") {
    constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0, So_init = 0.999;
    constexpr double Q_inj_water = -50000.0;
    constexpr double Q_prod_oil = 10000.0;
    constexpr double T_end = 500.0;

    constexpr size_t N_ref = 161;
    constexpr size_t all_grids[] = {21, 41, 81, 161};
    constexpr size_t N_all = sizeof(all_grids) / sizeof(all_grids[0]);
    constexpr size_t NG = N_all - 1;

    auto vdir = std::filesystem::path(__FILE__).parent_path().parent_path()
              / "results" / "validation";
    std::filesystem::create_directories(vdir);

    std::vector<double> Sw_fields[N_all];
    for (size_t g = 0; g < N_all; ++g) {
        Sw_fields[g] = run_five_spot(
            all_grids[g], Lx, Ly, hz, perm_mD, poro, P_init_atm, So_init,
            Q_inj_water, Q_prod_oil, T_end);
        REQUIRE(Sw_fields[g].size() == all_grids[g] * all_grids[g]);

        // Export Sw field
        auto path = vdir / ("five_spot_Sw_N"
                   + std::to_string(all_grids[g]) + ".csv");
        std::ofstream ofs(path.string());
        ofs << "i,j,Sw\n";
        for (size_t j = 0; j < all_grids[g]; ++j)
            for (size_t i = 0; i < all_grids[g]; ++i)
                ofs << i << "," << j << ","
                    << std::setprecision(8)
                    << Sw_fields[g][j * all_grids[g] + i] << "\n";
        ofs.close();
        WARN("Sw field: " << path.string());
    }

    // Convergence table
    double L2[NG];
    for (size_t g = 0; g < NG; ++g) {
        L2[g] = five_spot_L2(Sw_fields[g], all_grids[g],
                              Sw_fields[N_all - 1], N_ref, Lx, Ly);
    }

    auto csv_path = vdir / "five_spot_convergence.csv";
    std::ofstream csv(csv_path.string());
    csv << "N,h,L2,p\n";
    csv << all_grids[0] << "," << Lx / all_grids[0] << ","
        << L2[0] << ",\n";
    for (size_t g = 1; g < NG; ++g) {
        double p = std::log2(L2[g - 1] / L2[g]);
        csv << all_grids[g] << "," << Lx / all_grids[g] << ","
            << L2[g] << "," << p << "\n";
    }
    csv.close();
    WARN("Convergence CSV: " << csv_path.string());
    CHECK(true);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — скрытый тест не запустится
- Ручная проверка: `ctest --test-dir build -C Release -R "Five-spot.*convergence CSV" --output-on-failure`
- Должны появиться: `results/validation/five_spot_convergence.csv`, `five_spot_Sw_N{21,41,81,161}.csv`

**Подводные камни:**
- `#include <string>` — может потребоваться для `std::to_string`. На MSVC обычно тянется транзитивно, но если ошибка — добавить
- `#include <filesystem>` — уже есть в test_buckley_leverett.cpp, но может отсутствовать в test_five_spot.cpp. Проверить при сборке, добавить если нужно
- `#include <iomanip>` — уже включён (строка 4)
- Прогон всех 4 сеток + ref суммарно ~5–8 мин

**Примечание (аудит):** шаг 4 повторно прогоняет все сетки, включая N_ref=161, хотя шаг 3 уже делал то же. При последовательном запуске обоих тестов — двойная работа (~10 мин лишних). Это допустимо: тесты скрытые `[.]` и запускаются вручную по выбору. Если пользователь хочет и convergence check, и CSV export — запускает `ctest -R "Five-spot.*convergence"` (оба), если только проверку — `ctest -R "grid convergence"`.

**Зависимости:**
- Требует: шаги 1, 2
- Блокирует: шаг 5

**Оценка:** ~50 строк, ~10 минут

---

### Шаг 5: Python-скрипт визуализации

**Цель:** log-log convergence plot + сравнение Sw-полей на разных сетках.

**Файлы:** `scripts/plot_five_spot_convergence.py` (новый)

**Контекст:**
Два графика в одном скрипте:
1. **Log-log convergence:** L2 vs h, reference slope p=1 и p=0.5
2. **Sw fields comparison:** imshow для каждой сетки (4 панели)

Существующий `scripts/plot_convergence.py` — более сложный (snapshot-формат, metadata.json). Для VAL-004 проще написать отдельный скрипт по образцу `scripts/plot_bl_convergence.py`.

**Что сделать:**
1. Создать `scripts/plot_five_spot_convergence.py`:

```python
"""
VAL-004: Five-spot grid convergence visualization.
Log-log convergence plot + Sw field comparison.

Usage:
    python scripts/plot_five_spot_convergence.py
"""
import csv
import numpy as np
import matplotlib.pyplot as plt


def load_convergence():
    data = {'N': [], 'h': [], 'L2': [], 'p': []}
    with open('results/validation/five_spot_convergence.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data['N'].append(int(row['N']))
            data['h'].append(float(row['h']))
            data['L2'].append(float(row['L2']))
            data['p'].append(float(row['p']) if row['p'] else None)
    return data


def load_sw_field(N):
    sw = np.zeros((N, N))
    with open(f'results/validation/five_spot_Sw_N{N}.csv') as f:
        reader = csv.DictReader(f)
        for row in reader:
            i, j = int(row['i']), int(row['j'])
            sw[j, i] = float(row['Sw'])
    return sw


def plot_convergence(data):
    h = np.array(data['h'])
    L2 = np.array(data['L2'])

    fig, ax = plt.subplots(figsize=(8, 6))
    ax.loglog(h, L2, 'ko-', markersize=8, label='GDM (upstream)')

    h_ref = np.array([h[-1], h[0]])
    L2_ref_1 = L2[-1] * (h_ref / h[-1]) ** 1.0
    L2_ref_05 = L2[-1] * (h_ref / h[-1]) ** 0.5
    ax.loglog(h_ref, L2_ref_1, 'r--', alpha=0.5, label='slope = 1')
    ax.loglog(h_ref, L2_ref_05, 'b--', alpha=0.5, label='slope = 0.5')

    for i in range(len(h)):
        label = f'N={data["N"][i]}'
        if data['p'][i] is not None:
            label += f', p={data["p"][i]:.2f}'
        ax.annotate(label, (h[i], L2[i]), textcoords="offset points",
                    xytext=(10, 5), fontsize=9)

    ax.set_xlabel('h (м)', fontsize=12)
    ax.set_ylabel('L2 error', fontsize=12)
    ax.set_title('VAL-004: Grid convergence — five-spot (self-convergence)',
                 fontsize=13)
    ax.legend(fontsize=11)
    ax.grid(True, which='both', alpha=0.3)
    plt.tight_layout()
    plt.savefig('results/validation/five_spot_convergence.png', dpi=150)
    plt.show()


def plot_fields():
    grids = [21, 41, 81, 161]
    fig, axes = plt.subplots(1, 4, figsize=(20, 5))

    for ax, N in zip(axes, grids):
        sw = load_sw_field(N)
        im = ax.imshow(sw, origin='lower', cmap='Blues',
                       vmin=0, vmax=1, extent=[0, 500, 0, 500],
                       aspect='equal')
        ax.set_title(f'$S_w$, {N}×{N}', fontsize=12)
        ax.set_xlabel('x, м')
    axes[0].set_ylabel('y, м')
    fig.colorbar(im, ax=axes, shrink=0.8, label='$S_w$')

    fig.suptitle('VAL-004: Sw field at t=500 days', fontsize=14)
    fig.tight_layout(rect=[0, 0, 0.92, 0.95])
    plt.savefig('results/validation/five_spot_fields.png', dpi=150)
    plt.show()


if __name__ == '__main__':
    data = load_convergence()
    plot_convergence(data)
    plot_fields()
```

**Проверка после этого шага:**
- Предварительно: CSV-файлы из шага 4 должны существовать
- `python scripts/plot_five_spot_convergence.py`
- Два png: `five_spot_convergence.png` (log-log) и `five_spot_fields.png` (4 панели Sw)

**Зависимости:**
- Требует: шаг 4 (CSV файлы)
- Блокирует: шаг 6

**Оценка:** ~80 строк, ~10 минут

---

### Шаг 6: Визуальная верификация

**Цель:** убедиться, что результаты физически корректны.

**Файлы:** нет (только просмотр)

**Контекст:**
Каждый физический тест нужно проверить визуально, не только Catch2 assertions.

**Что сделать:**
1. Запустить скрытые тесты для генерации CSV:
   ```
   ctest --test-dir build -C Release -R "Five-spot.*convergence" --output-on-failure
   ```
2. Запустить Python-скрипт:
   ```
   python scripts/plot_five_spot_convergence.py
   ```
3. Проверить convergence plot:
   - Точки на log-log графике монотонно убывают
   - Наклон между slope=0.5 и slope=1 (ожидаемо для upstream с разрывом)
   - Порядок p > 0.3 для каждой пары
4. Проверить Sw fields:
   - На грубой сетке (21×21) фронт сильно размыт, grid orientation effect виден
   - На мелких сетках фронт острее, более круглый
   - 4-кратная симметрия визуально сохраняется
   - Sw ∈ [0, 1] (colormap Blues корректен)

**Проверка после этого шага:**
- Два png-файла в `results/validation/`
- Визуальное подтверждение корректности

**Зависимости:**
- Требует: шаг 5
- Блокирует: шаг 7

**Оценка:** ~10 минут

---

### Шаг 7: Обновить vault и GitHub issue

**Цель:** зафиксировать результат в vault и на GitHub.

**Файлы:** `vault/GDM/roadmap/валидационные кейсы.md`

**Контекст:**
После успешного прохождения тестов и визуальной верификации — обновить статус VAL-004. Формат аналогичен VAL-002.

**Что сделать:**
1. В `vault/GDM/roadmap/валидационные кейсы.md` изменить статус VAL-004:
   ```
   - **Статус:** ✅ ПРОЙДЕН <дата> (L2 убывает, порядок p=X.XX–Y.YY)
   ```
   Вставить фактические значения p из WARN-вывода теста.

2. Обновить строку **Метрика** фактическими значениями.

3. Прокомментировать GitHub issue:
   ```
   gh issue comment 47 --repo ArturSalamatin/GDM --body "VAL-004 пройден. ..."
   ```

4. Закрытие issue — при merge ветки.

**Зависимости:**
- Требует: шаг 6
- Блокирует: ничего

**Оценка:** ~5 минут

---

## Тестовая стратегия

### Тест 1: Grid convergence (новый, скрытый)

**Тест:** `"Five-spot: grid convergence of Sw field"`
**Тег:** `[five-spot][2d][validation][convergence][.]`
**Файл:** `tests/test_five_spot.cpp` (новый TEST_CASE)
**Сценарий:** серия сеток {21, 41, 81}, эталон 161×161. Five-spot, 5 скважин, T=500 дней
**Setup:** Lx=Ly=500м, k=100мД, φ=0.2, Corey n=3, So_init=0.999
**Эталон:** GDM на сетке 161×161 (self-convergence)
**Метрика:** L2-норма Sw (2D pointwise, nearest-neighbor), порядок p = log₂(L2[k-1]/L2[k])
**Tolerance:** L2 монотонно убывает, p > 0.3
**Предотвращает:** деградацию порядка сходимости transport solver на 2D-задаче с криволинейным фронтом

### Тест 2: Convergence CSV export (новый, скрытый)

**Тест:** `"Five-spot: convergence CSV export"`
**Тег:** `[five-spot][2d][validation][convergence][.export]`
**Файл:** `tests/test_five_spot.cpp` (новый TEST_CASE)
**Сценарий:** тот же + экспорт L2-таблицы и 2D Sw-полей
**Метрика:** нет assert (CHECK(true)), только экспорт
**Предотвращает:** ничего (вспомогательный)

---

## Критерии завершения

- [ ] Тест grid convergence проходит: L2 убывает, p > 0.3
- [ ] Все существующие тесты зелёные
- [ ] CSV-файлы: five_spot_convergence.csv, five_spot_Sw_N{21,41,81,161}.csv
- [ ] Convergence plot: log-log с reference slopes (p=1, p=0.5)
- [ ] Sw fields: 4 панели, фронт острее на мелких сетках
- [ ] Визуальная верификация пройдена
- [ ] Vault обновлён: статус VAL-004 = ✅ ПРОЙДЕН
- [ ] GitHub issue #47 прокомментирован

---

## Связанные заметки

- [[five-spot сравнение с MRST]] — параметры сценария
- [[val-002 grid-convergence-1d]] — образец реализации grid convergence
- [[задача Бакли-Леверетта — аналитический тест для одномерного вытеснения]] — теория upstream-сходимости
