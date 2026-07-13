---
tags:
  - план
  - рефакторинг
date: 2026-07-13
issue: DEBT-056
github: 25
branch: refactor/debt-056/mass-balance-tracker
status: готов к реализации
estimates:
  steps: 4
  files-changed: 4
  files-created: 3
  lines-new: ~150
  lines-removed: ~30
  manual-required: 0
audit:
  date: 2026-07-13
  findings: 0 / 2 / 1
  auto-fixed: 3
  manual-required: 0
  note: повторный аудит — удалён черновой блок теста, исправлен блок До GetOverallBalance, исправлен порядок файлов CMake
---

# DEBT-056: Выделить MassBalanceTracker из ReservoirSimulator

## Мотивация

Этап 3 из 4 декомпозиции DEBT-003. После DEBT-054 (JacobianAssembler) и DEBT-055 (NewtonSolver) в ReservoirSimulator остаётся логика учёта баланса масс: 11 аккумуляторных полей + 2 метода (`MassBalance`, `GetOverallBalance`). Выделение в MassBalanceTracker позволит:
- Тестировать баланс изолированно (без сетки, скважин, Newton)
- Упростить ReservoirSimulator (~15 строк полей + ~25 строк методов + инициализация)
- Подготовить DEBT-057 (TimeIntegrator), который будет оркестрировать `newton_solver_` и `balance_tracker_`

Связанные заметки: [[debt-003 refactoring-reservoir-simulator]], [[debt-054 jacobian-assembler]], [[debt-055 newton-solver]], [[знаковая конвенция баланса масс accumDebet минус интеграл дебита]]

## Текущее состояние

### Аккумуляторные поля (`ReservoirSImulator.h:66-68`)
```cpp
double prevOil, curOil, accumOil, accumOilOutFlux, accumDebet;
double prevWater, curWater, accumWater, accumWaterOutFlux, accumWaterDebet;
double curTime;
```

### Инициализация (конструктор, `ReservoirSimulator.cpp:66-74`)
```cpp
curOil = OilTotal();
accumDebet = 0.0;
accumOilOutFlux = 0.0;
accumOil = 0.0;
curWater = WaterTotal();
accumWaterDebet = 0.0;
accumWaterOutFlux = 0.0;
accumWater = 0.0;
curTime = 0.0;
```
Инициализация зависит от `OilTotal()` и `WaterTotal()` — это методы ReservoirSimulator, суммирующие объёмы по сетке. Значения передаются при создании трекера.

### Метод MassBalance (`ReservoirSimulator.cpp:474-491`)
```cpp
void ReservoirSimulator::MassBalance(double loc_tau) {
    prevOil = curOil; curOil = OilTotal(); accumOil += curOil - prevOil;
    accumOilOutFlux += OilContourFlux() * loc_tau;
    accumDebet -= OilDebitTotal() * loc_tau;
    prevWater = curWater; curWater = WaterTotal(); accumWater += curWater - prevWater;
    accumWaterOutFlux += WaterContourFlux() * loc_tau;
    accumWaterDebet -= WaterDebitTotal() * loc_tau;
    curTime += loc_tau;
}
```
Вызывает 6 методов ReservoirSimulator: `OilTotal()`, `WaterTotal()`, `OilContourFlux()`, `WaterContourFlux()`, `OilDebitTotal()`, `WaterDebitTotal()`. Все остаются в ReservoirSimulator.

### Метод GetOverallBalance (`ReservoirSimulator.cpp:215-219`)
```cpp
std::vector<double> ReservoirSimulator::GetOverallBalance() const {
    return std::vector<double>{curTime, accumOil, accumOilOutFlux, accumDebet,
        accumWater, accumWaterOutFlux, accumWaterDebet};
}
```

