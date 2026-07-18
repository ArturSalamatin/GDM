---
tags:
  - план
  - валидация
  - инвариантный
date: 2026-07-18
issue: VAL-039
github: 40
branch: val/val-039/pi-controller-breakthrough
status: готов к реализации
audit:
  date: 2026-07-18
  round: 9
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  note: "Раунд 9 — чистый. Все 7 измерений пройдены, проблем не найдено."
---

# VAL-039: PI-контроллер через breakthrough (1D + five-spot)

## Контекст

PI-контроллер (`PIController`) управляет адаптивным временным шагом в `NumericalParameters`. Формула:

```
mult = safety · (e_target / e_n)^α · (e_{n-1} / e_n)^β
```

где `e_n = newton_iters / max_iters`, `e_target = target_iters / max_iters`. Параметры по умолчанию: α=0.7, β=0.2, target=12, max=65, safety=1.0, max_growth=2.0, min_shrink=0.3.

Контроллер включается через `numPrm.SetUsePIController(true)`. По умолчанию выключен — используется линейная формула `τ_{n+1} = τ_n · (1 ± k·factor)`, factor=0.15.

**Проблема:** юнит-тесты PIController проверяют `ComputeMultiplier` изолированно. Ни один интеграционный тест не включает PI-контроллер. Поведение при breakthrough (резкий рост нелинейности → всплеск Newton-итераций → PI должен резко сократить dt → затем плавно восстановить) — не проверено.

**Цель:** три интеграционных теста, подтверждающих, что PI-контроллер:
1. Проводит симуляцию через breakthrough без сбоев
2. Не ломает симметрию в 2D
3. Не вносит систематическую ошибку по сравнению с fixed-dt

## Связанные заметки

- [[PI-контроллер safety=1 и target=12 для Newton-based timestep control]]
- [[математическая модель двухфазной фильтрации]]
- [[val-001 buckley-leverett-1d]]
- [[стратегия тестирования GDM]]

## Подводные камни

- ✅ **Единицы:** PI-контроллер безразмерный (работает с ratio), единицы не влияют
- ✅ **Граничные условия:** closed boundary, совпадает с существующими BL-тестами
- ⚠️ **dt-история:** `NumericalParameters` не хранит лог dt(t). Нужно добавить `TimestepRecord` — шаг 1
- ⚠️ **Массовый баланс:** точная проверка `|Δ_in_situ − net_injected| / net_injected < ε` требует публичного доступа к кумулятивным дебитам из `MassBalanceTracker`. Сейчас `balance_tracker_` — private в `ReservoirSimulator`. **Блокирующая зависимость:** нужен публичный геттер (шаг 0). Без него — только sanity check на рост массы
- ✅ **Симметрия five-spot:** уже проверена в `test_five_spot.cpp` без PI — образец есть
- ✅ **CSV-экспорт:** паттерн из `test_buckley_leverett.cpp` (строка 508–513)

## Инфраструктурное изменение: dt-история

Без истории dt(t) невозможно:
- визуально верифицировать поведение контроллера
- количественно проверить, что dt уменьшается при breakthrough

Минимальное изменение: `vector<TimestepRecord>` в `NumericalParameters`, запись в `update_currentMoment()`.

---

## Шаг 0: Публичный доступ к MassBalanceTracker

**Цель:** дать тестам доступ к кумулятивным дебитам для точной проверки массового баланса.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSImulator.h`

**Контекст:**
`MassBalanceTracker` хранит кумулятивные величины: `accumDebet_`, `accumOilOutFlux_` и т.д. Метод `GetBalance()` возвращает их. Но `balance_tracker_` — private member `ReservoirSimulator` (строка 71). Без публичного доступа тесты могут проверять только in-situ массу (`OilTotal()`, `WaterTotal()`), а не точную невязку баланса.

**Что сделать:**

В `ReservoirSImulator.h`, в public-секции (рядом с `OilTotal()`, `WaterTotal()`, строка ~156), добавить:

```cpp
const MassBalanceTracker& BalanceTracker() const { return balance_tracker_; }
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: все 313 зелёные (геттер пассивный)

**Оценка:** 1 строка, ~2 минуты

---

## Шаг 1: Добавить dt-историю в NumericalParameters

**Цель:** хранить лог (t, dt, newton_iters, success) для диагностики и тестов.

