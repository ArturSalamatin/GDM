---
tags:
  - план
  - баг
date: 2026-07-01
issue: BUG-009
github: 8
branch: fix/bug-009/active-cells-filter
status: готов к реализации
audit:
  date: 2026-07-01
  pass: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# BUG-009: Фильтр ActiveCells не работает — `size_t + 1 > 0` всегда true

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[code-review-2026-06-28-баги-и-корректность]] — CR-BUG-002 (описание бага)
   - [[стратегия тестирования GDM]] — паттерны тестов
3. Создай ветку: `git checkout -b fix/bug-009/active-cells-filter`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline):
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
6. Baseline: **282 теста, 60 сек, 100% pass**
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание проблемы

### Что происходит

В `ReservoirSimulator::AddWell_FixedProduction()` при привязке скважины к сетке строится вектор `ActiveCells`, чтобы отфильтровать перфорации, попавшие в неактивные ячейки. Фильтр **не работает**: все перфорации всегда считаются активными.

### Цепочка причинно-следственных связей

1. **Триггер:** вертикальная скважина проходит через неактивную ячейку (непроницаемый барьер, pinch-out)
2. **Корневая причина (BUG-011):** `ConvertTriple2Local()` в `AbstractGrid.h:182` объявлен с возвращаемым типом `size_t`. Внутри он вызывает `ConvertGlobal2Local()`, который возвращает `long int` — для неактивных ячеек возвращает отрицательное число (`-inActiveCellsNmbr`). Неявное приведение `long int → size_t` превращает `-1` в `SIZE_MAX` (~2⁶⁴)
3. **Проявление (BUG-009):** в `ReservoirSimulator.cpp:142` проверка `cellIdx+1 > 0`, где `cellIdx` имеет тип `size_t` (auto от `ConvertTriple2Local`). Для unsigned выражение `x + 1 > 0` **тождественно истинно** (unsigned ≥ 0 всегда)
4. **Побочный эффект (UB):** строка 141 `cells_[l] = &(Grid[well_local_position[l]])` выполняется ДО проверки фильтра. `well_local_position[l]` = `SIZE_MAX` → implicit conversion `size_t → int` → implementation-defined → доступ за пределы массива `Cells[]`
5. **Результат:** `ActiveCells = {true, true, ..., true}` → `RemovePerfsAtInactiveCells()` ничего не удаляет → скважина имеет перфорации в несуществующих ячейках → мусорные данные в productions/assembly

### Целевое состояние

После фикса:
- `ConvertTriple2Local()` возвращает `long int` (согласовано с `ConvertGlobal2Local()`)
- В `AddWell_FixedProduction()` для каждого слоя проверяется `cellIdx >= 0` ПЕРЕД доступом к `Grid[]`
- Неактивные ячейки корректно фильтруются: `ActiveCells[l] = false` для ячеек с отрицательным индексом
- `well_local_position`, `cells_` содержат только валидные (активные) индексы
- `operator()(i,j,k)` в `AbstractGrid.h:291-295` корректно обрабатывает неактивные ячейки

---

## Затронутые файлы

| Файл | Строки | Роль | Изменение |
|---|---|---|---|
| `HydroSolver/Solver/Grids/AbstractGrid.h` | 182-185 | `ConvertTriple2Local()` — возвращает `size_t` вместо `long int` | Изменить тип возврата |
| `HydroSolver/Solver/Grids/AbstractGrid.h` | 291-295 | `operator()(i,j,k)` — вызывает `ConvertTriple2Local`, присваивает в `size_t` | Адаптировать к `long int` |
| `HydroSolver/Solver/Grids/AbstractGrid.h` | 130-132 | `SomeGrid(active_cells)` — конструктор с UB (пустой `cell_idx_Global2Local`) | Фикс: `active_cells.size()` |
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 132-149 | `AddWell_FixedProduction()` — цикл привязки перфораций | Переписать: guard + фильтр |
| `tests/test_helpers.h` | 108-110 | `add_simple_well()` — `WellJobs` с 1 слоем | Создавать `Nz` слоёв |

