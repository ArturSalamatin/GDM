---
tags:
  - план
  - рефакторинг
date: 2026-07-18
issue: DEBT-060
github: 41
branch: refactor/debt-060/long-int-to-ptrdiff
status: в процессе
audit:
  date: 2026-07-18
  pass: 2
  findings: 0 / 1 / 0
  auto-fixed: 1
  manual-required: 0
---

# DEBT-060: `long int` → `ptrdiff_t` в индексации сетки

## Контекст

Базовые типы индексации в GDM — `size_t` (unsigned) и `ptrdiff_t` (signed). Решение зафиксировано в [[индексация через size_t и ptrdiff_t а не long]].

Исторически `cell_idx_Global2Local` хранит `vector<long int>`, а `ConvertGlobal2Local` / `ConvertTriple2Local` возвращают `long int`. На MSVC x64 (LLP64-модель) `long int` = 32 бит, но это **отдельный тип** от `int` (32 бит) и `ptrdiff_t` (64 бит). Конверсии `long int → ptrdiff_t` и `long int → size_t` имеют одинаковый ранг (integral conversion) → C2593 ambiguity при overload resolution `operator[]`.

В рамках DEBT-058 был установлен workaround — `template<std::signed_integral T>` concept в `operator[]`. Он компилируется, но не решает корневую проблему: `long int` остаётся в кодовой базе и не соответствует принятому стандарту типов.

### Связанные задачи

