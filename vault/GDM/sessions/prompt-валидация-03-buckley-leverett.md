---
tags:
  - промпт
  - валидация
  - buckley-leverett
  - двухфазный
  - аналитика
  - catch2
date: 2026-06-17
---

# Промпт: Валидация #03 — Buckley–Leverett (1D двухфазное вытеснение)

Скопируй этот промпт в новую сессию Claude Code целиком. Проект находится в `d:\Lessons\Grants\2027\GDM`.

---

## Контекст проекта

GDM — двухфазный (нефть–вода) гидродинамический симулятор. Обе фазы несжимаемые. C++23, CMake + Visual Studio 2022 (MSVC), Windows 10.

**Правила работы описаны в `CLAUDE.md` — прочитай его перед началом.**
Прочитай `vault/GDM/00-home/текущие приоритеты.md` для текущего статуса.

### Тестовый фреймворк

Catch2 v3 настроен (промпт #00). Общие утилиты: `tests/test_helpers.h`. Все тесты — в `gdm_tests`.

**ВАЖНО:** тесты #01 (давление) и #02 (баланс масс) должны быть пройдены до этого шага.

Сборка и запуск:
```powershell
cmake --build build --config Debug
.\build\Debug\gdm_tests.exe [buckley-leverett]
```

## Цель

Проверить корректность **транспорта насыщенности** (двухфазного потока) сравнением с аналитическим решением Buckley–Leverett для 1D вытеснения нефти водой.

## Физика задачи Buckley–Leverett

### Постановка

1D пласт длины L. Слева закачка воды с постоянным расходом q_t. Справа — открытая граница (постоянное давление). Начальное условие: S_w = S_wc (связанная вода).

### Параметры GDM

Из кода (`TwoPhaseFlowCell.cpp`):
- `permPower = 3` (показатель Кори)
- `S_wc = 0`, `S_or = 0` (PhaseFactory defaults)
- `μ_w = 2.0 мПа·с`, `μ_o = 4.3 мПа·с`
- M = μ_o/μ_w = 2.15

### Функция Бакли–Леверетта

```
f_w(S) = M·S³ / (M·S³ + (1−S)³)

f'_w(S) = 3·M·S²·(1−S)² / (M·S³ + (1−S)³)²
```

### Скачок: условие Ренкина–Гюгонио

При S_wc = 0: касательная из начала координат к кривой f_w(S):
```
f'_w(S_wf) = f_w(S_wf) / S_wf

→ 3·(1−S_wf)² = S_wf · (M·S_wf³ + (1−S_wf)³)
```

## Задание

### Шаг 1. Реализуй аналитическое решение

Добавь в `tests/test_helpers.h` или создай отдельный `tests/buckley_leverett_analytical.h`:

```cpp
namespace buckley_leverett {

// Fractional flow для модели Кори n=3
inline double f_w(double Sw, double M) {
    double s3 = Sw * Sw * Sw;
    double q3 = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);
    return M * s3 / (M * s3 + q3);
}

// Производная df_w/dSw
inline double df_w(double Sw, double M) {
    double s2 = Sw * Sw;
    double q2 = (1.0 - Sw) * (1.0 - Sw);
    double s3 = s2 * Sw;
    double q3 = q2 * (1.0 - Sw);
    double denom = M * s3 + q3;
    return 3.0 * M * s2 * q2 / (denom * denom);
}

// Фронтовая насыщенность: решение f'_w(S) = f_w(S)/S бисекцией
inline double find_Swf(double M, double tol = 1e-12) {
    // g(S) = f'_w(S) - f_w(S)/S = 0
    // На (0,1): g(ε) > 0, g(1−ε) < 0
    double lo = 0.01, hi = 0.99;
    while (hi - lo > tol) {
        double mid = 0.5 * (lo + hi);
        double val = df_w(mid, M) - f_w(mid, M) / mid;
        if (val > 0) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

// Аналитический профиль S_w(x, t)
// Возвращает вектор S_w для ячеек с центрами x_centers
inline std::vector<double> analytical_profile(
    const std::vector<double>& x_centers,
    double qt,       // объёмный расход, м³/день
    double phi,      // пористость
    double A,        // площадь сечения, м²
    double t,        // время, дни
    double M)        // μ_o/μ_w
{
    double Swf = find_Swf(M);
    double v_front = qt * df_w(Swf, M) / (phi * A);
    double x_front = v_front * t;

    std::vector<double> Sw(x_centers.size(), 0.0);
    for (size_t i = 0; i < x_centers.size(); ++i) {
        double x = x_centers[i];
        if (x >= x_front) {
            Sw[i] = 0.0;  // S_wc = 0
        } else {
            // Решить x = qt*t/(phi*A) * f'_w(S) → f'_w(S) = x*phi*A/(qt*t)
            double target = x * phi * A / (qt * t);
            // Бисекция: df_w(Swf) = target при x=x_front,
            // df_w(1) = 0 < target < df_w(Swf)
            // На зоне разрежения S ∈ (Swf, 1): df_w убывает
            double lo = Swf, hi = 1.0 - 1e-10;
            for (int iter = 0; iter < 100; ++iter) {
                double mid = 0.5 * (lo + hi);
                if (df_w(mid, M) > target) lo = mid;
                else hi = mid;
            }
            Sw[i] = 0.5 * (lo + hi);
        }
    }
    return Sw;
}

} // namespace buckley_leverett
```

### Шаг 2. Создай тестовый файл

Создай `tests/test_buckley_leverett.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include "buckley_leverett_analytical.h"

using Catch::Matchers::WithinAbs;
```

### Шаг 3. Тест аналитических формул

```cpp
TEST_CASE("BL analytical: fractional flow properties",
          "[buckley-leverett][analytical]") {
    constexpr double M = 4.3 / 2.0;  // = 2.15

    // Граничные значения
    REQUIRE_THAT(buckley_leverett::f_w(0.0, M), WithinAbs(0.0, 1e-15));
    REQUIRE_THAT(buckley_leverett::f_w(1.0, M), WithinAbs(1.0, 1e-15));

    // Монотонность
    double prev = 0.0;
    for (int i = 1; i <= 100; ++i) {
        double s = i / 100.0;
        double f = buckley_leverett::f_w(s, M);
        CHECK(f >= prev);
        prev = f;
    }

    // Фронтовая насыщенность
    double Swf = buckley_leverett::find_Swf(M);
    INFO("Swf = " << Swf);
    CHECK(Swf > 0.2);
    CHECK(Swf < 0.8);

    // Условие касательной: f'(Swf) = f(Swf)/Swf
    double lhs = buckley_leverett::df_w(Swf, M);
    double rhs = buckley_leverett::f_w(Swf, M) / Swf;
    REQUIRE_THAT(lhs, WithinRel(rhs, 1e-8));
}
```

### Шаг 4. Тест полной симуляции vs аналитика

```cpp
TEST_CASE("BL simulation: 1D waterflood vs analytical",
          "[buckley-leverett][analytical][transport]") {
    // Параметры
    constexpr size_t Nx = 200;
    constexpr double L = 100.0, hy = 1.0, hz = 1.0;
    constexpr double perm_mD = 100.0, poro = 0.2;
    constexpr double P_init_atm = 200.0;
    constexpr double M = 4.3 / 2.0;
    constexpr double A = hy * hz;

    // Подбери q_t чтобы фронт прошёл ~40% длины за T_end
    double Swf = buckley_leverett::find_Swf(M);
    double T_end = 50.0;  // дни
    double x_front_target = 0.4 * L;
    double v_front = x_front_target / T_end;
    double qt = v_front * poro * A / buckley_leverett::df_w(Swf, M);
    // qt в м³/день, переведи в кг/день для MER: Q_mass = qt * ρ_w

    // Построить 1D horizon: Nx×1×1, S_oil = 1 (S_w = 0)
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, 1, 1, L, hy, hz, perm_mD, poro, P_init_atm, 1.0);

    // ... добавить скважины (нагнетательная слева, RefPressure справа)
    // ... собрать MER, WellJobs, вызвать AddWell_FixedProduction

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(T_end / 100.0);
    sim.numPrm.set_currentMoment(0.0);

    // Добавить скважину (реализуй через add_simple_well helper)
    // ...

    // Запуск
    std::vector<double> timeMoments = {0.0};
    for (double t = T_end / 10; t <= T_end; t += T_end / 10)
        timeMoments.push_back(t);
    sim.Solve(timeMoments);

    // Сравнение
    auto Sw_num = sim.GetWaterSaturationField();
    double hx = L / Nx;
    std::vector<double> x_centers(Nx);
    for (size_t i = 0; i < Nx; ++i) x_centers[i] = (i + 0.5) * hx;

    auto Sw_exact = buckley_leverett::analytical_profile(
        x_centers, qt, poro, A, T_end, M);

    // L1 error
    double L1 = 0.0;
    for (size_t i = 0; i < Nx; ++i)
        L1 += std::abs(Sw_num[i] - Sw_exact[i]) * hx;
    L1 /= L;

    INFO("L1 error = " << L1);
    CHECK(L1 < 0.05);  // 5% средняя ошибка для Nx=200

    // Баланс масс
    double water_total = sim.WaterTotal();
    double water_injected = qt * 1000.0 * T_end;  // ρ_w * qt * T
    // water_total ≈ initial_water + injected - produced - boundary_flux
    // Точная проверка зависит от деталей BC
}
```

**ВАЖНО:** секция добавления скважины помечена `// ...` — нужно исследовать API и реализовать. Добавь helper `add_simple_well()` в `test_helpers.h`.

### Шаг 5. Тест сходимости по сетке

```cpp
TEST_CASE("BL convergence: L1 error decreases with refinement",
          "[buckley-leverett][analytical][convergence]") {
    constexpr double M = 4.3 / 2.0;
    std::vector<size_t> grids = {50, 100, 200};
    std::vector<double> errors;

    for (auto Nx : grids) {
        // ... построить, решить, посчитать L1 error
        double L1 = /* вычислить */;
        errors.push_back(L1);
        INFO("Nx=" << Nx << " L1=" << L1);
    }

    // Ошибка убывает
    for (size_t i = 1; i < errors.size(); ++i) {
        CHECK(errors[i] < errors[i-1]);
        double order = std::log2(errors[i-1] / errors[i]);
        INFO("Order " << grids[i-1] << "->" << grids[i] << ": " << order);
        CHECK(order > 0.4);  // upstream: order ~0.5–1.0 из-за скачка
    }
}
```

### Шаг 6. Зарегистрируй в CMakeLists.txt

```cmake
target_sources(gdm_tests PRIVATE tests/test_buckley_leverett.cpp)
```

## Добавление скважин — подробности

Цепочка данных для скважины (из предыдущих промптов):

```
SingleWell_MER_Data = vector<map<wstring, float>>
WellJobs = (name, WellJobsData, LayerAggregationTree, NmbrOfLayers)
WellPosition = GeosPoint(x, y)
```

Знак дебита: **отрицательный = закачка** (из комментария в MER_Data).

Рекомендация: создай в `test_helpers.h`:
```cpp
// Добавить вертикальную скважину с постоянным дебитом
void add_simple_well(
    reservoir_simulator::ReservoirSimulator& sim,
    reservoir_simulator::DevelopedHorizon& horizon,
    const std::wstring& name,
    double x, double y,          // координаты
    double oil_rate_kg_day,      // >0 добыча, <0 закачка
    double water_rate_kg_day,    // >0 добыча, <0 закачка
    double start_time = 0.0,
    double r_well = 0.1);
```

Реализация потребует изучения конструкторов `WellJobs`, `MER_Data` и `AccumulatedPerforations`. Это нетривиально — выдели время на разбор.

## Аналитическое решение: ключевые формулы

Для n = 3, M = μ_o/μ_w = 2.15, S_wc = S_or = 0:

```
f_w(S) = M·S³ / (M·S³ + (1−S)³)
f'_w(S) = 3·M·S²·(1−S)² / (M·S³ + (1−S)³)²

Фронт: 3·(1−S_wf)² = S_wf · (M·S_wf³ + (1−S_wf)³)
x_front(t) = qt·t/(φ·A) · f'_w(S_wf)
```

## Критерии успеха

1. `gdm_tests.exe [buckley-leverett]` — все тесты PASSED
2. Аналитические формулы корректны (self-test)
3. Профиль S_w(x) качественно совпадает: разрежение + скачок
4. Положение фронта совпадает ± 2–3 ячейки
5. L1-ошибка убывает при измельчении
6. Баланс масс выполняется

## По завершении

1. Создай `vault/GDM/knowledge/validation/buckley-leverett тест 1D.md`
2. Если найден баг — заметку в `vault/GDM/knowledge/debugging/`
3. Обнови `vault/GDM/00-home/текущие приоритеты.md`
