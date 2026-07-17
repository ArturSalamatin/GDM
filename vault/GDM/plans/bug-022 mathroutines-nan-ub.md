---
tags:
  - план
  - баг
date: 2026-07-17
issue: BUG-022
github: 32
branch: fix/bug-022/mathroutines-nan-ub
status: в процессе
audit:
  date: 2026-07-17
  round: 1
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# BUG-022: `LowerPointNonUniformMesh` — `return NAN` из функции `int` (UB)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[модуль линий тока Anomaly FlowField]]
3. Создай ветку: `git checkout -b fix/bug-022/mathroutines-nan-ub experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Краткое описание

Функция `MathRoutines::LowerPointNonUniformMesh` объявлена с возвратом `int`, но использует `return NAN` для сигнализации «точка вне сетки». По стандарту C++ (§7.3.10) преобразование NaN в целочисленный тип — **undefined behavior**. В call site `SomeFlowField::operator()` результат используется как индекс массива без проверки → потенциальный out-of-bounds доступ.

Дополнительно в том же файле 2 warning C4244:
- Строка 56: `std::floor()` возвращает `double`, усечение при `return` в `int`
- Строка 64: `std::distance()` возвращает `ptrdiff_t`, усечение при `return` в `int`

---

## Фаза 1: Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `HydroSolver/Solver/Math/MathRoutines.cpp` | 52–65 | определения `LowerPointUniformMesh`, `LowerPointNonUniformMesh` |
| `HydroSolver/Solver/Math/MathRoutines.h` | 50, 52 | объявления обеих функций |
| `HydroSolver/Anomaly/FlowField/SomeFlowField.cpp` | 66–84 | call site `LowerPointNonUniformMesh` — результат как индекс без проверки |
| `HydroSolver/Solver/Math/MathRoutines.cpp` | 24–43 | `InterpFieldConstTime` — call site `LowerPointUniformMesh`, уже проверяет `< 0` |
| `tests/unit/geometry/test_MathRoutines.cpp` | 79–117 | тесты для обеих функций |

---

## Фаза 2: Анализ

### 2.1. Воспроизведение

**Compile-time:** `cmake --build build --config Release` — MSVC выдаёт C4244 на строках 56, 62, 64 файла `MathRoutines.cpp`.

**Runtime UB:** вызов `LowerPointNonUniformMesh({0.0, 1.0, 2.0}, -1.0)`. Функция возвращает `NAN`, преобразованный в `int` — произвольное значение. Существующие тесты этот путь не проверяют (нет теста для точки вне сетки).

### 2.2. Цепочка причинно-следственных связей

1. **Триггер:** `SomeFlowField::operator()(queryT, queryP)` вызывается с `queryT` вне диапазона `time[]`
2. **Ошибка:** `LowerPointNonUniformMesh(time, queryT)` выполняет `return NAN` (строка 62) — UB, результат произвольный
3. **Распространение:** `auto lowT_idx` получает мусорное значение (строка 68 SomeFlowField.cpp)
4. **Проявление:** `time[lowT_idx]`, `field[lowT_idx]` — out-of-bounds доступ → crash или повреждение данных

В нормальном потоке выполнения `queryT` всегда лежит в диапазоне `time[]` (трассировка траекторий идёт в пределах расчётного времени), поэтому баг не проявлялся в тестах. Но стандарт не гарантирует никакого поведения — оптимизатор может удалить ветку с `if (queryX < mesh[0] || ...)` целиком, считая что UB невозможен.

### 2.3. Целевое состояние

1. `LowerPointNonUniformMesh` возвращает `-1` при `queryX` вне сетки (а не NAN)
2. `SomeFlowField::operator()` проверяет `lowT_idx < 0` перед использованием как индекса
3. Все 3 warning C4244 в `MathRoutines.cpp` устранены
4. Семантика `LowerPointUniformMesh` не меняется — он уже может возвращать отрицательное значение (тест строка 102 проверяет `< 0`)
5. `InterpFieldConstTime` уже проверяет `if (x_idx < 0 ...)` (строка 27) — не требует изменений

### 2.4. Варианты решения

**Вариант A: `return -1` как sentinel (рекомендуемый)**
- `return NAN` → `return -1`
- `static_cast<int>` для `std::floor` и `std::distance`
- Call site проверяет `< 0`
- **Плюсы:** минимальный diff, согласованная семантика с `LowerPointUniformMesh`, `InterpFieldConstTime` уже умеет обрабатывать `< 0`
- **Минусы:** sentinel value — не идиоматический C++. Но альтернатива (`std::optional`) — overkill для внутренней утилиты, а каскадные изменения не оправданы масштабом проблемы

**Вариант B: `std::optional<int>`**
- Возвращаемый тип → `std::optional<int>`, `return NAN` → `return std::nullopt`
- **Плюсы:** типобезопасно
- **Минусы:** каскадные изменения call sites (`InterpFieldConstTime` использует `x_idx < 0`, придётся переписывать; `SomeFlowField` тоже). Объём изменений несоразмерен проблеме

### 2.5. Выбор: Вариант A

Вариант A минимален и согласован с существующей семантикой. `LowerPointUniformMesh` уже де-факто использует отрицательные значения как сигнал «вне сетки» (тест строка 99–103). `InterpFieldConstTime` уже проверяет `< 0` (строка 27). Фиксим `NonUniform` в ту же конвенцию.

### 2.6. Поиск подводных камней

- ✅ **Побочные эффекты:** `LowerPointNonUniformMesh` вызывается из 1 места (`SomeFlowField::operator()` строка 68). `LowerPointUniformMesh` вызывается из 1 места (`InterpFieldConstTime` строка 26, 29). `InterpFieldConstTime` вызывается из `SomeField2D::operator()` (строка 51 `SomeFlowField.h`). Граф вызовов полностью прослежен
- ✅ **Потокобезопасность:** все функции pure — нет shared state, нет OpenMP
- ✅ **Граничные случаи:** `queryX == mesh[0]` — нижняя граница включена (текущий код: `<`, не `<=`); `queryX == mesh.back()` — верхняя граница включена (текущий код: `>`, не `>=`). Проверяется существующим тестом `LowerPointNonUniformMesh: query at first node`
- ✅ **Производительность:** `static_cast<int>` — zero-cost; замена `return NAN` → `return -1` — zero-cost
- ✅ **Обратная совместимость:** в нормальном потоке (queryX в диапазоне) поведение идентично. Вне диапазона — было UB, станет определённое `-1`
- ⚠️ **SomeFlowField call site:** текущий код НЕ проверяет результат `LowerPointNonUniformMesh` на ошибку → нужно добавить проверку `< 0`. Адресовано в шаге 3
- ✅ **Связь с другими задачами:** DEBT-025 (`const`-корректность `SomeFlowField`) — не конфликтует, разные строки
- ✅ **Зависимости сборки:** изменения только в `.cpp`/`.h`, без CMake

### 2.7. Обнаруженные проблемы

Нет новых проблем. Упомянутые в DEBT-021 warning C4477 (`SomeFlowField.cpp:90` — `snprintf %u` для `size_t`) и C4267 (`SomeFlowField.cpp:103`) — отдельная задача, вне скоупа BUG-022.

---

## Фаза 3: Детализация

Минимальный фикс = архитектурное решение — этапы объединены.

### Шаг 1: Фикс `LowerPointUniformMesh` — устранить C4244 (std::floor → int)

**Цель:** устранить warning C4244 на строке 56 — `std::floor()` возвращает `double`, неявное усечение при `return` в `int`.

**Файлы:** `HydroSolver/Solver/Math/MathRoutines.cpp`

**Контекст:**
`LowerPointUniformMesh` ищет индекс ячейки на равномерной сетке по формуле `floor((queryX - x0) / hx)`. Результат `std::floor` — `double`, функция возвращает `int`. Для точек ниже сетки результат отрицателен — это легитимная семантика, используемая в `InterpFieldConstTime` (строка 27: `if (x_idx < 0 ...)`). Нужно явное приведение.

**Что сделать:**
1. В файле `MathRoutines.cpp`, строка 56: добавить `static_cast<int>` вокруг `std::floor(...)`

**Изменения:**

До:
```cpp
	return std::floor((queryX - x0) / hx); // get the lower index
