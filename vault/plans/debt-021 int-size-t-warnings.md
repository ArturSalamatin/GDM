---
tags:
  - план
  - рефакторинг
date: 2026-07-16
issue: DEBT-021
github: 31
branch: refactor/debt-021/int-size-t-warnings
status: реализован
audit:
  date: 2026-07-16
  round: 4
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  cumulative: 13 findings across 4 rounds, all auto-fixed
---

# DEBT-021: int переменные цикла итерируют по size_t границам

## Контекст задачи

По всей кодовой базе GDM переменные цикла объявлены как `int`, а верхние границы — `size_t` (`ActiveCellsNmbr()`, `.size()`, `nx`, `ny`, `nz`). MSVC генерирует ~30 warnings C4267 (size_t→int conversion). Кроме того:
- C4297 (2 места): `throw` внутри `noexcept`-конструкторов
- C4101 (1 место): неиспользуемая переменная в catch

Warnings из amgcl и MSVC STL — вне скоупа (внешние библиотеки).

**Ссылки:**
- Issue: [#31](https://github.com/ArturSalamatin/GDM/issues/31)
- Vault: `vault/GDM/roadmap/известные баги и технический долг.md` → DEBT-021, DEBT-022, DEBT-028

## Связанные задачи

- **DEBT-022:** `activeCellsNmbr` — `int`, а `ActiveCellsNmbr()` возвращает `size_t` — корень warnings в AbstractGrid.h. Не в скоупе (отдельная задача, затрагивает `operator[](int)` с семантикой отрицательных индексов)
- **DEBT-023:** методы `AbstractGrid` без `const` — не в скоупе
- **DEBT-028:** `AddOffDiagBlock` — `int neibIdx` vs `size_t` — не в скоупе (отдельная задача)

## Текущее состояние (baseline)

- 310 тестов, 0 failed (Release + Debug)
- Warnings из кода проекта (Release, MSVC 17 2022, /W3):
  - C4267 (size_t→int): 15 мест gdm_core + 3 ex_benchmark + 18 test_streamlines + 1 test_JacobianAssembly = 37
  - C4297 (throw in noexcept): 2 места (SomeWell.cpp, MER_Descriptor.cpp)
  - C4101 (unreferenced variable): 1 место (RS.cpp:471)
  - C4244 (double/float/__int64→int): 3 места (MathRoutines.cpp) — НЕ в скоупе DEBT-021, отдельная проблема
  - C4477 (snprintf format mismatch): 1 место (SomeFlowField.cpp:90) — НЕ в скоупе DEBT-021
  - C4267 в SomeFlowField.cpp:103 — НЕ в скоупе DEBT-021 (не связан с int loop variables vs size_t bounds)

## Целевое состояние

- 0 warnings C4267/C4297/C4101 из кода проекта (все targets)
- Все 310 тестов зелёные (поведение не изменилось)
- Amgcl/STL warnings — допустимы, не трогаем
- MathRoutines.cpp (C4244) и SomeFlowField.cpp (C4477, C4267) — вне скоупа, отдельные задачи

## Варианты решения

### Вариант A: static_cast в каждом call site

Добавить `static_cast<int>(...)` или `static_cast<size_t>(...)` в каждом месте mismatch.

- **Плюсы:** минимальный diff, не меняет API
- **Минусы:** маскирует проблему, каждое новое место потребует нового cast'а. Противоречит правилу «исправлять причину»

### Вариант B (выбранный): исправление типов по группам

Менять типы переменных и параметров там, где это безопасно:
- Переменные цикла `int` → `size_t` (где граница — `size_t` и индекс ≥ 0)
- Параметры функций `int` → `size_t` (где аргумент — `size_t`)
- `noexcept` убрать с конструкторов, бросающих исключения
- Неиспользуемая переменная → убрать имя

**Ограничения:**
- `AbstractGrid::operator[](int idx)` — НЕ менять (отрицательные индексы — валидная семантика для неактивных ячеек)
- `activeCellsNmbr` (int) — НЕ менять (DEBT-022)
- `GetNeighboursPointer(int l)`, `CommonEdgeArea(int l)` — НЕ менять (DEBT-023)
- `connectivityGraph` (`vector<vector<int>>`) — НЕ менять (каскад через весь проект)

Где переменная цикла `size_t l` передаётся в API, принимающий `int` (например `Grid[l]`), — использовать `static_cast<int>(l)`. Это безопасно, потому что: (a) l ≥ 0 всегда, (b) сетка < 2³¹ ячеек в текущих сценариях.

## Подводные камни

- ✅ **Все call sites найдены:** grep по каждой изменяемой функции выполнен
- ✅ **Потокобезопасность:** OpenMP-секции закомментированы, shared state не затронут
- ✅ **Зависимости сборки:** CMakeLists.txt не меняется
- ✅ **Обратная совместимость API:** внутренний API, внешних пользователей нет
- ✅ **Тесты:** все 310 тестов покрывают затронутый код
- ✅ **Кодировки:** не затронуты
- ✅ **Платформозависимость:** `size_t` = 8 байт на x64 Windows, все сетки < 2³¹ → безопасно
- ⚠️ **Обратные циклы:** проверено — нет обратных циклов (`for(int j = N; j >= 0; --j)`) в затронутых местах
- ⚠️ **Сравнение с -1:** `l_Local > -1` — `l_Local` остаётся `long int` (результат `ConvertGlobal2Local`), не меняем
- ✅ **Связь с другими задачами:** не конфликтует (DEBT-022/023/028 — отдельные, не пересекаются)
- ✅ **Производительность:** `size_t` vs `int` — одна инструкция, zero overhead

---

## Этап 1: Подготовка

### Шаг 0: Baseline

**Цель:** зафиксировать текущее состояние

**Что сделать:**
1. `git checkout -b refactor/debt-021/int-size-t-warnings experimental`
2. `cmake -B build -S . -G "Visual Studio 17 2022"`
3. `cmake --build build --config Release` — запомнить количество warnings
4. `ctest --test-dir build -C Release --output-on-failure` — 310 pass
5. `cmake --build build --config Debug`
6. `ctest --test-dir build -C Debug --output-on-failure` — 310 pass

**Baseline:** 310 тестов, 0 failed, ~14 unique warning sites из кода проекта.

---

## Этап 2: Реализация

### Шаг 1: ReservoirSimulator.cpp — OverallFluxes, OilContourFlux, WaterContourFlux

**Цель:** устранить C4267 в ~10 местах: переменные цикла в `OverallFluxes()` (строки ~602–870), `OilContourFlux()` (~876), `WaterContourFlux()` (~1006)

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
В методах `OverallFluxes()` (строки ~602–870), `OilContourFlux()` (~876) и `WaterContourFlux()` (~1006) используются `size_t nx, ny, nz` (получены из `Grid.Nx()`, `Grid.Ny()`, `Grid.Nz()`), но циклы объявлены `for (int k = 0; k < nz; k++)` и промежуточные переменные `int l_Global = nx * ny * k + ...`. Это даёт C4267 на строках 612, 617, 623, 628, 661, 663, 701, 702, 747, 752, 759, 792, 794, 830, 831, 881, 1011.

**NB:** Метод `AccountForBoundaryConditions` (упоминался в GitHub issue) после DEBT-054 перенесён в `JacobianAssembler.cpp` и там уже использует `size_t`. В RS.cpp на его месте — `OverallFluxes()`.

**Что сделать:**
1. Все циклы `for (int k/j/i = ...)` в `OverallFluxes()` → `for (size_t k/j/i = ...)`
2. `int l_Global = nx * ny * k + nx * j + i` → `size_t l_Global = ...` (все 8 мест)
3. `int l_Local = Grid.ConvertGlobal2Local(l_Global)` → `long int l_Local = ...` (ConvertGlobal2Local возвращает `long int`, может быть -1)
4. `int l_Local_Neighbour` → `long int l_Local_Neighbour` (строки 665, 796)
5. В `OilContourFlux()` (~881): `for (int j = 0; j < ny; j++)` → `for (size_t j = ...)`
6. В `WaterContourFlux()` (~1011): `for (int j = 0; j < ny; j++)` → `for (size_t j = ...)`
7. Во всех местах, где `l_Local` передаётся в `Grid[l_Local]`, тип `long int` совместим с `operator[](int)` — неявное сужение, но `l_Local` уже проверен через `l_Local > -1`, и значение помещается в `int`

**Изменения (пример первого блока ~628):**
До:
```cpp
int l_Global = nx * ny * k + nx * j + i;
int l_Local = Grid.ConvertGlobal2Local(l_Global);
```
После:
```cpp
size_t l_Global = nx * ny * k + nx * j + i;
long int l_Local = Grid.ConvertGlobal2Local(l_Global);
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные (поведение не изменилось)
- Warnings C4267 в OverallFluxes, OilContourFlux, WaterContourFlux: 0 (остальные warnings в RS.cpp — шаги 3, 4, 14)

**Подводные камни:**
- `l_Local > -1` — сравнение `long int` с `-1` — корректно (знаковый тип)
- `Grid[l_Local]` — `operator[](int)` принимает int, `long int → int` — неявно, безопасно при `l_Local < 2³¹`

**Зависимости:** нет
**Оценка:** ~30 строк, ~10 минут

---

### Шаг 2: ReservoirSimulator.cpp — LoadFlowFieldFromFile_bin

**Цель:** устранить C4267 в циклах чтения бинарных данных потока

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
Метод `LoadFlowFieldFromFile_bin` (~535–596) читает бинарный файл потоков. `size_t nx, ny, nz` → `for (int j = 0; j < ny; j++)` на строках 569, 578.

**Что сделать:**
1. Строка ~569: `for (int j = 0; j < ny; j++)` → `for (size_t j = 0; j < ny; j++)`
2. Строка ~578: `for (int j = 0; j < ny + 1; j++)` → `for (size_t j = 0; j < ny + 1; j++)`

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings в LoadFlowFieldFromFile_bin: 0

**Подводные камни:**
- `ny + 1`: `size_t + int` → `size_t`, корректно

**Зависимости:** нет (независим от шага 1)
**Оценка:** ~2 строки, ~2 минуты

---

### Шаг 3: ReservoirSimulator.cpp — OilTotal, WaterTotal

**Цель:** устранить C4267 на строках 353, 360

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
```cpp
for (size_t l = 0; l < ActiveCellsNmbr; ++l)
    result += Grid[l].OilMass();
```
`ActiveCellsNmbr` — `size_t` (поле класса, копия из `Grid.ActiveCellsNmbr()`), `l` — `size_t`. Warning: `Grid[l]` вызывает `operator[](int)` → неявное сужение `size_t → int`.

Здесь `l` гарантированно ≥ 0 и < 2³¹ (размер сетки). `static_cast<int>(l)` — минимальный фикс.

**Что сделать:**
1. Строка ~353: `Grid[l].OilMass()` → `Grid[static_cast<int>(l)].OilMass()`
2. Строка ~360: `Grid[l].WaterMass()` → `Grid[static_cast<int>(l)].WaterMass()`

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в RS.cpp: 0

**Подводные камни:**
- `static_cast<int>(l)` при l > INT_MAX → UB. Но ActiveCellsNmbr < 100k в текущих сценариях, масштабирование до 2³¹ нереалистично. Полное исправление — в DEBT-022 (менять `operator[]` на `size_t` с отдельным API для inactive cells)

**Зависимости:** нет
**Оценка:** ~2 строки, ~2 минуты

---

### Шаг 4: ReservoirSimulator.cpp — unreferenced variable C4101

**Цель:** устранить C4101 на строке 471

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
```cpp
catch (std::exception& e)
{
    reservoir_simulator::WarningFactory::NoFolderCreated();
```
Переменная `e` не используется.

**Что сделать:**
1. `catch (std::exception& e)` → `catch (const std::exception&)` — убрать имя, добавить const

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings в RS.cpp: 0

**Зависимости:** нет
**Оценка:** ~1 строка, ~1 минута

---

### Шаг 5: LinearProblem::Solve — int → size_t

**Цель:** устранить C4267 в NewtonSolver.cpp:53, ex_benchmark_series_cpr.cpp:216,358, ex_benchmark_series_ts.cpp:167, test_amgcl_benchmark.cpp:227,369

**Файлы:**
- `HydroSolver/Solver/Math/LinearProblem.h` (декларация)
- `HydroSolver/Solver/Math/LinearProblem.cpp` (реализация)
- `examples/ex_benchmark_series_cpr.cpp` (функция `solve_with_scalar`)
- `tests/test_amgcl_benchmark.cpp` (функция `solve_with_scalar`)

**Контекст:**
`LinearProblem::Solve(int maxIter)` принимает `int`, а `CurrentAMG_maxSolverIterationCount()` возвращает `size_t`. Все call sites передают `size_t`. Вызовы из тестов: `lp.Solve(1)` и `lp.Solve(100)` — литералы `int`, совместимы с `size_t`.

В `ex_benchmark_series_cpr.cpp:25` и `test_amgcl_benchmark.cpp:25` определена функция `solve_with_scalar(LinearProblem& lp, int maxIter, ...)`, которая дублирует логику `Solve` для скалярных солверов. Вызывается с `CurrentAMG_maxSolverIterationCount()` → `size_t`, сигнатура также требует `int → size_t`.

amgcl `prm.solver.maxiter` — `size_t` (проверено: richardson.hpp:76, lgmres.hpp:133).

**Что сделать:**
1. `LinearProblem.h:70` — `SolveResult Solve(int maxIter)` → `SolveResult Solve(size_t maxIter)`
2. `LinearProblem.cpp:114` — `SolveResult LinearProblem::Solve(int maxIter)` → `SolveResult LinearProblem::Solve(size_t maxIter)`
3. `prm.solver.maxiter = maxIter` внутри Solve — amgcl `maxiter` тоже `size_t` (richardson.hpp:76, lgmres.hpp:133), cast не нужен
4. `ex_benchmark_series_cpr.cpp:25` — `solve_with_scalar(LinearProblem& lp, int maxIter, ...)` → `solve_with_scalar(LinearProblem& lp, size_t maxIter, ...)`. Внутри: `prm.solver.maxiter = maxIter` — amgcl `size_t`, ok
5. `test_amgcl_benchmark.cpp:25` — аналогичная функция `solve_with_scalar`, тот же фикс

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в NewtonSolver.cpp: 0
- Warnings C4267 в ex_benchmark_series_cpr.cpp: 0
- Warnings C4267 в ex_benchmark_series_ts.cpp: 0
- Warnings C4267 в test_amgcl_benchmark.cpp: 0

**Подводные камни:**
- amgcl `maxiter` — `size_t`, cast не нужен. Литералы `1`, `100` в тестах (`lp.Solve(1)`) — неявное расширение `int → size_t`, корректно

**Зависимости:** нет
**Оценка:** ~5 строк, ~5 минут

---

### Шаг 6: NumericalParameters — update_overallCurIterCount

**Цель:** устранить C4267 в NumericalParameters.cpp:19

**Файлы:** `HydroSolver/Reservoir/NumericalParameters.h`

**Контекст:**
```cpp
size_t overallSolverIterationCount = 0;
void update_overallCurIterCount(int increment) { overallSolverIterationCount += increment; }
```
Вызывается с `CurrentAMG_IterationsCount()` (→ `size_t`). `overallSolverIterationCount` — `size_t`. Нет причин принимать `int`.

**Что сделать:**
1. `NumericalParameters.h:100` — `void update_overallCurIterCount(int increment)` → `void update_overallCurIterCount(size_t increment)`

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в NumericalParameters.cpp: 0

**Подводные камни:**
- Единственный call site — `NumericalParameters.cpp:19` с `size_t` аргументом

**Зависимости:** нет
**Оценка:** ~1 строка, ~1 минута

---

### Шаг 7: OilField.cpp — MakeCell(int l) → MakeCell(size_t l)

**Цель:** устранить C4267 на строках 40, 71 в OilField.cpp

**Файлы:**
- `HydroSolver/Solver/Grids/OilField.h` (декларация)
- `HydroSolver/Solver/Grids/OilField.cpp` (реализация)

**Контекст:**
`MakeCell(const int l, ...)` вызывается из циклов `for (size_t l = 0; ...)`. Параметр `l` используется как индекс в `std::vector<double>` (permeability[l], porosity[l] и т.д.), все они — size_t-индексированные.

**Что сделать:**
1. `OilField.h:41` — `TwoPhaseFlowCell MakeCell(const int l, ...)` → `TwoPhaseFlowCell MakeCell(const size_t l, ...)`
2. `OilField.cpp:7` — аналогично в реализации

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в OilField.cpp: 0

**Подводные камни:**
- `l` используется только для индексации vector → `size_t` корректен
- Нет других call sites кроме OilField.cpp (private метод)

**Зависимости:** нет
**Оценка:** ~2 строки, ~2 минуты

---

### Шаг 8: MER_Descriptor.cpp — убрать noexcept с конструктора

**Цель:** устранить C4297 на строке 173

**Файлы:**
- `HydroSolver/Descriptors/MER_Descriptor.h` (декларация, строка 78)
- `HydroSolver/Descriptors/MER_Descriptor.cpp` (реализация, строка 120)

**Контекст:**
Конструктор `MER_Data::MER_Data(const WellName&, const SingleWell_MER_Data&) noexcept` содержит `throw std::exception(...)` на строке 173. C4297 — «функция помечена noexcept, но может бросить исключение». При реальном throw из noexcept → `std::terminate()` — UB.

**Что сделать:**
1. `MER_Descriptor.h:78` — убрать `noexcept` из декларации
2. `MER_Descriptor.cpp:121` — убрать `noexcept` из определения

**Проверка после этого шага:**
- Сборка + тесты Release
- Warning C4297 в MER_Descriptor.cpp: 0

**Подводные камни:**
- Убирание `noexcept` — не ломает ABI (internal code, нет DLL boundaries)
- `noexcept` на default-конструкторе (`MER_Data() noexcept = default`, строка 76) — оставить, там throw невозможен

**Зависимости:** нет
**Оценка:** ~2 строки, ~2 минуты

---

### Шаг 9: SomeWell.cpp — убрать noexcept с конструкторов

**Цель:** устранить C4297 на строке 136

**Файлы:**
- `HydroSolver/Reservoir/Well/SomeWell.h` (декларация)
- `HydroSolver/Reservoir/Well/SomeWell.cpp` (реализация)

**Контекст:**
Конструктор `SomeWell::SomeWell(...) noexcept` содержит `throw std::runtime_error(...)` на строке 136. Аналогично, `WellEnvironment::WellEnvironment(...) noexcept` — проверить, бросает ли (нет throw в текущем коде, но noexcept на WellEnvironment не нужен, если дочерний SomeWell бросает).

**Что сделать:**
1. `SomeWell.h:168` — убрать `noexcept` из `SomeWell(...)` (многопараметрический конструктор)
2. `SomeWell.cpp:110` — убрать `noexcept` из определения
3. `SomeWell.h:30` — убрать `noexcept` из `WellEnvironment::WellEnvironment(...)` (вызывается из SomeWell, который бросает)
4. `SomeWell.cpp:12` — убрать `noexcept` из определения WellEnvironment
5. Default-конструкторы (`SomeWell() noexcept = default`, `WellEnvironment() noexcept = default`) — оставить

**Проверка после этого шага:**
- Сборка + тесты Release
- Warning C4297 в SomeWell.cpp: 0

**Подводные камни:**
- WellEnvironment конструктор вызывается из SomeWell — если SomeWell бросает в теле, а WellEnvironment noexcept, проблема только в SomeWell. Но убрать noexcept с WellEnvironment тоже корректно, потому что он вызывает `push_back` (может throw `bad_alloc`)

**Зависимости:** нет
**Оценка:** ~4 строки, ~3 минуты

---

### Шаг 10: test_streamlines.cpp — size_t → int в Catch2 контексте

**Цель:** устранить C4267 (18 мест) в test_streamlines.cpp

**Файлы:** `tests/test_streamlines.cpp`

**Контекст:**
Warnings возникают при передаче `size_t` значений в Catch2 макросы (`REQUIRE`, `INFO`) и в конструкторы, принимающие `int`. Нужно определить точные места warnings и исправить.

Два сценария:
1. `portrait[i].size()` в `REQUIRE(portrait[i].size() >= 2)` — comparison `size_t` с `int` literal → cast literal: `REQUIRE(portrait[i].size() >= 2u)` или `static_cast<size_t>(2)`
2. Передача `size_t` в API, принимающие `int` → `static_cast<int>(...)`

**Что сделать:**
1. Проанализировать каждый warning site (строки 70, 72, 73, 145, 151, 153, 154, 160, 161, 218, 224, 226, 227, 234, 235)
2. Для каждого — определить: это Catch2 macro issue (internal comparison) или API mismatch
3. Применить минимальный фикс: `static_cast` или unsigned literal

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в test_streamlines.cpp: 0

**Зависимости:** нет
**Оценка:** ~18 строк, ~10 минут

---

### Шаг 11: test_JacobianAssembly.cpp

**Цель:** устранить C4267 на строке 34

**Файлы:** `tests/unit/math/test_JacobianAssembly.cpp`

**Контекст:**
```cpp
int nNeib = static_cast<int>(neighbours.size());
```
`neighbours` — `vector<ProcessCell*>`, `.size()` — `size_t`. Warning: `size_t → int`. Но `nNeib` используется далее в цикле `for (int ni = 0; ni < nNeib; ni++)`.

**Что сделать:**
1. Строка ~34: `const auto& cell = grid[l]` → `const auto& cell = grid[static_cast<int>(l)]` — warning C4267 (size_t→int через operator[](int))
2. Строка ~37: `int nNeib = static_cast<int>(neighbours.size())` → `size_t nNeib = neighbours.size()` — удалить лишний static_cast
3. Строка ~54: `for (int ni = 0; ni < nNeib; ni++)` → `for (size_t ni = 0; ni < nNeib; ni++)`
4. Проверить, что `ni` не используется с signed arithmetic

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в test_JacobianAssembly.cpp: 0

**Зависимости:** нет
**Оценка:** ~3 строки, ~3 минуты

---

### Шаг 12: AbstractGrid.h — циклы в AcceptState, ReverseState, UpdateState (превентивный)

**Цель:** устранить потенциальные C4018/C4267 в циклах по `ActiveCellsNmbr()` в AbstractGrid.h. При /W3 warnings не генерируются (signed/unsigned comparison в inline методах), но код содержит `int < size_t` — потенциальная проблема при /W4 или другом компиляторе

**Файлы:** `HydroSolver/Solver/Grids/AbstractGrid.h`

**Контекст:**
```cpp
for (int l = 0; l < ActiveCellsNmbr(); l++)
    Cells[l].AcceptState();
```
`ActiveCellsNmbr()` возвращает `size_t`, `l` — `int`. Предупреждение C4018 (signed/unsigned comparison). `Cells[l]` — std::vector subscript с `int` → ok (неявное расширение `int → size_t`).

**Что сделать:**
1. Строка ~41: `for (int l = 0; l < ActiveCellsNmbr(); l++)` → `for (size_t l = 0; l < ActiveCellsNmbr(); l++)` в `AcceptState()`
2. Строка ~49: аналогично в `ReverseState()`
3. Строка ~57: аналогично в `UpdateState()`
4. Строка ~64: `for (int neighbourIdx = 0; neighbourIdx < connectivityGraph[l].size(); neighbourIdx++)` в `GetNeighboursPointer(int l)` → `for (size_t neighbourIdx = 0; ...)`
5. Строка ~139: `for (int l = 0, l0 = -1; l < totalCellNmbr; l++)` → `for (size_t l = 0; l < totalCellNmbr; l++)` в конструкторе `SomeGrid(const std::vector<bool>&)`. Переменная `l0` объявлена, но не используется (C4101) — удалить из декларации

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4018/C4267/C4101 в AbstractGrid.h: 0

**Подводные камни:**
- `Cells[l]` — vector subscript, `size_t l` — совместим
- `l * eqNmbr` в `UpdateState` (строка 58): `size_t * int` → `size_t`, ok
- Строка 139: `l0 = -1` не может быть `size_t`, но `l0` не используется — просто удалить. `l` используется как subscript (`active_cells[l]`, `cell_idx_Global2Local[l]`) и передаётся в `cell_idx_Local2Global.push_back(l)` (вектор `size_t`) — корректно
- Строка 285: `for (int l = 0; l < activeCellsNmbr; l++)` — `activeCellsNmbr` = `int`, нет warning (внутри `#ifdef GDM_DUMP_DEBUG`)

**Зависимости:** нет
**Оценка:** ~5 строк, ~5 минут

---

### Шаг 12b: JacobianAssembler.cpp — fillMatrixBlockRow (превентивный)

**Цель:** устранить потенциальный C4267 на строке 85. При /W3 warning не генерируется, но код содержит `int < size_t` — потенциальная проблема

**Файлы:** `HydroSolver/Reservoir/JacobianAssembler.cpp`

**Контекст:**
После DEBT-054 `fillMatrixBlockRow` перенесён из ReservoirSimulator в JacobianAssembler. Строка 85:
```cpp
for (int neibCount = 0; neibCount < neighbourCells.size(); neibCount++)
```
`neighbourCells` — `vector<TwoPhaseFlowCell*>`, `.size()` → `size_t`.

**NB:** `accountForBoundaryConditions` в том же файле (~строка 146) уже использует `size_t` в циклах — исправлять не нужно. Строка 33 `for (int l = 0; l < static_cast<int>(grid.ActiveCellsNmbr()); l++)` — explicit cast, warning отсутствует.

**Что сделать:**
1. Строка ~85: `for (int neibCount = 0; neibCount < neighbourCells.size(); neibCount++)` → `for (size_t neibCount = 0; neibCount < neighbourCells.size(); neibCount++)`
2. Строка ~137: `problem.AddOffDiagBlock(l, neibCount, blOffDiag)` → `problem.AddOffDiagBlock(l, static_cast<int>(neibCount), blOffDiag)` — `AddOffDiagBlock` принимает `int neibIdx` (DEBT-028), передача `size_t` создаст новый C4267

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в JacobianAssembler.cpp: 0

**Подводные камни:**
- `static_cast<int>(neibCount)` безопасен: количество соседей ≤ 6 (структурированная сетка). Полное исправление `int neibIdx → size_t` — DEBT-028

**Зависимости:** нет
**Оценка:** ~1 строка, ~3 минуты

---

### Шаг 13: FluxReader.h — циклы чтения данных (превентивный)

**Цель:** устранить потенциальный C4267 в FluxReader.h. При /W3 warning не генерируется (строка 67: `int < size_t` в inline method), но код содержит `int < size_t`

**Файлы:** `HydroSolver/Reservoir/FluxReader.h`

**Контекст:**
```cpp
for (int t = 0; t < data.size(); t++)       // data.size() → size_t — WARNING
    for (int k = 0; k < nz; k++)            // nz — int параметр, нет warning
        for (int j = 0; j < ny; j++)         // ny — int параметр, нет warning
            for (int i = 0; i < nx; i++)     // nx — int параметр, нет warning
```
Только `data.size()` → `size_t`. Параметры `nx/ny/nz` — `int` (сигнатура `readCube(int nx, int ny, int nz, ...)`), поэтому внутренние циклы НЕ генерируют warnings.

**Что сделать:**
1. `for (int t = 0; t < data.size(); t++)` → `for (size_t t = 0; t < data.size(); t++)`
2. Внутренние циклы `k/j/i` — оставить `int` (граница `int`, warning отсутствует)

**Проверка после этого шага:**
- Сборка + тесты Release
- Warnings C4267 в FluxReader.h: 0

**Зависимости:** нет
**Оценка:** ~1 строка, ~1 минута

---

### Шаг 14: ReservoirSimulator.cpp — GetFlowFieldsPtr, GetWaterSaturationField, GetPressureField, SaveFlowField (превентивный)

**Цель:** устранить потенциальные C4267 в циклах ~168, ~193, ~202, ~441, ~489, ~523. При /W3 warnings не генерируются (signed/unsigned comparison), но код содержит `int < size_t`

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
Строка 168: `for (int k = 0; k < nz(); k++)` — `nz()` возвращает `size_t`.
Строки 193, 202: `for (int l = 0; l < Grid.TotalCellsNmbr(); l++)` — `TotalCellsNmbr()` возвращает `size_t`.
Строка 441: `for (int k = 0, l = 0; k < Grid.Nz(); k++)` — `Grid.Nz()` → `size_t`.
Строки 489, 523: `for (int k = 0; k < flowFields.size(); k++)` — `.size()` → `size_t`.

**Что сделать:**
1. Строка ~168: `int k` → `size_t k`
2. Строки ~193, ~202: `int l` → `size_t l`. `Grid.ConvertGlobal2Local(l)` принимает `size_t` — ok. Но `Grid[Grid.ConvertGlobal2Local(l)]` — возвращает `long int`, а `operator[]` принимает `int` → но `long int → int` не генерирует C4267 (оба signed). Если grid subscript выдаёт warning — добавить `static_cast<int>(...)`
3. Строка ~441: `for (int k = 0, l = 0; k < Grid.Nz(); k++)` → `for (size_t k = 0; k < Grid.Nz(); k++)`. Переменная `l` объявлена, но не используется — удалить
4. Строки ~489, ~523: `int k` → `size_t k`

**NB:** OilContourFlux (~881) и WaterContourFlux (~1011) уже исправлены в шаге 1 (пункты 5–6), дублировать не нужно.

**Проверка после этого шага:**
- Сборка + тесты Release
- Все warnings C4267/C4101 в RS.cpp: 0 (вместе с шагами 1–4)

**Зависимости:** после шагов 1–4
**Оценка:** ~8 строк, ~5 минут

---

### Шаг 15: NumericalParameters.h — CurrentAMG_maxSolverIterationCount()

**Цель:** исправить подозрительное приведение типов в геттере

**Файлы:** `HydroSolver/Reservoir/NumericalParameters.h`

**Контекст:**
```cpp
size_t CurrentAMG_maxSolverIterationCount() const { return (int)round(AMG_maxSolverIterCount); }
```
`AMG_maxSolverIterCount` — `double`, `round()` → `double`, `(int)` → truncation, `return size_t` → implicit widening. Промежуточное `(int)` — бессмысленно и потенциально опасно (если double > INT_MAX).

**Что сделать:**
1. `return (int)round(AMG_maxSolverIterCount)` → `return static_cast<size_t>(round(AMG_maxSolverIterCount))`

**Проверка после этого шага:**
- Сборка + тесты Release

**Зависимости:** до шага 5 (Solve) — порядок не важен, но лучше после
**Оценка:** ~1 строка, ~1 минута

---

## Этап 3: Верификация

### Шаг 16: Финальная сборка и тесты (Release + Debug)

**Цель:** убедиться что 0 warnings из кода проекта, все тесты зелёные

**Что сделать:**
1. `cmake --build build --config Release` — проверить вывод на warnings. Из кода проекта — 0. Amgcl/STL — допустимы
2. `ctest --test-dir build -C Release --output-on-failure` — 310 pass
3. `cmake --build build --config Debug` — проверить warnings
4. `ctest --test-dir build -C Debug --output-on-failure` — 310 pass
5. Сравнить с baseline: тесты pass, время, разница Debug vs Release

**Критерии:**
- 0 warnings из кода проекта (оба конфига)
- 310/310 тестов (оба конфига)

---

## Тестовая стратегия

Новых тестов не требуется — это чисто механический рефакторинг типов. Все изменения — замена `int → size_t` в переменных цикла и параметрах, что не меняет поведение (все значения ≥ 0 и < INT_MAX).

**Regression:** все 310 тестов — regression suite. Ключевые:
- `test_mass_balance` — затрагивает OverallFluxes, OilContourFlux, WaterContourFlux через ReservoirSimulator::Solve
- `test_five_spot` — полный цикл симуляции с Newton solver и AMG
- `test_buckley_leverett` — OilTotal/WaterTotal (шаг 3)
- `test_streamlines` — PhasePortrait (шаг 10)
- `test_JacobianAssembly` — JacobianAssembler (шаг 11)
- `test_amgcl_benchmark` — LinearProblem::Solve (шаг 5)

## Критерии завершения

- [ ] Все шаги 1–15 выполнены
- [ ] Release: 310/310, 0 warnings C4267/C4297/C4101 из кода проекта (все targets)
- [ ] Debug: 310/310, 0 warnings C4267/C4297/C4101 из кода проекта (все targets)
- [ ] Поведение не изменилось (regression tests)
- [ ] Vault обновлён: DEBT-021 → ✅ исправлено
- [ ] GitHub issue #31 прокомментирован
- [ ] `vault/GDM/00-home/текущие приоритеты.md` обновлён

## Обнаруженные проблемы

Связанные DEBT-022, DEBT-023, DEBT-028 уже зарегистрированы.

Новые проблемы (обнаружены при аудите раунда 3):
- **MathRoutines.cpp(56,62,64):** C4244 (double/float/__int64→int). Строка 62: `return NAN` в функции с int return type — UB. Зарегистрировать как новый BUG/DEBT
- **SomeFlowField.cpp(90):** C4477 — `snprintf %u` с `size_t` аргументом. Зарегистрировать как DEBT
- **SomeFlowField.cpp(103):** C4267 — `int n = field.size()`. Можно включить в DEBT-021, но не связан с «int loop variable vs size_t bound» — отдельная проблема

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай issue [#31](https://github.com/ArturSalamatin/GDM/issues/31) — таблица warnings с файлами и строками
3. Создай ветку: `git checkout -b refactor/debt-021/int-size-t-warnings experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни: 310 тестов, ~123s Release — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты
