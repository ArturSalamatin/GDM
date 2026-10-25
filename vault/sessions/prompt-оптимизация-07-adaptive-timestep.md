---
tags:
  - промпт
  - производительность
  - численные-методы
  - адаптивный-шаг
  - pi-controller
date: 2026-06-21
updated: 2026-06-24
---

# Сессия 7: PI-контроллер адаптивного шага по времени

## Предварительно

Прочитай `vault/GDM/00-home/текущие приоритеты.md`.

## Предыстория

Текущее управление шагом — фиксированные множители (NumericalParameters.h:19, .cpp:92–103):
- **Успех:** `increase_schemeTau()` → `tau * (1 + factor)` = +15%
- **Провал:** `decrease_schemeTau()` → `tau * (1 - 2*factor)` = -30%
- `factor = 0.15` — `static constexpr`, зашит в класс

### Проблемы

1. **Нет обратной связи от Newton.** 2 итерации и 11 итераций дают одинаковый +15%.
2. **Нет памяти.** Не учитывает предыдущие шаги. Осциллирует вокруг оптимума.
3. **-30% при провале слишком консервативен** для лёгких расхождений, но **недостаточен** для тяжёлых.
4. **+15% слишком медленный рост** когда Newton сходится за 2–3 итерации.

### Текущая архитектура вызовов

```
ReservoirSimulator::Solve() строки 397-431:
  while (currentMoment < timeMoments[i]):
    update_maxTauAllowed(...)           ← ограничивает шаг событиями скважин
    PerformNewtonLoop(CurrentIntegrationStep(), NextTimeMoment())
    if (IsSuccessfullNewtonTrial()):
      MassBalance(...)
      Grid.AcceptState()
      update_currentMoment()            ← внутри вызывает increase_schemeTau()
    else:
      decrease_schemeTau()              ← откат + wastedTrialsCount++
      continue

NumericalParameters::update_currentMoment() строки 48-63:
  currentMoment += CurrentIntegrationStep()
  if (CurrentIntegrationStep() < CurrentTimeStepTillNextSaveMomemnt()):
    increase_schemeTau()                ← шаг не был ограничен save moment
```

**Ключевой момент:** `increase_schemeTau()` вызывается ВНУТРИ `update_currentMoment()` с условием: только если шаг не был ограничен `timeStepTillNextSaveMomemnt`. Это правильно — если шаг был обрезан save moment'ом, нет смысла увеличивать, базовый `schemeTau` уже больше.

## Теория: PI-контроллер Söderlind

Ссылка: G. Söderlind, "Automatic control and adaptive time-stepping", Numer. Algorithms, 31:281–310, 2002.

Стандартный PI-контроллер:
```
τ_{n+1} = τ_n × safety × (e_target / e_n)^α × (e_{n-1} / e_n)^β
```

Где:
- `e_n = newton_iters / max_iters` — нормированная "ошибка" текущего шага
- `e_target = target_iters / max_iters` — целевая
- `α = 0.7 / p` (p = порядок метода, у нас p=1 для implicit Euler) → `α = 0.7`
- `β = 0.4 / p` → `β = 0.4`
- `safety = 0.85` — перестраховка

### Примеры поведения

| Newton итерации | e_n | Множитель (P only) | Множитель (PI, prev=4) |
|---|---|---|---|
| 2 | 0.17 | 1.36 | 1.36 |
| 3 | 0.25 | 1.12 | 1.12 |
| 4 (target) | 0.33 | 0.85 | 0.85 |
| 6 | 0.50 | 0.63 | зависит от prev |
| 10 | 0.83 | 0.42 | ≤ 0.42 |
| 12 (fail) | 1.00 | 0.35 | ≤ 0.35 |

**Замечание про safety:** с safety=0.85 при target=4 итерациях множитель ≈ 0.85, т.е. шаг слегка уменьшается. Это консервативно. Можно задать target=3 и safety=0.9 для более агрессивного роста.