### Call sites
| Место | Что вызывает | Контекст |
|---|---|---|
| `Solve()` (`.cpp:418`) | `MassBalance(numPrm.CurrentIntegrationStep())` | после успешного Newton trial |
| `PrintReservoirState()` (`.cpp:231-232`) | `GetOverallBalance()` | вывод в файл |
| `CalculationManager.cpp:63,72` | `PrintReservoirState()` | косвенно через GetOverallBalance |
| `test_amgcl_benchmark.cpp:248,390` | `sim.MassBalance(loc_tau)` | ручной time loop |
| `test_amgcl_benchmark.cpp:274,416` | `sim.GetOverallBalance()` | проверка баланса |
| `test_variable_debit.cpp:131` | `sim.GetOverallBalance()` | проверка баланса |
| `test_3d_completions.cpp:144` | `sim.GetOverallBalance()` | проверка баланса |
| `test_visual_verification.cpp:128` | `sim.GetOverallBalance()` | проверка баланса |
| `examples/example_runner.h:158,237` | `sim.GetOverallBalance()` | CSV-экспорт баланса |
| `examples/ex_benchmark_series_ts.cpp:188,215` | `sim.MassBalance`, `sim.GetOverallBalance` | ручной time loop |
| `examples/ex_benchmark_series_cpr.cpp:237,264,379,406` | `sim.MassBalance`, `sim.GetOverallBalance` | ручной time loop (2 секции) |

Все call sites используют публичный API (`sim.MassBalance()`, `sim.GetOverallBalance()`), который сохраняется как делегирующие обёртки — изменения call sites не нужны.

## Целевое состояние

```
ReservoirSimulator
  ├── newton_solver_: NewtonSolver
  ├── balance_tracker_: MassBalanceTracker   // НОВЫЙ, владеет 11 полями + curTime
  ├── Solve(timeMoments)                     // вызывает balance_tracker_.Update(...)
  ├── MassBalance(loc_tau)                   // делегирует balance_tracker_ (backward compat)
  ├── GetOverallBalance()                    // делегирует balance_tracker_
  └── OilTotal, WaterTotal, ...              // остаются (используют Grid, Wells)
```

Тесты и внешний код продолжают вызывать `sim.MassBalance(loc_tau)` и `sim.GetOverallBalance()` через делегирующие обёртки — API не меняется.

## Варианты решения

### Вариант A: Update() принимает вычисленные значения (рекомендуемый)

```cpp
void Update(double loc_tau,
            double oilTotal, double waterTotal,
            double oilContourFlux, double waterContourFlux,
            double oilDebitTotal, double waterDebitTotal);
```

ReservoirSimulator вызывает свои 6 методов и передаёт результат как `double`.

**Плюсы:** MassBalanceTracker — чистый аккумулятор без зависимостей. Тестируемый изолированно с подставными числами. Повторяет паттерн DEBT-054/055 (данные передаются, а не захватываются).

**Минусы:** 7 параметров в Update(). Все `double`, семантически ясно.

### Вариант B: Update() принимает коллбэки

**Минусы:** lifetime coupling, overhead `std::function`, сложнее тестировать. Не повторяет паттерн DEBT-054/055.

**Выбор:** Вариант A.

## Поиск подводных камней

- [✅] **Все call sites найдены:** `MassBalance` — 5 мест (Solve, 2× test_amgcl_benchmark, 2× examples), `GetOverallBalance` — 9 мест (PrintReservoirState, 4× tests, 4× examples). Все вызывают через публичный API, изменения не нужны
- [✅] **Потокобезопасность:** MassBalance не вызывается из OpenMP-секций (вызывается после Newton loop, не внутри)
- [✅] **Зависимости сборки:** MassBalanceTracker.h не включает тяжёлые заголовки (только стандартные типы)
- [✅] **Обратная совместимость API:** `sim.MassBalance(loc_tau)` и `sim.GetOverallBalance()` остаются
- [✅] **Тесты:** все существующие тесты компилируются — call sites не меняются
- [✅] **Связь с другими задачами:** DEBT-057 (TimeIntegrator) зависит от DEBT-056, но не конфликтует
- [✅] **Производительность:** MassBalance вызывается 1 раз за time step, не горячий путь

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные заметки: [[debt-003 refactoring-reservoir-simulator]]
3. Создай ветку: `git checkout -b refactor/debt-056/mass-balance-tracker`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Baseline: **303 теста, ~156 сек**
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Шаг 1: Создать MassBalanceTracker (заголовок + реализация + CMake)

**Цель:** Новый класс MassBalanceTracker компилируется, но нигде не используется.

**Файлы:**
- `HydroSolver/Reservoir/MassBalanceTracker.h` — создать
- `HydroSolver/Reservoir/MassBalanceTracker.cpp` — создать
- `CMakeLists.txt` — добавить source