- **DEBT-058** ([#39](https://github.com/ArturSalamatin/GDM/issues/39)) — static_cast cleanup, concept-workaround
- **DEBT-021** — int → size_t в циклах (завершён)
- **DEBT-022** — activeCellsNmbr int → size_t (завершён)

## Затронутые файлы

| Файл | Что менять |
|---|---|
| `HydroSolver/Solver/Grids/AbstractGrid.h` | Хранение, API, operator[], конструктор |
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 14 переменных `long int` + литерал `Grid[0]` |
| `HydroSolver/Reservoir/JacobianAssembler.cpp` | 3 переменные `long int` |
| `HydroSolver/Reservoir/NewtonSolver.cpp` | `int l` в OMP-цикле с `grid[l]` |
| `tests/unit/math/test_JacobianAssembly.cpp` | 2 переменные `long int` + 2 цикла `int l` |

**Вне scope:** `HydroSolver/Descriptors/MER_Descriptor.h:101` — `long int monthID` (не индекс сетки, другая семантика).

## Выбранный вариант

Вариант B из issue #41: заменить `long int` на `ptrdiff_t` во всей цепочке, `operator[]` — два явных overload (`ptrdiff_t` + `size_t`), без шаблонов.

### Обработка `int` аргументов

При `operator[](ptrdiff_t)` + `operator[](size_t)` вызов `grid[l]` с `int l` — **ambiguous** (`int → ptrdiff_t` и `int → size_t` обе integral conversion). Call sites с `int`:

1. `ReservoirSimulator.cpp:139` — `Grid[0]` (литерал) → `Grid[size_t{0}]`
2. `NewtonSolver.cpp:75,77` — `int l` из OMP-цикла → `grid[static_cast<size_t>(l)]` (l гарантированно ≥ 0, это индекс активной ячейки)
3. `test_JacobianAssembly.cpp:604,608,704,708` — `int l` → заменить на `size_t l` (нет OMP-ограничения)

## Поиск подводных камней

- [✅] **Все call sites найдены:** grep `Grid\[` и `grid\[` по всем .h/.cpp (37 мест, каждый проверен)
- [✅] **Потокобезопасность:** OMP в NewtonSolver.cpp:69 — `int l` остаётся для OMP, `grid[static_cast<size_t>(l)]` безопасно (read-write на разных ячейках)
- [✅] **Зависимости сборки:** только header `AbstractGrid.h`, нет CMake-изменений
- [✅] **Обратная совместимость API:** `ConvertGlobal2Local` возвращает `ptrdiff_t` вместо `long int` — ABI-несовместимо, но внутренний API, внешних потребителей нет
- [✅] **Тесты:** все call sites обновляются, поведение идентично (ptrdiff_t ⊃ long int по диапазону)
- [✅] **Платформозависимость:** `ptrdiff_t` — стандартный тип, одинаково работает на всех платформах
- [✅] **Связь с другими задачами:** concept-workaround из DEBT-058 удаляется (это цель задачи)
- [✅] **Производительность:** `ptrdiff_t` = 64 бит vs `long int` = 32 бит — `vector<ptrdiff_t>` в 2× больше по памяти. Для текущих сеток (≤10404 ячейки) это ~80 КБ → пренебрежимо
- [⚠️] **`static_cast<long int>` в конструкторе:** строки 156, 162 — `static_cast<long int>(activeCellsNmbr)` и `-static_cast<long int>(inActiveCellsNmbr)`. Заменить на `static_cast<ptrdiff_t>(...)`. Каст обоснован: `size_t → ptrdiff_t` narrowing для значений > PTRDIFF_MAX, но для реальных сеток безопасно
- [⚠️] **`static_cast<size_t>(l)` на call sites:** `ReservoirSimulator.cpp:135` — `static_cast<size_t>(cellIdx)` где `cellIdx` уже проверен `>= 0`. После замены на `ptrdiff_t` каст остаётся корректным

## Baseline

- Тесты: **313 pass** (Release ~98 сек, Debug ~396 сек)
- Warnings (проектный код): **0**
- Ветка: `refactor/debt-058/static-cast-cleanup` (текущая, concept-workaround)

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[индексация через size_t и ptrdiff_t а не long]]
3. Создай ветку: `git checkout -b refactor/debt-060/long-int-to-ptrdiff experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"`; `cmake --build build --config Release`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Этап 1: AbstractGrid.h — хранение и API

### Шаг 1: Заменить `long int` на `ptrdiff_t` и убрать concept

**Цель:** привести тип хранения, возвращаемые типы и `operator[]` к `ptrdiff_t` + `size_t`.

**Файлы:** `HydroSolver/Solver/Grids/AbstractGrid.h`

**Контекст:**
`cell_idx_Global2Local` (строка 29) хранит `vector<long int>`. Значения: неотрицательный локальный индекс для активных ячеек, отрицательный для неактивных. `ConvertGlobal2Local` (строка 117) и `ConvertTriple2Local` (строка 195) возвращают `long int`. `operator[]` (строки 79-105) сейчас использует `template<std::signed_integral T>` + explicit `size_t` (workaround из DEBT-058). `operator()` (строка 310) объявляет `long int idx`.

**Что сделать:**

1. Убрать `#include <concepts>` (строка 2) — больше не нужен
2. Строка 29: `std::vector<long int> cell_idx_Global2Local` → `std::vector<ptrdiff_t> cell_idx_Global2Local`
3. Строка 117: `long int ConvertGlobal2Local(size_t idx) const` → `ptrdiff_t ConvertGlobal2Local(size_t idx) const`
4. Строка 145: `cell_idx_Global2Local{ std::vector<long int>(active_cells.size(), -1) }` → `cell_idx_Global2Local{ std::vector<ptrdiff_t>(active_cells.size(), -1) }`
5. Строка 156: `cell_idx_Global2Local[l] = static_cast<long int>(activeCellsNmbr)` → `cell_idx_Global2Local[l] = static_cast<ptrdiff_t>(activeCellsNmbr)`
6. Строка 162: `cell_idx_Global2Local[l] = -static_cast<long int>(inActiveCellsNmbr)` → `cell_idx_Global2Local[l] = -static_cast<ptrdiff_t>(inActiveCellsNmbr)`
7. Строки 79-95: заменить `template<std::signed_integral T>` overloads на явный `ptrdiff_t`:

До:
```cpp
template<std::signed_integral T>
const ProcessCell& operator [] (T idx) const
{
    if (idx < 0)
        return CellsInactive[-(idx + 1)];
    else
        return Cells[idx];
}

template<std::signed_integral T>
ProcessCell& operator [] (T idx)
{
    if (idx < 0)
        return CellsInactive[-(idx + 1)];
    else
        return Cells[idx];
}
```

После:
```cpp
const ProcessCell& operator [] (ptrdiff_t idx) const
{
    if (idx < 0)
        return CellsInactive[-(idx + 1)];
    else
        return Cells[idx];
}

ProcessCell& operator [] (ptrdiff_t idx)
{
    if (idx < 0)
        return CellsInactive[-(idx + 1)];
    else
        return Cells[idx];
}
```

8. Строки 250, 257, 264, 271: `connections.push_back(cell_idx_Global2Local[...])` — `cell_idx_Global2Local[...]` теперь `ptrdiff_t`, а `connections` = `vector<int>`. Добавить `static_cast<int>(...)`:
   - `connections.push_back(static_cast<int>(cell_idx_Global2Local[l - Nx()]))` (строка 250)
   - `connections.push_back(static_cast<int>(cell_idx_Global2Local[l - 1]))` (строка 257)
   - `connections.push_back(static_cast<int>(cell_idx_Global2Local[l + 1]))` (строка 264)
   - `connections.push_back(static_cast<int>(cell_idx_Global2Local[l + Nx()]))` (строка 271)
   Каст обоснован: `connectivityGraph` хранит локальные индексы **активных** ячеек (всегда ≥ 0, ≤ activeCellsNmbr ≤ ~10k). Менять тип `connectivityGraph` на `vector<vector<ptrdiff_t>>` — отдельная задача, каскадный эффект по всему коду
9. Строка 195: `long int ConvertTriple2Local(const std::vector<size_t>& idx) const` → `ptrdiff_t ConvertTriple2Local(const std::vector<size_t>& idx) const`
10. Строка 310: `long int idx = ConvertTriple2Local(...)` → `ptrdiff_t idx = ConvertTriple2Local(...)`

**Проверка после этого шага:**
- `cmake --build build --config Release` — ожидаются ошибки C2593 на call sites с `int` аргументами (`NewtonSolver.cpp`, `test_JacobianAssembly.cpp`), `long int` аргументы (`ReservoirSimulator.cpp`, `JacobianAssembler.cpp`) тоже дадут ошибку
- Это ожидаемо — исправляется в шагах 2-4
- Если ошибок нет (MSVC повёл себя иначе) — тем лучше, переходить к шагу 2

**Подводные камни:**
- `#include <cstddef>` для `ptrdiff_t` — уже доступен через `<vector>` и другие STL-заголовки, но если компилятор жалуется — добавить `#include <cstddef>`
- `connections.push_back(cell_idx_Global2Local[...])` в `SetConnectivityGraph_3D` — `ptrdiff_t → int` narrowing. Решается `static_cast<int>(...)`. Менять тип `connectivityGraph` — вне scope (каскадный эффект: `GetNeighboursIdx`, `GetConnectivityGraph`, все consumer'ы графа)

**Зависимости:**
- Требует: нет
- Блокирует: шаги 2, 3, 4

**Оценка:** ~14 правок, ~8 минут

---

## Этап 2: Call sites — ReservoirSimulator.cpp

### Шаг 2: Заменить `long int` → `ptrdiff_t` и `Grid[0]` → `Grid[size_t{0}]`

**Цель:** обновить все переменные, принимающие результат `ConvertGlobal2Local` / `ConvertTriple2Local`, на `ptrdiff_t`. Исправить литерал `Grid[0]`.

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Что сделать:**

1. Строка 131: `long int cellIdx = Grid.ConvertTriple2Local(...)` → `ptrdiff_t cellIdx = Grid.ConvertTriple2Local(...)`
2. Строка 139: `cells_[l] = &(Grid[0])` → `cells_[l] = &(Grid[size_t{0}])` — литерал `int 0` ambiguous между `ptrdiff_t` и `size_t`, а `size_t{0}` — exact match для unsigned overload. Семантически корректно: ячейка 0 — активная, unsigned доступ
3. Строка 629: `long int l_Local = Grid.ConvertGlobal2Local(l_Global)` → `ptrdiff_t l_Local = ...`
4. Строка 664: `long int l_Local = Grid.ConvertGlobal2Local(l_Global)` → `ptrdiff_t l_Local = ...`
5. Строка 665: `long int l_Local_Neighbour = Grid.ConvertGlobal2Local(l_Global + nx)` → `ptrdiff_t l_Local_Neighbour = ...`
6. Строка 703: `long int l_Local = ...` → `ptrdiff_t l_Local = ...`
7. Строка 760: `long int l_Local = ...` → `ptrdiff_t l_Local = ...`
8. Строка 795: `long int l_Local = ...` → `ptrdiff_t l_Local = ...`
9. Строка 796: `long int l_Local_Neighbour = ...` → `ptrdiff_t l_Local_Neighbour = ...`
10. Строка 832: `long int l_Local = ...` → `ptrdiff_t l_Local = ...`
11. Строка 887: `long int l = Grid.ConvertGlobal2Local(...)` → `ptrdiff_t l = ...`
12. Строка 929: `long int l = ...` → `ptrdiff_t l = ...`
13. Строка 970: `long int l = ...` → `ptrdiff_t l = ...`
14. Строка 1017: `long int l = ...` → `ptrdiff_t l = ...`
15. Строка 1046: `long int l = ...` → `ptrdiff_t l = ...`

Можно использовать `replace_all` для `long int l_Local` и подобных паттернов.

**Проверка после этого шага:**
- Сборка должна пройти для этого файла (все `long int` → `ptrdiff_t`, exact match)
- Ошибки могут оставаться в других файлах (шаги 3-4)

**Зависимости:**
- Требует: шаг 1
- Блокирует: нет

**Оценка:** ~15 правок (механические), ~5 минут

---

## Этап 3: Call sites — JacobianAssembler.cpp и NewtonSolver.cpp

### Шаг 3: JacobianAssembler.cpp — `long int` → `ptrdiff_t`

**Цель:** обновить переменные на call sites.

**Файлы:** `HydroSolver/Reservoir/JacobianAssembler.cpp`

**Что сделать:**

1. Строка 158: `long int l = grid.ConvertGlobal2Local(...)` → `ptrdiff_t l = ...`
2. Строка 219: `long int l = grid.ConvertGlobal2Local(...)` → `ptrdiff_t l = ...`
3. Строка 282: `long int l = grid.ConvertGlobal2Local(...)` → `ptrdiff_t l = ...`

Можно использовать `replace_all` для `long int l = grid.ConvertGlobal2Local`.

**Примечание:** строка 33 (`int l` в OMP-цикле) НЕ затрагивается — `fillMatrixBlockRow(l, ...)` принимает `size_t l`, conversion `int → size_t` происходит при вызове функции, а `grid[l]` используется уже внутри функции с `size_t l`.

**Проверка после этого шага:**
- `cmake --build build --config Release` — этот файл компилируется

**Зависимости:**
- Требует: шаг 1
- Блокирует: нет

**Оценка:** ~3 правки, ~2 минуты

### Шаг 4: NewtonSolver.cpp — `int l` в OMP-цикле с `grid[l]`

**Цель:** устранить ambiguity для `grid[l]` где `l` = `int` из OMP-цикла.

**Файлы:** `HydroSolver/Reservoir/NewtonSolver.cpp`

**Контекст:**
Строка 71: `for (int l = 0; l < grid.ActiveCellsNmbr(); l++)` — MSVC OpenMP 2.0 требует signed int для переменной цикла. `grid[l]` на строках 75, 77 с `int l` будет ambiguous при `operator[](ptrdiff_t)` + `operator[](size_t)`.

**Что сделать:**

Вариант: добавить промежуточную переменную `size_t sl = l;` и использовать `grid[sl]`.

До (строки 71-77):
```cpp
for (int l = 0; l < grid.ActiveCellsNmbr(); l++)
{
    double corr[B];
    problem.UnpackCellCorrections(l, corr);
    grid[l].UpdateState(corr);

    const std::vector<double>& stateVaiables = grid[l].GetVariableFieldProperties();
```

После:
```cpp
for (int l = 0; l < grid.ActiveCellsNmbr(); l++)
{
    size_t sl = l;
    double corr[B];
    problem.UnpackCellCorrections(l, corr);
    grid[sl].UpdateState(corr);

    const std::vector<double>& stateVaiables = grid[sl].GetVariableFieldProperties();
```

**Подводные камни:**
- `l` гарантированно ≥ 0 (цикл от 0), поэтому `size_t sl = l` безопасно
- `problem.UnpackCellCorrections(l, corr)` — оставить `l` (int), это отдельный API

**Проверка после этого шага:**
- `cmake --build build --config Release`
- Должно компилироваться без ошибок

**Зависимости:**
- Требует: шаг 1
- Блокирует: нет

**Оценка:** ~3 правки, ~2 минуты

---

## Этап 4: Тесты

### Шаг 5: test_JacobianAssembly.cpp — `long int` → `ptrdiff_t`, `int l` → `size_t l`

**Цель:** обновить типы переменных в тестах.

**Файлы:** `tests/unit/math/test_JacobianAssembly.cpp`

**Что сделать:**

1. Строка 207: `long int l = grid.ConvertGlobal2Local(globalIdx)` → `ptrdiff_t l = ...`
2. Строка 220: `long int l = grid.ConvertGlobal2Local(globalIdx)` → `ptrdiff_t l = ...`
3. Строка 604: `for (int l = 0; l < ncells; l++)` → `for (size_t l = 0; l < ncells; l++)` — нет OMP, можно `size_t`
4. Строка 704: `for (int l = 0; l < ncells; l++)` → `for (size_t l = 0; l < ncells; l++)` — нет OMP, можно `size_t`

**Подводные камни:**
- Строки 605-606: `crs.GlobalIndex(l, 0)` — проверить тип параметра. Если `int` → `size_t l` может вызвать warning. Проверить при сборке
- Строки 705-706: аналогично

**Проверка после этого шага:**
- `cmake --build build --config Release` — 0 ошибок, 0 warnings из проектного кода
- `ctest --test-dir build -C Release --output-on-failure` — 313/313 pass

**Зависимости:**
- Требует: шаг 1
- Блокирует: нет

**Оценка:** ~4 правки, ~2 минуты

---

## Этап 5: Верификация

### Шаг 6: Финальная проверка

**Цель:** убедиться что `long int` больше нет в проектных файлах (кроме MER_Descriptor.h) и что concept убран.

**Что сделать:**

1. `grep -rn "long int" HydroSolver/ tests/ --include="*.h" --include="*.cpp"` — должен показать только `MER_Descriptor.h:101`
2. `grep -rn "signed_integral\|#include <concepts>" HydroSolver/` — пусто
3. `cmake --build build --config Release` — 0 errors, 0 warnings (проектный код)
4. `ctest --test-dir build -C Release --output-on-failure` — 313/313
5. `cmake --build build --config Debug`
6. `ctest --test-dir build -C Debug --output-on-failure` — 313/313

**Проверка после этого шага:**
- Release: 313/313, 0 warnings
- Debug: 313/313

**Зависимости:**
- Требует: шаги 1-5

**Оценка:** ~15 минут (сборка + тесты)

---

## Тестовая стратегия

Новые тесты не требуются. Замена `long int` → `ptrdiff_t` не меняет поведение — `ptrdiff_t` ⊃ `long int` по диапазону значений. Все 313 существующих тестов — regression-тесты неизменности поведения.

Ключевые тесты, покрывающие затронутый код:
- `unit.level2.JacobianAssembler.*` — JacobianAssembler call sites
- `unit.level3.NewtonSolver.*` — NewtonSolver call sites
- `integration.*` — end-to-end через ReservoirSimulator
- `unit.level4.*` — bitwise match тесты (поймают любое изменение результатов)

---

## Критерии завершения

- [ ] Все шаги выполнены (1-6)
- [ ] 0 `long int` в проектных файлах (кроме `MER_Descriptor.h:101`)
- [ ] 0 `template<std::signed_integral>` / `#include <concepts>` в `AbstractGrid.h`
- [ ] `operator[]` — два явных overload: `ptrdiff_t` + `size_t`
- [ ] 313/313 тестов (Release + Debug)
- [ ] 0 warnings из проектного кода
- [ ] Vault обновлён: DEBT-060 статус, текущие приоритеты
- [ ] GitHub issue #41 прокомментирован

---

## Обнаруженные проблемы

Нет.
