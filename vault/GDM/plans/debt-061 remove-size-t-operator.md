---
tags:
  - план
  - рефакторинг
date: 2026-07-20
issue: DEBT-061
github: 45
branch: refactor/debt-061/remove-size-t-operator
status: реализован
audit:
  date: 2026-07-20
  round: 4
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-061: Удаление избыточных перегрузок `operator[](size_t)` в `SomeGrid`

## Мотивация

`SomeGrid` (шаблонный класс сетки в `AbstractGrid.h`) определяет 4 перегрузки `operator[]`:
- пара `(ptrdiff_t)` — с guard на отрицательные индексы → `CellsInactive` для неактивных ячеек
- пара `(size_t)` — прямой доступ к `Cells` без проверки

Перегрузки `(size_t)` избыточны: все call sites с `size_t` передают заведомо неотрицательные индексы (циклы по `ActiveCellsNmbr`), и `operator[](ptrdiff_t)` обработает их корректно через ветку `idx >= 0 → Cells[idx]`.

Наличие обеих пар создаёт **риск неверного overload resolution**: при вызове с `int`, `long`, `unsigned int` компилятор может выбрать не ту перегрузку — если выбрана `(size_t)`, отрицательный `ptrdiff_t` молча конвертируется в огромное беззнаковое → out-of-bounds.