Файлы, которые **не изменяются** (зона неприкосновенности):
- `HydroSolver/Solver/Grids/AbstractGrid.h:104-107` — `ConvertGlobal2Local()` (уже корректен, `long int`)
- `HydroSolver/Reservoir/Well/WellJobs.cpp:110-118` — `RemovePerfsAtInactiveCells()` (корректен, принимает `vector<bool>`)
- `HydroSolver/Reservoir/Well/Wells.cpp` — конструктор `WellFixedProduction` (принимает `vector<size_t>`, не меняется)
- `HydroSolver/Reservoir/Well/SomeWell.h:35` — `ItsLocalIDs` тип `vector<size_t>` (не меняется — в него попадут только валидные size_t >= 0)

---

## Варианты решения

### Вариант A: Минимальный — исправить только проверку в ReservoirSimulator

В `AddWell_FixedProduction()` вызвать `ConvertGlobal2Local()` напрямую (обойдя `ConvertTriple2Local`):
```cpp
long int cellIdxSigned = Grid.ConvertGlobal2Local(
    Grid.Nx() * Grid.Ny() * ItsGlobalIDs[l][2] +
    Grid.Nx() * ItsGlobalIDs[l][1] + ItsGlobalIDs[l][0]);
ActiveCells.push_back(cellIdxSigned >= 0);
```

**Плюсы:** 1 файл, ~5 строк.
**Минусы:** не исправляет BUG-011 (тип `ConvertTriple2Local`). Out-of-bounds на строке 141 по-прежнему возможен. `operator()(i,j,k)` по-прежнему сломан для неактивных ячеек. Оставляет ловушку для будущего использования `ConvertTriple2Local`.

### Вариант B: Исправить `ConvertTriple2Local` + цикл в `AddWell` (рекомендуемый)

1. `ConvertTriple2Local` → `long int` (2 строки в `AbstractGrid.h`)
2. `operator()(i,j,k)` → `long int idx` (2 строки в `AbstractGrid.h`)
3. Переписать цикл в `AddWell_FixedProduction`: проверять `cellIdx >= 0` перед `Grid[]` (10 строк)

**Плюсы:** исправляет BUG-009 и BUG-011. Согласовывает типы в иерархии Grid. Устраняет UB.
**Минусы:** 2 файла, ~15 строк. Нужно проверить 2 точки вызова `ConvertTriple2Local` + 1 вызов `operator()`.

### Вариант C: `std::optional<size_t>` для `ConvertTriple2Local`

**Плюсы:** type-safe, compiler-enforced.
**Минусы:** меняет API сильнее, `optional` в hot path (хотя `ConvertTriple2Local` не в hot path). Нестандартно для существующего кодекса, где negative index — устоявшийся паттерн.

### Выбор: Вариант B

`long int` уже используется в `ConvertGlobal2Local()` и в `operator[](int idx)` — это устоявшийся паттерн: отрицательный индекс = неактивная ячейка. Вариант B восстанавливает согласованность типов без изменения API-дизайна.

---

## Подводные камни

- [✅] **Побочные эффекты:** `ConvertTriple2Local` вызывается в 2 местах: `ReservoirSimulator.cpp:139` и `AbstractGrid.h:293`. Оба адресованы в шагах 1 и 2.
- [✅] **Потокобезопасность:** `AddWell_FixedProduction` вызывается при инициализации, до `#pragma omp parallel`. Безопасно.
- [✅] **Граничные случаи:** пустая скважина (`NmbrOfOpenedCells() = 0`) — вектор пуст, цикл не выполняется, `denom = 0` → срабатывает guard из BUG-008.
- [✅] **Производительность:** `AddWell` — O(Nz), вызывается 1 раз при инициализации. Не в hot path.
- [✅] **Обратная совместимость:** при всех `active_cells[l] = true` (текущие тесты) поведение не изменится — `ConvertTriple2Local` для активных ячеек возвращает неотрицательное число, которое одинаково для `size_t` и `long int`.
- [✅] **Порядок вызовов:** `ConvertTriple2Local` → `ConvertGlobal2Local` → `cell_idx_Global2Local[]`. Порядок инициализации не меняется.
- [✅] **Состояние при ошибке:** при неактивной ячейке `cellIdx < 0` → строки `well_local_position[l]` и `cells_[l]` не заполняются мусором (фильтруются до доступа).
- [✅] **Численная устойчивость:** типы — целочисленные, overflow невозможен для реальных размеров сетки (< 10⁹ ячеек).
- [⚠️] **Связь с BUG-011:** этот фикс закрывает и BUG-011 (шаг 1). Нужно обновить roadmap — адресовано в шаге 5.
- [✅] **Зависимости сборки:** только изменения в `.h` и `.cpp`, без CMake-изменений.

