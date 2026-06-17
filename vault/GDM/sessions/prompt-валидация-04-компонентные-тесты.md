---
tags:
  - промпт
  - валидация
  - unit-tests
  - компоненты
  - catch2
date: 2026-06-17
---

# Промпт: Валидация #04 — компонентные тесты (unit-level)

Скопируй этот промпт в новую сессию Claude Code целиком. Проект находится в `d:\Lessons\Grants\2027\GDM`.

---

## Контекст проекта

GDM — двухфазный (нефть–вода) гидродинамический симулятор. Обе фазы несжимаемые. C++23, CMake + Visual Studio 2022 (MSVC), Windows 10.

**Правила работы описаны в `CLAUDE.md` — прочитай его перед началом.**
Прочитай `vault/GDM/00-home/текущие приоритеты.md` для текущего статуса.

### Тестовый фреймворк

Catch2 v3 настроен (промпт #00). Утилиты: `tests/test_helpers.h`. Executable: `gdm_tests`.

**Контекст выполнения:** выполняй если тесты #01–#03 выявили расхождения, или как самостоятельную проверку.

```powershell
cmake --build build --config Debug
.\build\Debug\gdm_tests.exe [components]
```

## Цель

Unit-тесты отдельных компонент: относительные проницаемости, fractional flow, CSR-матрица, якобиан.

## Задание

### Создай `tests/test_components.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "test_helpers.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
```

## Тест 4.1: Относительные проницаемости Кори

Модель (`TwoPhaseFlowCell.cpp`, строки 62–66):
```
k_rw = (S̃_w)³,  k_ro = (1−S̃_w)³
dk_rw/dS̃ = 3(S̃_w)²,  dk_ro/dS̃ = −3(1−S̃_w)²
```

Для тестирования нужна ячейка `TwoPhaseFlowCell`. Конструктор:
```cpp
TwoPhaseFlowCell(center, size, constProp, varProp)
```
Порядок `constProp` определяется в `OilField.cpp` — **прочитай и определи точный формат**.

```cpp
TEST_CASE("Corey relperm: boundary values", "[components][relperm]") {
    // Создай ячейку с S̃_w = 0
    // ...
    // REQUIRE_THAT(cell.RelativePermeabilityWater(), WithinAbs(0.0, 1e-15));
    // REQUIRE_THAT(cell.RelativePermeabilityOil(), WithinAbs(1.0, 1e-15));

    // Создай ячейку с S̃_w = 1
    // ...
    // REQUIRE_THAT(cell.RelativePermeabilityWater(), WithinAbs(1.0, 1e-15));
    // REQUIRE_THAT(cell.RelativePermeabilityOil(), WithinAbs(0.0, 1e-15));
}

TEST_CASE("Corey relperm: intermediate values", "[components][relperm]") {
    auto Sw = GENERATE(0.0, 0.25, 0.5, 0.75, 1.0);
    double expected_krw = Sw * Sw * Sw;
    double expected_kro = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);

    // Создай ячейку с данным Sw
    // ...

    CAPTURE(Sw);
    // REQUIRE_THAT(cell.RelativePermeabilityWater(), WithinAbs(expected_krw, 1e-12));
    // REQUIRE_THAT(cell.RelativePermeabilityOil(), WithinAbs(expected_kro, 1e-12));
}

TEST_CASE("Corey relperm: monotonicity", "[components][relperm]") {
    double prev_krw = 0.0, prev_kro = 1.0;
    for (int i = 1; i <= 100; ++i) {
        double Sw = i / 100.0;
        double krw = Sw * Sw * Sw;
        double kro = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);
        CHECK(krw >= prev_krw);
        CHECK(kro <= prev_kro);
        prev_krw = krw;
        prev_kro = kro;
    }
}

TEST_CASE("Corey relperm: derivative vs finite difference",
          "[components][relperm]") {
    auto Sw = GENERATE(0.1, 0.3, 0.5, 0.7, 0.9);
    constexpr double eps = 1e-7;

    double krw_plus = (Sw + eps) * (Sw + eps) * (Sw + eps);
    double krw_minus = (Sw - eps) * (Sw - eps) * (Sw - eps);
    double dkrw_numerical = (krw_plus - krw_minus) / (2 * eps);
    double dkrw_analytical = 3.0 * Sw * Sw;

    CAPTURE(Sw);
    REQUIRE_THAT(dkrw_analytical, WithinRel(dkrw_numerical, 1e-5));
}
```

## Тест 4.2: Fractional flow

```cpp
TEST_CASE("Fractional flow: boundary values", "[components][fractional-flow]") {
    constexpr double M = 4.3 / 2.0;

    // При S̃_w → 0: f_w → 0
    // При S̃_w → 1: f_w → 1
    // Проверить через ячейку или через формулу

    double fw_0 = M * 0.0 / (M * 0.0 + 1.0);
    REQUIRE_THAT(fw_0, WithinAbs(0.0, 1e-15));

    double fw_1 = M * 1.0 / (M * 1.0 + 0.0);
    REQUIRE_THAT(fw_1, WithinAbs(1.0, 1e-15));
}