**Файлы:**
- `HydroSolver/Reservoir/NumericalParameters.h`
- `HydroSolver/Reservoir/NumericalParameters.cpp`

**Контекст:**
`update_currentMoment()` вызывается в `TimeIntegrator::Integrate()` после каждого успешного шага (TimeIntegrator.cpp:26). `decrease_schemeTau()` вызывается при неуспешном (TimeIntegrator.cpp:32). Это два места, где нужно записывать историю.

**Что сделать:**

1. В `NumericalParameters.h`:

Struct `TimestepRecord` нельзя поместить внутрь класса NumericalParameters так, чтобы и `timestep_log_` (protected), и тестовый код (external) имели к нему доступ — protected-секция идёт первой (строки 18–43), а public — после (строки 45+). Поэтому struct выносится **перед классом**, в namespace `reservoir_simulator`.

```cpp
// В NumericalParameters.h, после #include "PIController.h" (строка 7),
// внутри namespace reservoir_simulator (строка 9), перед class NumericalParameters (строка 17):
struct TimestepRecord {
    double time;
    double dt;
    size_t newton_iters;
    bool accepted;
};

// В protected-секции NumericalParameters (после строки 44 `bool use_pi_controller_`):
std::vector<TimestepRecord> timestep_log_;

// В public-секции NumericalParameters (рядом с SetUsePIController, строка ~114):
const std::vector<TimestepRecord>& TimestepLog() const { return timestep_log_; }
void ClearTimestepLog() { timestep_log_.clear(); }
```

NB: `<vector>` уже включён транзитивно через `defines.h`, отдельный `#include` не нужен.

3. В `NumericalParameters.cpp`, `update_currentMoment()` (строка 53–68): перед `currentMoment += CurrentIntegrationStep()` (строка 55) добавить запись:

```cpp
timestep_log_.push_back({currentMoment, CurrentIntegrationStep(),
    currentNewtonIterationCount, true});
```

4. В `decrease_schemeTau()` (строка 108–121): записать лог **в самом начале функции**, до того как `schemeTau` перезаписывается (строки 112/114 меняют `schemeTau`, после чего `CurrentIntegrationStep()` вернёт уже уменьшенный dt):

```cpp
void NumericalParameters::decrease_schemeTau() {
    // запись ДО изменения schemeTau — иначе CurrentIntegrationStep() вернёт новый dt
    timestep_log_.push_back({currentMoment, CurrentIntegrationStep(),
        currentNewtonIterationCount, false});
    if (use_pi_controller_) {
        ...
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 313 тестов зелёные (dt-лог пассивный, не меняет поведение)

**Оценка:** ~15 строк, ~10 минут

---

## Шаг 2: Юнит-тест dt-истории

**Цель:** убедиться, что `TimestepLog()` корректно записывает шаги.

**Файлы:**
- `tests/unit/reservoir/test_NumericalParameters.cpp` (существующий)

**Что сделать:**

Добавить тест в конец файла:

```cpp
TEST_CASE("NumericalParameters: TimestepLog records accepted steps",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentMoment(0.0);
    np.set_currentAMG_Error(0.5);
    np.set_currentNewtonIterationCount(5);

    // Имитируем успешный шаг
    // update_currentMoment() записывает и сдвигает время
    // Но нужен update_maxTauAllowed — он требует wells.
    // Используем прямой вызов: timestep_log_ через update_currentMoment
    // NB: update_currentMoment() — public, вызовем напрямую
    np.update_currentMoment();

    auto& log = np.TimestepLog();
    REQUIRE(log.size() == 1);
    CHECK(log[0].time == 0.0);
    CHECK(log[0].dt == 10.0);
    CHECK(log[0].newton_iters == 5);
    CHECK(log[0].accepted == true);
}

