---
tags:
  - план
  - баг
date: 2026-07-02
issue: BUG-013
github: 9
branch: fix/bug-013/sparsity-pattern-args-order
status: реализован
audit:
  date: 2026-07-02
  pass: 2
  findings: 0 / 0 / 2
  auto-fixed: 2
  manual-required: 0
---

# BUG-013: SparsityPattern 3-arg конструктор — аргументы vector\<bool\> перепутаны

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[SparsityPattern abort в Debug при пустых массивах]] — BUG-019 (исправлен, но описывает архитектуру класса)
   - [[юнит-тесты уровень 2 средняя глубина зависимостей]] — паттерны тестов для SparsityPattern
3. Создай ветку: `git checkout -b fix/bug-013/sparsity-pattern-args-order experimental`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   if ($?) { cmake --build build --config Release }
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Baseline: 284 теста, ~277 сек (Release)
7. Баг латентен — воспроизвести можно только после добавления теста (шаг 2)
8. Начни с шага 1. После каждого шага: сборка + тесты

## Суть бага

### Корневая причина

В файле `HydroSolver/Solver/Math/SparsityPattern.cpp`, строка 151-153, 3-аргументный (convenience) конструктор `SparsityPattern` делегирует к 4-аргументному, формируя `blockPattern` по умолчанию (все блоки ненулевые):

```cpp
SparsityPattern::SparsityPattern(
    const unsigned char eqNmbr_, const int cellNmbr,
    const std::vector<std::vector<int>>& connectivityGraph) :
  SparsityPattern(eqNmbr_, cellNmbr,
    connectivityGraph, std::vector<bool>(true, eqNmbr_* eqNmbr_)) {}
```

Аргументы `std::vector<bool>` перепутаны:
- `std::vector<bool>(true, eqNmbr_ * eqNmbr_)` — первый аргумент `true` неявно преобразуется в `size_t(1)`, создавая вектор **размера 1**
- Правильно: `std::vector<bool>(eqNmbr_ * eqNmbr_, true)` — размер `eqNmbr²`, все элементы `true`

### Цепочка причинно-следственных связей

1. **Триггер:** вызов `SparsityPattern(eqNmbr, cellNmbr, graph)` с `eqNmbr > 1`
2. **Место ошибки:** строка 153 — `vector<bool>(true, N)` создаёт вектор размера 1
3. **Делегирование к 4-arg:** `blPattern` размера 1 вместо `eqNmbr²`
4. **Распространение в теле 4-arg конструктора:**
   - `blockSize = blPattern.size() = 1` (строка 64) — неверный размер блока
   - `blockPattern.begin() + (i + 1) * eqNmbr` при `eqNmbr=2` = `begin() + 2` → **OOB** (строка 71)
   - `blockPattern[eqNmbr * blockPatternRow + eqI]` при индексе > 0 → **OOB** (строки 103, 112, 120, 132)
   - `offDiagBlocks_raw` resize по `blockSize=1` → неверные размеры → OOB записи (строка 125)
5. **Проявление:** crash в Debug (assertion failed), silent data corruption в Release

### Текущее состояние

3-arg конструктор **не вызывается** нигде в production-коде и тестах. Все пути создания `SparsityPattern` используют 4-arg конструктор с явным `blockPattern`. Баг латентен, но при первом использовании 3-arg конструктора — немедленный crash или повреждение CRS-матрицы.

### Дополнительная проблема: тип cellNmbr

3-arg конструктор принимает `const int cellNmbr` (строка 151), а 4-arg — `const size_t cellNmbr` (строка 58). При делегировании происходит implicit conversion `int → size_t`. Для отрицательных значений — UB. Нужно согласовать типы.

### Целевое состояние

- 3-arg конструктор создаёт `blockPattern` размера `eqNmbr²`, все элементы `true`
- Результат идентичен 4-arg конструктору с `{true, true, ..., true}` (eqNmbr² штук)
- `NmbrOfNonzerosPerUnitBlock() == eqNmbr²`
- Все CRS-массивы (`Row`, `Col`, `DiagBlocks`, `OffDiagBlocks`) совпадают между 3-arg и 4-arg