## Этапы

### Этап 0: Диагностика — сбор данных о текущем поведении

**Цель:** понять распределение Newton-итераций по шагам, частоту wasted trials.

**Файлы:** `NumericalParameters.h`, `NumericalParameters.cpp`

Добавить в NumericalParameters:
```cpp
struct TimestepDiag {
    double tau;
    size_t newton_iters;
    bool success;
};
std::vector<TimestepDiag> timestep_history_;

// В increase_schemeTau (перед изменением schemeTau):
timestep_history_.push_back({CurrentIntegrationStep(), CurrentNewtonIterationCount(), true});

// В decrease_schemeTau (перед изменением schemeTau):
timestep_history_.push_back({CurrentIntegrationStep(), CurrentNewtonIterationCount(), false});

// Геттер:
const auto& TimestepHistory() const { return timestep_history_; }
```

**Файл:** `test_amgcl_benchmark.cpp` — после run_benchmark дампить `sim.numPrm.TimestepHistory()` в CSV.

**Pitfall:** `NumericalParameters numPrm` в ReservoirSimulator — public member. Доступ к `TimestepHistory()` напрямую. Но `run_benchmark` уничтожает `sim` после возврата. Нужно скопировать историю ДО уничтожения:
```cpp
auto history = sim.numPrm.TimestepHistory();
// ... sim уничтожается
write_timestep_csv(name + "_timesteps.csv", history);
```

**Альтернатива:** пропустить этот этап и реализовать PI-контроллер сразу. Диагностика нужна для калибровки `e_target` и графиков в статью.

Рекомендация: **реализовать диагностику.** ~20 строк кода, данные для верификации.

### Этап 1: PIController — отдельный класс

**Файл:** `HydroSolver/Reservoir/PIController.h` (новый)

```cpp
#pragma once
#include <algorithm>
#include <cmath>

namespace reservoir_simulator
{

struct PIControllerParams {
    double alpha = 0.7;       // P-компонент
    double beta = 0.4;        // I-компонент
    size_t target_iters = 4;  // целевое число Newton-итераций
    size_t max_iters = 12;    // максимум Newton-итераций (= fail)
    double safety = 0.85;     // safety factor
    double max_growth = 2.0;  // max увеличение за шаг
    double min_shrink = 0.3;  // min уменьшение за шаг
};

class PIController {
    PIControllerParams params_;
    double prev_error_ = -1.0; // < 0 = нет предыдущего шага

public:
    explicit PIController(PIControllerParams p = {}) : params_(p) {}

    // Возвращает множитель для шага: tau_new = tau_old * ComputeMultiplier(...)
    double ComputeMultiplier(size_t newton_iters, bool success);

    // Сброс истории (начало симуляции)
    void Reset();

    const PIControllerParams& Params() const { return params_; }
};

} // namespace reservoir_simulator
```

**Файл:** `HydroSolver/Reservoir/PIController.cpp` (новый)

```cpp
#include "PIController.h"

namespace reservoir_simulator
{

double PIController::ComputeMultiplier(size_t newton_iters, bool success)
{
    double e_n;
    if (!success)
        e_n = 1.0;
    else
        e_n = static_cast<double>(newton_iters) / params_.max_iters;

    double e_target = static_cast<double>(params_.target_iters) / params_.max_iters;

    double mult;
    if (prev_error_ < 0) {
        // Первый шаг — чистый P-контроль
        mult = std::pow(e_target / e_n, params_.alpha);
    } else {
        mult = std::pow(e_target / e_n, params_.alpha)
             * std::pow(prev_error_ / e_n, params_.beta);
    }

    mult *= params_.safety;
    mult = std::clamp(mult, params_.min_shrink, params_.max_growth);
    prev_error_ = e_n;

    return mult;
}

void PIController::Reset()
{
    prev_error_ = -1.0;
}

} // namespace reservoir_simulator
```