**Контекст:**
MassBalanceTracker — чистый аккумулятор: владеет 11 полями (`prevOil`, `curOil`, `accumOil`, `accumOilOutFlux`, `accumDebet`, `prevWater`, `curWater`, `accumWater`, `accumWaterOutFlux`, `accumWaterDebet`) и `curTime`. Не имеет зависимостей от `OilField`, `Wells`, `LinearProblem`. Инициализируется через `Initialize(oilTotal, waterTotal)`, обновляется через `Update(loc_tau, ...)` с 7 параметрами. Возвращает баланс через `GetBalance()`.

**Что сделать:**

1. Создать `HydroSolver/Reservoir/MassBalanceTracker.h`:

```cpp
#pragma once
#include <vector>

namespace reservoir_simulator {

class MassBalanceTracker {
public:
    void Initialize(double oilTotal, double waterTotal);

    void Update(double loc_tau,
                double oilTotal, double waterTotal,
                double oilContourFlux, double waterContourFlux,
                double oilDebitTotal, double waterDebitTotal);

    std::vector<double> GetBalance() const;

private:
    double prevOil_ = 0.0, curOil_ = 0.0;
    double accumOil_ = 0.0, accumOilOutFlux_ = 0.0, accumDebet_ = 0.0;
    double prevWater_ = 0.0, curWater_ = 0.0;
    double accumWater_ = 0.0, accumWaterOutFlux_ = 0.0, accumWaterDebet_ = 0.0;
    double curTime_ = 0.0;
};

} // namespace reservoir_simulator
```

2. Создать `HydroSolver/Reservoir/MassBalanceTracker.cpp`:

```cpp
#include "MassBalanceTracker.h"

namespace reservoir_simulator {

void MassBalanceTracker::Initialize(double oilTotal, double waterTotal) {
    curOil_ = oilTotal;
    accumDebet_ = 0.0;
    accumOilOutFlux_ = 0.0;
    accumOil_ = 0.0;
    curWater_ = waterTotal;
    accumWaterDebet_ = 0.0;
    accumWaterOutFlux_ = 0.0;
    accumWater_ = 0.0;
    curTime_ = 0.0;
}

void MassBalanceTracker::Update(double loc_tau,
                                 double oilTotal, double waterTotal,
                                 double oilContourFlux, double waterContourFlux,
                                 double oilDebitTotal, double waterDebitTotal) {
    prevOil_ = curOil_;
    curOil_ = oilTotal;
    accumOil_ += curOil_ - prevOil_;
    accumOilOutFlux_ += oilContourFlux * loc_tau;
    accumDebet_ -= oilDebitTotal * loc_tau;  // знак −= по конвенции, см. [[знаковая конвенция баланса масс accumDebet минус интеграл дебита]]

    prevWater_ = curWater_;
    curWater_ = waterTotal;
    accumWater_ += curWater_ - prevWater_;
    accumWaterOutFlux_ += waterContourFlux * loc_tau;
    accumWaterDebet_ -= waterDebitTotal * loc_tau;

    curTime_ += loc_tau;
}

std::vector<double> MassBalanceTracker::GetBalance() const {
    return {curTime_, accumOil_, accumOilOutFlux_, accumDebet_,
            accumWater_, accumWaterOutFlux_, accumWaterDebet_};
}

} // namespace reservoir_simulator
```

3. В `CMakeLists.txt` — добавить `${HYDRO}/Reservoir/MassBalanceTracker.cpp` в секцию `# Reservoir` (строки 41-44). Вставить перед или после существующих файлов:

До:
```
    # Reservoir
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NewtonSolver.cpp
```
После:
```
    # Reservoir
    ${HYDRO}/Reservoir/MassBalanceTracker.cpp
    ${HYDRO}/Reservoir/ReservoirSimulator.cpp
    ${HYDRO}/Reservoir/JacobianAssembler.cpp
    ${HYDRO}/Reservoir/NewtonSolver.cpp
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 303 теста зелёные (MassBalanceTracker создан, но не используется)

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2, 3

**Оценка:** ~65 строк, ~5 минут

---

## Шаг 2: Побитовые тесты MassBalanceTracker

**Цель:** Доказать что `MassBalanceTracker::Update()` + `GetBalance()` дают побитово идентичные результаты с `ReservoirSimulator::MassBalance()` + `GetOverallBalance()`.

**Файлы:**
- `tests/unit/math/test_MassBalanceTracker_standalone.cpp` — создать
- `CMakeLists.txt` — добавить в gdm_unit_level4

**Контекст:**
Паттерн из DEBT-055: два идентичных симулятора, один использует старый код, другой — новый класс. Побитовое сравнение результатов (`REQUIRE(==)` для каждого элемента вектора).

Для побитового теста нужно:
1. Создать два одинаковых симулятора `sim_old` и `sim_new`
2. Прогнать Newton loop на обоих (через `sim.PerformNewtonLoop`)
3. Если Newton успешен — вызвать `sim_old.MassBalance(loc_tau)` на одном, и вручную вызвать `MassBalanceTracker::Update()` с теми же значениями на другом
4. Сравнить `sim_old.GetOverallBalance()` с `tracker.GetBalance()`

Ключевая деталь: `MassBalance` вызывает `OilTotal()`, `WaterTotal()`, `OilContourFlux()`, `WaterContourFlux()`, `OilDebitTotal()`, `WaterDebitTotal()` — это методы ReservoirSimulator. Для побитового теста нужно вызвать эти же методы на `sim_new` и передать значения в `tracker.Update()`.

**Что сделать:**

1. Создать `tests/unit/math/test_MassBalanceTracker_standalone.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
#include "../../test_helpers.h"
#include "../../../HydroSolver/Reservoir/MassBalanceTracker.h"
#include "../../../HydroSolver/Reservoir/ReservoirSImulator.h"

using namespace reservoir_simulator;

namespace {

ReservoirSimulator make_sim(
    size_t Nx, size_t Ny, size_t Nz,
    double Lx, double Ly, double hz,
    double perm_mD, double poro,
    double P_init_atm, double oil_saturation)
{
    auto h = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz,
        perm_mD, poro, P_init_atm, oil_saturation);
    auto np = test_helpers::default_num_params();
    return ReservoirSimulator(np, h, h.oil, h.water, h.other);
}

void compare_balance(ReservoirSimulator& sim_old,
                     ReservoirSimulator& sim_new,
                     double tau)
{
    // Initialize tracker with pre-Newton state (matches constructor)
    MassBalanceTracker tracker;
    tracker.Initialize(sim_new.OilTotal(), sim_new.WaterTotal());

    // Run Newton on both
    sim_old.PerformNewtonLoop(tau, tau);
    sim_new.PerformNewtonLoop(tau, tau);

    REQUIRE(sim_old.numPrm.IsSuccessfullNewtonTrial() ==
            sim_new.numPrm.IsSuccessfullNewtonTrial());

    if (!sim_old.numPrm.IsSuccessfullNewtonTrial())
        return;

    // Old path: MassBalance updates internal fields
    sim_old.MassBalance(tau);

    // New path: call Update with same values from sim_new
    tracker.Update(tau,
        sim_new.OilTotal(), sim_new.WaterTotal(),
        sim_new.OilContourFlux(), sim_new.WaterContourFlux(),
        sim_new.OilDebitTotal(), sim_new.WaterDebitTotal());

    // Compare
    auto bal_old = sim_old.GetOverallBalance();
    auto bal_new = tracker.GetBalance();

    REQUIRE(bal_old.size() == bal_new.size());
    for (size_t i = 0; i < bal_old.size(); ++i) {
        INFO("balance[" << i << "]");
        REQUIRE(bal_old[i] == bal_new[i]);
    }
}

} // anonymous namespace

TEST_CASE("MassBalanceTracker: bitwise match 1D 5x1x1 no wells",
    "[MassBalanceTracker][bitwise]")
{
    auto sim_old = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto sim_new = make_sim(5, 1, 1, 500.0, 100.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    compare_balance(sim_old, sim_new, 0.1);
}

TEST_CASE("MassBalanceTracker: bitwise match 2D 5x5x1 with wells",
    "[MassBalanceTracker][bitwise]")
{
    auto horizon = test_helpers::make_uniform_horizon(
        5, 5, 1, 500.0, 500.0, 10.0, 100.0, 0.2, 100.0, 0.8);
    auto np = test_helpers::default_num_params();

    auto sim_old = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);
    auto sim_new = ReservoirSimulator(np, horizon, horizon.oil, horizon.water, horizon.other);

    test_helpers::add_simple_well(sim_old, horizon, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_old, horizon, L"PROD", 450.0, 250.0, 5.0, 0.0);
    test_helpers::add_simple_well(sim_new, horizon, L"INJ", 50.0, 250.0, 0.0, -10.0);
    test_helpers::add_simple_well(sim_new, horizon, L"PROD", 450.0, 250.0, 5.0, 0.0);

    compare_balance(sim_old, sim_new, 0.1);
}