### Связанные задачи

- BUG-019 ([#3](https://github.com/ArturSalamatin/GDM/issues/3)) — SparsityPattern abort в Debug при пустых массивах. Исправлен 2026-06-29. Не конфликтует.
- DEBT-045 — рефакторинг интерфейса SparsityPattern (const/noexcept, контейнеры). Не конфликтует.

## Варианты решения

### Вариант A: поменять аргументы местами (минимальный)

```cpp
connectivityGraph, std::vector<bool>(eqNmbr_* eqNmbr_, true)) {}
```

- **Плюсы:** 1 строка
- **Минусы:** конструкция `vector<bool>(expr, true)` визуально неоднозначна — `expr` может быть bool-выражением
- **Риски:** нет (3-arg не вызывается в production)
- **Трудоёмкость:** 1 файл, 1 строка

### Вариант B: static_cast для явного типа + согласование cellNmbr

```cpp
SparsityPattern::SparsityPattern(
    const unsigned char eqNmbr_, const size_t cellNmbr,
    const std::vector<std::vector<int>>& connectivityGraph) :
  SparsityPattern(eqNmbr_, cellNmbr, connectivityGraph,
    std::vector<bool>(static_cast<size_t>(eqNmbr_) * eqNmbr_, true)) {}
```

- **Плюсы:** `static_cast<size_t>` делает тип первого аргумента явным — невозможно спутать с bool. Тип `cellNmbr` согласован с 4-arg конструктором
- **Минусы:** нет
- **Риски:** нет (3-arg не вызывается нигде)
- **Трудоёмкость:** 2 файла (h + cpp), 3 строки

### Выбор: вариант B

`static_cast<size_t>` делает первый аргумент `vector<bool>` гарантированно integral, а не boolean. Согласование `cellNmbr` убирает implicit int→size_t conversion.

## Подводные камни

- ✅ **Побочные эффекты:** 3-arg конструктор нигде не вызывается в production (grep подтверждает)
- ✅ **Потокобезопасность:** конструктор — не в OMP-секции
- ✅ **Граничные случаи:** eqNmbr=0 → size=0 (пустой blockPattern); eqNmbr=1 → size=1 (единичный блок)
- ✅ **Производительность:** конструктор вызывается один раз при setup
- ✅ **Обратная совместимость:** 3-arg не вызывается → не ломает ничего
- ✅ **Порядок вызовов:** делегирующий конструктор, порядок фиксирован стандартом
- ✅ **Связь с другими задачами:** BUG-019 исправлен, DEBT-045 не конфликтует
- ✅ **Зависимости сборки:** никаких изменений в includes/CMake

## Затронутые файлы

| Файл | Шаги | Роль |
|---|---|---|
| `HydroSolver/Solver/Math/SparsityPattern.cpp` | 1 | 3-arg конструктор — фикс аргументов |
| `HydroSolver/Solver/Math/SparsityPattern.h` | 1 | Объявление — согласование типа cellNmbr |
| `tests/unit/math/test_SparsityPattern.cpp` | 2 | Новые тесты для 3-arg конструктора |

---

## Шаги реализации

### Шаг 1: Фикс аргументов vector\<bool\> и согласование типа cellNmbr

**Цель:** Исправить порядок аргументов `std::vector<bool>` и привести тип `cellNmbr` к `size_t` для согласования с 4-arg конструктором.

**Файлы:**
- `HydroSolver/Solver/Math/SparsityPattern.h`
- `HydroSolver/Solver/Math/SparsityPattern.cpp`

**Контекст:**
Класс `SparsityPattern` (пространство имён `reservoir_simulator::linear_problem`) строит CRS-структуру (Compressed Row Storage) блочной разреженной матрицы по графу связности ячеек сетки. Каждый блок — подматрица `eqNmbr × eqNmbr`, где `eqNmbr` — число уравнений на ячейку (обычно 2: давление + насыщенность). `blockPattern` задаёт маску ненулевых элементов внутри блока.

4-arg конструктор (строка 57) принимает явный `blockPattern`. 3-arg конструктор (строка 151) — convenience, формирует full-block pattern (все элементы `true`) и делегирует к 4-arg. Из-за перепутанных аргументов `vector<bool>` создаётся вектор размера 1 вместо `eqNmbr²`.

**Что сделать:**

1. В файле `HydroSolver/Solver/Math/SparsityPattern.h`, строка ~50:
   Изменить тип `cellNmbr` с `int` на `size_t`

2. В файле `HydroSolver/Solver/Math/SparsityPattern.cpp`, строка ~151:
   - Изменить тип `cellNmbr` с `int` на `size_t`
   - Исправить аргументы `vector<bool>`: добавить `static_cast<size_t>` и поменять аргументы местами

**Важно:** блоки «До/После» ниже показывают логику изменений. Файл использует **табы** для indentation. При использовании Edit tool — копируй `old_string` из файла (Read), а не из блока «До:» в этом плане.

**Изменения (старый → новый код):**

Файл: `HydroSolver/Solver/Math/SparsityPattern.h`

До:
```cpp
			SparsityPattern(const unsigned char eqNmbr_, const int cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph);
```

После:
```cpp
			SparsityPattern(const unsigned char eqNmbr_, const size_t cellNmbr,
				const std::vector<std::vector<int>>& connectivityGraph);
```

Файл: `HydroSolver/Solver/Math/SparsityPattern.cpp`

До:
```cpp
		SparsityPattern::SparsityPattern(const unsigned char eqNmbr_, const int cellNmbr, const std::vector<std::vector<int>>& connectivityGraph) :
			SparsityPattern(eqNmbr_, cellNmbr,
				connectivityGraph, std::vector<bool>(true, eqNmbr_* eqNmbr_)) {}
```

После:
```cpp
		SparsityPattern::SparsityPattern(const unsigned char eqNmbr_, const size_t cellNmbr, const std::vector<std::vector<int>>& connectivityGraph) :
			SparsityPattern(eqNmbr_, cellNmbr, connectivityGraph,
				std::vector<bool>(static_cast<size_t>(eqNmbr_) * eqNmbr_, true)) {}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "SparsityPattern"` — все 6 существующих тестов зелёные
- Регрессия: `ctest --test-dir build -C Release` — все 284 теста зелёные
- Сборка Debug: `cmake --build build --config Debug`
- Тест Debug: `ctest --test-dir build -C Debug -R "SparsityPattern"` — 6 тестов зелёные

**Подводные камни:**
- Изменение типа `cellNmbr` в 3-arg конструкторе: нет вызывающего кода → безопасно

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~3 строки, ~2 минуты

---

### Шаг 2: Тесты 3-arg конструктора

**Цель:** Покрыть 3-arg конструктор тестами: воспроизводитель бага + сравнение с 4-arg + граничные случаи.

**Файлы:**
- `tests/unit/math/test_SparsityPattern.cpp`

**Контекст:**
Файл `tests/unit/math/test_SparsityPattern.cpp` содержит 6 тестов для `SparsityPattern`, все используют 4-arg конструктор. 3-arg конструктор не покрыт. Файл уже зарегистрирован в `CMakeLists.txt` (строка 172).

Теги тестов: `[unit][level2][math][SparsityPattern]` — согласно [[стратегия тестирования GDM]] и существующим тестам.

Геттеры для проверки (из `SparsityPattern.h`):
- `EqNmbr()` → `unsigned char`
- `NmbrOfNonzerosPerUnitBlock()` → `size_t` (= count of `true` в `blockPattern`)
- `TotalNmbrOfBlocks()` → `size_t`
- `Row()` → `const vector<size_t>&`
- `Col()` → `const vector<size_t>&`
- `DiagBlocks()` → `const vector<size_t>&`
- `OffDiagBlocks()` → `const vector<size_t>&`
- `NmbrOfElementsAboveBlockRow()` → `const vector<size_t>&`

**Что сделать:**

В файле `tests/unit/math/test_SparsityPattern.cpp`, после последнего теста (строка ~70), добавить 3 теста:

1. **Тест-воспроизводитель:** 3-arg конструктор с 2-cell chain, eqNmbr=2 → `NmbrOfNonzerosPerUnitBlock() == 4` (без фикса возвращает 1)

2. **Тест сравнения:** 3-arg и 4-arg конструкторы с одинаковым графом → все геттеры совпадают

3. **Граничный тест:** 3-arg конструктор с eqNmbr=1, single cell → `NmbrOfNonzerosPerUnitBlock() == 1`

**Изменения (новый код):**

Файл: `tests/unit/math/test_SparsityPattern.cpp`

Добавить после строки 70:

```cpp

TEST_CASE("SparsityPattern: 3-arg ctor blockSize equals eqNmbr squared",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1}, {0}};
    SparsityPattern sp(2, 2, graph);

    CHECK(sp.NmbrOfNonzerosPerUnitBlock() == 4);
    CHECK(sp.TotalNmbrOfBlocks() == 4);
    CHECK(sp.Row().size() == 5);
}

TEST_CASE("SparsityPattern: 3-arg ctor matches 4-arg with full block",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{1, 2}, {0, 2}, {0, 1}};
    std::vector<bool> fullBlock = {true, true, true, true};
    SparsityPattern sp4(2, 3, graph, fullBlock);
    SparsityPattern sp3(2, 3, graph);

    CHECK(sp3.EqNmbr() == sp4.EqNmbr());
    CHECK(sp3.NmbrOfNonzerosPerUnitBlock() == sp4.NmbrOfNonzerosPerUnitBlock());
    CHECK(sp3.TotalNmbrOfBlocks() == sp4.TotalNmbrOfBlocks());
    CHECK(sp3.Row() == sp4.Row());
    CHECK(sp3.Col() == sp4.Col());
    CHECK(sp3.DiagBlocks() == sp4.DiagBlocks());
    CHECK(sp3.OffDiagBlocks() == sp4.OffDiagBlocks());
    CHECK(sp3.NmbrOfElementsAboveBlockRow() == sp4.NmbrOfElementsAboveBlockRow());
}

TEST_CASE("SparsityPattern: 3-arg ctor single cell eqNmbr 1",
          "[unit][level2][math][SparsityPattern]") {
    std::vector<std::vector<int>> graph = {{}};
    SparsityPattern sp(1, 1, graph);

    CHECK(sp.NmbrOfNonzerosPerUnitBlock() == 1);
    CHECK(sp.TotalNmbrOfBlocks() == 1);
    CHECK(sp.Row().size() == 2);
    CHECK(sp.Row()[0] == 0);
    CHECK(sp.Row()[1] == 1);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "SparsityPattern"` — 9 тестов зелёные (6 старых + 3 новых)
- Регрессия: `ctest --test-dir build -C Release` — все 287 тестов зелёные
- Сборка Debug: `cmake --build build --config Debug`
- Тест Debug: `ctest --test-dir build -C Debug -R "SparsityPattern"` — 9 тестов зелёные

**Подводные камни:**
- 3-cell fully connected graph (`{{1,2},{0,2},{0,1}}`) → 9 блоков (3 диагональных + 6 off-diagonal). Достаточно сложный граф для проверки совпадения CRS-структуры
- eqNmbr=1 — единственный случай, где баг бы не проявился (vector<bool>(1, 1) = vector<bool>(true, 1)), поэтому тестируем отдельно как граничный случай

**Зависимости:**
- Требует: шаг 1 (без фикса тест-воспроизводитель упадёт — OOB в конструкторе)
- Блокирует: шаг 3

**Оценка:** ~35 строк, ~5 минут

---

### Шаг 3: Vault и GitHub

**Цель:** Обновить vault-записи и прокомментировать GitHub issue.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md`
- `vault/GDM/knowledge/debugging/BUG-013 sparsity pattern args order.md` (новый)
- `vault/GDM/00-home/index.md`

**Что сделать:**

1. Обновить статус BUG-013 в `vault/GDM/roadmap/известные баги и технический долг.md`:
   - Статус: ✅ исправлено YYYY-MM-DD
   - (Поля **План:** и **Ветка:** уже добавлены — не дублировать)

2. Создать заметку `vault/GDM/knowledge/debugging/BUG-013 sparsity pattern args order.md`:
   - Симптом, причина, решение
   - Ссылки на связанные заметки

3. Обновить `vault/GDM/00-home/index.md` — добавить заметку в секцию Debugging

4. Прокомментировать GitHub issue #9:
   ```powershell
   $env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
   gh issue comment 9 --repo ArturSalamatin/GDM --body "Исправлено: аргументы vector<bool> переставлены, тип cellNmbr согласован с 4-arg конструктором. Добавлены 3 теста: воспроизводитель, сравнение 3-arg/4-arg, граничный случай eqNmbr=1."
   ```

**Зависимости:**
- Требует: шаг 2
- Блокирует: ничего

**Оценка:** ~5 минут

---

## Тестовая стратегия

**Тест 1: Воспроизводитель (шаг 2)**
- **Тест:** `SparsityPattern: 3-arg ctor blockSize equals eqNmbr squared`
- **Тег:** `[unit][level2][math][SparsityPattern]`
- **Файл:** `tests/unit/math/test_SparsityPattern.cpp` (существующий)
- **Сценарий:** вызов 3-arg конструктора с eqNmbr=2, проверка `NmbrOfNonzerosPerUnitBlock() == 4`
- **Setup:** 2-cell chain graph `{{1},{0}}`
- **Ожидание:** без фикса — OOB/crash (eqNmbr=2, blockPattern.size()=1); с фиксом — `NmbrOfNonzerosPerUnitBlock() == 4`, `TotalNmbrOfBlocks() == 4`
- **Предотвращает:** перепутанные аргументы `vector<bool>` — класс ошибок implicit bool→size_t conversion

**Тест 2: Сравнение 3-arg / 4-arg (шаг 2)**
- **Тест:** `SparsityPattern: 3-arg ctor matches 4-arg with full block`
- **Тег:** `[unit][level2][math][SparsityPattern]`
- **Файл:** `tests/unit/math/test_SparsityPattern.cpp` (существующий)
- **Сценарий:** создать SparsityPattern двумя способами (3-arg и 4-arg с full block), сравнить все геттеры
- **Setup:** 3-cell fully connected graph `{{1,2},{0,2},{0,1}}`, eqNmbr=2
- **Ожидание:** все геттеры совпадают: `EqNmbr`, `NmbrOfNonzerosPerUnitBlock`, `TotalNmbrOfBlocks`, `Row`, `Col`, `DiagBlocks`, `OffDiagBlocks`, `NmbrOfElementsAboveBlockRow`
- **Предотвращает:** любое расхождение между convenience-конструктором и основным

**Тест 3: Граничный случай (шаг 2)**
- **Тест:** `SparsityPattern: 3-arg ctor single cell eqNmbr 1`
- **Тег:** `[unit][level2][math][SparsityPattern]`
- **Файл:** `tests/unit/math/test_SparsityPattern.cpp` (существующий)
- **Сценарий:** 3-arg конструктор с eqNmbr=1, single cell — вырожденный случай
- **Setup:** single cell graph `{{}}`, eqNmbr=1
- **Ожидание:** `NmbrOfNonzerosPerUnitBlock() == 1`, `TotalNmbrOfBlocks() == 1`, `Row().size() == 2`
- **Предотвращает:** регрессия для скалярного уравнения (eqNmbr=1)

**Regression:** все 284 существующих теста (Release + Debug) не должны сломаться.

**Visual:** не применимо — SparsityPattern — чисто алгебраическая структура без визуализации.

## Критерии завершения

- [ ] Тест-воспроизводитель зелёный (`SparsityPattern: 3-arg ctor blockSize equals eqNmbr squared`)
- [ ] Все 3 новых теста зелёные
- [ ] Все 284 существующих теста зелёные в Release
- [ ] Все тесты зелёные в Debug
- [ ] Vault обновлён: BUG-013 → статус ✅, заметка в debugging, index обновлён
- [ ] GitHub issue #9 прокомментирован с результатом