TEST_CASE("NumericalParameters: TimestepLog records wasted trials",
          "[unit][level2][reservoir][NumericalParameters]") {
    NumericalParameters np;
    np.set_initial_schemeTau(10.0);
    np.set_currentMoment(0.0);
    np.set_currentAMG_Error(0.5);
    np.set_currentNewtonIterationCount(100);

    np.decrease_schemeTau();

    auto& log = np.TimestepLog();
    REQUIRE(log.size() == 1);
    CHECK(log[0].accepted == false);
}
```

**Проверка после этого шага:**
- Сборка + тесты: baseline + 2 теста (gdm_unit_level2)

**Оценка:** ~25 строк, ~10 минут

---

## Шаг 3: Тест «BL 1D с PI через breakthrough»

**Цель:** PI-контроллер ведёт 1D-симуляцию через момент прорыва воды к продюсеру. Проверяем корректность, массовый баланс и наличие dt-просадки.

**Файлы:**
- `tests/test_pi_controller_integration.cpp` (новый)
- `CMakeLists.txt` — добавить файл в `gdm_tests`

**Контекст:**
Сценарий: 1D пласт, Nx=50, L=500 м, hy=1 м, hz=1 м (тонкий срез, как в BL-тестах). Инжектор слева (вода), продюсер справа (нефть). Начальная водонасыщенность Sw_init=0.2. PV = 500·1·1·0.2 = 100 м³, Q_inj_vol = 1 м³/день → breakthrough ≈ 100 дней. Прогон до t=600 дней (~6 PVI). PI-контроллер включён.

Breakthrough: момент, когда фронт воды достигает продюсера. До breakthrough Newton сходится быстро (dt растёт). В момент breakthrough нелинейность скачком возрастает → Newton нужно больше итераций → PI должен уменьшить dt → затем восстановить.

**Что сделать:**

1. Создать `tests/test_pi_controller_integration.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <cmath>
#include <fstream>
#include <filesystem>

using Catch::Matchers::WithinAbs;

namespace {

constexpr size_t Nx_1d = 50;
constexpr double Lx_1d = 500.0, hy_1d = 1.0, hz_1d = 1.0;
constexpr double perm_mD = 100.0, poro = 0.2;
constexpr double P_init_atm = 200.0;
constexpr double So_init = 0.8;
constexpr double Sw_init = 1.0 - So_init;
constexpr double rho_water = 1000.0, rho_oil = 800.0;
constexpr double Q_inj = -1000.0;   // кг/день закачки (отрицательный)
constexpr double Q_prod = 800.0;    // кг/день добычи нефти
constexpr double T_breakthrough = 600.0; // дней — гарантированно после прорыва

} // namespace