**Pitfalls:**

1. **e_n = 0.** Невозможно — Newton делает минимум 1 итерацию (`update_currentNewtonIterationCount()` вызывается внутри while-цикла до проверки сходимости, NumericalParameters.h:102). Минимум e_n = 1/12 ≈ 0.083.

2. **prev_error_ / e_n.** При `prev_error_ = 1/12` и `e_n = 1/12` → 1.0. При `prev_error_ → 0` (невозможно, но теоретически) → деление на ноль нет, потому что e_n ≥ 1/12.

3. **Порядок вызова.** `ComputeMultiplier` обновляет `prev_error_` внутри. Один вызов на timestep. Не вызывать дважды — второй вызов увидит обновлённый prev_error_.

4. **Reset при провале?** Нет. PI помнит провал → следующий шаг осторожнее. Reset только при старте.

5. **Стандарт для `PIController.cpp` — нет #include "stdafx.h"?** В текущем проекте некоторые .cpp включают `../stdafx.h` (NumericalParameters.cpp:1), а некоторые нет (CRSStructure.cpp). Для нового файла — не включать stdafx.h, если он не в precompiled headers. Проверить при компиляции.

### Этап 2: Интеграция в NumericalParameters

**Файл:** `NumericalParameters.h`

Добавить:
```cpp
#include "PIController.h"
```

В protected секцию:
```cpp
PIController pi_controller_;
bool use_pi_controller_ = false; // false по умолчанию, включать явно
```

В public:
```cpp
void SetPIControllerParams(PIControllerParams p) { pi_controller_ = PIController(p); }
void SetUsePIController(bool f) { use_pi_controller_ = f; }
bool UsesPIController() const { return use_pi_controller_; }
```

**Файл:** `NumericalParameters.cpp`

Изменить `increase_schemeTau()`:
```cpp
void NumericalParameters::increase_schemeTau() {
    if (CurrentAMG_Error() > 0.0) {
        if (use_pi_controller_) {
            double mult = pi_controller_.ComputeMultiplier(
                CurrentNewtonIterationCount(), true);
            schemeTau = CurrentIntegrationStep() * mult;
        } else {
            schemeTau = CurrentIntegrationStep() * (1 + factor);
        }
    }
}
```

Изменить `decrease_schemeTau()`:
```cpp
void NumericalParameters::decrease_schemeTau() {
    if (use_pi_controller_) {
        double mult = pi_controller_.ComputeMultiplier(
            CurrentNewtonIterationCount(), false);
        schemeTau = CurrentIntegrationStep() * mult;
    } else {
        schemeTau = CurrentIntegrationStep() * (1 - 2 * factor);
    }
    update_wastedTrialsCount();
    set_currentNewtonIterationCount(0);
}
```

**Pitfalls:**

1. **Условие `CurrentAMG_Error() > 0.0` в increase.** Проверяет, что solve реально работал. Оставить для PI тоже.

2. **CurrentNewtonIterationCount() при провале.** При Newton divergence (не сошёлся за 12) → count = 12. При AMG fail на итерации 5 → count = 5, но `success = false`. Оба случая дают `e_n ≥ 5/12`. Для PI достаточно — оба являются "плохим" результатом.

3. **update_wastedTrialsCount() и set_currentNewtonIterationCount(0) в decrease** — оставить. PI управляет множителем, счётчики — диагностика.

