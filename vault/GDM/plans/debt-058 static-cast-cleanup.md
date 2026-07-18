---
tags:
  - план
  - рефакторинг
date: 2026-07-18
issue: DEBT-058
github: 39
branch: refactor/debt-058/static-cast-cleanup
status: реализован
audit:
  date: 2026-07-18
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-058: Устранить static_cast — исправить типы, чтобы касты стали ненужны

## Контекст

В ~90 местах проектного кода (HydroSolver + tests + examples) используется `static_cast` для конвертации между `int`, `size_t`, `long int`, `double`. Это симптом несогласованных типов в интерфейсах. Часть уже закрыта: DEBT-021 (#31), DEBT-022 (#33), DEBT-028 (#34), DEBT-039 (#35).

**Критическое наблюдение:** `AbstractGrid::operator[](int)` использует знак аргумента как семантику: `idx < 0` → неактивная ячейка, `idx ≥ 0` → активная. Нельзя просто заменить `int` на `size_t`. Решение: добавить перегрузку `operator[](size_t)` для active cells (без проверки на отрицательность), а сигнатуру `operator[](int)` оставить для полиморфного доступа через `ConvertGlobal2Local` (на Windows `long int` = `int`, identity conversion).

**Baseline:** 313 тестов, все зелёные, 0 warnings, ~102 сек.

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-058/static-cast-cleanup`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"`; `cmake --build build --config Release`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release`
5. Запомни: 313 тестов, ~102 сек, 0 warnings
6. Начни с шага 1. После каждого шага: сборка + тесты

## Группы изменений

| Группа | Корневая причина | Кастов | Шаг |
|--------|-----------------|--------|-----|
| A | `AbstractGrid::operator[](int)` | ~25 | 1–2 |
| B | `FlowField::operator[](int)` | 15 | 3 |
| C | `SomeFlowField` конструкторы `int nx, ny` | C4267 | 4 |
| D | `MathRoutines` return `int` | 2 | 5 |
| E | `PIController` `size_t / size_t` | 2 | 6 |
| F | `NumericalParameters` getter `size_t` | 1 | 7 |
| G | `update_currentAMGState(tuple<int,...>)` | 6 | 8 |
| H | `MER_Descriptor` `double → size_t` | 2 | оставить |
| I | `SparsityPattern` `unsigned char eqNmbr_` | 2 | 9 |
| J | `Config_JSON` `double → size_t` | 1 | оставить |
| K | tests/examples прочие | ~10 | 10 |

Группы H и J — касты обоснованы (double из JSON парсинга, double-арифметика времени). Оставляем как есть.

---

## Шаг 1: `AbstractGrid::operator[]` — добавить перегрузку `size_t` для active cells

**Цель:** устранить `Grid[static_cast<int>(l)]` в циклах по active cells, где `l` — `size_t` ≥ 0.

**Файлы:**
- `HydroSolver/Solver/Grids/AbstractGrid.h` (строки 79, 87, 299)

**Контекст:**
`operator[](int)` использует знак аргумента: `idx < 0` → `CellsInactive[-(idx+1)]`, `idx ≥ 0` → `Cells[idx]`. Это корректная семантика, менять её не нужно. Но call sites, которые итерируют по active cells с `size_t l`, вынуждены кастить `size_t → int`.

Добавляем перегрузку `operator[](size_t)`, которая обращается напрямую к `Cells[idx]` без проверки на отрицательность.

**Что сделать:**

1. В `AbstractGrid.h` после существующего `operator[](int)` (строка ~93) добавить:

```cpp
const ProcessCell& operator [] (size_t idx) const
{
    return Cells[idx];
}

ProcessCell& operator [] (size_t idx)
{
    return Cells[idx];
}
```

2. Существующие `operator[](int)` оставить — они нужны для `long int` аргументов из `ConvertGlobal2Local`.

3. В `AbstractGrid.h:299` — `operator()(size_t, size_t, size_t)` вызывает `(*this)[static_cast<int>(idx)]` где `idx` — `long int` из `ConvertTriple2Local`. Убрать каст — `long int` неявно конвертируется в `int`, оба `operator[]` с `int` аргументом подходят:

До: `return (*this)[static_cast<int>(idx)];`
После: `return (*this)[idx];`

Здесь `idx` — `long int`, вызовется `operator[](int)` через неявную конвертацию. Это корректно, потому что `idx` может быть отрицательным (неактивная ячейка).

4. `GetNeighboursPointer(int l)` и `CommonEdgeArea(int l)` принимают `int` — вызывают каст в `JacobianAssembler.cpp:64` (`static_cast<int>(l)` для `size_t l`). Исправляются в шаге 2.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release`
- Все 313 тестов зелёные
- Перегрузки не вызывают ambiguity: `int` → `operator[](int)`, `size_t` → `operator[](size_t)`, `long int` → `operator[](int)` (неявная конвертация)

**Подводные камни:**
- ⚠️ Проверить, нет ли ambiguity при вызове с `unsigned int`, `short`, другими целочисленными типами. Grep: `Grid\[` в HydroSolver/*.cpp — все call sites используют `int`, `size_t` или `long int`.
- ✅ `cell_idx_Global2Local` хранит `long int` → `ConvertGlobal2Local` возвращает `long int` → вызовется `operator[](int)` через implicit narrowing. Это существующее поведение, не ухудшаем.

**Зависимости:** нет
**Оценка:** ~8 строк, 5 мин

---

## Шаг 2: Убрать `static_cast<int>` в call sites `AbstractGrid::operator[]`

**Цель:** убрать касты, которые стали ненужны после добавления перегрузки `operator[](size_t)`.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSimulator.cpp` (строки 136, 353, 360)
- `HydroSolver/Reservoir/JacobianAssembler.cpp` (строки 33, 64)
- `tests/unit/math/test_JacobianAssembly.cpp` (~20 мест)
- `tests/unit/math/test_MatrixAssembly.cpp` (строки 16, 27)
- `tests/unit/math/test_LinearProblemAssembly.cpp` (строка 16)
- `tests/unit/math/assembly_test_helpers.h` (строка 166 — `check_nnz_count`)

**Что сделать:**

1. `ReservoirSimulator.cpp:136` — `cells_[l] = &(Grid[static_cast<int>(cellIdx)]);`
   `cellIdx` — `long int` из `ConvertTriple2Local`, уже проверен `>= 0`. Здесь убрать каст:
   До: `cells_[l] = &(Grid[static_cast<int>(cellIdx)]);`
   После: `cells_[l] = &(Grid[cellIdx]);`
   Вызовется `operator[](int)` через implicit conversion `long int → int`. Корректно: `cellIdx` проверен ≥ 0.

2. `ReservoirSimulator.cpp:353,360` — `Grid[static_cast<int>(l)]` где `l` — `size_t` из цикла по active cells.
   До: `result += Grid[static_cast<int>(l)].OilMass();`
   После: `result += Grid[l].OilMass();`
   Вызовется `operator[](size_t)` — перегрузка из шага 1.

3. `JacobianAssembler.cpp:33` — `for (int l = 0; l < static_cast<int>(grid.ActiveCellsNmbr()); l++)`
   ⚠️ **OpenMP-ограничение:** этот цикл защищён `#pragma omp parallel for` (через `#ifdef USE_PARALLEL`, определён в `Defines.h:7`). MSVC OpenMP 2.0 требует **signed integer** переменную цикла — `size_t` (unsigned) вызовет ошибку компиляции.
   **Решение:** оставить `int l`, каст `static_cast<int>(grid.ActiveCellsNmbr())` обоснован ограничением OpenMP. Вынести в отдельную переменную для читаемости:
   До: `for (int l = 0; l < static_cast<int>(grid.ActiveCellsNmbr()); l++)`
   После: `const int ncells = static_cast<int>(grid.ActiveCellsNmbr()); for (int l = 0; l < ncells; l++)`
   `fillMatrixBlockRow(l, ...)` принимает `size_t l` — implicit `int → size_t` корректен (l ≥ 0 в цикле). Тот же паттерн в `NewtonSolver.cpp:71` — аналогичный OpenMP-цикл, не трогаем.

4. `JacobianAssembler.cpp:64` — `const int li = static_cast<int>(l);`
   Переменная `li` используется для `grid.GetNeighboursPointer(li)` и `grid.CommonEdgeArea(li)`, которые принимают `int`. Параметр `l` в `fillMatrixBlockRow` — `size_t`.
   **Решение:** менять `GetNeighboursPointer` и `CommonEdgeArea` на `size_t` (они никогда не принимают отрицательные):
   - `GetNeighboursPointer(int l)` → `GetNeighboursPointer(size_t l)`
   - `CommonEdgeArea(int l)` → `CommonEdgeArea(size_t l)`
   Тогда каст `li` станет ненужен: `grid[l]` вызовет `operator[](size_t)`, а `grid.GetNeighboursPointer(l)` и `grid.CommonEdgeArea(l)` примут `size_t` напрямую. Удалить `li`.

5. `test_JacobianAssembly.cpp` — ~20 мест с `static_cast<int>(...)`:
   - Строки 34–37: `grid[static_cast<int>(l)]`, `grid.GetNeighboursPointer(static_cast<int>(l))`, `grid.CommonEdgeArea(static_cast<int>(l))`, `static_cast<int>(neighbours.size())` — после изменения сигнатур `operator[]`, `GetNeighboursPointer`, `CommonEdgeArea` на `size_t` касты станут ненужны. Убрать.
   - Строки 155–158: `static_cast<int>(grid.ActiveCellsNmbr())`, `static_cast<int>(grid.Nx())` и т.д. — если переменные `ncells`, `nx`, `ny`, `nz` станут `size_t`, касты не нужны.
   - Строки 193, 206, 210, 219, 223: аналогично.
   - Строки 241, 477, 483, 578, 584, 684, 685: аналогично.

6. `test_MatrixAssembly.cpp:16,27` — `static_cast<int>(graph.size())`, `static_cast<int>(graph[l].size())` — `graph` — `vector<vector<int>>`, `.size()` → `size_t`. Переменные цикла `ncells`, `ni` → `size_t`.

7. `test_LinearProblemAssembly.cpp:16` — аналогично.

8. `assembly_test_helpers.h` — три функции принимают `int ncells`:
   - `check_nnz_count(const MatrixCSR& m, int ncells, ...)` (строка 166) → `size_t ncells`
   - `build_dense_from_blocks(int ncells, Layout, ...)` (строка 72) → `size_t ncells`
   - `place_block(... int cell_row, int cell_col, ... int ncells)` (строка 51) → `size_t ncells` (остальные параметры — `int`, не менять: передаются из `std::pair<int,int>`)
   Иначе при передаче `size_t ncells` с call sites будет implicit narrowing warning.

**Проверка после этого шага:**
- Сборка + тесты
- Grep `static_cast<int>` в JacobianAssembler.cpp — 1 вхождение (OpenMP-цикл, обоснован)
- Grep `static_cast<int>` в ReservoirSimulator.cpp — 0 вхождений для `Grid[`

**Подводные камни:**
- ⚠️ **OpenMP:** цикл `for (int l = ...)` в `JacobianAssembler.cpp:33` и `NewtonSolver.cpp:71` — `#pragma omp parallel for` требует signed int. Не менять переменную цикла на `size_t`. Аналогичный паттерн в `Anomaly.cpp:270`.
- ⚠️ `GetNeighboursPointer` и `CommonEdgeArea` внутри обращаются к `connectivityGraph[l]` и `commonEdgeArea[l]` — `l` используется как индекс `std::vector`. `size_t` — корректный тип для индексации вектора.
- ✅ `AddDiagBlock(size_t l, ...)` и `AddOffDiagBlock(size_t l, size_t neibIdx, ...)` — уже принимают `size_t` (после DEBT-028). Подтверждено: `LinearProblem.h:77-79`.
- ⚠️ В тестах `nNeib = static_cast<int>(neighbours.size())` — `nNeib` используется в цикле `for (int ni = 0; ni < nNeib; ni++)`. Менять на `size_t` (нет OpenMP в тестах).

**Зависимости:** шаг 1
**Оценка:** ~70 строк, 25 мин

---

## Шаг 3: `FlowField::operator[](int)` → `size_t`

**Цель:** устранить 15 `portrait[static_cast<int>(i)]` в `test_streamlines.cpp`.

**Файлы:**
- `HydroSolver/Anomaly/FlowField/FlowField.h` (строка 193)
- `tests/test_streamlines.cpp` (~15 мест)

**Контекст:**
`FlowField::operator[](int i)` обращается к `trajectoryEnsemble[i]` — `std::vector`, индексируется `size_t`. `int` здесь не несёт семантики отрицательных индексов (в отличие от `AbstractGrid`).

**Что сделать:**

1. `FlowField.h:193` — менять сигнатуру:
   До: `const reservoir_simulator::phasePortrait::Trajectory& operator[] (int i) const`
   После: `const reservoir_simulator::phasePortrait::Trajectory& operator[] (size_t i) const`

2. `test_streamlines.cpp` — убрать все `static_cast<int>(i)`:
   До: `portrait[static_cast<int>(i)]`
   После: `portrait[i]`
   ~15 замен (строки 70, 72, 73, 145, 151, 153, 154, 160, 161, 218, 224, 226, 227, 234, 235).

**Проверка после этого шага:**
- Сборка + тесты
- Grep `static_cast<int>` в test_streamlines.cpp — 0 вхождений

**Подводные камни:**
- ✅ `NumberOfTrajectories()` возвращает `size_t` — итерация по `size_t i` корректна.
- ✅ `Trajectory::operator[]` принимает `std::size_t` — нет конфликта.

**Зависимости:** нет (независим от шагов 1–2)
**Оценка:** ~20 строк, 5 мин

---

## Шаг 4: `SomeFlowField` конструкторы `int nx, ny` → `size_t`

**Цель:** устранить неявное сужение `size_t → int` (C4267) при вызове из `ReservoirSimulator`.

**Файлы:**
- `HydroSolver/Anomaly/FlowField/SomeFlowField.h` (строки 11, 12, 72, 81, 89, 106–107, 136, 152–153)
- `HydroSolver/Anomaly/FlowField/SomeFlowField.cpp` (строка 105 — НЕ МЕНЯТЬ, бинарный формат)

**Контекст:**
Цепочка конструкторов: `SomeFlowField(int nx_, int ny_)` → `FlowFieldSnapshot(int nx, int ny)` → `PorosityField(int nx, int ny)` → `FlowFieldComponentX/Y(int nx, int ny)` → `create_uniform_mesh(int n)`.

Все `int nx, ny` → `size_t nx, ny`. Но: поля `int nx, ny` в `SomeFlowField` (строка 136) тоже → `size_t`.

**Исключение:** `SomeFlowField::write()` (строка 105): `auto n = static_cast<int>(field.size())` — бинарный формат записи фиксирует `int32_t`. Здесь каст обоснован и остаётся. Рекомендация: заменить `int` на `int32_t` для явности (но это отдельная задача, не в scope).

**Что сделать:**

1. `create_uniform_mesh` (строка 11): `double h, int n, double start` → `double h, size_t n, double start`
2. `create_uniform_mesh_2D` (строка 12): все `int nx`/`int ny` → `size_t`
3. `PorosityField` конструктор (строка 72): `int nx, int ny` → `size_t nx, size_t ny`
4. `FlowFieldComponentX` конструктор (строка 81): `int nx, int ny` → `size_t nx, size_t ny`
5. `FlowFieldComponentY` конструктор (строка 89): `int nx, int ny` → `size_t nx, size_t ny`
6. `FlowFieldSnapshot` конструктор (строки 106–107): `int nx, int ny` → `size_t nx, size_t ny`
7. Поля `int nx, ny` (строка 136) → `size_t nx, ny`
8. `SomeFlowField` конструктор (строка 152): `int nx_, int ny_` → `size_t nx_, size_t ny_`
9. Реализацию `create_uniform_mesh` в .cpp (если есть) — обновить сигнатуру

**Проверка после этого шага:**
- Сборка + тесты
- Нет новых warnings (проверить `/clp:WarningsOnly`)

**Подводные камни:**
- ⚠️ `create_uniform_mesh` использует `n` в цикле `for (int i = 0; i <= n; i++)` (или аналогичном). Если `n` → `size_t`, переменную цикла тоже менять.
- ⚠️ Проверить, не передаётся ли `nx`/`ny` куда-то ещё как `int` (grep в SomeFlowField.cpp).
- ✅ `SomeFlowField::write()` — `auto n = static_cast<int>(field.size())` — оставляем.

**Зависимости:** нет (независим от шагов 1–3)
**Оценка:** ~20 строк, 10 мин

---

## Шаг 5: `MathRoutines` — `int → ptrdiff_t`

**Цель:** устранить сужение `ptrdiff_t → int` в `LowerPointNonUniformMesh`.

**Файлы:**
- `HydroSolver/Solver/Math/MathRoutines.cpp` (строки 56, 64)
- `HydroSolver/Solver/Math/MathRoutines.h` — объявление

**Контекст:**
`LowerPointUniformMesh` (строка 56): `return static_cast<int>(std::floor(...))` — `std::floor` возвращает `double`. Каст `double → int` — потеря дробной части. Тип `int` обоснован: результат может быть -1 (выход за сетку). Но `int` сужает на 64-bit для очень крупных сеток.

`LowerPointNonUniformMesh` (строка 64): `return static_cast<int>(iter)` — `iter` = `std::distance(begin, upper_bound(...)) - 1`, тип `ptrdiff_t`. Каст `ptrdiff_t → int` сужает.

**Что сделать:**

1. Возвращаемый тип обеих функций: `int` → `ptrdiff_t`:
   - `LowerPointUniformMesh` → `ptrdiff_t`
   - `LowerPointNonUniformMesh` → `ptrdiff_t`
2. Убрать `static_cast<int>`:
   - Строка 56: `return static_cast<int>(std::floor(...))` → `return static_cast<ptrdiff_t>(std::floor(...))`
   - Строка 64: `return static_cast<int>(iter)` → `return iter`
3. Обновить объявления в `MathRoutines.h`
4. Grep по call sites: проверить, кто использует возвращаемое значение

**Проверка после этого шага:**
- Сборка + тесты

**Подводные камни:**
- ⚠️ **Call sites:**
  - `MathRoutines.cpp:26,29`: `int x_idx = LowerPointUniformMesh(...)` → менять `int x_idx` → `ptrdiff_t x_idx`.
    НО: строка 27 содержит `x_idx > x_mesh.size() - 2` — сравнение `ptrdiff_t > size_t` = signed/unsigned mismatch warning (C4018 на MSVC). Решение: `static_cast<ptrdiff_t>(x_mesh.size()) - 2` или `x_idx > static_cast<ptrdiff_t>(x_mesh.size() - 2)`. Аналогично для `y_idx` (строка 30).
  - `SomeFlowField.cpp:68`: `auto lowT_idx = LowerPointNonUniformMesh(...)` → `auto` подхватит `ptrdiff_t` автоматически. Строка 71: `lowT_idx == time.size() - 1` — аналогичный signed/unsigned mismatch. Решение: каст `time.size()`.
- ✅ Согласуется с vault-решением [[индексация через size_t и ptrdiff_t а не long]]: знаковые индексы → `ptrdiff_t`.

**Зависимости:** нет
**Оценка:** ~10 строк, 10 мин

---

## Шаг 6: `PIController` — касты `size_t → double`

**Цель:** устранить `static_cast<double>` в PIController.

**Файлы:**
- `HydroSolver/Reservoir/PIController.cpp` (строки 12, 14)
- `HydroSolver/Reservoir/PIController.h` (определение `PIControllerParams`)

**Контекст:**
`PIControllerParams::target_iters` и `max_iters` — `size_t`. В `ComputeMultiplier` выполняется деление: `static_cast<double>(newton_iters) / params_.max_iters`. Каст нужен, чтобы деление было вещественным. Если `max_iters` и `target_iters` → `double`, каст не нужен, но семантически это целые числа.

**Что сделать:**

Вариант: оставить типы `size_t`, но привести делитель к `double` неявно:

До:
```cpp
e_n = static_cast<double>(newton_iters) / params_.max_iters;
double e_target = static_cast<double>(params_.target_iters) / params_.max_iters;
```
После:
```cpp
e_n = newton_iters / static_cast<double>(params_.max_iters);
double e_target = params_.target_iters / static_cast<double>(params_.max_iters);
```

Это не устраняет каст, а перемещает его. Лучше: ввести вспомогательную переменную.

До:
```cpp
e_n = static_cast<double>(newton_iters) / params_.max_iters;
double e_target = static_cast<double>(params_.target_iters) / params_.max_iters;
```
После:
```cpp
const double inv_max = 1.0 / params_.max_iters;
e_n = newton_iters * inv_max;
double e_target = params_.target_iters * inv_max;
```

`size_t * double` → промотируется в `double` без каста. Каст устранён.

**Проверка после этого шага:**
- Сборка + тесты

**Подводные камни:**
- ✅ `1.0 / size_t` — `size_t` промотируется в `double`, деление вещественное. Корректно.
- ⚠️ Проверить precision: `1.0 / max_iters` vs `newton_iters / (double)max_iters` — математически эквивалентны, но floating-point порядок операций меняется. Для целых < 2^53 разницы нет.

**Зависимости:** нет
**Оценка:** ~4 строки, 3 мин

---

## Шаг 7: `AMG_maxSolverIterCount` int → `size_t`, getter без каста

**Цель:** устранить `static_cast<size_t>(AMG_maxSolverIterCount)` в getter, приведя тип поля к `size_t`.

**Файлы:**
- `HydroSolver/Reservoir/NumericalParameters.h` (строки 21, 30, 68, 97)
- `HydroSolver/Reservoir/NumericalParameters.cpp` (строки 24, 34, 39)
- `tests/unit/reservoir/test_NumericalParameters.cpp` (строки 94–95 — тестовый struct `TestableNumericalParameters`)

**Контекст:**
После DEBT-039 поле `AMG_maxSolverIterCount` стало `int` (было `double`). Getter возвращает `size_t` через каст. Количество итераций — натуральное число, `size_t` семантически корректен. AMGCL `Solve(size_t max_iters)` ожидает `size_t` — тип будет согласован сквозь всю цепочку.

Закомментированный `AMG_maxSolverIterCount -= 3` (строка 31 .cpp) — мёртвый код. Если когда-нибудь раскомментируют, `size_t -= 3` потребует guard `if (val >= 3)`. Но это не аргумент для выбора типа живого кода.

**Что сделать:**

1. `NumericalParameters.h:21` — константа:
   До: `static constexpr int AMG_MAXSOLVERITERCOUNT = 45;`
   После: `static constexpr size_t AMG_MAXSOLVERITERCOUNT = 45;`

2. `NumericalParameters.h:30` — поле:
   До: `int AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT;`
   После: `size_t AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT;`

3. `NumericalParameters.h:68` — getter, убрать каст:
   До: `size_t CurrentAMG_maxSolverIterationCount() const { return static_cast<size_t>(AMG_maxSolverIterCount); }`
   После: `size_t CurrentAMG_maxSolverIterationCount() const { return AMG_maxSolverIterCount; }`

4. `NumericalParameters.cpp:34` — литерал `15` → `size_t{15}`:
   До: `AMG_maxSolverIterCount = std::max(15, AMG_maxSolverIterCount);`
   После: `AMG_maxSolverIterCount = std::max(size_t{15}, AMG_maxSolverIterCount);`

5. `NumericalParameters.cpp:24` — `+= 1` — ОК для `size_t`, не менять.
6. `NumericalParameters.cpp:39` — `std::min(AMG_maxSolverIterCount, AMG_MAXSOLVERITERCOUNT)` — оба `size_t` после изменений, ОК.
7. `NumericalParameters.h:97` — `set_currentAMG_maxSolverIterationCount()` — присвоение `AMG_MAXSOLVERITERCOUNT` → оба `size_t`, ОК.

8. `test_NumericalParameters.cpp:94` — тестовый struct `TestableNumericalParameters`:
   До: `void set_AMG_maxSolverIterCount(int val) { AMG_maxSolverIterCount = val; }`
   После: `void set_AMG_maxSolverIterCount(size_t val) { AMG_maxSolverIterCount = val; }`
   До: `int get_AMG_maxSolverIterCount() const { return AMG_maxSolverIterCount; }`
   После: `size_t get_AMG_maxSolverIterCount() const { return AMG_maxSolverIterCount; }`
   Без этого: `return size_t` → `int` → warning C4244 → ошибка сборки (warnings = errors).

**Проверка после этого шага:**
- Сборка + тесты
- Getter возвращает `size_t` без каста — `Solve(size_t)` получает `size_t` напрямую

**Подводные камни:**
- ✅ `AMG_maxSolverIterCount += 1` — `size_t + int(1)` → промотируется в `size_t`. Корректно.
- ✅ `std::max(size_t{15}, size_t)` — одинаковые типы, компилируется.
- ⚠️ Закомментированный `AMG_maxSolverIterCount -= 3` — если раскомментируют без guard, underflow. Но это мёртвый код, не в scope.

**Зависимости:** нет
**Оценка:** ~5 строк, 5 мин

---

## Шаг 8: `update_currentAMGState` — `tuple<int, ...>` → `tuple<size_t, ...>`

**Цель:** устранить `static_cast<int>(res.iters)` в NewtonSolver и examples.

**Файлы:**
- `HydroSolver/Reservoir/NumericalParameters.h` (строка 106)
- `HydroSolver/Reservoir/NumericalParameters.cpp` (строка 14)
- `HydroSolver/Reservoir/NewtonSolver.cpp` (строка 55)
- `examples/ex_benchmark_series_cpr.cpp` (строки 219, 361)
- `examples/ex_benchmark_series_ts.cpp` (строка 170)
- `tests/test_amgcl_benchmark.cpp` (строки 230, 372)

**Контекст:**
`update_currentAMGState` принимает `tuple<int, double, bool>`. `res.iters` из AMGCL — `size_t`. Каст `size_t → int`.

Внутри функции `std::get<0>(AMGstate)` присваивается в `AMG_currentIterationCount` (тип `size_t`).

**Что сделать:**

1. Менять сигнатуру:
   До: `void update_currentAMGState(const std::tuple<int, double, bool>& AMGstate);`
   После: `void update_currentAMGState(const std::tuple<size_t, double, bool>& AMGstate);`

2. Обновить реализацию в .cpp: тип параметра.

3. Убрать касты во всех call sites:
   До: `{ static_cast<int>(res.iters), res.error, res.converged }`
   После: `{ res.iters, res.error, res.converged }`

   6 call sites: NewtonSolver.cpp:55, ex_benchmark_series_cpr.cpp:219,361, ex_benchmark_series_ts.cpp:170, test_amgcl_benchmark.cpp:230,372.

**Проверка после этого шага:**
- Сборка + тесты

**Подводные камни:**
- ⚠️ Внутри `update_currentAMGState` значение `std::get<0>` сохраняется в `AMG_currentIterationCount` — проверить его тип. Если `size_t` → корректно. Если `int` → будет implicit conversion.
- ✅ После шага 7 поле `AMG_maxSolverIterCount` — `size_t`, `AMG_currentIterationCount` — `size_t`, `std::get<0>(AMGstate)` — `size_t`. Типы согласованы, warnings не будет.

**Зависимости:** желательно после шага 7
**Оценка:** ~12 строк, 5 мин

---

## Шаг 9: `SparsityPattern` — `unsigned char eqNmbr_` касты

**Цель:** устранить `static_cast<size_t>(eqNmbr_) * eqNmbr_` и `static_cast<size_t>(0)`.

**Файлы:**
- `HydroSolver/Solver/Math/SparsityPattern.cpp` (строки 146, 158)

**Контекст:**
`eqNmbr_` — `unsigned char` в `CRSStructure` (обычно 2 — двухфазная система). Каст нужен, чтобы `eqNmbr_ * eqNmbr_` не переполнился (unsigned char max = 255, 255*255 = 65025 — вмещается в `unsigned int`, но `vector<bool>(eqNmbr_ * eqNmbr_)` ожидает `size_t`).

**Что сделать:**

1. Строка 146: `std::erase(offDiagBlocks_raw, static_cast<size_t>(0))` → `std::erase(offDiagBlocks_raw, size_t{0})`. Или просто `std::erase(offDiagBlocks_raw, size_t(0))`.
   Альтернатива: `offDiagBlocks_raw` — `vector<size_t>`, `std::erase(vec, 0)` — литерал `0` имеет тип `int`. Менять на `size_t{0}` — семантически яснее без `static_cast`.

2. Строка 158: `std::vector<bool>(static_cast<size_t>(eqNmbr_) * eqNmbr_, true)`
   → `std::vector<bool>(size_t{eqNmbr_} * eqNmbr_, true)`
   Или: сохранить как есть — `unsigned char → size_t` — безопасное расширение.
   Или: менять `eqNmbr_` → `size_t` в `CRSStructure` — но это затронет десятки мест и `unsigned char` здесь семантически корректен (число уравнений всегда 1–4).

   **Решение:** заменить `static_cast<size_t>` на `size_t{...}` — минимальное изменение, убирает `static_cast`:
   `std::vector<bool>(size_t{eqNmbr_} * eqNmbr_, true)`

**Проверка после этого шага:**
- Сборка + тесты

**Подводные камни:**
- ✅ `unsigned char` → `size_t` — narrowing нет, `size_t{eqNmbr_}` компилируется.

**Зависимости:** нет
**Оценка:** ~2 строки, 2 мин

---

## Шаг 10: Прочие касты в tests и examples

**Цель:** устранить оставшиеся `static_cast` в тестовом/example-коде.

**Файлы:**
- `tests/test_3d_completions.cpp` (строки 296–302, 436–437)
- `tests/test_helpers.h` (строки 83, 94–98, 101, 113)
- `tests/well_schedule_builder.h` (строки 51, 59–63, 66–67)
- `examples/ex_benchmark_series_cpr.cpp` (строки 446, 470)
- `examples/ex_benchmark_series_ts.cpp` (строки 255, 279)
- `HydroSolver/Helpers/DataPrinter.h` (строка 56)

**Что сделать:**

1. `test_3d_completions.cpp` — `static_cast<size_t>(125.0 / hx)` — вычисление индекса из координат. Каст `double → size_t` — обоснован (координаты делятся нацело). Оставить, но можно заменить на `size_t(...)` если стилистически предпочтительнее. **Оставляем.**

2. `test_helpers.h` — `static_cast<float>(...)` для MER-записей, `static_cast<double>(horizon.grid_size.Nz)` → нужен для деления. `static_cast<size_t>(nz)` → можно обойтись без каста, если `nz` уже `size_t`. Рефакторинг: `double nz = static_cast<double>(horizon.grid_size.Nz)` → если `Nz` — `size_t`, деление `1.0 / Nz` промотирует в double. Но `nz` используется для арифметики с `double` — каст обоснован. **Оставляем.**

3. `well_schedule_builder.h` — `static_cast<float>(...)` для MER-записей, `static_cast<int>(std::ceil(...))` → `int n_records`. Касты обоснованы (формат записи). **Оставляем.**

4. `examples` — `static_cast<double>(r.total_amg_iters) / r.n_amg_solves` — деление `size_t / size_t` нужно быть вещественным. Аналогично шагу 6: `r.total_amg_iters * (1.0 / r.n_amg_solves)` или `r.total_amg_iters / double(r.n_amg_solves)`. **Оставляем** — это diagnostics, каст явный и безопасный.

5. `DataPrinter.h:56` — `static_cast<size_t>(3)` → `size_t{3}` или `size_t(3)`.

**Решение:** в этом шаге меняем только тривиальные литеральные касты (`static_cast<size_t>(3)` → `size_t{3}`). Остальные (float для MER, double для деления) — обоснованы и остаются.

**Проверка после этого шага:**
- Сборка + тесты

**Зависимости:** нет
**Оценка:** ~2 строки, 2 мин

---

## Шаг 11: Финальная проверка

**Цель:** убедиться, что задача завершена.

**Что сделать:**

1. `cmake --build build --config Release -- /clp:WarningsOnly` — 0 warnings
2. `ctest --test-dir build -C Release` — 313 тестов зелёных
3. Grep `static_cast` в HydroSolver/ — подсчитать оставшиеся:
   - Ожидание: `SomeFlowField.cpp:105` (бинарный формат), `Config_JSON.h:35` (JSON parsing), `MER_Descriptor.cpp:80,107` (временна́я арифметика), `AbstractGrid.h:144,150` (`size_t → long int`), `JacobianAssembler.cpp:33` (OpenMP требует signed int), `ReservoirSimulator.cpp:135` (`long int → size_t`, cellIdx проверен ≥ 0), `MathRoutines.cpp:56` (`double → ptrdiff_t` через floor) — все обоснованы
4. Grep `static_cast` в tests/ — ожидание: `test_helpers.h` (float/double для MER), `well_schedule_builder.h` (float/int для MER), `test_3d_completions.cpp` (координаты)
   Grep `static_cast` в examples/ — ожидание: `example_runner.h:189` (`wstreambuf*`), `ex_benchmark_series_*.cpp` (double деление для diagnostics)
5. Обновить vault:
   - Статус DEBT-058 в `технический долг.md` → `✅ реализовано <дата>`
   - Комментарий в GitHub issue #39
   - Обновить `текущие приоритеты.md`

**Зависимости:** все предыдущие шаги
**Оценка:** 5 мин

---

## Чеклист подводных камней (фаза 2.5)

- [x] **Все call sites найдены:** grep по `operator[]`, `GetNeighboursPointer`, `CommonEdgeArea`, `update_currentAMGState`, `CurrentAMG_maxSolverIterationCount`, `LowerPoint*Mesh`, `create_uniform_mesh`, `FlowField::operator[]`, `AMG_maxSolverIterCount` (включая тестовый struct `TestableNumericalParameters` в `test_NumericalParameters.cpp`), `check_nnz_count` (в `assembly_test_helpers.h`)
- [x] **Потокобезопасность:** ⚠️ OpenMP `#pragma omp parallel for` в `JacobianAssembler.cpp:33` и `NewtonSolver.cpp:71` — переменные цикла `int l` НЕ менять на `size_t` (MSVC OpenMP 2.0 требует signed). `#define USE_PARALLEL` активен в `Defines.h:7`. `UpdateGrid` — не использует `operator[]`
- [x] **Зависимости сборки:** CMakeLists.txt не затрагивается (только .h/.cpp)
- [x] **Обратная совместимость API:** `operator[](int)` остаётся, добавляется перегрузка. `update_currentAMGState` — internal API.
- [x] **Тесты:** все 313 существующих тестов должны проходить без изменений
- [x] **Кодировки:** не затрагиваются
- [x] **Платформозависимость:** `size_t` = 64-bit на x64 Windows, `int` = 32-bit. `ptrdiff_t` = 64-bit. Изменения безопасны.
- [x] **Мёртвый код:** не удаляется
- [x] **Связь с другими задачами:** DEBT-059 закрыт (не конфликтует). Нет активных веток, затрагивающих те же файлы.
- [x] **Производительность:** перегрузка `operator[](size_t)` — один уровень индирекции меньше (нет проверки `idx < 0`). Для hot loops (OilTotal, WaterTotal) — микроскопическое ускорение.

## Обнаруженные проблемы

1. `AbstractGrid.h:144,150` — `static_cast<long int>(activeCellsNmbr)` и `-static_cast<long int>(inActiveCellsNmbr)` — `activeCellsNmbr` и `inActiveCellsNmbr` имеют тип `size_t`, каст в `long int` для хранения в `cell_idx_Global2Local` (вектор `long int`). Каст обоснован — отрицательные значения нужны для кодирования неактивных ячейок. Не блокирует DEBT-058.

2. `ReservoirSimulator.cpp` — множество мест с `Grid[l_Local]` где `l_Local` — `long int` из `ConvertGlobal2Local`. Implicit conversion `long int → int` для `operator[](int)`. После шага 1 с перегрузкой `operator[](size_t)` — может возникнуть ambiguity `long int` → `int` vs `long int` → `size_t`. **Решение:** На Windows (LLP64) `long int` = `int` = 32-bit. Overload resolution: `long int → int` — identity (exact match), `long int → size_t` — integral conversion. `operator[](int)` побеждает однозначно. Ambiguity и warnings нет. ✅ Безопасно.

3. ~~Шаг 7~~ — решено: поле `AMG_maxSolverIterCount` → `size_t`, getter возвращает `size_t` без каста, `Solve(size_t)` получает `size_t` напрямую. Проблемы нет.

4. **OpenMP-ограничение** (найдено аудитом): `JacobianAssembler.cpp:33` и `NewtonSolver.cpp:71` — `#pragma omp parallel for` с `int l`. MSVC OpenMP 2.0 не поддерживает unsigned переменные цикла. `#define USE_PARALLEL` активен (`Defines.h:7`). Каст `static_cast<int>(grid.ActiveCellsNmbr())` в этих циклах **обоснован** и не может быть устранён без отказа от OpenMP.

5. **`long int` vs `ptrdiff_t`** (не в scope): `ConvertGlobal2Local` возвращает `long int`, а по vault-решению [[индексация через size_t и ptrdiff_t а не long]] должен `ptrdiff_t`. На Windows `long int` = `int` = 32-bit, что может быть недостаточно для очень крупных сеток. Выделить в отдельную задачу, если потребуется.

6. **Шаг 5 — signed/unsigned comparison** (найдено аудитом): `MathRoutines.cpp:27` — `x_idx > x_mesh.size() - 2` → если `x_idx` станет `ptrdiff_t`, сравнение `ptrdiff_t > size_t` даст warning C4018. Нужен каст. `SomeFlowField.cpp:71` — аналогично `lowT_idx == time.size() - 1`.

## Критерии завершения

- [ ] Все 11 шагов выполнены
- [ ] 313 тестов зелёные
- [ ] 0 warnings
- [ ] Grep `static_cast` в HydroSolver/ — только обоснованные касты (бинарный I/O, JSON parsing, временна́я арифметика, Global2Local encoding)
- [ ] Vault обновлён: статус DEBT-058, `текущие приоритеты.md`
- [ ] GitHub issue #39 прокомментирован

## Связанные заметки

- [[технический долг]] — секция DEBT-058
- [[индексация через size_t и ptrdiff_t а не long]] — архитектурное решение: `size_t` для беззнаковых индексов, `ptrdiff_t` для знаковых. `long` и `int` не используются для индексации. Шаг 5 (`MathRoutines → ptrdiff_t`) прямо реализует это решение
- [[debt-022 activecellsnmbr-size-t]] — дочерняя задача (завершена)
- [[debt-028 neibidx-size-t]] — дочерняя задача (завершена)
- [[debt-039 amg-itercount-type]] — дочерняя задача (завершена)
- [[debt-021 int-size-t-warnings]] — дочерняя задача (завершена)