```

После:
```cpp
	return static_cast<int>(std::floor((queryX - x0) / hx));
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — warning C4244 на строке 56 исчез
- Тесты: `ctest --test-dir build -C Release -R "LowerPointUniformMesh"` — все 4 теста зелёные
- Регрессия: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- Нет. `static_cast<int>(std::floor(...))` — каноническая форма для этого паттерна

**Зависимости:**
- Нет
- Блокирует: —

**Оценка:** ~1 строка, ~2 минуты

---

### Шаг 2: Фикс `LowerPointNonUniformMesh` — `return NAN` → `return -1`, устранить C4244

**Цель:** устранить UB (`return NAN` из функции `int`) и два warning C4244 (строки 62, 64).

**Файлы:** `HydroSolver/Solver/Math/MathRoutines.cpp`

**Контекст:**
`LowerPointNonUniformMesh` ищет индекс ячейки на неравномерной сетке бинарным поиском (`upper_bound`). Если `queryX` вне диапазона — текущий код делает `return NAN`, что является UB (NaN → int). Замена на `return -1` согласована с семантикой `LowerPointUniformMesh` (который уже может возвращать отрицательное) и с call site `InterpFieldConstTime`, где проверяется `if (x_idx < 0 ...)`.

Строка 64: `std::distance()` возвращает `ptrdiff_t` (signed, 8 байт на x64), неявное усечение до `int` (4 байта). Для сеток в GDM (≤10000 ячеек) усечение безопасно, но нужен явный `static_cast`.

