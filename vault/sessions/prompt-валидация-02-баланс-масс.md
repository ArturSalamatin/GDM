---
tags:
  - промпт
  - валидация
  - баланс
  - conservation
  - catch2
date: 2026-06-17
---

# Промпт: Валидация #02 — материальный баланс

Скопируй этот промпт в новую сессию Claude Code целиком. Проект находится в `d:\Lessons\Grants\2027\GDM`.

---

## Контекст проекта

GDM — двухфазный (нефть–вода) гидродинамический симулятор. Обе фазы несжимаемые. C++23, CMake + Visual Studio 2022 (MSVC), Windows 10.

**Правила работы описаны в `CLAUDE.md` — прочитай его перед началом.**
Прочитай `vault/GDM/00-home/текущие приоритеты.md` для понимания текущей фазы.

### Тестовый фреймворк

Catch2 v3 уже настроен (промпт #00). Тестовый executable: `gdm_tests`. Общие утилиты: `tests/test_helpers.h`.

Сборка и запуск:
```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -R balance
.\build\Debug\gdm_tests.exe [balance]
```

## Цель

Проверить материальный баланс по нефти и воде на каждом временном шаге. Это инвариант, который должен выполняться для **любого** сценария с точностью ε_Newton.

## Физика: закон сохранения массы

Для каждой фазы α ∈ {oil, water}:

```
M_α(t+Δt) = M_α(t) − Q_α_prod·Δt + Q_α_inj·Δt − Φ_α_boundary·Δt
```

Где:
- `M_α(t) = Σ_cells φ·S_α·ρ_α·V_cell` — масса фазы в пласте
- `Q_α_prod` — суммарная добыча (кг/день)
- `Q_α_inj` — суммарная закачка (кг/день)
- `Φ_α_boundary` — поток через внешние границы

## Что уже есть в коде

```cpp
double OilTotal() const;         // масса нефти в пласте, кг
double WaterTotal() const;       // масса воды в пласте, кг
double OilDebitTotal() const;    // суммарный дебит нефти по скважинам, кг/день
double WaterDebitTotal() const;  // суммарный дебит воды по скважинам, кг/день
double OilContourFlux() const;   // поток нефти через контур
```

Метод `MassBalance()` вызывается в `Solve()` после принятия шага (~строки 640–673 в ReservoirSimulator.cpp).

## Задание

### Шаг 1. Изучи существующий механизм баланса

Прочитай:
- `ReservoirSimulator::MassBalance()` (~строки 640–673)
- `OilTotal()`, `WaterTotal()` (строки ~200–220)
- `OilDebitTotal()`, `WaterDebitTotal()` (строки ~230–260)
- `OilContourFlux()` (строки ~260–280)

Определи:
1. Что конкретно считается в `OilContourFlux()`?
2. `MassBalance()` считает баланс только для нефти. А для воды?
3. Единицы `OilContourFlux()` — кг/день?

### Шаг 2. Создай тестовый файл

Создай `tests/test_mass_balance.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;
```

### Шаг 3. Тест: замкнутый пласт

```cpp
TEST_CASE("Mass balance: closed reservoir, no wells",
          "[balance][conservation]") {
    // P_init = RefPressure → нулевой поток → масса не меняется
    auto horizon = test_helpers::make_uniform_horizon(
        20, 20, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = 200.0 * 101325.0;  // = P_init → нулевой поток

    double oil_0 = sim.OilTotal();
    double water_0 = sim.WaterTotal();

    INFO("Initial oil mass: " << oil_0 << " kg");
    INFO("Initial water mass: " << water_0 << " kg");

    // Ожидаемые значения:
    // oil: 0.2 * 0.8 * 800 * (1000*1000*10) = 1.28e9 кг
    // water: 0.2 * 0.2 * 1000 * (1000*1000*10) = 4.0e8 кг
    REQUIRE_THAT(oil_0, WithinRel(1.28e9, 1e-3));
    REQUIRE_THAT(water_0, WithinRel(4.0e8, 1e-3));

    // Шаг по времени
    sim.Solve({0.0, 10.0, 20.0, 50.0, 100.0});

    double oil_final = sim.OilTotal();
    double water_final = sim.WaterTotal();

    // Масса не должна измениться
    REQUIRE_THAT(oil_final, WithinRel(oil_0, 1e-10));
    REQUIRE_THAT(water_final, WithinRel(water_0, 1e-10));
}
```

### Шаг 4. Тест: пласт с граничным потоком

```cpp
TEST_CASE("Mass balance: open boundary flow",
          "[balance][conservation]") {
    // P_init > RefPressure → поток наружу → масса убывает
    constexpr double P_init_atm = 250.0;
    constexpr double P_ref_atm = 200.0;

    auto horizon = test_helpers::make_uniform_horizon(
        20, 20, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, P_init_atm, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.RefPressure = P_ref_atm * 101325.0;

    double oil_prev = sim.OilTotal();
    double water_prev = sim.WaterTotal();

    // Пошаговый контроль
    std::vector<double> times = {0.0, 10.0, 20.0, 50.0};
    for (size_t step = 1; step < times.size(); ++step) {
        sim.Solve({times[step-1], times[step]});

        double oil_cur = sim.OilTotal();
        double water_cur = sim.WaterTotal();
        double dt = times[step] - times[step-1];

        INFO("Step " << step << ": t=" << times[step]);

        // Масса убывает (поток наружу)
        CHECK(oil_cur <= oil_prev + 1e-6);  // может быть ≈ равна если поток мал

        // S_w + S_oil = 1
        auto Sw = sim.GetWaterSaturationField();
        auto So = sim.GetOilSaturationField();
        for (size_t i = 0; i < Sw.size(); ++i) {
            REQUIRE_THAT(Sw[i] + So[i], WithinAbs(1.0, 1e-12));
        }

        oil_prev = oil_cur;
        water_prev = water_cur;
    }
}
```

### Шаг 5. Тест: физические ограничения

```cpp
TEST_CASE("Physical constraints hold after simulation",
          "[balance][conservation]") {
    auto horizon = test_helpers::make_uniform_horizon(
        20, 20, 1, 1000.0, 1000.0, 10.0, 100.0, 0.2, 200.0, 0.8);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.numPrm.set_initial_schemeTau(10.0);
    sim.numPrm.set_currentMoment(0.0);

    sim.Solve({0.0, 10.0, 50.0, 100.0});

    auto P = sim.GetPressureField();
    auto Sw = sim.GetWaterSaturationField();
    auto So = sim.GetOilSaturationField();

    for (size_t i = 0; i < P.size(); ++i) {
        INFO("Cell " << i);
        CHECK(P[i] > 0.0);
        CHECK(Sw[i] >= -1e-12);
        CHECK(Sw[i] <= 1.0 + 1e-12);
        CHECK(So[i] >= -1e-12);
        CHECK(So[i] <= 1.0 + 1e-12);
        REQUIRE_THAT(Sw[i] + So[i], WithinAbs(1.0, 1e-12));
    }
}
```

### Шаг 6. Зарегистрируй в CMakeLists.txt

```cmake
target_sources(gdm_tests PRIVATE tests/test_mass_balance.cpp)
```

### Шаг 7. Собери и запусти

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure -R balance
```

## Важно: пошаговый вызов Solve()

`Solve()` НЕ сбрасывает состояние. Он обновляет `currentMoment` в конце. Для повторного вызова нужно передать `{t_prev, t_next}`. Изучи цикл в `Solve()` (строки ~383–431): он начинает с `i = 1`, итерирует `while (currentMoment < timeMoments[i])`.

## Критерии успеха

1. `ctest -R balance` — все тесты PASSED
2. Замкнутый пласт: |ΔM_oil| < 10⁻¹⁰ кг (машинная точность)
3. Пласт с потоком: масса монотонно убывает, S_w + S_oil = 1
4. Все физические ограничения выполняются

## По завершении

1. Создай vault-заметку в `vault/GDM/knowledge/validation/`
2. Если найден баг — заметку в `vault/GDM/knowledge/debugging/`
3. Обнови `vault/GDM/00-home/текущие приоритеты.md`
