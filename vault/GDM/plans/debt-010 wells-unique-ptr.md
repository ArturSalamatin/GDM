---
tags:
  - план
  - рефакторинг
date: 2026-07-20
issue: DEBT-010
github: 43
branch: refactor/debt-010/wells-unique-ptr
status: готов к реализации
audit:
  date: 2026-07-20
  repeat: 2026-07-20
  findings: 0 / 0 / 1
  auto-fixed: 1
  manual-required: 0
---

# DEBT-010: `new`/`delete` для скважин → `unique_ptr`

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[code-review-2026-06-28-утечки-ресурсов-и-память]] (CR-MEM-001)
3. Создай ветку: `git checkout -b refactor/debt-010/wells-unique-ptr`
4. Собери:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline):
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

## Контекст

`ReservoirSimulator::Wells` — `std::map<WellName, wells::SomeWell*>` с ручным `new` при вставке и `delete` в деструкторе. Это source of leaks при исключениях и нарушение Rule of Five.

Дополнительная проблема: `SomeWell` содержит виртуальный метод `AddWellToMatrix` (чисто виртуальный), но деструктор не объявлен виртуальным. `delete` через указатель на базовый класс — UB по стандарту ([expr.delete]/3), даже если наследник (`WellFixedProduction`) не добавляет data-членов.

`WellName` — alias для `std::string` (определён в `HydroSolver/defines.h:12`).

## Целевое состояние

```cpp
// SomeWell.h — виртуальный деструктор
virtual ~SomeWell() = default;

// ReservoirSImulator.h
std::map<WellName, std::unique_ptr<wells::SomeWell>> Wells;

// ReservoirSImulator.h — объявление НЕ меняется (SomeWell incomplete в .h)
virtual ~ReservoirSimulator();
// ReservoirSimulator.cpp — тело → `= default` (SomeWell complete в .cpp)
```

Наблюдатели (NewtonSolver, JacobianAssembler, NumericalParameters) принимают `const std::map<WellName, std::unique_ptr<wells::SomeWell>>&`.

Anomalies хранит `std::map<std::string, wells::SomeWell*>` как собственную **копию** (не ссылку). Поскольку `Anomalies` — наблюдатель (только читает через указатели), его поле `wells` остаётся `std::map<std::string, wells::SomeWell*>` (не-владеющий индекс), но конструктор принимает `const map<string, unique_ptr<SomeWell>>&` и извлекает raw pointers через `.get()`.

## Выбор варианта

**Вариант A (выбран): `unique_ptr` в map.** Минимальный diff, идиоматический C++, механические изменения.

Альтернатива (отвергнута): `vector<unique_ptr>` + индексный `map<WellName, SomeWell*>` — два контейнера, больше кода, сложнее синхронизация.

## Чеклист подводных камней

- [x] **Все call sites найдены:** 17 файлов (перечислены ниже, включая examples/ и AnomalySimulator.h)
- [x] **Потокобезопасность:** `wells` передаётся как `const&`, скважины мутируются только через `AddWellToMatrix` (не const) — `unique_ptr` это не меняет
- [x] **Зависимости сборки:** CMakeLists.txt не затрагивается (файлы не добавляются/удаляются)
- [x] **Обратная совместимость API:** `GetWells()` меняет тип возврата — все call sites обновляются
- [x] **Тесты:** 3 standalone-теста используют `sim.Wells` напрямую — обновить доступ
- [x] **Кодировки:** не затрагивает
- [x] **Платформозависимость:** не затрагивает
- [x] **Мёртвый код:** нет
- [x] **Связь с другими задачами:** DEBT-054/055 (рефакторинг NewtonSolver/JacobianAssembler) — планы уже используют `SomeWell*` в сигнатурах, нужно будет обновить. Не блокирует, не конфликтует при правильной последовательности
- [x] **Forward compatibility с FEAT-001 (BHP-скважина):** `WellFixedBHP : public SomeWell` с data-членами (P_wf, WI). `unique_ptr<SomeWell>` + `virtual ~SomeWell()` корректно уничтожает наследника. `Wells[name] = make_unique<WellFixedBHP>(...)` — переключение режима mid-simulation проще, чем с raw pointer (автоматический delete старого объекта). Дополнительных virtual методов в SomeWell для DEBT-010 не нужно
- [x] **Производительность:** `unique_ptr` — zero overhead для доступа; `make_unique` вместо `new` — без изменений