---

## Этапы

### Этап 1: Фикс типа `ConvertTriple2Local` (BUG-011) + фильтра ActiveCells (BUG-009) + конструктора `SomeGrid`
Шаги 1–2, 2.5.

### Этап 2: Тесты
Шаг 3.

### Этап 3: Верификация и vault
Шаги 4–5.

---

## Шаг 1: Изменить тип возврата `ConvertTriple2Local` на `long int`

**Цель:** устранить корневую причину — неявное приведение `-1 → SIZE_MAX` при возврате из `ConvertTriple2Local`.

**Файлы:** `HydroSolver/Solver/Grids/AbstractGrid.h`

**Контекст:**
`SomeGrid::ConvertGlobal2Local(size_t)` (строка 104) возвращает `long int` — для неактивных ячеек возвращает отрицательное число (формат: `-inActiveCellsNmbr`, где `inActiveCellsNmbr` ≥ 1). Массив `cell_idx_Global2Local` имеет тип `vector<long int>`. Это устоявшийся паттерн: `operator[](int idx)` (строка 78) обрабатывает отрицательные индексы корректно — обращается к `CellsInactive[-(idx+1)]`.

`ConvertTriple2Local` (строка 182) — обёртка над `ConvertGlobal2Local`, но объявлен как `size_t`. Это обрезает знак.

`operator()(size_t i, size_t j, size_t k)` (строка 291) вызывает `ConvertTriple2Local` и присваивает результат в `size_t idx`. Затем вызывает `operator[](idx)` — но `operator[]` принимает `int`, и при `SIZE_MAX → int` — implementation-defined.

**Что сделать:**

1. В файле `HydroSolver/Solver/Grids/AbstractGrid.h`, строка 182:
   изменить тип возврата `ConvertTriple2Local` с `size_t` на `long int`

2. В файле `HydroSolver/Solver/Grids/AbstractGrid.h`, строка 293:
   изменить тип переменной `idx` в `operator()` с `size_t` на `long int`

**Изменения (старый → новый код):**

Место 1 — `ConvertTriple2Local` (строка 182):

До:
```cpp
size_t ConvertTriple2Local(const std::vector<size_t>& idx) const
{
    return ConvertGlobal2Local(Nx() * Ny() * idx[2] + Nx() * idx[1] + idx[0]);
}
```

После:
```cpp
long int ConvertTriple2Local(const std::vector<size_t>& idx) const
{
    return ConvertGlobal2Local(Nx() * Ny() * idx[2] + Nx() * idx[1] + idx[0]);
}
```

Место 2 — `operator()` (строка 291):

До:
```cpp
const ProcessCell& operator() (size_t i, size_t j, size_t k) const
{
    size_t idx = ConvertTriple2Local(std::vector<size_t>{i, j, k});
    return (*this)[idx];
}
```