4. **Вызов из update_currentMoment() — СОХРАНИТЬ условие!** `increase_schemeTau()` вызывается из `update_currentMoment()` строка 57–58 с условием `CurrentIntegrationStep() < CurrentTimeStepTillNextSaveMomemnt()`. Это условие **обязательно и для PI-контроллера**. Причины:

    a) Если шаг был обрезан save moment (schemeTau=10, фактический шаг=2), вызов `increase_schemeTau()` перезапишет `schemeTau = 2 * mult`, потеряв накопленное значение 10. Условие защищает от этого.

    b) PI-контроллер может захотеть уменьшить schemeTau при медленной сходимости. Но если шаг обрезан, Newton-итераций мало (маленький шаг = лёгкая задача), и PI ошибочно запомнит `prev_error = 2/12 = 0.17`. На следующем полном шаге PI увидит резкое «ухудшение» и чрезмерно снизит шаг.

    c) Единственный случай, когда PI должен уменьшить schemeTau при успешном solve — когда шаг был полным (не обрезанным), но Newton еле сошёлся (10–11 итераций). Условие это позволяет — `CurrentIntegrationStep() == schemeTau < timeStepTillNextSaveMomemnt`.

    **Решение:** НЕ менять `update_currentMoment()`. Условие `if (CurrentIntegrationStep() < CurrentTimeStepTillNextSaveMomemnt())` остаётся для всех режимов. Код `update_currentMoment()` не трогать.

    **Единственная проблема:** если шаг ровно равен save moment (`CurrentIntegrationStep() == timeStepTillNextSaveMomemnt`), условие не выполняется и increase не вызывается. В этом случае schemeTau сохраняет предыдущее значение. Это правильно — шаг был ограничен внешним событием, не солвером.

5. **schemeTau после PI может быть > timeStepTillNextSaveMomemnt.** Это ок — `CurrentIntegrationStep()` = `min(schemeTau, timeStepTillNextSaveMomemnt)`. PI обновляет `schemeTau`, а `update_maxTauAllowed` на следующем шаге обновляет `timeStepTillNextSaveMomemnt`. Фактический шаг берёт минимум.

6. **Backward compatibility.** `use_pi_controller_ = true` по умолчанию. Для старых тестов, которые не задают PI params, поведение изменится. **Рекомендация:** на первой итерации `use_pi_controller_ = false` по умолчанию, включать явно в benchmark. Переключить на `true` после верификации на всех 35 тестах.

### Этап 3: Unit tests для PIController

**Файл:** `tests/test_pi_controller.cpp` (новый)

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "PIController.h"

using namespace reservoir_simulator;

TEST_CASE("PIController: fast convergence gives growth > 1", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(2, true);
    CHECK(mult > 1.0);
    CHECK(mult <= 2.0);
}

TEST_CASE("PIController: slow convergence gives shrink < 1", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(10, true);
    CHECK(mult < 1.0);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: at target with safety gives ~safety", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(4, true); // target = 4
    CHECK(mult > 0.7);
    CHECK(mult < 1.1);
}

TEST_CASE("PIController: failure gives strong decrease", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(12, false);
    CHECK(mult < 0.5);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: I-term smooths after fast→slow", "[timestep][pi]") {
    PIController pi;
    pi.ComputeMultiplier(2, true);       // fast
    double m_pi = pi.ComputeMultiplier(10, true);  // slow with PI memory

    PIController pi_p_only({.alpha = 0.7, .beta = 0.0});
    pi_p_only.ComputeMultiplier(2, true);
    double m_p = pi_p_only.ComputeMultiplier(10, true);

    // PI ≤ P когда ухудшение (I помнит что было хорошо)
    CHECK(m_pi <= m_p + 0.01);
}

TEST_CASE("PIController: clamped to max_growth", "[timestep][pi]") {
    PIController pi;
    double mult = pi.ComputeMultiplier(1, true);
    CHECK(mult <= 2.0);
}

TEST_CASE("PIController: clamped to min_shrink", "[timestep][pi]") {
    PIController pi({.min_shrink = 0.3});
    double mult = pi.ComputeMultiplier(12, false);
    CHECK(mult >= 0.3);
}