## Затронутые файлы (полный список)

| # | Файл | Шаг | Роль |
|---|---|---|---|
| 1 | `HydroSolver/Reservoir/Well/SomeWell.h` | 1 | виртуальный деструктор |
| 2 | `HydroSolver/Reservoir/ReservoirSImulator.h` | 2 | тип `Wells`, `GetWells()` |
| 3 | `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 2 | `new` → `make_unique`, удалить деструктор, `OilDebitTotal`, `WaterDebitTotal` |
| 4 | `HydroSolver/Reservoir/NewtonSolver.h` | 3 | сигнатуры `Solve`, `SingleIteration` |
| 5 | `HydroSolver/Reservoir/NewtonSolver.cpp` | 3 | сигнатуры + доступ |
| 6 | `HydroSolver/Reservoir/JacobianAssembler.h` | 3 | сигнатура `Assemble` |
| 7 | `HydroSolver/Reservoir/JacobianAssembler.cpp` | 3 | сигнатура + доступ `well->` |
| 8 | `HydroSolver/Reservoir/NumericalParameters.h` | 3 | сигнатура `update_maxTauAllowed` |
| 9 | `HydroSolver/Reservoir/NumericalParameters.cpp` | 3 | сигнатура + доступ `well->` |
| 10 | `HydroSolver/Anomaly/Anomaly.h` | 4 | тип `wells`, конструктор |
| 11 | `HydroSolver/Anomaly/Anomaly.cpp` | 4 | конструктор, `wells.at()` |
| 12 | `tests/unit/math/test_JacobianAssembler_standalone.cpp` | 5 | `sim.Wells` |
| 13 | `tests/unit/math/test_NewtonSolver_standalone.cpp` | 5 | `sim.Wells` |
| 14 | `tests/unit/math/test_TimeIntegrator_standalone.cpp` | 5 | `sim.GetWells()` |
| 15 | `examples/ex_benchmark_series_ts.cpp` | 5 | `sim.GetWells()` — без изменений |
| 16 | `examples/ex_benchmark_series_cpr.cpp` | 5 | `sim.GetWells()` (×2) — без изменений |
| 17 | `HydroSolver/Reservoir/AnomalySimulator.h` | 2 | наследник ReservoirSimulator — не меняется, но причина сохранения virtual dtor |

---

## Шаг 1: Виртуальный деструктор в `SomeWell`

**Цель:** устранить UB при `delete` через базовый указатель. Изолированное изменение, не ломает API.

**Файлы:** `HydroSolver/Reservoir/Well/SomeWell.h`

**Контекст:**
`SomeWell` наследует от `WellEnvironment` ← `WellTrajectory`. Ни один из базовых классов не объявляет виртуального деструктора. `SomeWell` содержит чисто виртуальный метод `AddWellToMatrix` (строка 173), что делает его полиморфным базовым классом. Однако деструктор неявный и невиртуальный.

`ReservoirSimulator::~ReservoirSimulator()` делает `delete p` где `p` — `SomeWell*`, указывающий на `WellFixedProduction`. По [expr.delete]/3 это UB если тип `*p` — не most-derived type, а базовый класс без виртуального деструктора.

Сейчас `WellFixedProduction` не добавляет data-членов сверх `SomeWell`, поэтому на практике деструкция корректна на MSVC. Но стандарт это не гарантирует. Кроме того, FEAT-001 (режим BHP) добавит `WellFixedBHP : public SomeWell` с собственными data-членами (P_wf, WI) — без виртуального деструктора `unique_ptr<SomeWell>` для `WellFixedBHP` даст не только формальное UB, но и реальную утечку памяти.

**Что сделать:**

1. В `SomeWell.h`, после строки 168 (закрывающая скобка конструктора), перед строкой 170 (`initialize_MER_data`), добавить:

До (строки 168–170):
```cpp
			const std::vector<size_t>& itsLocalIDs, double itsAppWellRadius);

		void initialize_MER_data(
```

После:
```cpp
			const std::vector<size_t>& itsLocalIDs, double itsAppWellRadius);

		virtual ~SomeWell() = default;

		void initialize_MER_data(
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты должны остаться зелёными (поведение не изменилось — `WellFixedProduction` не добавляет членов)

**Подводные камни:**
- MSVC может сгенерировать warning о non-virtual dtor в `WellEnvironment` / `WellTrajectory` — маловероятно, так как у них нет виртуальных методов

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2 (без виртуального деструктора `unique_ptr<SomeWell>` тоже будет UB)

**Оценка:** 1 строка, ~2 минуты

---

## Шаг 2: `unique_ptr` в `ReservoirSimulator`

**Цель:** заменить `std::map<WellName, wells::SomeWell*>` на `std::map<WellName, std::unique_ptr<wells::SomeWell>>`, удалить ручной `new`/`delete`.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSImulator.h`
- `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
`Wells` — публичное поле `ReservoirSimulator`. Создание: `Wells.insert({name, new WellFixedProduction(...)})` в `AddWell_FixedProduction` (строка 143). Удаление: цикл `for (auto [name, p] : Wells) delete p;` в деструкторе (строка 150–153).

После этого шага `Wells` владеет объектами через `unique_ptr`, деструктор не нужен.

`ReservoirSimulator` передаёт `Wells` в:
- `newton_solver_.Solve(...)` (строки 413–414)
- `newton_solver_.SingleIteration(...)` (строки 423–424)
- `newton_solver_.assembler_.Assemble(...)` (строки 429–430)
- `numPrm.update_maxTauAllowed(...)` (строка 392)
- `GetWells()` (строка 157–159)

Также итерирует `Wells` в `OilDebitTotal()` (строка 367) и `WaterDebitTotal()` (строка 376).

**Что сделать:**

### 2.1. `ReservoirSImulator.h` — тип `Wells`

До (строка 55):
```cpp
	std::map<WellName, wells::SomeWell*> Wells;
```

После:
```cpp
	std::map<WellName, std::unique_ptr<wells::SomeWell>> Wells;
```

### 2.2. `ReservoirSImulator.h` — возврат `GetWells()`

До (строка 47):
```cpp
	const std::map<WellName, wells::SomeWell*>& GetWells() const;
```

После:
```cpp
	const std::map<WellName, std::unique_ptr<wells::SomeWell>>& GetWells() const;
```

### 2.3. `ReservoirSimulator.cpp` — `AddWell_FixedProduction`

До (строки 143–147):
```cpp
		Wells.insert({ name,
			new wells::WellFixedProduction(name, name, intersecCoords,
			std::make_unique<const mer_descriptor::MER_Data>(name, well_data),
			perforationsOfWell.AccumulatePerforations(ActiveCells),
			cells_, well_local_position, appRadiusWell) });
```

После:
```cpp
		Wells.emplace(name,
			std::make_unique<wells::WellFixedProduction>(name, name, intersecCoords,
			std::make_unique<const mer_descriptor::MER_Data>(name, well_data),
			perforationsOfWell.AccumulatePerforations(ActiveCells),
			cells_, well_local_position, appRadiusWell));
```

Почему `emplace` вместо `insert`: `unique_ptr` не копируется, нужен move. `emplace` конструирует `pair` in-place и принимает `unique_ptr&&`.

### 2.4. `ReservoirSimulator.cpp` — деструктор тело → `= default`

**Важно:** объявление в `.h` (`virtual ~ReservoirSimulator();`) **не трогаем**.

Причина: в `ReservoirSImulator.h` тип `SomeWell` — incomplete (forward declaration, строка 26). `unique_ptr<SomeWell>` требует complete type при вызове деструктора ([unique.ptr.single.dtor]). Если сделать `= default` в `.h`, деструктор `unique_ptr<SomeWell>::~unique_ptr()` инстанциируется в каждом TU, включающем этот header, с incomplete type → **UB** по стандарту, на практике — ошибка компиляции (MSVC C4150).

В `.cpp` тип `SomeWell` — complete (через `#include "Well/Wells.h"`, строка 2). Определение `= default` здесь корректно.

До (строки 150–154):
```cpp
	ReservoirSimulator::~ReservoirSimulator()
	{
		for (auto [name, p] : Wells)
			delete p;
	}
```

После:
```cpp
	ReservoirSimulator::~ReservoirSimulator() = default;
```

### 2.5. `ReservoirSimulator.cpp` — `GetWells()`

До (строки 156–160):
```cpp
	const std::map<WellName, wells::SomeWell*>& 
		ReservoirSimulator::GetWells() const
	{
		return Wells;
	}
```

После:
```cpp
	const std::map<WellName, std::unique_ptr<wells::SomeWell>>& 
		ReservoirSimulator::GetWells() const
	{
		return Wells;
	}
```

### 2.6. `ReservoirSimulator.cpp` — `OilDebitTotal()`

До (строки 367–371):
```cpp
		for (const auto& [name, well] : Wells)
		{
			result += well->CurOilDebit_Num();
		}
```

Без изменений: `well` — `const std::unique_ptr<SomeWell>&`, оператор `->` работает идентично raw pointer. Код компилируется как есть.

### 2.7. `ReservoirSimulator.cpp` — `WaterDebitTotal()`

До (строки 376–380):
```cpp
		for (const auto& pair : Wells)
		{
			auto& well = pair.second;
			result += well->CurWaterDebit_Num();
		}
```

Без изменений: `pair.second` — `const std::unique_ptr<SomeWell>&`, `->` работает.

### 2.8. Прямые передачи `Wells` в `PerformNewtonLoop`, `SingleIteration`, `AssembleMyProblem`

Строки 414, 424, 430 — передают `Wells` как аргумент. Тип аргумента изменится на шаге 3. Код не компилируется до шага 3.

**Проверка после этого шага:**
- Сборка: **не скомпилируется** — наблюдатели ещё принимают `SomeWell*`. Это нормально — продолжай шаг 3 без промежуточной сборки.

**Подводные камни:**
- `Wells` — публичное поле. Тесты обращаются к нему напрямую (`sim.Wells`) — исправляется на шаге 5
- `map::emplace` с `unique_ptr` — если ключ уже есть, `unique_ptr` не вставится, но объект будет разрушен `make_unique`-ом при выходе из scope временного. Это корректно — скважины с дублирующимися именами не должны существовать

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаг 3, 4, 5

**Оценка:** ~15 строк, ~10 минут

---

## Шаг 3: Обновить наблюдателей (NewtonSolver, JacobianAssembler, NumericalParameters)

**Цель:** изменить сигнатуры функций, принимающих `wells`, на `const std::map<WellName, std::unique_ptr<wells::SomeWell>>&`.

**Файлы:**
- `HydroSolver/Reservoir/NewtonSolver.h`
- `HydroSolver/Reservoir/NewtonSolver.cpp`
- `HydroSolver/Reservoir/JacobianAssembler.h`
- `HydroSolver/Reservoir/JacobianAssembler.cpp`
- `HydroSolver/Reservoir/NumericalParameters.h`
- `HydroSolver/Reservoir/NumericalParameters.cpp`

**Контекст:**
Все эти модули — наблюдатели: принимают `const map<WellName, SomeWell*>&` и итерируют по скважинам. Никто не владеет указателями. Изменение чисто механическое: тип параметра + способ доступа к значению.

**Что сделать:**

### 3.1. `NewtonSolver.h` — два метода

До (строки 23, 32):
```cpp
		const std::map<WellName, wells::SomeWell*>& wells,
```

После (оба места):
```cpp
		const std::map<WellName, std::unique_ptr<wells::SomeWell>>& wells,
```

### 3.2. `NewtonSolver.cpp` — два метода

До (строки 15, 46):
```cpp
	const std::map<WellName, wells::SomeWell*>& wells,
```

После (оба места):
```cpp
	const std::map<WellName, std::unique_ptr<wells::SomeWell>>& wells,
```

Тело `Solve` (строка 23): передаёт `wells` в `SingleIteration` — без изменений (тип совпадает).
Тело `SingleIteration` (строка 50): передаёт `wells` в `assembler_.Assemble` — без изменений после 3.3.

### 3.3. `JacobianAssembler.h`

До (строка 20):
```cpp
		const std::map<WellName, wells::SomeWell*>& wells);
```

После:
```cpp
		const std::map<WellName, std::unique_ptr<wells::SomeWell>>& wells);
```

### 3.4. `JacobianAssembler.cpp`

До (строка 23):
```cpp
		const std::map<WellName, wells::SomeWell*>& wells)
```

После:
```cpp
		const std::map<WellName, std::unique_ptr<wells::SomeWell>>& wells)
```

Тело (строка 44): `for (auto& [name, well] : wells)` — `well` станет `const std::unique_ptr<SomeWell>&`. Оператор `well->AddWellToMatrix(...)` (строка 47) — работает без изменений, `->` у `unique_ptr` возвращает raw pointer. `well->NmbrOfOpenedCells()` (строка 48) — аналогично.

**Важно:** `AddWellToMatrix` — не const метод (мутирует внутреннее состояние скважины). `const map<..., unique_ptr<SomeWell>>&` гарантирует что `unique_ptr` не переназначается, но `SomeWell*` внутри `unique_ptr` — не const. Поэтому `well->AddWellToMatrix()` компилируется: `unique_ptr::operator->()` возвращает `SomeWell*` (не `const SomeWell*`), даже для `const unique_ptr&`.

### 3.5. `NumericalParameters.h`

До (строка 111):
```cpp
	void update_maxTauAllowed(double nextRefMoment, const std::map<WellName, wells::SomeWell*>& wells);
```

После:
```cpp
	void update_maxTauAllowed(double nextRefMoment, const std::map<WellName, std::unique_ptr<wells::SomeWell>>& wells);
```

### 3.6. `NumericalParameters.cpp`

До (строка 72):
```cpp
void NumericalParameters::update_maxTauAllowed(double nextRefMoment, const std::map<WellName, wells::SomeWell*>& wells)
```

После:
```cpp
void NumericalParameters::update_maxTauAllowed(double nextRefMoment, const std::map<WellName, std::unique_ptr<wells::SomeWell>>& wells)
```

Тело (строка 82): `for (const auto& [name, well] : wells)` — `well->TimeToNextMomemnt(...)` (строка 89) — работает без изменений.

### 3.7. Добавить `#include <memory>` если нужно

Проверить, есть ли `#include <memory>` в каждом файле (или транзитивно через `stdafx.h` / `SomeWell.h`). `SomeWell.h` уже использует `std::unique_ptr<MER_Data>`, значит `<memory>` уже включен транзитивно.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Должна скомпилироваться (кроме тестов, если они напрямую обращаются к `sim.Wells` — см. шаг 5)
- Если тесты не компилируются — это нормально, продолжай шаг 4–5

**Подводные камни:**
- `const unique_ptr<SomeWell>&` → `operator->()` возвращает `SomeWell*` (не `const SomeWell*`) — мутирующие вызовы компилируются. Это корректно: constness `unique_ptr` защищает владение, не объект
- Если `NumericalParameters.h` не включает `SomeWell.h` напрямую, а только forward-declare — `unique_ptr` требует полного определения. Проверить: `NumericalParameters.h` включает его через цепочку `#include`. Если нет — добавить forward declaration + `#include` в `.cpp`

**Зависимости:**
- Требует: шаг 2
- Блокирует: шаг 5

**Оценка:** ~12 строк (6 сигнатур × 2 файла), ~10 минут

---

## Шаг 4: Обновить `Anomalies`

**Цель:** `Anomalies` хранит `std::map<std::string, SomeWell*> wells` как собственную копию. Нужно изменить на не-владеющий указатель, совместимый с `unique_ptr` в `ReservoirSimulator`.

**Файлы:**
- `HydroSolver/Anomaly/Anomaly.h`
- `HydroSolver/Anomaly/Anomaly.cpp`

**Контекст:**
`Anomalies` — модуль отслеживания аномалий. Его конструктор принимает `const std::map<std::string, SomeWell*>& wells` и **копирует** map в поле `this->wells` (строка 220: `(*this).wells = wells;`).

`Anomalies` использует `wells` только для чтения:
- `wells.find(wellName)` — поиск скважины по имени (строка 236)
- `wells.at(wellName)` — получение указателя (строки 239, 275)
- Через указатель вызывает: `well->IntersectionCoords()`, `well->IntersectionCoordsNum()` — const-методы

Проблема: `Anomalies` не может хранить `map<string, unique_ptr<SomeWell>>` — это бы означало co-ownership. Вместо этого `Anomalies` должен хранить не-владеющие указатели.

Два подхода:
- **A:** Оставить `map<string, SomeWell*>` (не-владеющий), но конструктор принимает `const map<WellName, unique_ptr<SomeWell>>&` и извлекает raw pointers → `this->wells[name] = ptr.get()`
- **B:** Хранить `const map<WellName, unique_ptr<SomeWell>>*` (указатель на оригинальный map)

**Выбор: вариант A** — минимальный diff, `Anomalies` остаётся самодостаточным.

**Что сделать:**

### 4.1. `Anomaly.h` — конструктор

До (строка 177):
```cpp
	Anomalies(const std::map<std::string, SomeWell*>& wells, const std::string& anomalyPath);
```

После:
```cpp
	Anomalies(const std::map<std::string, std::unique_ptr<SomeWell>>& wells, const std::string& anomalyPath);
```

### 4.2. `Anomaly.h` — поле `wells`

Поле `wells` остаётся `std::map<std::string, wells::SomeWell*>` (строка 165) — это не-владеющий индекс. Без изменений.

### 4.3. `Anomaly.cpp` — конструктор

До (строки 218–221):
```cpp
 Anomalies::Anomalies(const std::map<std::string, SomeWell*>& wells, const std::string& anomalyPath)
{
	(*this).wells = wells;
	(*this).anomalyData.Push(anomalyPath);
}
```

После:
```cpp
 Anomalies::Anomalies(const std::map<std::string, std::unique_ptr<SomeWell>>& wells, const std::string& anomalyPath)
{
	for (const auto& [name, ptr] : wells)
		this->wells[name] = ptr.get();
	this->anomalyData.Push(anomalyPath);
}
```

Почему цикл вместо `=`: `map<string, unique_ptr<SomeWell>>` нельзя скопировать в `map<string, SomeWell*>` напрямую — типы значений разные.

### 4.4. Проверить `#include <memory>`

`Anomaly.h` включает `SomeWell.h` (транзитивно или напрямую), который уже использует `unique_ptr`. Проверить наличие `#include <memory>` в цепочке.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Если `Anomalies` конструируется где-то с `map<string, SomeWell*>` — найти это место и обновить
- Тесты: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- Время жизни: `Anomalies::wells` содержит raw pointers на объекты, принадлежащие `ReservoirSimulator::Wells`. Если `ReservoirSimulator` разрушится раньше `Anomalies` — dangling pointers. Это **существующая** проблема (было так же с raw pointers), DEBT-010 её не создаёт и не решает
- `WellSignals::set_trajectories_fixed_time` принимает `const SomeWell*` — без изменений, `wells.at(name)` возвращает `SomeWell*` из не-владеющего `map`

**Зависимости:**
- Требует: шаг 2
- Блокирует: ничего (параллельно с шагом 3, но проще делать последовательно)

**Оценка:** ~8 строк, ~5 минут

---

## Шаг 5: Обновить тесты

**Цель:** исправить тесты, которые обращаются к `sim.Wells` напрямую.

**Файлы:**
- `tests/unit/math/test_JacobianAssembler_standalone.cpp`
- `tests/unit/math/test_NewtonSolver_standalone.cpp`
- `tests/unit/math/test_TimeIntegrator_standalone.cpp`

**Контекст:**
Standalone-тесты передают `sim.Wells` как аргумент в `Assemble()`, `Solve()`, `update_maxTauAllowed()`. Тип `sim.Wells` изменился с `map<WellName, SomeWell*>` на `map<WellName, unique_ptr<SomeWell>>`. Сигнатуры вызываемых методов уже обновлены (шаг 3), поэтому передача `sim.Wells` напрямую будет работать — типы совпадают.

**Что сделать:**

### 5.1. `test_JacobianAssembler_standalone.cpp`

Строка 30:
```cpp
    assembler.Assemble(loc_tau, nextTimeMoment,
        sim.Grid, sim.MyProblem, sim.RefPressure, sim.Wells);
```

`sim.Wells` теперь `map<..., unique_ptr<SomeWell>>`. `Assemble` принимает `const map<..., unique_ptr<SomeWell>>&` (после шага 3.3). Типы совпадают — **без изменений**.

### 5.2. `test_NewtonSolver_standalone.cpp`

Строка 34:
```cpp
    solver.Solve(tau, tau, sim_new.Grid, sim_new.MyProblem,
        sim_new.numPrm, sim_new.RefPressure, sim_new.Wells, profile);
```

Аналогично — **без изменений**.

### 5.3. `test_TimeIntegrator_standalone.cpp`

Строка 32:
```cpp
	sim_new.numPrm.update_maxTauAllowed(nextRef, sim_new.GetWells());
```

`GetWells()` возвращает `const map<..., unique_ptr<SomeWell>>&`. `update_maxTauAllowed` принимает тот же тип. **Без изменений**.

### 5.4. Проверить integration-тесты

`test_amgcl_benchmark.cpp` (строки 210, 352):
```cpp
sim.numPrm.update_maxTauAllowed(target, sim.GetWells());
```
**Без изменений** — `GetWells()` и `update_maxTauAllowed` обновлены.

`test_perf_regression.cpp` (строка 121):
```cpp
sim.numPrm.update_maxTauAllowed(target, sim.GetWells());
```
**Без изменений**.

### 5.5. Проверить `examples/`

`examples/ex_benchmark_series_ts.cpp` (строка 150) и `examples/ex_benchmark_series_cpr.cpp` (строки 199, 341) вызывают `sim.GetWells()` и передают результат в `update_maxTauAllowed`. После обновления `GetWells()` (шаг 2.2) и `update_maxTauAllowed` (шаг 3.5) типы совпадают — **без изменений**.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- **Все тесты должны быть зелёными**
- Количество тестов и результат — идентичны baseline

**Подводные камни:**
- Если где-то в тестах или examples копируется `sim.Wells` в локальную переменную `std::map<WellName, SomeWell*>` — это не скомпилируется. Grep по `sim.Wells` и `sim.GetWells()` во всём проекте выявил только передачу как аргумент, копий нет.

**Зависимости:**
- Требует: шаги 2, 3, 4
- Блокирует: ничего

**Оценка:** 0 строк изменений (все совместимы). ~5 минут на верификацию.

---

## Шаг 6: Очистка и верификация

**Цель:** финальная проверка, что все следы old-style ownership убраны.

**Что сделать:**

### 6.1. Grep по `SomeWell\*` в `HydroSolver/`

```powershell
# Должны остаться только:
# - Anomaly.h:165 (не-владеющий map) — корректно
# - WellSignals::set_trajectories_fixed_time (const SomeWell*) — корректно
# - Другие не-владеющие наблюдатели (если есть)
```

Если найдётся что-то ещё — обновить.

### 6.2. Проверить деструктор `ReservoirSimulator`

```powershell
# Grep: в .h должно остаться `virtual ~ReservoirSimulator();` (объявление, SomeWell incomplete)
# Grep: в .cpp должно быть `ReservoirSimulator::~ReservoirSimulator() = default;` (SomeWell complete)
# Grep: `delete p` / `delete` в деструкторе — не должно быть
```

### 6.3. Финальная сборка + тесты

```powershell
cmake --build build --config Release 2>&1 | Select-String "warning"
ctest --test-dir build -C Release --output-on-failure
```

- Warnings = 0 (не считая внешних библиотек)
- Все тесты зелёные
- Количество тестов = baseline

### 6.4. Побитовое сравнение результатов

Запустить интеграционный тест (например `test_buckley_leverett` или `test_five_spot`) и сравнить вывод с baseline. Результаты должны быть идентичны — `unique_ptr` не меняет поведение, только владение.

**Проверка после этого шага:**
- Все тесты зелёные
- 0 новых warnings
- Результаты вычислений идентичны baseline

**Зависимости:**
- Требует: все предыдущие шаги

**Оценка:** ~10 минут

---

## Обнаруженные проблемы

### 1. Отсутствие виртуального деструктора в `WellEnvironment` / `WellTrajectory`

`SomeWell` → `WellEnvironment` → `WellTrajectory`. После шага 1 `SomeWell` получит `virtual ~SomeWell() = default;`, но `WellEnvironment` и `WellTrajectory` — нет.

Анализ: `WellTrajectory` и `WellEnvironment` не имеют виртуальных методов, не являются полиморфными базовыми классами. Никто не делает `delete` через `WellEnvironment*` или `WellTrajectory*` — все удаления идут через `SomeWell*`. Виртуальный деструктор в `SomeWell` достаточен для всей цепочки наследования — он обеспечивает корректное разрушение через vtable.

**Решение:** виртуальные деструкторы в `WellEnvironment`/`WellTrajectory` **не нужны**. Добавлять не будем — это было бы избыточно и не соответствует C++ идиомам (виртуальный деструктор нужен в наиболее производном полиморфном базовом классе).

### 2. `AnomalySimulator : public ReservoirSimulator` (найдено аудитом)

`AnomalySimulator` — пустой наследник `ReservoirSimulator` (`HydroSolver/Reservoir/AnomalySimulator.h`). Первоначальный план предписывал удалить `virtual ~ReservoirSimulator()` — это было бы некорректно. Деструктор сохраняется: объявление `virtual ~ReservoirSimulator();` в `.h` (без `= default` — SomeWell incomplete), определение `= default` в `.cpp` (SomeWell complete через `#include "Well/Wells.h"`).

### 3. `ReservoirSimulator` — нарушение Rule of Five

Даже с `unique_ptr`, `ReservoirSimulator` не определяет copy/move. С `unique_ptr` в `map` copy-конструктор будет deleted (что правильно — копирование бессмысленно). Move-конструктор — автоматический (корректный). Это лучше, чем текущее состояние (copy компилируется, но приводит к double-delete).

**Решение:** `unique_ptr` решает проблему автоматически. Дополнительных действий не нужно.

## Критерии завершения

- [ ] Виртуальный деструктор в `SomeWell`
- [ ] `Wells` — `map<WellName, unique_ptr<SomeWell>>`
- [ ] Деструктор `ReservoirSimulator`: объявление `virtual ~ReservoirSimulator();` в `.h`, определение `= default` в `.cpp`
- [ ] Все наблюдатели обновлены
- [ ] Все тесты зелёные
- [ ] 0 новых warnings
- [ ] Результаты вычислений идентичны baseline
- [ ] Vault обновлён: статус DEBT-010
- [ ] GitHub issue #43 прокомментирован