После:
```cpp
const ProcessCell& operator() (size_t i, size_t j, size_t k) const
{
    long int idx = ConvertTriple2Local(std::vector<size_t>{i, j, k});
    return (*this)[static_cast<int>(idx)];
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: **282 теста pass**. Поведение не изменяется, т.к. во всех текущих тестах все ячейки активны → `ConvertGlobal2Local` возвращает неотрицательное число → `long int` ≡ `size_t` для положительных значений.
- `static_cast<int>(idx)` необходим: `operator[]` принимает `int`, `idx` теперь `long int`. CLAUDE.md: warnings = ошибки → ставим cast проактивно.

**Подводные камни:**
- `operator()` вызывается в `ReservoirSimulator.cpp:322,325` для чтения координат: `Grid(0, j, 0).X()` и `Grid(i, 0, 0).X()`. Эти ячейки всегда активны (угловые/боковые), так что `idx >= 0` гарантировано.

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~4 строки, ~5 минут

---

## Шаг 2: Переписать цикл привязки перфораций в `AddWell_FixedProduction`

**Цель:** исправить фильтр `ActiveCells` — проверять `cellIdx >= 0` перед доступом к `Grid[]`, чтобы неактивные ячейки не вызывали out-of-bounds.

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
`AddWell_FixedProduction()` (строка 114) создаёт вертикальную скважину. Цикл (строки 137-143) проходит по слоям (k=0..Nz-1), для каждого вычисляет локальный индекс ячейки через `ConvertTriple2Local`, заполняет `well_local_position[l]`, `cells_[l]` и `ActiveCells[l]`.

Текущая проблема:
- Строка 140: `well_local_position[l] = cellIdx` — если `cellIdx < 0` (неактивная), это `size_t = negative` → UB
- Строка 141: `cells_[l] = &(Grid[well_local_position[l]])` — out-of-bounds ДО проверки фильтра
- Строка 142: `ActiveCells.push_back(cellIdx+1 > 0)` — после шага 1 `cellIdx` имеет тип `long int`, поэтому проверка будет работать. Но строки 140-141 всё ещё присваивают мусор

Решение: проверять `cellIdx >= 0` ПЕРЕД строками 140-141. Для неактивных ячеек — пропускать доступ к Grid, устанавливать placeholder значения, `ActiveCells = false`.

Важно: `well_local_position` и `cells_` передаются в конструктор `WellFixedProduction` и сохраняются как `ItsLocalIDs` и в `WellTrajectory`. Для неактивных ячеек в этих массивах будут placeholder-значения (0 и nullptr), но это допустимо — `RemovePerfsAtInactiveCells()` очистит jobs для этих слоёв (ActiveCells[l] = false), и они никогда не будут использоваться в assembly.

**Что сделать:**

1. В файле `HydroSolver/Reservoir/ReservoirSimulator.cpp`, цикл строки 137-143:
   - Заменить `auto cellIdx = ...` на `long int cellIdx = ...` (явный тип после шага 1)
   - Добавить проверку `cellIdx >= 0` перед заполнением `well_local_position[l]` и `cells_[l]`
   - Для неактивных ячеек: `well_local_position[l] = 0`, `cells_[l] = &(Grid[0])` (placeholder), `ActiveCells = false`
   - Для активных: текущая логика без изменений

**Изменения (старый → новый код):**

До (строки 137-143):
```cpp
for (size_t l = 0; l < well_local_position.size(); ++l)
{
    auto cellIdx = Grid.ConvertTriple2Local(ItsGlobalIDs[l]);
    well_local_position[l] = cellIdx;
    cells_[l] = &(Grid[well_local_position[l]]);
    ActiveCells.push_back(cellIdx+1 > 0);
}
```

После:
```cpp
for (size_t l = 0; l < well_local_position.size(); ++l)
{
    long int cellIdx = Grid.ConvertTriple2Local(ItsGlobalIDs[l]);
    bool isActive = cellIdx >= 0;
    ActiveCells.push_back(isActive);
    if (isActive) {
        well_local_position[l] = static_cast<size_t>(cellIdx);
        cells_[l] = &(Grid[static_cast<int>(cellIdx)]);
    } else {
        well_local_position[l] = 0;
        cells_[l] = &(Grid[0]);
    }
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: **282 теста pass**. Все текущие тесты используют `active_cells.assign(N, true)` → `cellIdx >= 0` для всех ячеек → ветка `else` не выполняется → поведение идентично старому.

**Подводные камни:**
- Placeholder `well_local_position[l] = 0` и `cells_[l] = &(Grid[0])` для неактивных ячеек: эти значения **не должны использоваться** — `RemovePerfsAtInactiveCells(ActiveCells)` очистит jobs для неактивных слоёв, и `AccumulatePerforations` создаст `PerforationsOfWell` без этих слоёв. Затем `SomeWell::SetWellState()` строит `ItsCurCellIDs` и `ItsCurLocalIDs` только для открытых перфораций — неактивные слои исключены.
- Если все ячейки неактивны: `ActiveCells = {false,...,false}` → `RemovePerfsAtInactiveCells` очистит все jobs → скважина без перфораций → `NmbrOfOpenedCells() = 0` → `SetRefWellPressure()` → `denom = 0` → guard из BUG-008 сработает → `P_well = avg(P_reservoir)`.

**Зависимости:**
- Требует: шаг 1 (тип `ConvertTriple2Local` должен быть `long int`)
- Блокирует: шаг 3

**Оценка:** ~10 строк, ~10 минут

---

## Шаг 2.5: Исправить конструктор `SomeGrid` — `cell_idx_Global2Local` создаётся пустым

**Цель:** устранить UB в конструкторе `SomeGrid` — вектор `cell_idx_Global2Local` инициализируется размером `totalCellNmbr`, но `totalCellNmbr = 0` в момент member-initializer (default `= 0`). Цикл в теле конструктора обращается к `cell_idx_Global2Local[l]` на пустом векторе — out-of-bounds.

**Файлы:** `HydroSolver/Solver/Grids/AbstractGrid.h`

**Контекст:**
`SomeGrid(const vector<bool>& active_cells)` (строка 130) — конструктор, вызываемый через цепочку `ReservoirSimulator` → `OilField(horizon)` → `SomeStructuredGrid3Dim(...)` → `SomeGrid(active_cells)`.

Порядок инициализации членов определяется порядком **декларации**, не порядком в member-initializer list:
- `cell_idx_Global2Local` объявлен на строке 27 — инициализируется **первым**
- `totalCellNmbr` объявлен на строке 32 — инициализируется **позже**

В member-initializer list: `cell_idx_Global2Local{ std::vector<long int>(totalCellNmbr, -1) }` → `totalCellNmbr = 0` (default) → вектор размера 0. В теле: `totalCellNmbr = active_cells.size()` → цикл `cell_idx_Global2Local[l] = ...` → **UB** (запись в пустой вектор).

Текущие тесты не crash-ят (MSVC Release не проверяет bounds), но это latent UB, который crash-ит в Debug или при другом memory layout.

Решение: использовать `active_cells.size()` вместо `totalCellNmbr` в member-initializer list — параметр конструктора доступен в initializer list.

**Что сделать:**

В файле `HydroSolver/Solver/Grids/AbstractGrid.h`, строка 132:
заменить `totalCellNmbr` на `active_cells.size()` в аргументе `cell_idx_Global2Local`.

**Изменения (старый → новый код):**

До (строки 130-133):
```cpp
SomeGrid(const std::vector<bool>& active_cells)
    :Cells{}, CellsInactive{}, IsCellActive{ active_cells },
    cell_idx_Global2Local{ std::vector<long int>(totalCellNmbr, -1) }
{
```

После:
```cpp
SomeGrid(const std::vector<bool>& active_cells)
    :Cells{}, CellsInactive{}, IsCellActive{ active_cells },
    cell_idx_Global2Local{ std::vector<long int>(active_cells.size(), -1) }
{
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: **282 теста pass**. Поведение не изменяется — ранее UB случайно «работал» на MSVC Release.

**Подводные камни:**
- Это исправление делает `cell_idx_Global2Local` вектором правильного размера (N элементов, все `-1`) уже в member-initializer list. Присваивание `totalCellNmbr = active_cells.size()` в теле конструктора (строка 134) по-прежнему нужно — `totalCellNmbr` используется в цикле и в `TotalCellsNmbr()`.

**Зависимости:**
- Требует: ничего (независим от шагов 1-2, но логически связан)
- Блокирует: шаг 3 (тесты с неактивными ячейками crash-нут без этого фикса)

**Оценка:** ~1 строка, ~5 минут

---

## Шаг 2.7: Исправить `add_simple_well` — `WellJobs` с `Nz` слоями

**Цель:** `add_simple_well` создаёт `WellJobs` с единственным слоем (`RawWellPerforationData.size() = 1`), но скважина проходит через `Nz` слоёв. `RemovePerfsAtInactiveCells(ActiveCells)` итерирует `ActiveCells.size() = Nz` и обращается к `RawWellPerforationData[i]` для `i = 0..Nz-1` — **out-of-bounds при Nz > 1**.

**Файлы:** `tests/test_helpers.h`

**Контекст:**
`add_simple_well()` (строка 73 `test_helpers.h`) создаёт `WellJobs` через конструктор `WellJobs(name, jobs_in_layer)` (строка 70 `WellJobs.h`), который инициализирует `RawWellPerforationData{jobs}` = вектор с **1 элементом** (один «слой»). В production-коде конструктор (строка 50 `WellJobs.cpp`) правильно создаёт `RawWellPerforationData(NmbrOfLayers, ...)`.

Все текущие тесты используют Nz=1, поэтому проблема не проявляется. Но тест BUG-009 (шаг 3) использует Nz=3 → OOB.

Решение: создавать `WellJobsPerLayer` с `Nz` элементами — одинаковая перфорация в каждом слое.

**Что сделать:**

1. В `tests/test_helpers.h`, функция `add_simple_well()`, строки 108-110:
   - Заменить создание единственной `JobsInLayer` на создание `WellJobsPerLayer` с `Nz` слоями
   - Использовать конструктор `WellJobs(name, jobs_per_layer)` (строка 74 `WellJobs.h`)

**Изменения (старый → новый код):**

До (строки 107-110):
```cpp
    // Перфорация: один слой Nz=1, открыта на всю глубину, с t=0.
    reservoir_simulator::JobsInLayer jobs_in_layer;
    jobs_in_layer.emplace_back(0.0, nz * hz, true, 0.0);
    reservoir_simulator::WellJobs well_jobs(name, jobs_in_layer);
```

После:
```cpp
    // Перфорация: один элемент на каждый слой.
    // WellJobs хранит RawWellPerforationData[layerID], RemovePerfsAtInactiveCells
    // итерирует ActiveCells.size() = Nz → нужно Nz элементов.
    reservoir_simulator::JobsInLayer one_layer;
    one_layer.emplace_back(0.0, hz, true, 0.0);
    reservoir_simulator::WellJobsPerLayer jobs_per_layer(
        static_cast<size_t>(nz), one_layer);
    reservoir_simulator::WellJobs well_jobs(name, jobs_per_layer);
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: **282 теста pass**. Для Nz=1 (все текущие тесты) `WellJobsPerLayer` имеет 1 элемент — поведение идентично старому.

**Подводные камни:**
- Высота перфорации: было `nz * hz` (полная глубина), стало `hz` (один слой). Это корректно: каждый слой имеет свою перфорацию высотой `hz`. `AccumulatePerforations()` соберёт все слои.
- `WellJobsPerLayer` = `vector<JobsInLayer>` = `vector<vector<WellJobTime>>`. Конструктор `WellJobs(name, jobs_per_layer)` (строка 74) инициализирует `RawWellPerforationData{jobs_per_layer}` — copy-init, корректно.
- Существующие тесты не сломаются: Nz=1 → 1 элемент в `WellJobsPerLayer`, то же поведение.

**Зависимости:**
- Требует: ничего (независим, но логически нужен перед шагом 3)
- Блокирует: шаг 3

**Оценка:** ~4 строки, ~5 минут

---

## Шаг 3: Тест — перфорация в неактивной ячейке

**Цель:** подтвердить, что неактивные ячейки корректно фильтруются при привязке скважины к сетке.

**Файлы:** `tests/test_inactive_cells.cpp` (новый), `CMakeLists.txt`

**Контекст:**
Текущие тесты используют `active_cells.assign(N, true)` — все ячейки активны. Нужен тест с `active_cells[k] = false` для одного слоя, чтобы скважина проходила через неактивную ячейку.

Уровень теста: **интеграционный** (уровень ~4). Нужен `ReservoirSimulator` с `DevelopedHorizon`, скважина через `add_simple_well` или `AddWell_FixedProduction`.

Helper `make_uniform_horizon()` из `tests/test_helpers.h` создаёт `DevelopedHorizon` с `active_cells.assign(N, true)`. Нужно модифицировать: после вызова `make_uniform_horizon()` установить `h.active_cells[k_layer * Nx * Ny + ...] = false` для нужного слоя.

Проблема: `make_uniform_horizon()` возвращает `DevelopedHorizon`, у которого `active_cells` — public-поле `vector<bool>`. Можно модифицировать после создания.

Тест-дизайн:
- Сетка 3×3×3 (27 ячеек)
- Средний слой (k=1) — неактивен: `active_cells[9..17] = false`
- Вертикальная скважина в центре (x=1.5, y=1.5) — проходит через все 3 слоя
- Ожидание: скважина имеет перфорации только в слоях k=0 и k=2, слой k=1 исключён
- Проверка: симулятор создаётся без crash, можно сделать 1 шаг, массы конечны

**Что сделать:**

1. Создать файл `tests/test_inactive_cells.cpp`
2. Добавить файл в `CMakeLists.txt` (секция `target_sources` для тестов)

**Тест:**

```cpp
#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("Well skips inactive cell layer",
          "[integration][wells][inactive-cells]") {
    size_t Nx = 3, Ny = 3, Nz = 3;
    double Lx = 30.0, Ly = 30.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    // Деактивировать средний слой (k=1)
    for (size_t j = 0; j < Ny; ++j)
        for (size_t i = 0; i < Nx; ++i)
            horizon.active_cells[Nx * Ny * 1 + Nx * j + i] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // Скважина в центре — проходит через все 3 слоя
    // Ожидание: перфорация в слое k=1 отфильтрована
    test_helpers::add_simple_well(sim, horizon, L"PROD", 15.0, 15.0, 1.0, 0.0);

    double dt = 0.1;
    sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    double water = sim.WaterTotal();
    REQUIRE(std::isfinite(oil));
    REQUIRE(std::isfinite(water));
    REQUIRE(oil > 0.0);
}

TEST_CASE("All layers inactive — well has zero production",
          "[integration][wells][inactive-cells]") {
    size_t Nx = 3, Ny = 3, Nz = 1;
    double Lx = 30.0, Ly = 30.0, hz = 10.0;
    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 0.8);

    // Деактивировать ячейку скважины (центральная ячейка k=0)
    // Скважина в (15, 15) → ячейка (1, 1, 0) → global index = 3*1+1 = 4
    horizon.active_cells[4] = false;

    auto numPrm = test_helpers::default_num_params();
    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};

    // Скважина попадает в неактивную ячейку
    test_helpers::add_simple_well(sim, horizon, L"PROD", 15.0, 15.0, 1.0, 0.0);

    // Guard BUG-008: SetRefWellPressure при denom=0 → P_well = avg(P_reservoir)
    double dt = 0.1;
    sim.SingleIteration(dt, dt);

    double oil = sim.OilTotal();
    REQUIRE(std::isfinite(oil));
}
```

**Замечание по тесту:** `horizon.active_cells` модифицируется ПЕРЕД передачей в конструктор `ReservoirSimulator` — это ключевое условие. Все helper-функции в namespace `test_helpers`. Конструктор `ReservoirSimulator` принимает `{numPrm, horizon, oil, water, other}` — **`numPrm` первым**.

**CMakeLists.txt — добавить файл:**

Найти секцию `target_sources(gdm_tests ...)` и добавить `tests/test_inactive_cells.cpp`.

Grep-команда для поиска: `grep -n "test_" CMakeLists.txt | tail -20`

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release -R "inactive-cells" --output-on-failure`
- Ожидаемый результат: **2 новых теста pass**
- Регрессия: `ctest --test-dir build -C Release --output-on-failure`
- Ожидаемый результат: **284 теста pass** (282 + 2 новых)

**Подводные камни:**
- Баг конструктора `SomeGrid` (`cell_idx_Global2Local` пустой) исправлен в шаге 2.5. Без шага 2.5 тест crash-нет при создании `ReservoirSimulator` с `active_cells` содержащим `false`.
- `SetConnectivityGraph_3D()` вызывается в конструкторе `SomeStructuredGrid3Dim` (строка 203). Она строит граф связности, пропуская неактивные ячейки (проверка `active_cells[l]`). С mixed active/inactive это должно работать корректно.

**Зависимости:**
- Требует: шаги 1-2, 2.5, 2.7
- Блокирует: шаг 4

**Оценка:** ~50 строк, ~20 минут

---

## Шаг 4: Верификация

**Цель:** убедиться, что фикс не сломал существующие сценарии.

**Файлы:** нет изменений

**Что сделать:**

1. Прогнать полный набор тестов:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   Ожидаемый результат: **284 теста pass** (282 baseline + 2 новых)

2. Проверить five-spot (если он есть в тестах):
   ```powershell
   ctest --test-dir build -C Release -R "five-spot" --output-on-failure
   ```
   Ожидаемый результат: pass без изменения результатов

3. Проверить multi-layer тесты:
   ```powershell
   ctest --test-dir build -C Release -R "multi-layer\|3D\|completions" --output-on-failure
   ```
   Ожидаемый результат: pass без изменения результатов

**Зависимости:**
- Требует: шаги 1-3
- Блокирует: шаг 5

**Оценка:** ~5 минут (только запуск тестов)

---

## Шаг 5: Обновить vault и GitHub issue

**Цель:** зафиксировать результат в vault и на GitHub.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md`
- `vault/GDM/knowledge/debugging/BUG-009 active cells filter.md` (новый)

**Что сделать:**

1. В `известные баги и технический долг.md`:
   - BUG-009: изменить статус на `✅ исправлено <дата>`
   - BUG-011: изменить статус на `✅ исправлено <дата>` (закрыт вместе с BUG-009), добавить `- **GitHub:** [#8](https://github.com/ArturSalamatin/GDM/issues/8)`

2. Создать заметку `vault/GDM/knowledge/debugging/BUG-009 active cells filter.md`:
   - Причина: `ConvertTriple2Local` возвращал `size_t` вместо `long int` → `cellIdx+1 > 0` тождественно истинно
   - Решение: тип `long int`, guard `cellIdx >= 0` перед доступом к Grid
   - Связь с BUG-011

3. Прокомментировать GitHub issue #8:
   ```powershell
   gh issue comment 8 --repo ArturSalamatin/GDM --body "Исправлено: ConvertTriple2Local → long int + guard cellIdx >= 0 в AddWell. 2 новых теста. BUG-011 закрыт вместе."
   ```

4. Обновить `vault/GDM/00-home/index.md` — добавить ссылку на заметку debugging

**Зависимости:**
- Требует: шаг 4
- Блокирует: ничего

**Оценка:** ~15 минут

---

## Тестовая стратегия

**Тест 1: Well skips inactive cell layer**
- **Тег:** `[integration][wells][inactive-cells]`
- **Файл:** `tests/test_inactive_cells.cpp` (новый)
- **Сценарий:** вертикальная скважина через 3-слойный пласт, средний слой неактивен
- **Setup:** 3×3×3, `active_cells[k=1] = false`, скважина в центре
- **Ожидание:** `SingleIteration` не crash-ит, массы конечны и > 0
- **Предотвращает:** out-of-bounds при перфорации в неактивной ячейке

**Тест 2: All layers inactive — well has zero production**
- **Тег:** `[integration][wells][inactive-cells]`
- **Файл:** `tests/test_inactive_cells.cpp` (новый)
- **Сценарий:** единственная ячейка скважины неактивна
- **Setup:** 3×3×1, `active_cells[4] = false`, скважина в (15,15)
- **Ожидание:** `SingleIteration` не crash-ит, guard BUG-008 срабатывает
- **Предотвращает:** crash при полностью неактивной скважине

**Regression:** все 282 существующих теста без изменений.

---

## Обнаруженные проблемы

### ✅ Подтверждённый баг: конструктор `SomeGrid` — `cell_idx_Global2Local` создаётся пустым (UB)

`AbstractGrid.h:130-132`: `cell_idx_Global2Local{ std::vector<long int>(totalCellNmbr, -1) }` — `totalCellNmbr = 0` (default member init), `cell_idx_Global2Local` объявлен **раньше** `totalCellNmbr` (строка 27 vs 32), поэтому в member-initializer list `totalCellNmbr` ещё 0. Вектор создаётся пустым. Цикл в теле конструктора обращается к `cell_idx_Global2Local[l]` → UB.

**Статус:** адресовано в шаге 2.5. Фикс: `active_cells.size()` вместо `totalCellNmbr` в member-initializer list. Зафиксировать как новый BUG в roadmap при реализации.

---

## Критерии завершения

- [  ] Тест "Well skips inactive cell layer" — зелёный
- [  ] Тест "All layers inactive — well has zero production" — зелёный
- [  ] Все существующие 282 теста — зелёные
- [  ] `ConvertTriple2Local` возвращает `long int`
- [  ] `operator()(i,j,k)` использует `long int idx`
- [  ] Конструктор `SomeGrid` — `cell_idx_Global2Local` инициализируется с `active_cells.size()`
- [  ] В `AddWell_FixedProduction` guard `cellIdx >= 0` стоит ПЕРЕД доступом к `Grid[]`
- [  ] BUG-009 закрыт в vault (статус ✅)
- [  ] BUG-011 закрыт в vault (статус ✅)
- [  ] GitHub issue #8 прокомментирован
- [  ] Заметка в `knowledge/debugging/` создана

---

## Сводка

| Метрика | Значение |
|---|---|
| Файлов изменённых | 3 (`AbstractGrid.h`, `ReservoirSimulator.cpp`, `test_helpers.h`) |
| Файлов новых | 1 (`test_inactive_cells.cpp`) + CMakeLists.txt |
| Строк изменений | ~20 (фикс) + ~55 (тест) |
| Шагов | 7 (1, 2, 2.5, 2.7, 3, 4, 5) |
| Закрывает | BUG-009, BUG-011 + latent UB в конструкторе `SomeGrid` + OOB в `add_simple_well` |
| Baseline | 282 теста, 60 сек |
| Ожидаемый результат | 284 теста |
| Оценка времени | ~65 минут |