**Что сделать:**
1. Строка 62: `return NAN` → `return -1`
2. Строка 64: `return iter` → `return static_cast<int>(iter)`

**Изменения:**

До:
```cpp
 int math_routines::MathRoutines::LowerPointNonUniformMesh(const std::vector<double>& mesh, double queryX)
{
	if (queryX < mesh[0] || queryX > mesh.back())
		return NAN;
	auto iter = std::distance(mesh.begin(), upper_bound(mesh.begin(), mesh.end(), queryX)) - 1;
	return iter;
}
```

После:
```cpp
 int math_routines::MathRoutines::LowerPointNonUniformMesh(const std::vector<double>& mesh, double queryX)
{
	if (queryX < mesh[0] || queryX > mesh.back())
		return -1;
	auto iter = std::distance(mesh.begin(), upper_bound(mesh.begin(), mesh.end(), queryX)) - 1;
	return static_cast<int>(iter);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — все 3 C4244 из MathRoutines.cpp исчезли
- Тесты: `ctest --test-dir build -C Release -R "LowerPointNonUniformMesh"` — 2 теста зелёные
- Регрессия: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- Семантика `queryX == mesh.back()`: текущий код проверяет `queryX > mesh.back()`, т.е. равенство допускается. `upper_bound` для `queryX == mesh.back()` вернёт `mesh.end()`, `std::distance - 1` = `mesh.size() - 1` — последний элемент. Это корректно.
- Семантика `queryX == mesh[0]`: проверка `queryX < mesh[0]`, равенство допускается. `upper_bound` для `queryX == mesh[0]` вернёт итератор на `mesh[1]`, `distance - 1 = 0`. Корректно, подтверждается тестом `query at first node`.

**Зависимости:**
- Требует: —
- Блокирует: шаг 3 (call site зависит от семантики `-1`)

**Оценка:** ~2 строки, ~3 минуты

---

### Шаг 3: Защита call site `SomeFlowField::operator()`

**Цель:** добавить проверку `lowT_idx < 0` в `SomeFlowField::operator()`, чтобы при `queryT` вне диапазона `time[]` не было out-of-bounds доступа.

**Файлы:** `HydroSolver/Anomaly/FlowField/SomeFlowField.cpp`

**Контекст:**
`SomeFlowField::operator()(queryT, queryP)` вызывает `LowerPointNonUniformMesh(time, queryT)` (строка 68) и использует результат как индекс массивов `time[]` и `field[]`. После фикса шага 2 при `queryT` вне диапазона функция вернёт `-1` вместо UB. Нужна проверка.

Поведение при ошибке: вернуть `{0.0, 0.0}` — нулевую скорость. Это согласовано с boundary handling в модуле FlowField: заметка [[модуль линий тока Anomaly FlowField]] описывает «за пределами сетки → {0,0} → траектория останавливается». Вызывающий код (`IntegrateODE` через `f_const`) уже обрабатывает нулевую скорость — траектория просто перестаёт двигаться.

Аналогично нужна защита от `lowT_idx >= time.size() - 1` после декремента: если `lowT_idx == time.size() - 1`, текущий код делает `lowT_idx--`, что корректно. Но если `time.size() == 0` — деление на ноль в `time.size() - 1` (unsigned underflow). Однако `time` всегда непустой (заполняется в `AddFlowFieldSnapShot`), поэтому дополнительная проверка на пустоту избыточна.

**Что сделать:**
1. Строка 68–72 SomeFlowField.cpp: после вызова `LowerPointNonUniformMesh` добавить проверку `< 0`

**Изменения:**

До:
```cpp
		std::pair<double, double> SomeFlowField::operator()(double queryT, const phasePortrait::Point& queryP)
		{
			auto lowT_idx = math_routines::MathRoutines::LowerPointNonUniformMesh(time, queryT);
			if (lowT_idx == time.size() - 1)
				lowT_idx--;
			auto upT_idx = lowT_idx + 1;
```

После:
```cpp
		std::pair<double, double> SomeFlowField::operator()(double queryT, const phasePortrait::Point& queryP)
		{
			auto lowT_idx = math_routines::MathRoutines::LowerPointNonUniformMesh(time, queryT);
			if (lowT_idx < 0)
				return {0.0, 0.0};
			if (lowT_idx == time.size() - 1)
				lowT_idx--;
			auto upT_idx = lowT_idx + 1;
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release -R "streamline"` — тесты линий тока зелёные
- Регрессия: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- Тип `lowT_idx`: `auto` выводит `int` (возвращаемый тип `LowerPointNonUniformMesh`). Сравнение `int < 0` корректно
- `time.size() - 1`: `size_t - 1` при пустом `time` → underflow. Но `time` гарантированно непустой (минимум 1 snapshot). Не добавляем проверку, чтобы не раздувать скоуп

**Зависимости:**
- Требует: шаг 2 (иначе `LowerPointNonUniformMesh` возвращает UB, а не -1)
- Блокирует: —

**Оценка:** ~2 строки, ~3 минуты

---

### Шаг 4: Тест — `LowerPointNonUniformMesh` вне диапазона возвращает -1

**Цель:** добавить тесты, проверяющие поведение `LowerPointNonUniformMesh` при `queryX` вне диапазона сетки.

**Файлы:** `tests/unit/geometry/test_MathRoutines.cpp`

**Контекст:**
Существующие тесты покрывают только `queryX` внутри диапазона (строки 107–117). Нет тестов для `queryX < mesh[0]` и `queryX > mesh.back()`. Для `LowerPointUniformMesh` тест на `query below mesh returns negative` есть (строка 99), а для `NonUniform` — нет. Тест фиксирует контракт: при выходе за диапазон возвращается отрицательное значение.

**Что сделать:**
1. После теста `LowerPointNonUniformMesh: query at first node` (строка ~117) добавить 2 теста: query ниже и query выше сетки

**Изменения:**

После строки 117 (после закрывающей `}` теста `query at first node`) добавить:

```cpp
TEST_CASE("LowerPointNonUniformMesh: query below mesh returns negative",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 0.5, 1.5, 4.0, 10.0};
    CHECK(MathRoutines::LowerPointNonUniformMesh(mesh, -1.0) < 0);
}

