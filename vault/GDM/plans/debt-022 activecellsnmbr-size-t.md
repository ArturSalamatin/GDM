---
tags:
  - план
  - рефакторинг
date: 2026-07-17
issue: DEBT-022
github: 33
branch: refactor/debt-022/activecellsnmbr-size-t
status: готов к реализации
audit:
  date: 2026-07-17
  round: 2
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  note: повторный аудит подтвердил корректность после автофикса раунда 1
---

# DEBT-022: `activeCellsNmbr` int → size_t

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-022/activecellsnmbr-size-t`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Baseline: 312 тестов, все зелёные
6. Начни с шага 1. После каждого шага: сборка + тесты

---

## Текущее состояние

Файл: `HydroSolver/Solver/Grids/AbstractGrid.h`

```cpp
// строки 31-33
int activeCellsNmbr = 0;
int inActiveCellsNmbr = 0;
size_t totalCellNmbr = 0;
```

`ActiveCellsNmbr()` (строка 123) возвращает `size_t` — неявное расширение `int → size_t`.

### Downstream-использования `activeCellsNmbr`

| Строка | Контекст | Проблема |
|---|---|---|
| 123 | `size_t ActiveCellsNmbr() const { return activeCellsNmbr; }` | неявный `int → size_t` |
| 144 | `cell_idx_Global2Local[l] = activeCellsNmbr;` | `int` присваивается в `vector<long int>` |
| 145 | `activeCellsNmbr++;` | инкремент `int` |
| 212 | `int N = activeCellsNmbr;` | `int` для `reserve(N)` |
| 285 | `for (int l = 0; l < activeCellsNmbr; l++)` | `int` цикл (остаток DEBT-021) |

### Downstream-использования `inActiveCellsNmbr`

| Строка | Контекст | Проблема |
|---|---|---|
| 149 | `inActiveCellsNmbr++;` | инкремент `int` |
| 150 | `cell_idx_Global2Local[l] = -inActiveCellsNmbr;` | отрицательный маркер неактивной ячейки |

### Семантика маркера неактивных ячеек

`cell_idx_Global2Local` имеет тип `vector<long int>`. Для **активных** ячеек значение = локальный индекс (≥ 0). Для **неактивных** — отрицательное число: `-1`, `-2`, ... (равно `-inActiveCellsNmbr` после инкремента).

Все call sites проверяют `l_Local > -1` перед использованием как индекса (строки 630, 703, 760, 832, 887, 929, 970 в `ReservoirSimulator.cpp`). Исключение — строки 182, 194, 203 (`GetOilSaturationField` и аналоги) — они проходят по `TotalCellsNmbr()` и используют `ConvertGlobal2Local` без guard-а, но все ячейки в текущих тестах активны.

### Дополнительные связи

- `connectivityGraph` — `vector<vector<int>>`, хранит локальные индексы. `connections.push_back(cell_idx_Global2Local[...])` — `long int → int` (строки 238, 245, 252, 259). Но `active_cells[l]` проверяется заранее, поэтому только неотрицательные значения попадают в `connections`
- `GetNeighboursPointer(int l)`, `GetNeighboursIdx(int l)` — принимают `int` (строки 61, 110). Это DEBT-021 остаток, но не блокирует данную задачу
- Тесты в `test_JacobianAssembly.cpp` используют `static_cast<int>(grid.ActiveCellsNmbr())` (строки 155, 241, 477, 578, 684) — не сломается, т.к. `ActiveCellsNmbr()` уже возвращает `size_t`
- `using base::activeCellsNmbr` (строка 165) — в `SomeStructuredGrid3Dim`, нужен для доступа к protected полю

---

## Целевое состояние

```cpp
// строки 31-33
size_t activeCellsNmbr = 0;
size_t inActiveCellsNmbr = 0;
size_t totalCellNmbr = 0;
```

- Геттер `ActiveCellsNmbr()` — без неявного каста
- Маркер неактивных: `cell_idx_Global2Local[l] = -static_cast<long int>(inActiveCellsNmbr)` — семантика та же, каст явный
- `int N = activeCellsNmbr` → `size_t N`
- `for (int l = 0; l < activeCellsNmbr; l++)` → `for (size_t l = 0; ...)`

---

## Варианты решения

### Вариант A: только `activeCellsNmbr` → `size_t`

- 3 изменения (строки 31, 212, 285)
- Минимальный diff
- `inActiveCellsNmbr` остаётся `int` — нет проблемы с маркером
- Непоследовательность: два счётчика рядом — разные типы

### Вариант B (рекомендуемый): оба поля → `size_t`

- 6 изменений (строки 31, 32, 144, 150, 212, 285)
- Все три счётчика — `size_t`, согласованно
- Строка 144: `= activeCellsNmbr` — нужен `static_cast<long int>`, иначе C4267 (size_t → long int на MSVC x64)
- Строка 150: `= -inActiveCellsNmbr` — нужен явный каст, т.к. отрицание `size_t` без каста = огромное положительное число
- Риск: если каст на строке 150 сделать неправильно, маркер неактивных сломается

---

## Подводные камни

- ✅ **Все call sites найдены:** grep завершён, все использования задокументированы выше
- ✅ **Потокобезопасность:** `activeCellsNmbr` модифицируется только при построении сетки (конструктор), не в OpenMP-циклах
- ✅ **Зависимости сборки:** изменения только в одном `.h` файле, CMake не затрагивается
- ✅ **Обратная совместимость API:** `ActiveCellsNmbr()` уже возвращает `size_t`, публичный API не меняется
- ✅ **Тесты:** `static_cast<int>(grid.ActiveCellsNmbr())` в тестах — продолжат работать
- ⚠️ **Маркер неактивных:** строка 150, `= -inActiveCellsNmbr` — при `size_t` нужен явный `= -static_cast<long int>(inActiveCellsNmbr)`. Если забыть каст → `long int = -(size_t)` = огромное положительное, guard `> -1` пропустит
- ⚠️ **Сужение size_t → long int:** строка 144, `cell_idx_Global2Local[l] = activeCellsNmbr` — после смены на `size_t`, присвоение `size_t` (8 байт) в `long int` (4 байта на MSVC x64) — сужающее преобразование, может породить C4267. Фикс: `cell_idx_Global2Local[l] = static_cast<long int>(activeCellsNmbr);`
- ✅ **Связь с другими задачами:** не конфликтует. DEBT-028 (int neibIdx) — независимый. `using base::activeCellsNmbr` продолжит работать
- ✅ **Параллельные ветки:** нет активных веток, затрагивающих AbstractGrid.h

---

## Шаги

### Шаг 1: `activeCellsNmbr` и `inActiveCellsNmbr` → `size_t`

**Цель:** сменить тип обоих полей-счётчиков с `int` на `size_t` и адаптировать все downstream-использования в `AbstractGrid.h`.

**Файлы:** `HydroSolver/Solver/Grids/AbstractGrid.h`

**Что сделать:**

1. Строка 31: `int activeCellsNmbr = 0;` → `size_t activeCellsNmbr = 0;`

2. Строка 32: `int inActiveCellsNmbr = 0;` → `size_t inActiveCellsNmbr = 0;`

3. Строка 144: присвоение в `cell_idx_Global2Local` — добавить явный каст для предотвращения C4267:
   ```
   До:  cell_idx_Global2Local[l] = activeCellsNmbr;
   После: cell_idx_Global2Local[l] = static_cast<long int>(activeCellsNmbr);
   ```
   Почему: `cell_idx_Global2Local` — `vector<long int>`, а `long int` на MSVC x64 = 4 байта. `size_t` = 8 байт — сужающее преобразование без каста даст warning C4267.

4. Строка 150: маркер неактивных ячеек — КРИТИЧНО:
   ```
   До:  cell_idx_Global2Local[l] = -inActiveCellsNmbr;
   После: cell_idx_Global2Local[l] = -static_cast<long int>(inActiveCellsNmbr);
   ```
   Почему: отрицание `size_t` без каста даст `size_t` (огромное положительное число), которое при присвоении в `long int` — implementation-defined. Явный каст в `long int` до отрицания гарантирует корректный отрицательный маркер.

5. Строка 212: `int N = activeCellsNmbr;` → `size_t N = activeCellsNmbr;`

6. Строка 285: `for (int l = 0; l < activeCellsNmbr; l++)` → `for (size_t l = 0; l < activeCellsNmbr; l++)`

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 312 тестов должны остаться зелёными

**Подводные камни:**
- Маркер строки 150 — точка риска. Guard `l_Local > -1` в `ReservoirSimulator.cpp` должен продолжить работать: `long int(-1) > -1` = false ✓
- Строка 144 — без каста будет warning C4267 (size_t → long int)

**Оценка:** 6 изменений в 1 файле, ~5 минут

---

### Шаг 2: проверка чистоты warnings

**Цель:** убедиться, что изменения не породили новые warnings.

**Что сделать:**

1. Полная пересборка:
   ```powershell
   cmake --build build --config Release -- /t:Rebuild 2>&1 | Select-String "warning C" | Where-Object { $_.Line -notmatch "amgcl|Catch2|Eigen|xutility|xmemory|xstring" }
   ```

2. Ожидание: 0 warnings из проектного кода

**Подводные камни:**
- Если `connections.push_back(cell_idx_Global2Local[...])` (строки 238, 245, 252, 259) начнёт давать warning `long int → int` — это существующая проблема (до наших изменений тоже `long int → int`), не регрессия

**Оценка:** ~3 минуты (ожидание rebuild)

---

## Критерии завершения

- [ ] `size_t activeCellsNmbr` и `size_t inActiveCellsNmbr` в `AbstractGrid.h`
- [ ] Активных: `static_cast<long int>(activeCellsNmbr)` в строке 144
- [ ] Маркер неактивных: `-static_cast<long int>(inActiveCellsNmbr)` в строке 150
- [ ] `size_t N` в `SetConnectivityGraph_3D`
- [ ] `size_t l` в `printConnectivity`
- [ ] Все 312 тестов зелёные
- [ ] 0 новых warnings из проектного кода
- [ ] Vault обновлён (статус DEBT-022)
- [ ] GitHub issue #33 прокомментирован

---

## Обнаруженные проблемы

1. **Строки 182, 194, 203** (`GetOilSaturationField`, `GetWaterSaturationField`, `GetPressureField`): `Grid[Grid.ConvertGlobal2Local(l)]` без guard на отрицательные значения. Сейчас работает потому что все ячейки активны в текущих тестах. При неактивных ячейках — UB. Это отдельная проблема, не блокирует DEBT-022.

2. **`connectivityGraph` = `vector<vector<int>>`**, хранит `long int` через неявное сужение. Отдельная задача типизации, не блокирует.

---

## Связанные заметки

- [[известные баги и технический долг]] — секция DEBT-022
- Related: #31 (DEBT-021, закрыт)
- DEBT-058 в `технический долг.md` — общая задача типизации int/size_t (связана)