TEST_CASE("MassBalanceTracker: bitwise match Sw=0",
    "[MassBalanceTracker][bitwise]")
{
    auto sim_old = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    auto sim_new = make_sim(3, 3, 1, 300.0, 300.0, 10.0, 100.0, 0.2, 100.0, 1.0);
    compare_balance(sim_old, sim_new, 0.1);
}
```

2. В `CMakeLists.txt` — добавить `tests/unit/math/test_MassBalanceTracker_standalone.cpp` в `gdm_unit_level4` (рядом с test_NewtonSolver_standalone.cpp, ~строка 212).

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 306 тестов (303 + 3 новых), все зелёные
- Побитовое совпадение: `bal_old[i] == bal_new[i]` для всех 7 элементов

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 3

**Оценка:** ~95 строк, ~10 минут

---

## Шаг 3: Интеграция MassBalanceTracker в ReservoirSimulator

**Цель:** ReservoirSimulator владеет `balance_tracker_`, 11 аккумуляторных полей удалены, `MassBalance()` и `GetOverallBalance()` делегируют трекеру.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSImulator.h` — изменить
- `HydroSolver/Reservoir/ReservoirSimulator.cpp` — изменить

**Контекст:**
Паттерн идентичен DEBT-054/055: поля и логика переезжают в новый класс, старые методы остаются как делегирующие обёртки.

**Что сделать:**

1. В `ReservoirSImulator.h`:
   - Добавить `#include "MassBalanceTracker.h"` (рядом с `#include "NewtonSolver.h"`, строка ~11)
   - Удалить 3 строки полей (строки 66-68):
     ```
     double prevOil, curOil, accumOil, accumOilOutFlux, accumDebet;
     double prevWater, curWater, accumWater, accumWaterOutFlux, accumWaterDebet;
     double curTime;
     ```
   - Добавить поле `MassBalanceTracker balance_tracker_;` рядом с `NewtonSolver newton_solver_;` (строка ~73)

До:
```cpp
#include "NewtonSolver.h"
```
После:
```cpp
#include "MassBalanceTracker.h"
#include "NewtonSolver.h"
```

До (строки 66-68):
```cpp
		double prevOil, curOil, accumOil, accumOilOutFlux, accumDebet;
		double prevWater, curWater, accumWater, accumWaterOutFlux, accumWaterDebet;
		double curTime;
```
После:
```cpp
		// (поля удалены — перенесены в MassBalanceTracker)
```
Примечание: строки просто удаляются, комментарий не нужен. В документе показан для наглядности.

До (строка ~73):
```cpp
		NewtonSolver newton_solver_;
```
После:
```cpp
		MassBalanceTracker balance_tracker_;
		NewtonSolver newton_solver_;
```

2. В `ReservoirSimulator.cpp`:
   - Инициализация в конструкторе (строки 66-74) — заменить 9 строк прямой инициализации на вызов `balance_tracker_.Initialize()`:

До:
```cpp
		curOil = OilTotal();
		accumDebet = 0.0;
		accumOilOutFlux = 0.0;
		accumOil = 0.0;
		curWater = WaterTotal();
		accumWaterDebet = 0.0;
		accumWaterOutFlux = 0.0;
		accumWater = 0.0;
		curTime = 0.0;
```
После:
```cpp
		balance_tracker_.Initialize(OilTotal(), WaterTotal());
```

   - Метод `MassBalance` (строки 474-491) — заменить на делегацию:

До:
```cpp
	void ReservoirSimulator::MassBalance(double loc_tau)
	{
		prevOil = curOil;
		curOil = OilTotal();
		accumOil += curOil - prevOil;

		accumOilOutFlux += OilContourFlux() * loc_tau;

		accumDebet -= OilDebitTotal() * loc_tau;

		prevWater = curWater;
		curWater = WaterTotal();
		accumWater += curWater - prevWater;

		accumWaterOutFlux += WaterContourFlux() * loc_tau;

		accumWaterDebet -= WaterDebitTotal() * loc_tau;

		curTime += loc_tau;
	}
```
После:
```cpp
	void ReservoirSimulator::MassBalance(double loc_tau)
	{
		balance_tracker_.Update(loc_tau,
			OilTotal(), WaterTotal(),
			OilContourFlux(), WaterContourFlux(),
			OilDebitTotal(), WaterDebitTotal());
	}
```

   - Метод `GetOverallBalance` (строки 215-219) — заменить на делегацию:

До:
```cpp
	std::vector<double> ReservoirSimulator::GetOverallBalance() const
	{
		return std::vector<double>{curTime, accumOil, accumOilOutFlux, accumDebet,
			accumWater, accumWaterOutFlux, accumWaterDebet};
	}
```
После:
```cpp
	std::vector<double> ReservoirSimulator::GetOverallBalance() const
	{
		return balance_tracker_.GetBalance();
	}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- 306 тестов, все зелёные
- Побитовые тесты из шага 2 подтверждают идентичность
- Тесты баланса масс (`test_amgcl_benchmark`, `test_variable_debit`, `test_3d_completions`, `test_visual_verification`) зелёные

**Подводные камни:**
- Порядок вычисления аргументов `Update()`: в C++ порядок вычисления аргументов функции не определён стандартом, но все 6 вызовов (`OilTotal()` и т.д.) — const-методы без побочных эффектов, поэтому порядок не влияет на результат
- `prevOil`, `prevWater` больше не доступны снаружи. Проверка: grep показывает, что эти поля не используются нигде кроме `MassBalance()` — безопасно

**Зависимости:**
- Требует: шаг 1, шаг 2
- Блокирует: шаг 4

**Оценка:** ~30 строк изменений (удаление ~25, добавление ~10), ~10 минут

---

## Шаг 4: Vault и GitHub

**Цель:** Обновить документацию, статус задачи, прокомментировать issue.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md` — обновить статус DEBT-056
- `vault/GDM/00-home/текущие приоритеты.md` — добавить строку DEBT-056 ✅
- `vault/GDM/00-home/index.md` — добавить ссылку на план

**Что сделать:**

1. В `известные баги и технический долг.md` — обновить статус DEBT-056 на ✅
2. В `текущие приоритеты.md` — добавить строку:
   ```
   - **DEBT-056:** ✅ MassBalanceTracker выделен (2026-07-XX, ветка refactor/debt-056/mass-balance-tracker, +3 теста)
   ```
3. В `index.md` — добавить:
   ```
   - [[debt-056 mass-balance-tracker]] — DEBT-056: выделить MassBalanceTracker из ReservoirSimulator (этап 3 DEBT-003)
   ```
4. GitHub: `gh issue comment 25 --repo ArturSalamatin/GDM --body "Реализовано. +3 побитовых теста. Merge → experimental."`

**Зависимости:**
- Требует: шаг 3

**Оценка:** ~5 минут

---

## Критерии завершения

- [ ] MassBalanceTracker.h + MassBalanceTracker.cpp созданы
- [ ] 11 аккумуляторных полей + curTime удалены из ReservoirSimulator
- [ ] `MassBalance()` и `GetOverallBalance()` делегируют `balance_tracker_`
- [ ] Все существующие тесты зелёные (303)
- [ ] 3 новых побитовых теста зелёные (306 всего)
- [ ] CMakeLists.txt обновлён
- [ ] Vault обновлён: статус, приоритеты, index
- [ ] GitHub issue #25 прокомментирован
- [ ] Производительность не деградировала (MassBalance — не горячий путь)

## Обнаруженные проблемы

Нет.

## Связанные заметки

- [[debt-003 refactoring-reservoir-simulator]] — общий план декомпозиции
- [[debt-054 jacobian-assembler]] — этап 1 (завершён)
- [[debt-055 newton-solver]] — этап 2 (завершён)
- [[технический долг]] — реестр DEBT-056
- Блокирует: DEBT-057 (TimeIntegrator)