Связано с: [[debt-060 long-int-to-ptrdiff]], [[debt-058 static-cast-cleanup]], BUG-023 (закрыт как ложноположительный).

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-061/remove-size-t-operator`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Запомни количество тестов и результат — это baseline
6. Начни с шага 1

## Анализ call sites

Все места вызова `operator[]` на объектах типа `SomeGrid` (или `OilField`):

### Call sites с `ptrdiff_t` (результат `ConvertGlobal2Local`) — не затрагиваются:
- `ReservoirSimulator.cpp:136` — `Grid[cellIdx]` где `cellIdx` = `ptrdiff_t`
- `ReservoirSimulator.cpp:178,190,199` — `Grid[Grid.ConvertGlobal2Local(l)]`
- `ReservoirSimulator.cpp:628,665,666,702,759,796,797,831` — `Grid[l_Local]` / `Grid[l_Local_Neighbour]` где `l_Local` = `ptrdiff_t`

### Call sites с `ptrdiff_t` (результат `ConvertGlobal2Local`) — не затрагиваются:
- `ReservoirSimulator.cpp:883,925,966,1013,1042` — `ptrdiff_t l = Grid.ConvertGlobal2Local(...)` → `Grid[l]` уже вызывает `operator[](ptrdiff_t)`
- `JacobianAssembler.cpp:158,219,282` — `ptrdiff_t l = grid.ConvertGlobal2Local(...)` → `grid[l]` уже вызывает `operator[](ptrdiff_t)`

### Call sites с `size_t` — нужно адаптировать:
- `ReservoirSimulator.cpp:139` — `Grid[size_t{0}]` → заменить на `Grid[ptrdiff_t{0}]`
- `ReservoirSimulator.cpp:349` — `Grid[l].OilMass()` где `for (size_t l = 0; l < ActiveCellsNmbr; ++l)`
- `ReservoirSimulator.cpp:356` — `Grid[l].WaterMass()` где `for (size_t l = 0; l < ActiveCellsNmbr; ++l)`
- `NewtonSolver.cpp:76,78` — `grid[sl]` где `sl` = `size_t` (внутри OpenMP parallel for)
- `JacobianAssembler.cpp:64` — `grid[l]` где `l` = `size_t` (параметр `fillMatrixBlockRow`)
- `test_JacobianAssembly.cpp:489,590,608,695,708` — `sim.Grid[l]` где `l` = `size_t`

### Прямой доступ к `Cells[]` внутри `AbstractGrid.h` — не затрагивается:
Строки 42, 50, 58, 67, 248–278 используют `Cells[...]` напрямую, минуя `operator[]`.

## Подводные камни

- ✅ **Все call sites найдены:** grep по `Grid[` и `grid[` в `*.cpp`, `*.h`
- ✅ **Потокобезопасность:** не затрагивается (operator[] — read-only доступ к элементу)
- ⚠️ **Warning C4267/C4018:** после удаления `operator[](size_t)` вызов `Grid[l]` с `size_t l` потребует неявной конверсии `size_t → ptrdiff_t` — MSVC на `/W3` даст C4267. Если изменить тип цикловой переменной на `ptrdiff_t`, а границу оставить `size_t`, MSVC даст C4018 (signed/unsigned comparison). Решение: кастить границу `static_cast<ptrdiff_t>(ActiveCellsNmbr)` (только 2 места). Где `l` используется в арифметике с `size_t` — кастить `l` при вызове `grid[static_cast<ptrdiff_t>(l)]`
- ✅ **Обратная совместимость API:** `operator[](ptrdiff_t)` принимает все целочисленные типы через implicit conversion
- ✅ **Тесты:** все используют `size_t` или `ptrdiff_t` — оба корректно конвертируются
- ✅ **Связь с другими задачами:** DEBT-060 (long int → ptrdiff_t) уже завершён, конфликтов нет

## Варианты решения

### Вариант A: Удалить `operator[](size_t)` + изменить тип переменных цикла

Удалить перегрузки `(size_t)`. В циклах, где `size_t l` используется **только** как аргумент `operator[]`, заменить тип на `ptrdiff_t`. Это устраняет implicit narrowing conversion.

**Плюсы:** нет warnings, нет кастов, единая точка доступа к ячейкам
**Минусы:** переменная цикла `ptrdiff_t` с границей `size_t` — MSVC `/W3` даст C4018 (signed/unsigned comparison) → кастить границу `static_cast<ptrdiff_t>(...)`

### Вариант B: Удалить `operator[](size_t)` + `static_cast` на call sites

Удалить перегрузки `(size_t)`. Добавить `static_cast<ptrdiff_t>(l)` на каждом call site.

**Плюсы:** типы циклов не меняются
**Минусы:** 12+ новых кастов — прямо противоречит DEBT-058

### Выбранный вариант: A

Минимум diff, нет кастов, нет warnings.

Однако в `fillMatrixBlockRow(size_t l, ...)` (JacobianAssembler) и `test_JacobianAssembly.cpp` — `l` используется и для `grid[l]`, и для арифметики с `size_t`. Менять тип нецелесообразно — здесь локальный `static_cast<ptrdiff_t>(l)` при вызове `grid[]`. Таких мест 6 (1 JacobianAssembler + 5 тесты). В `NewtonSolver.cpp` промежуточная `size_t sl` удаляется целиком — `grid[l]` с `int l` проходит через widening `int → ptrdiff_t` без warnings.

---

## Шаги реализации

### Шаг 1: Удалить `operator[](size_t)` из `AbstractGrid.h`

**Цель:** убрать избыточные перегрузки, оставив единую пару `operator[](ptrdiff_t)`

**Файлы:** `HydroSolver/Solver/Grids/AbstractGrid.h`

**Что сделать:**
1. Удалить строки 94–102 (обе перегрузки `operator[](size_t)`)

**Изменения:**

До (строки 93–102):
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

После: (удалено полностью)

**Проверка:** не компилировать — call sites ещё не адаптированы.

**Оценка:** удаление 9 строк

---

### Шаг 2: Адаптировать `ReservoirSimulator.cpp` — циклы по `ActiveCellsNmbr`

**Цель:** устранить implicit `size_t → ptrdiff_t` conversion в циклах, где `l` используется только для `Grid[l]`

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:** в `OilTotal()` и `WaterTotal()` переменная `l` используется **только** как аргумент `Grid[l]` — можно безопасно менять тип на `ptrdiff_t`.

**Что сделать:**
1. Строка 139: заменить `Grid[size_t{0}]` на `Grid[ptrdiff_t{0}]`
2. Строка 348: заменить `for (size_t l = 0; l < ActiveCellsNmbr; ++l)` на `for (ptrdiff_t l = 0; l < static_cast<ptrdiff_t>(ActiveCellsNmbr); ++l)`
3. Строка 355: аналогично

**Изменения:**

Строка 139:
```cpp
// До:
				cells_[l] = &(Grid[size_t{0}]);
// После:
				cells_[l] = &(Grid[ptrdiff_t{0}]);
```

Строки 348, 355:
```cpp
// До:
		for (size_t l = 0; l < ActiveCellsNmbr; ++l)
// После:
		for (ptrdiff_t l = 0; l < static_cast<ptrdiff_t>(ActiveCellsNmbr); ++l)
```

**Проверка:** не компилировать — остались ещё call sites.

**Оценка:** 3 строки

---

### Шаг 3: Адаптировать `JacobianAssembler.cpp`

**Цель:** устранить implicit conversion в `fillMatrixBlockRow`

**Файлы:** `HydroSolver/Reservoir/JacobianAssembler.cpp`

**Контекст:** `fillMatrixBlockRow(size_t l, ...)` — параметр `l` используется и для `grid[l]`, и для арифметики с `size_t` (позиции в CSR-матрице). Менять сигнатуру нецелесообразно. Локальный `static_cast` при вызове `grid[]`. Остальные call sites в этом файле (строки 166, 227, 290) уже используют `ptrdiff_t l` (результат `ConvertGlobal2Local`) — не затрагиваются.

**Что сделать:**

Строка 64 — заменить `grid[l]` на `grid[static_cast<ptrdiff_t>(l)]`.

**Изменения:**
```cpp
// До:
		const TwoPhaseFlowCell& cell = grid[l];
// После:
		const TwoPhaseFlowCell& cell = grid[static_cast<ptrdiff_t>(l)];
```

**Проверка:** не компилировать — остались тесты.

**Оценка:** 1 строка

---

### Шаг 4: Адаптировать `NewtonSolver.cpp`

**Цель:** устранить промежуточную переменную `sl`, использовать `grid[l]` напрямую

**Файлы:** `HydroSolver/Reservoir/NewtonSolver.cpp`

**Контекст:** строка 73 создаёт `size_t sl = l` исключительно для вызова `operator[](size_t)`. Переменная `sl` не используется нигде, кроме `grid[sl]` (строки 76, 78). Цикловая переменная `l` имеет тип `int` (строка 71 — OpenMP требует знаковый тип). После удаления перегрузки `(size_t)` вызов `grid[l]` с `int l` пройдёт через implicit widening conversion `int → ptrdiff_t` (32-bit signed → 64-bit signed) — без потери данных, без warnings на `/W3`. `static_cast` не нужен.

**Что сделать:**

1. Удалить строку 73: `size_t sl = l;`
2. Строки 76, 78: заменить `grid[sl]` на `grid[l]`

**Изменения:**
```cpp
// До:
			size_t sl = l;
			double corr[B];
			problem.UnpackCellCorrections(l, corr);
			grid[sl].UpdateState(corr);

			const std::vector<double>& stateVaiables = grid[sl].GetVariableFieldProperties();
// После:
			double corr[B];
			problem.UnpackCellCorrections(l, corr);
			grid[l].UpdateState(corr);

			const std::vector<double>& stateVaiables = grid[l].GetVariableFieldProperties();
```

**Проверка:** не компилировать — остались тесты.

**Оценка:** удаление 1 строки, замена 2 строк

---

### Шаг 5: Адаптировать `test_JacobianAssembly.cpp`

**Цель:** устранить implicit conversion в тестах

**Файлы:** `tests/unit/math/test_JacobianAssembly.cpp`

**Контекст:** строки 489, 590, 608, 695, 708 — `sim.Grid[l]` где `l` = `size_t`. Переменная `l` участвует в арифметике с `size_t`, менять тип нецелесообразно.

**Что сделать:**

Заменить `sim.Grid[l]` на `sim.Grid[static_cast<ptrdiff_t>(l)]` в 5 местах.

**Изменения:**
```cpp
// До:
        sim.Grid[l].UpdateState(corr);
// После:
        sim.Grid[static_cast<ptrdiff_t>(l)].UpdateState(corr);
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все тесты должны остаться зелёными (поведение не изменилось)

**Оценка:** 5 строк

---

### Шаг 6: Обновить vault

**Цель:** зафиксировать результат

**Что сделать:**
1. В `vault/GDM/roadmap/технический долг.md` — обновить статус DEBT-061 на `✅ исправлено <дата>`
2. Прокомментировать GitHub issue #45: результат, количество изменённых файлов

---

## Тестовая стратегия

Новые тесты **не нужны** — это рефакторинг без изменения поведения. Все существующие тесты служат regression suite.

Ключевые тесты, покрывающие `operator[]`:
- `test_smoke` — базовый прогон, `GetWaterSaturationField()` / `GetOilSaturationField()`
- `test_buckley_leverett` — все 6 тест-кейсов используют `GetWaterSaturationField()`
- `test_mass_balance` — `GetPressureField()`, `GetWaterSaturationField()`, `GetOilSaturationField()`
- `test_JacobianAssembly` — прямой доступ `sim.Grid[l]`
- `test_five_spot`, `test_3d_completions`, `test_pi_controller_integration` — `GetWaterSaturationField()`, `GetPressureField()`

## Критерии завершения

- [ ] Перегрузки `operator[](size_t)` удалены из `AbstractGrid.h`
- [ ] Все call sites адаптированы (нет warnings на `/W3`)
- [ ] Сборка чистая (0 warnings, 0 errors)
- [ ] Все существующие тесты зелёные
- [ ] Vault обновлён
- [ ] GitHub issue #45 прокомментирован