TEST_CASE("Fractional flow: S-shape and monotonicity",
          "[components][fractional-flow]") {
    constexpr double M = 4.3 / 2.0;

    double prev_fw = 0.0;
    for (int i = 1; i <= 100; ++i) {
        double Sw = i / 100.0;
        double s3 = Sw * Sw * Sw;
        double q3 = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);
        double fw = M * s3 / (M * s3 + q3);
        CHECK(fw >= prev_fw);
        prev_fw = fw;
    }
}

TEST_CASE("Fractional flow: formula matches cell F_Water",
          "[components][fractional-flow]") {
    // Создай ячейку, прочитай F_Water(), сравни с формулой
    auto Sw = GENERATE(0.1, 0.3, 0.5, 0.7, 0.9);
    constexpr double M = 4.3 / 2.0;

    double s3 = Sw * Sw * Sw;
    double q3 = (1.0 - Sw) * (1.0 - Sw) * (1.0 - Sw);
    double expected_fw = M * s3 / (M * s3 + q3);

    // ... создай ячейку с данным Sw
    // REQUIRE_THAT(cell.F_Water(), WithinRel(expected_fw, 1e-10));
}
```

## Тест 4.3: Sparsity pattern

```cpp
TEST_CASE("Sparsity pattern: 3x3 grid structure", "[components][sparsity]") {
    auto horizon = test_helpers::make_uniform_horizon(
        3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // 9 ячеек, блоки 2x2
    // Угловые (0,2,6,8): 3 соседа (включая себя)
    // Рёберные (1,3,5,7): 4 соседа
    // Центральная (4): 5 соседей

    // Прочитай LinearProblem.h и MatrixCSR.h чтобы найти API
    // для получения числа ненулевых блоков на строку

    // Если прямого API нет — используй
    // sim.MyProblem.NmbrOfNonZerosPerRow(l) или аналог.
    // Изучи CSR-формат: row_ptr, col_idx
}
```

## Тест 4.4: Тривиальное решение

```cpp
TEST_CASE("Trivial solve: uniform IC converges in 1 Newton iteration",
          "[components]") {
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = 200.0 * 101325.0;

    sim.Solve({0.0, 10.0});

    // Проверяем что Ньютон сошёлся за 1 итерацию
    // numPrm.WastedTrialsCount() = 0
    CHECK(sim.numPrm.WastedTrialsCount() == 0);
}
```

## Тест 4.5: Якобиан через конечные разности (опционально)

```cpp
TEST_CASE("Jacobian: analytical vs finite difference",
          "[components][jacobian]") {
    // Для сетки 1×1×1 собрать матрицу, затем:
    // 1. Прибавить ε к S̃_w, пересобрать → ΔF/ΔS = (F(S+ε)−F(S−ε))/(2ε)
    // 2. Сравнить с блоком матрицы [0,0] и [1,0]

    // Это продвинутый тест, требует доступа к внутренностям Grid и
    // VariableFieldProperties. Реализуй если другие тесты выявили проблему.

    // SKIP("Not yet implemented — implement if Newton divergence detected");
}
```

## Зарегистрируй в CMakeLists.txt

```cmake
target_sources(gdm_tests PRIVATE tests/test_components.cpp)
```

## Детали: создание TwoPhaseFlowCell

Прочитай `OilField.cpp` — метод, создающий ячейки из `DevelopedHorizon`. Grep по `TwoPhaseFlowCell(` чтобы найти порядок `constProp`:

```
constProp = {k, φ, S_or, S_wc, μ_oil, μ_water, ρ_oil, ρ_water, ...}
varProp = {S_w, P}
```

**Точный формат определяется кодом — прочитай перед реализацией.** Если создание отдельной ячейки слишком громоздко, используй полный `ReservoirSimulator` с сеткой 1×1×1 и обращайся к ячейкам через `sim.Grid[0]`.

## Критерии успеха

1. `gdm_tests.exe [components]` — все тесты PASSED
2. k_r совпадает с ручным расчётом (ε < 10⁻¹²)
3. f_w совпадает с формулой
4. Sparsity pattern корректен
5. Тривиальная задача: 0 wasted trials

## По завершении

1. Если найден баг — `vault/GDM/knowledge/debugging/`
2. Обнови `vault/GDM/00-home/текущие приоритеты.md`