TEST_CASE("PIController: reset restores virgin state", "[timestep][pi]") {
    PIController pi;
    pi.ComputeMultiplier(10, true);
    pi.Reset();
    double mult = pi.ComputeMultiplier(2, true);

    PIController pi_fresh;
    double mult_fresh = pi_fresh.ComputeMultiplier(2, true);
    CHECK(mult == Catch::Approx(mult_fresh));
}

TEST_CASE("PIController: custom params", "[timestep][pi]") {
    PIControllerParams p{.alpha = 0.5, .beta = 0.3, .target_iters = 3,
                         .max_iters = 10, .safety = 0.9};
    PIController pi(p);
    double mult = pi.ComputeMultiplier(3, true); // at target
    // ~0.9 (safety)
    CHECK(mult > 0.8);
    CHECK(mult < 1.0);
}
```

**CMakeLists.txt:** добавить `test_pi_controller.cpp` в GDM_TEST_SOURCES, `PIController.cpp` в GDM_CORE_SOURCES.

**Pitfall — designated initializers:** `PIControllerParams{.alpha=0.7, .beta=0.0}` — C++20 designated initializers. MSVC 2022 с `/std:c++latest` (наш C++23) поддерживает.

**Pitfall — include path:** `PIController.h` лежит в `HydroSolver/Reservoir/`. CMakeLists.txt: `target_include_directories(gdm_core PUBLIC ${HYDRO})` где `${HYDRO}` = `HydroSolver`. Из тестов (линкуют gdm_core):
```cpp
#include "Reservoir/PIController.h"  // относительно HydroSolver/
```
Из `NumericalParameters.h` (тот же каталог `HydroSolver/Reservoir/`):
```cpp
#include "PIController.h"  // тот же каталог
```

### Этап 4: Benchmark comparison — A/B тест

**Файл:** `test_amgcl_benchmark.cpp`

```cpp
TEST_CASE("AMGCL benchmark: Series TS — timestep control",
          "[benchmark][amgcl][seriesTS][.slow]")
{
    fs::create_directories("results");
    constexpr int B = 2;
    using S = Solver_AMG<B>;  // текущий production solver

    S::params solver_prm;
    solver_prm.precond.relax.k = 1;
    solver_prm.solver.M = 15;
    solver_prm.solver.K = 5;

    // --- Baseline: фиксированные множители ---
    SECTION("TS_BL: fixed factor=0.15") {
        auto r = run_benchmark<S>("TS_BL_fixed", solver_prm,
                                  Layout::InterleavedSwP, false); // usePIController=false
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // --- PI default: alpha=0.7, beta=0.4, target=4 ---
    SECTION("TS_PI1: PI default target=4") {
        auto r = run_benchmark<S>("TS_PI1_default", solver_prm,
                                  Layout::InterleavedSwP, true,
                                  PIControllerParams{});
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // --- PI aggressive: target=3 ---
    SECTION("TS_PI2: PI target=3") {
        PIControllerParams p{.target_iters = 3};
        auto r = run_benchmark<S>("TS_PI2_target3", solver_prm,
                                  Layout::InterleavedSwP, true, p);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // --- PI conservative: target=5, safety=0.9 ---
    SECTION("TS_PI3: PI target=5 safety=0.9") {
        PIControllerParams p{.target_iters = 5, .safety = 0.9};
        auto r = run_benchmark<S>("TS_PI3_target5", solver_prm,
                                  Layout::InterleavedSwP, true, p);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }

    // --- PI no-I: чистый P-контроль (beta=0) ---
    SECTION("TS_PI4: P-only (beta=0)") {
        PIControllerParams p{.beta = 0.0};
        auto r = run_benchmark<S>("TS_PI4_Ponly", solver_prm,
                                  Layout::InterleavedSwP, true, p);
        report(r);
        append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }
}
```

**Pitfall — run_benchmark параметризация:**
Текущий `run_benchmark` не принимает Layout или PI параметры. Нужна перегрузка:
```cpp
template<typename SolverType>
BenchmarkResult run_benchmark(const std::string& name,
                              typename SolverType::params& prm,
                              Layout layout = Layout::InterleavedSwP,
                              bool usePIController = false,
                              PIControllerParams piParams = {})
{
    // ... создание ReservoirSimulator с layout
    if (usePIController) {
        sim.numPrm.SetUsePIController(true);
        sim.numPrm.SetPIControllerParams(piParams);
    } else {
        sim.numPrm.SetUsePIController(false);
    }
    // ... остальной бенчмарк
}
```

**Pitfall — default parameters в шаблоне:** Если `run_benchmark` уже имеет 2 параметра (name, prm), добавление 3 default-параметров не ломает существующие вызовы. Backward compatible.

**Pitfall — run_benchmark_scalar vs run_benchmark:**
Сессия 6b добавляет `run_benchmark_scalar` для CPR. Сессия 7 параметризует обычный `run_benchmark`. Они независимы, но обе добавляют Layout parameter. Если сессии выполняются последовательно — ок. Если параллельно — конфликт. Рекомендация: выполнять 6b до 7.

### Этап 5: Диагностический CSV-вывод

**Файл:** `test_amgcl_benchmark.cpp`

После каждого run_benchmark в Series TS:
```cpp
void write_timestep_csv(const std::string& filename,
                        const std::vector<NumericalParameters::TimestepDiag>& history)
{
    std::ofstream f("results/" + filename);
    f << "step,tau,newton_iters,success\n";
    for (size_t i = 0; i < history.size(); ++i) {
        f << i << "," << history[i].tau << ","
          << history[i].newton_iters << ","
          << (history[i].success ? "true" : "false") << "\n";
    }
}
```

Позволяет построить графики в Python/Excel:
- `τ(step)` — динамика шага (PI vs fixed)
- `Newton_iters(step)` — стабильность сходимости
- Гистограмма Newton-итераций

### Этап 6: Тюнинг параметров

| Параметр | Диапазон | Начало | Влияние |
|---|---|---|---|
| `target_iters` | 3–6 | 4 | Рабочая точка. Ниже → крупнее шаги, больше провалов |
| `safety` | 0.7–0.95 | 0.85 | Перестраховка. Ниже → меньше провалов |
| `alpha` | 0.5–1.0 | 0.7 | P-компонент. Выше → резче реакция |
| `beta` | 0.0–0.6 | 0.4 | I-компонент. Выше → сильнее сглаживание |
| `max_growth` | 1.5–3.0 | 2.0 | Лимит роста |
| `min_shrink` | 0.1–0.4 | 0.3 | Лимит падения |

Порядок:
1. Default → сравнить с baseline
2. Если много wasted trials → снизить safety до 0.8, или target до 3
3. Если осциллирует → увеличить beta до 0.5
4. Если медленно растёт → увеличить max_growth до 2.5 или target до 5
5. Если Newton стабильно 2 → target = 3, убрать перестраховку

### Этап 7: Production — переключение по умолчанию

**Только после:**
1. 35 существующих тестов проходят с `use_pi_controller_ = true`
2. Benchmark: `t_total(PI) ≤ t_total(fixed)` или объяснение почему больше но лучше (меньше total Newton iters)
3. balance_ok = true для всех PI-конфигураций

Изменения:
1. `use_pi_controller_ = true` по умолчанию в NumericalParameters.h
2. `factor = 0.15` — оставить как fallback, не удалять
3. Прогнать все 35 тестов + benchmark
4. Если regression — откатить на false

**Pitfall — тесты с конкретными ожиданиями:**
Некоторые тесты проверяют финальное состояние (Sw, P) с определённой точностью. PI-контроллер может дать другую последовательность шагов → другую численную ошибку. Fully implicit scheme безусловно устойчива, но точность зависит от шага. Если PI даёт крупнее шаги → больше ошибка усечения → может не пройти тест с margin 1e-3.

**Решение:** проверить, какие тесты фиксируют точные значения vs какие проверяют физику (баланс, монотонность). Для PI-контроллера физические ограничения должны выполняться, а точные значения могут сдвинуться.

## Порядок зависимостей

```
Этап 0 (диагностика)          — опционален, ~20 строк
    ↓
Этап 1 (PIController class)   — ядро, 2 новых файла
    ↓
Этап 3 (unit tests)           — верификация ядра
    ↓
Сборка + прогон unit tests
    ↓
Этап 2 (интеграция в NumericalParameters)
    ↓
Прогон 35 тестов с use_pi_controller_=false (backward compat)
    ↓
Прогон 35 тестов с use_pi_controller_=true
    ↓
Этап 4 (benchmark A/B)
    ↓
Этап 5 (CSV диагностика)
    ↓
Этап 6 (тюнинг — итеративный)
    ↓
Этап 7 (production)
```

## Критерии успеха

1. ✅ Unit tests PIController проходят
2. ✅ 35 существующих тестов проходят с PI (use_pi_controller_=true)
3. 📊 PI: n_wasted_trials ≤ фиксированный
4. 📊 PI: t_total или n_newton_iters улучшен
5. 📊 Newton-итерации стабильнее (меньше дисперсия)

## Файлы для создания/изменения

| Файл | Тип | Изменение |
|---|---|---|
| `HydroSolver/Reservoir/PIController.h` | **новый** | PIControllerParams + PIController |
| `HydroSolver/Reservoir/PIController.cpp` | **новый** | ComputeMultiplier, Reset |
| `NumericalParameters.h` | изм | PIController member, use_pi flag, setter |
| `NumericalParameters.cpp` | изм | increase/decrease через PI, диагностика |
| `CMakeLists.txt` | изм | +PIController.cpp |
| `tests/test_pi_controller.cpp` | **новый** | unit tests |
| `test_amgcl_benchmark.cpp` | изм | Series TS, run_benchmark перегрузка |

## Риски

1. **PI-контроллер не учитывает тип провала.** AMG fail на итерации 3 и Newton divergence на итерации 12 — разная тяжесть, но оба дают `success=false`. Расширение: передавать `newton_iters` при fail тоже и использовать `e_n = iters/max_iters` вместо `e_n = 1.0`. Но на первой итерации — `e_n = 1.0` для любого fail достаточно.

2. **Взаимодействие с `update_maxTauAllowed`.** Save moments и well events ограничивают шаг. PI обновляет `schemeTau`, `CurrentIntegrationStep()` берёт min. При переходе через save moment `timeStepTillNextSaveMomemnt` обновляется → шаг может резко измениться. PI помнит prev_error от предыдущего шага → следующий множитель может быть нерелевантен. **Mitigation:** prev_error от обрезанного шага = оптимистичен (мало итераций из-за маленького шага). PI увеличит schemeTau → это нормально, следующий шаг попробует больше.

3. **Первый шаг.** `schemeTau` инициализируется `std::numeric_limits<double>::max()`, потом ограничивается `update_maxTauAllowed`. PI на первом шаге (без prev_error) — чистый P-контроль. Если первый шаг маленький (1e-3 дня) и Newton 2 → PI даст множитель ~1.36 → шаг ~1.36e-3. Это медленный старт. Фиксированный даёт +15% → 1.15e-3. PI чуть лучше, но оба медленные. **Можно рассмотреть:** начальный PI без safety или с max_growth=3 для первых 5 шагов. Но это over-engineering.

## Связанные заметки

- [[план профилирования и оптимизации AMGCL]]
- [[prompt-оптимизация-06b-CPR-benchmark]]
- [[2026-06-23 сессия 6a layout абстракция]]
- [[prompt-оптимизация-04-iluk-reuse-openmp-adaptive]]
- [[prompt-оптимизация-02-структурные-оптимизации]]