TEST_CASE("PI controller: BL 1D through breakthrough",
          "[pi-controller][integration][validation]") {
    auto horizon = test_helpers::make_uniform_horizon(
        Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.numPrm.SetUsePIController(true);

    double hx = Lx_1d / Nx_1d;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy_1d * 0.5, 0.0, Q_inj);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx_1d - hx * 0.5, hy_1d * 0.5, Q_prod, 0.0);

    sim.Solve({0.0, T_breakthrough});

    auto Sw = sim.GetWaterSaturationField();

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-6);
            CHECK(Sw[i] <= 1.0 + 1e-6);
        }
    }

    SECTION("Breakthrough occurred — Sw at producer > Sw_init") {
        CHECK(Sw[Nx_1d - 1] > Sw_init + 0.01);
    }

    SECTION("Mass balance — per-phase") {
        // Покомпонентная проверка через MassBalanceTracker (шаг 0 даёт доступ).
        // GetBalance() возвращает 7 элементов:
        //   [0] time
        //   [1] accumOil     = Σ(curOil − prevOil)     = ΔM_oil (изменение in-situ)
        //   [2] accumOilFlux = Σ oilContourFlux·dt      = поток через границу
        //   [3] accumDebet   = −Σ oilDebitTotal·dt       = кумул. дебит скважин
        //   [4] accumWater   = ΔM_water
        //   [5] accumWaterFlux
        //   [6] accumWaterDebet = −Σ waterDebitTotal·dt
        //
        // Тождество (см. [[знаковая конвенция баланса масс accumDebet минус интеграл дебита]]):
        //   ΔM + outflux − debet_accum = 0
        //
        // Знаковая конвенция:
        //   debet_accum < 0 при добыче (Debit > 0 → accumDebet = −∫Debit·dt < 0)
        //   debet_accum > 0 при закачке (Debit < 0 → accumDebet = −∫Debit·dt > 0)
        //   outflux > 0 = утекло из пласта через открытую границу
        //   Closed boundary → outflux = 0.

        auto bal = sim.BalanceTracker().GetBalance();

        double accumOil       = bal[1];
        double accumOilFlux   = bal[2];
        double accumOilDebet  = bal[3];
        double accumWater      = bal[4];
        double accumWaterFlux  = bal[5];
        double accumWaterDebet = bal[6];

        // --- Нефть ---
        double oil_residual = accumOil + accumOilFlux - accumOilDebet;
        double oil_scale = std::max(std::abs(accumOilDebet), std::abs(accumOil));
        INFO("OIL: ΔM=" << accumOil << " flux=" << accumOilFlux
             << " debet=" << accumOilDebet << " residual=" << oil_residual);
        // Знаковая проверка: продюсер добывает нефть → accumOilDebet < 0
        CHECK(accumOilDebet < 0.0);
        // Закрытая граница → flux ≈ 0
        CHECK(std::abs(accumOilFlux) < 1e-10);
        // Невязка
        if (oil_scale > 0.0)
            CHECK(std::abs(oil_residual) / oil_scale < 1e-6);

        // --- Вода ---
        double water_residual = accumWater + accumWaterFlux - accumWaterDebet;
        double water_scale = std::max(std::abs(accumWaterDebet), std::abs(accumWater));
        INFO("WATER: ΔM=" << accumWater << " flux=" << accumWaterFlux
             << " debet=" << accumWaterDebet << " residual=" << water_residual);
        // Знаковая проверка: инжектор закачивает воду → accumWaterDebet > 0
        CHECK(accumWaterDebet > 0.0);
        CHECK(std::abs(accumWaterFlux) < 1e-10);
        if (water_scale > 0.0)
            CHECK(std::abs(water_residual) / water_scale < 1e-6);
    }

    SECTION("Wasted trials bounded") {
        CHECK(sim.numPrm.WastedTrialsCount() < 100);
    }

    SECTION("dt history shows dip near breakthrough") {
        auto& log = sim.numPrm.TimestepLog();
        REQUIRE(log.size() > 10);

        // Найти минимальный dt после t > 50 (пропустить начальный разгон)
        double dt_min = 1e30;
        double t_at_min = 0.0;
        double dt_max_early = 0.0;
        for (auto& rec : log) {
            if (!rec.accepted) continue;
            if (rec.time < 50.0) continue;
            if (rec.time < T_breakthrough * 0.3 && rec.dt > dt_max_early)
                dt_max_early = rec.dt;
            if (rec.dt < dt_min) {
                dt_min = rec.dt;
                t_at_min = rec.time;
            }
        }
        // PI должен сократить dt хотя бы до 50% от раннего максимума
        INFO("dt_min=" << dt_min << " at t=" << t_at_min
             << ", dt_max_early=" << dt_max_early);
        CHECK(dt_min < dt_max_early * 0.5);
    }
}
```

2. В `CMakeLists.txt`: добавить `tests/test_pi_controller_integration.cpp` в список исходников `gdm_tests`.

**Проверка после этого шага:**
- Сборка + тесты: ~315 тестов зелёные (5 новых SECTION в gdm_tests; точное число зависит от baseline)
- Если тест dt-dip не проходит — PI-контроллер не реагирует на breakthrough. Это либо параметры (target_iters слишком высокий), либо Newton сходится легко даже при фронте. В этом случае: (а) уменьшить target_iters до 6, (б) увеличить Nx до 100 для более резкого фронта. Зафиксировать в заметке.

**Оценка:** ~80 строк, ~30 минут

---

## Шаг 4: Тест «Five-spot 2D с PI — симметрия»

**Цель:** PI-контроллер не нарушает четвертную симметрию five-spot.

**Файлы:**
- `tests/test_pi_controller_integration.cpp` (добавить тест)

**Контекст:**
Five-spot: 21×21, инжектор в центре, 4 продюсера в углах. С fixed-dt симметрия идеальна (тест `test_five_spot.cpp` проверяет Δ < 1e-6). PI-контроллер не зависит от пространства (один dt на всю сетку), поэтому симметрия должна сохраняться. Но если PI-controller содержит баг с state (например, I-term дрейфует), симметрия может нарушиться.

**Что сделать:**

Добавить тест:

```cpp
TEST_CASE("PI controller: five-spot symmetry preserved",
          "[pi-controller][integration][five-spot]") {
    constexpr size_t Nx = 21, Ny = 21, Nz = 1;
    constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;
    constexpr double rho_w = 1000.0, rho_o = 800.0;
    constexpr double Q_inj_vol = 50.0;
    constexpr double Q_prod_vol = 12.5;
    constexpr double T_end = 500.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, perm_mD, poro, P_init_atm, 0.999);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(5.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.numPrm.SetUsePIController(true);

    double hx = Lx / Nx, hy = Ly / Ny;

    test_helpers::add_simple_well(sim, horizon,
        "INJ", 10.5 * hx, 10.5 * hy, 0.0, -Q_inj_vol * rho_w);
    test_helpers::add_simple_well(sim, horizon,
        "P1", 0.5 * hx, 0.5 * hy, Q_prod_vol * rho_o, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P2", 0.5 * hx, (Ny - 0.5) * hy, Q_prod_vol * rho_o, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P3", (Nx - 0.5) * hx, 0.5 * hy, Q_prod_vol * rho_o, 0.0);
    test_helpers::add_simple_well(sim, horizon,
        "P4", (Nx - 0.5) * hx, (Ny - 0.5) * hy, Q_prod_vol * rho_o, 0.0);

    std::vector<double> timeMoments = {0.0};
    for (double t = 50.0; t <= T_end; t += 50.0)
        timeMoments.push_back(t);

    sim.Solve(timeMoments);

    auto Sw = sim.GetWaterSaturationField();

    SECTION("Quarter symmetry") {
        for (size_t i = 0; i < Nx / 2; ++i) {
            for (size_t j = 0; j < Ny / 2; ++j) {
                double s00 = Sw[j * Nx + i];
                double s10 = Sw[j * Nx + (Nx - 1 - i)];
                double s01 = Sw[(Ny - 1 - j) * Nx + i];
                double s11 = Sw[(Ny - 1 - j) * Nx + (Nx - 1 - i)];
                REQUIRE_THAT(s00, WithinAbs(s10, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s01, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s11, 1e-6));
            }
        }
    }

    SECTION("Wasted trials bounded") {
        CHECK(sim.numPrm.WastedTrialsCount() < 50);
    }

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-10);
            CHECK(Sw[i] <= 1.0 + 1e-10);
        }
    }
}
```

**Проверка после этого шага:**
- Сборка + тесты: baseline + 2 (шаг 2) + 5 (шаг 3) + 3 (шаг 4) тестов
- Five-spot с PI — медленный тест (21×21, 500 дней). Если > 30 сек, уменьшить T_end до 300 или Nx/Ny до 15.

**Оценка:** ~55 строк, ~20 минут

---

## Шаг 5: Тест «PI vs fixed-dt — сравнение профилей»

**Цель:** PI-контроллер не вносит систематическую ошибку в профиль Sw по сравнению с fixed-dt.

**Файлы:**
- `tests/test_pi_controller_integration.cpp` (добавить тест)

**Контекст:**
Один и тот же 1D-сценарий прогоняется дважды: с PI и без. Финальные профили Sw сравниваются поточечно. Ожидание: |Sw_PI[i] − Sw_fixed[i]| < ε для всех i. ε зависит от разницы в dt-расписании — при PI dt динамически меняется, что может дать численную диффузию, немного отличную от fixed-dt. Tolerance ~0.02 (2% по насыщенности).

**Что сделать:**

```cpp
TEST_CASE("PI controller: PI vs fixed-dt profile comparison",
          "[pi-controller][integration][validation][.]") {
    // Этот тест помечен [.] — запускается только явно,
    // т.к. прогоняет симуляцию дважды

    constexpr double T = 400.0;
    constexpr double dt_fixed = 1.0;

    auto run_sim = [](bool use_pi) {
        auto horizon = test_helpers::make_uniform_horizon(
            Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
        auto numPrm = test_helpers::default_num_params();

        reservoir_simulator::ReservoirSimulator sim{
            numPrm, horizon, horizon.oil, horizon.water, horizon.other};
        sim.RefPressure = P_init_atm * 101325.0;
        sim.numPrm.set_initial_schemeTau(dt_fixed);
        sim.numPrm.set_currentMoment(0.0);
        if (use_pi)
            sim.numPrm.SetUsePIController(true);

        double hx = Lx_1d / Nx_1d;
        test_helpers::add_simple_well(sim, horizon,
            "INJ", hx * 0.5, hy_1d * 0.5, 0.0, Q_inj);
        test_helpers::add_simple_well(sim, horizon,
            "PROD", Lx_1d - hx * 0.5, hy_1d * 0.5, Q_prod, 0.0);

        sim.Solve({0.0, T});
        return sim.GetWaterSaturationField();
    };

    auto Sw_pi = run_sim(true);
    auto Sw_fixed = run_sim(false);

    REQUIRE(Sw_pi.size() == Sw_fixed.size());

    SECTION("Pointwise difference bounded") {
        double max_diff = 0.0;
        for (size_t i = 0; i < Sw_pi.size(); ++i) {
            double diff = std::abs(Sw_pi[i] - Sw_fixed[i]);
            if (diff > max_diff) max_diff = diff;
        }
        INFO("max |Sw_PI - Sw_fixed| = " << max_diff);
        CHECK(max_diff < 0.02);
    }

    SECTION("L2 norm of difference") {
        double hx = Lx_1d / Nx_1d;
        double sum_sq = 0.0;
        for (size_t i = 0; i < Sw_pi.size(); ++i) {
            double d = Sw_pi[i] - Sw_fixed[i];
            sum_sq += d * d;
        }
        double L2 = std::sqrt(sum_sq * hx / Lx_1d);
        INFO("L2(Sw_PI - Sw_fixed) = " << L2);
        CHECK(L2 < 0.01);
    }
}
```

**Проверка после этого шага:**
- Запустить явно: `ctest --test-dir build -C Release -R "PI vs fixed"`
- Если max_diff > 0.02 — PI-контроллер систематически ошибается. Анализировать dt-историю обоих прогонов.

**Оценка:** ~55 строк, ~15 минут

---

## Шаг 6: CSV-экспорт dt(t) и Sw(x)

**Цель:** визуальная верификация — графики dt(t) и Sw(x) для PI и fixed-dt.

**Файлы:**
- `tests/test_pi_controller_integration.cpp` (добавить export-тест)

**Контекст:**
Тест помечен `[.export]` — запускается только вручную. Экспортирует:
1. `results/validation/pi_dt_history.csv` — t, dt, newton_iters, accepted (1D BL с PI)
2. `results/validation/pi_vs_fixed_sw_profile.csv` — x, Sw_PI, Sw_fixed (сравнение профилей)

Эти CSV импортируются в Python/matplotlib для построения overlay-графиков.

**Что сделать:**

```cpp
TEST_CASE("PI controller: CSV export for visual verification",
          "[pi-controller][validation][.export]") {
    // --- 1D BL с PI ---
    auto horizon = test_helpers::make_uniform_horizon(
        Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = P_init_atm * 101325.0;
    sim.numPrm.set_initial_schemeTau(1.0);
    sim.numPrm.set_currentMoment(0.0);
    sim.numPrm.SetUsePIController(true);

    double hx = Lx_1d / Nx_1d;
    test_helpers::add_simple_well(sim, horizon,
        "INJ", hx * 0.5, hy_1d * 0.5, 0.0, Q_inj);
    test_helpers::add_simple_well(sim, horizon,
        "PROD", Lx_1d - hx * 0.5, hy_1d * 0.5, Q_prod, 0.0);

    sim.Solve({0.0, T_breakthrough});

    std::filesystem::create_directories("results/validation");

    // dt-history
    {
        std::ofstream csv("results/validation/pi_dt_history.csv");
        csv << "t,dt,newton_iters,accepted\n";
        for (auto& rec : sim.numPrm.TimestepLog())
            csv << rec.time << "," << rec.dt << ","
                << rec.newton_iters << "," << rec.accepted << "\n";
    }

    // Sw profile: PI vs fixed
    auto Sw_pi = sim.GetWaterSaturationField();
    {
        // Прогон без PI
        auto h2 = test_helpers::make_uniform_horizon(
            Nx_1d, 1, 1, Lx_1d, hy_1d, hz_1d, perm_mD, poro, P_init_atm, So_init);
        auto np2 = test_helpers::default_num_params();
        reservoir_simulator::ReservoirSimulator sim2{
            np2, h2, h2.oil, h2.water, h2.other};
        sim2.RefPressure = P_init_atm * 101325.0;
        sim2.numPrm.set_initial_schemeTau(1.0);
        sim2.numPrm.set_currentMoment(0.0);

        test_helpers::add_simple_well(sim2, h2,
            "INJ", hx * 0.5, hy_1d * 0.5, 0.0, Q_inj);
        test_helpers::add_simple_well(sim2, h2,
            "PROD", Lx_1d - hx * 0.5, hy_1d * 0.5, Q_prod, 0.0);

        sim2.Solve({0.0, T_breakthrough});
        auto Sw_fixed = sim2.GetWaterSaturationField();

        std::ofstream csv("results/validation/pi_vs_fixed_sw_profile.csv");
        csv << "x,Sw_PI,Sw_fixed\n";
        for (size_t i = 0; i < Nx_1d; ++i)
            csv << (i + 0.5) * hx << "," << Sw_pi[i] << "," << Sw_fixed[i] << "\n";
    }

    INFO("CSV exported to results/validation/pi_*.csv");
    CHECK(true);
}
```

**Проверка после этого шага:**
- `ctest --test-dir build -C Release -R "CSV export" -N` — тест виден
- Запустить: `ctest --test-dir build -C Release -R "PI controller: CSV"`
- Проверить наличие CSV: `ls results/validation/pi_*.csv`

**Оценка:** ~55 строк, ~15 минут

---

## Шаг 7: CMakeLists.txt — регистрация нового файла

**Цель:** добавить `test_pi_controller_integration.cpp` в сборку.

**Файлы:**
- `CMakeLists.txt`

**Что сделать:**

Найти блок `add_executable(gdm_tests ...)` и добавить строку:

```
tests/test_pi_controller_integration.cpp
```

рядом с другими integration-тестами (`test_five_spot.cpp`, `test_buckley_leverett.cpp`).

**Проверка после этого шага:**
- Конфигурация: `cmake -B build -S . -G "Visual Studio 17 2022"`
- Сборка + тесты

**Примечание:** этот шаг выполняется одновременно с шагом 3 (при создании файла).

**Оценка:** 1 строка, ~2 минуты

---

## Шаг 8: Обновление vault

**Цель:** обновить статус VAL-039 и документировать результаты.

**Файлы:**
- `vault/GDM/roadmap/валидационные кейсы.md` — статус ⬜ → 🟢
- `vault/GDM/00-home/текущие приоритеты.md` — отметить выполнение
- `vault/GDM/00-home/index.md` — добавить ссылку на план

**Что сделать:**

1. В `валидационные кейсы.md` обновить VAL-039:
   - Статус: `⬜ НЕ НАЧАТО` → `🟢 ПОКРЫТ ТЕСТАМИ`
   - (ссылка на план и запись в index.md **уже добавлены** при создании плана — не дублировать)

2. В `текущие приоритеты.md` строка 99: зачеркнуть `PI-контроллер: интеграционные тесты с breakthrough`

**Оценка:** ~5 минут

---

## Критерии завершения

- [ ] Публичный геттер BalanceTracker() доступен (шаг 0)
- [ ] dt-история записывается в NumericalParameters (шаг 1)
- [ ] Юнит-тест dt-истории зелёный (шаг 2)
- [ ] BL 1D с PI: Sw ∈ [0,1], breakthrough произошёл, невязка баланса масс < 1e-6, dt-dip виден (шаг 3)
- [ ] Five-spot с PI: симметрия < 1e-6 (шаг 4)
- [ ] PI vs fixed-dt: max |ΔSw| < 0.02, L2 < 0.01 (шаг 5)
- [ ] CSV-экспорт работает, dt(t) и Sw(x) построены (шаг 6)
- [ ] Все существующие тесты зелёные
- [ ] Vault обновлён (шаг 8)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай: `vault/GDM/knowledge/decisions/PI-контроллер safety=1 и target=12 для Newton-based timestep control.md`
3. Создай ветку: `git checkout -b val/val-039/pi-controller-breakthrough`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони baseline: `ctest --test-dir build -C Release --output-on-failure`
6. Запомни: 313 тестов, ~105 сек
7. Начни с шага 0 (геттер баланса), затем шаг 1 (dt-история), затем шаг 2 (юнит-тест), затем шаги 3+7 одновременно (тестовый файл + CMake), затем шаги 4–6, затем шаг 8 (vault)