TEST_CASE("LowerPointNonUniformMesh: query above mesh returns negative",
          "[unit][level1][geometry][MathRoutines]") {
    std::vector<double> mesh = {0.0, 0.5, 1.5, 4.0, 10.0};
    CHECK(MathRoutines::LowerPointNonUniformMesh(mesh, 15.0) < 0);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release -R "LowerPointNonUniformMesh"` — 4 теста зелёные (2 старых + 2 новых)
- Регрессия: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- Нет. Тесты проверяют `< 0`, не конкретное `-1` — это даёт свободу изменить sentinel в будущем (например на `std::optional`), не ломая тесты

**Зависимости:**
- Требует: шаг 2 (без фикса тесты бы проходили только случайно — UB может дать любое значение)
- Блокирует: —

**Оценка:** ~10 строк, ~3 минуты

---

## Тестовая стратегия

### Тест-воспроизводитель

**Тест:** `LowerPointNonUniformMesh: query below mesh returns negative`
**Тег:** `[unit][level1][geometry][MathRoutines]`
**Файл:** `tests/unit/geometry/test_MathRoutines.cpp` (новый)
**Сценарий:** вызов `LowerPointNonUniformMesh` с `queryX < mesh[0]`
**Ожидание:** возвращаемое значение `< 0`
**Предотвращает:** UB от `return NAN` в функции `int`

### Граничный тест

**Тест:** `LowerPointNonUniformMesh: query above mesh returns negative`
**Тег:** `[unit][level1][geometry][MathRoutines]`
**Файл:** `tests/unit/geometry/test_MathRoutines.cpp` (новый)
**Сценарий:** вызов `LowerPointNonUniformMesh` с `queryX > mesh.back()`
**Ожидание:** возвращаемое значение `< 0`
**Предотвращает:** UB и out-of-bounds при обращении за верхнюю границу

### Regression

Существующие тесты, которые не должны сломаться:
- `LowerPointUniformMesh: query at mesh node` — индекс на узле
- `LowerPointUniformMesh: query between nodes` — индекс между узлами
- `LowerPointUniformMesh: query at origin` — нижняя граница
- `LowerPointUniformMesh: query below mesh returns negative` — ниже сетки
- `LowerPointNonUniformMesh: query between nodes` — индекс между узлами
- `LowerPointNonUniformMesh: query at first node` — нижняя граница
- `InterpFieldConstTime: interpolates on 3x3 grid` — билинейная интерполяция
- Все тесты `[streamlines]` — `Streamlines: single injector radial`, `Streamlines: two wells`

---

## Критерии завершения

- [ ] Warning C4244 на строках 56, 62, 64 `MathRoutines.cpp` — устранены
- [ ] `return NAN` заменён на `return -1` — UB устранён
- [ ] `SomeFlowField::operator()` проверяет `lowT_idx < 0` — out-of-bounds защищён
- [ ] 2 новых теста зелёные
- [ ] Все существующие тесты зелёные (Release + Debug)
- [ ] Vault обновлён: BUG-022 → `✅ исправлено`
- [ ] GitHub issue #32 прокомментирован

---

## Связанные заметки

- [[модуль линий тока Anomaly FlowField]] — архитектура FlowField, boundary handling
- [[debt-021 int-size-t-warnings]] — задача, при аудите которой обнаружен BUG-022
