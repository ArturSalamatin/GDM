---
tags:
  - план
  - баг
date: 2026-06-29
issue: BUG-019
github: 3
branch: fix/bug-019/sparsity-pattern-debug-abort
status: готов к реализации
---

# BUG-019: SparsityPattern — abort в Debug при пустых массивах

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[SparsityPattern abort в Debug при пустых массивах]] — подробный анализ
   - [[стратегия тестирования GDM]] — паттерны тестов
3. Создай ветку: `git checkout -b fix/bug-019/sparsity-pattern-debug-abort`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline (ожидаемо: 6 тестов SparsityPattern, все зелёные в Release)
7. Собери Debug и воспроизведи баг:
   ```powershell
   cmake --build build --config Debug
   ctest --test-dir build -C Debug -R "SparsityPattern: single cell"
   ```
   Ожидаемо: abort, Debug Assertion Failed
8. Начни с шага 1. После каждого шага: сборка + тесты

## Описание бага

Конструктор `SparsityPattern` (уровень 2 в пирамиде тестов: средняя глубина зависимостей) содержит два независимых дефекта, оба проявляются только в Debug-сборке MSVC:

1. **`*std::max_element()` на пустом контейнере** — UB, ловится Debug assertion
2. **`vector::erase()` без сохранения итератора** — инвалидация итератора, ловится Debug iterator debugging

В Release оба дефекта молчат: assertions отключены, UB не детектируется.

## Цепочка причинно-следственных связей

### Проблема 1: `max_element` на пустом range

```
SparsityPattern(eqNmbr=2, cellNmbr=1, graph={{}}, blockPattern)  [SparsityPattern.cpp:57]
  → для 1 ячейки без соседей: offDiagBlocks_raw остаётся пустым   [cpp:88-89: resize на 0]
  → конструктор безусловно вызывает printOffDiagBlocks()           [cpp:161]
  → printOffDiagBlocks() → printOffDiagBlocks2Stream()             [cpp:32-33]
  → *std::max_element(offDiagBlocks_raw.begin(), end())            [.h:108]
  → offDiagBlocks_raw пуст → max_element возвращает end()
  → разыменование end() = UB → Debug Assertion "can't dereference out of range vector iterator"
```

Аналогичная проблема для `col_raw` (`.h:60`) и `diagBlocks_raw` (`.h:86`): при определённых конфигурациях (пустой blockPattern, 0 ненулевых элементов) эти контейнеры тоже могут быть пусты.

### Проблема 2: `erase` без сохранения итератора

```
SparsityPattern конструктор                                        [SparsityPattern.cpp:57]
  → erase-цикл для очистки нулей из offDiagBlocks_raw             [cpp:143-149]
  → offDiagBlocks_raw.erase(l) без l = ...erase(l)                [cpp:146]
  → erase инвалидирует все итераторы ≥ точки удаления
  → следующее сравнение l != end() использует инвалидированный итератор
  → Debug Assertion "vector iterators incompatible"
```

## Целевое состояние

1. Тесты "SparsityPattern: single cell has 1 block" и "SparsityPattern: partial block pattern (diagonal only)" проходят и в Debug, и в Release
2. Конструктор не делает файловый I/O (print-вызовы убраны) — решает DEBT-044
3. Print-методы (`printPattern`, `printDiagonalBlocks`, `printOffDiagBlocks`) безопасны при пустых контейнерах
4. Erase-цикл корректен или заменён на идиоматичный `std::erase`
5. Все существующие тесты зелёные (6 тестов SparsityPattern + все остальные)

## Анализ вариантов решения

### Вариант A: Минимальный фикс

**Суть:** guard-проверки на пустоту в print-методах + fix erase.

**Изменения:**
- `SparsityPattern.h:59-61` — `if (col_raw.empty()) return s;`
- `SparsityPattern.h:85-87` — `if (diagBlocks_raw.empty()) return s;`
- `SparsityPattern.h:107-109` — `if (offDiagBlocks_raw.empty()) return s;`
- `SparsityPattern.cpp:146` — `l = offDiagBlocks_raw.erase(l);`

**Плюсы:** минимальный diff (4 строки), быстро, безопасно.
**Минусы:** конструктор продолжает делать файловый I/O. Засоряет cwd файлами `test_SparsityPattern*.txt`. DEBT-044 остаётся открытым.
**Трудоёмкость:** 2 файла, ~4 строки.

### Вариант B: Архитектурное решение (рекомендуется)

**Суть:** убрать I/O из конструктора + fix erase + guard в print-методах.

**Изменения:**
- `SparsityPattern.cpp:159-161` — удалить три print-вызова из конструктора
- `SparsityPattern.cpp:143-149` — заменить erase-цикл на `std::erase(offDiagBlocks_raw, 0)` (C++20, доступен при C++23)
- `SparsityPattern.h:58-61` — добавить `if (col_raw.empty()) return s;` в начало `printPattern2Stream`
- `SparsityPattern.h:84-87` — добавить `if (diagBlocks_raw.empty()) return s;` в начало `printDiagBlocks2Stream`
- `SparsityPattern.h:106-109` — добавить `if (offDiagBlocks_raw.empty()) return s;` в начало `printOffDiagBlocks2Stream`

**Плюсы:** устраняет корневую причину обоих дефектов. Конструктор чистый — нет I/O. Print-методы безопасны при любых данных. Закрывает DEBT-044. `std::erase` — идиоматичный C++20/23.
**Минусы:** чуть больший diff (~12 строк). Print-методы остаются в классе (пишут в файл), но больше не вызываются автоматически.
**Трудоёмкость:** 2 файла, ~12 строк.

### Выбор: Вариант B

Разница в трудоёмкости минимальна, а вариант B закрывает DEBT-044 и делает конструктор чистым. Guard-проверки в print-методах нужны в обоих вариантах (методы public-accessible через protected наследование), поэтому вариант A — подмножество варианта B.

## Чеклист подводных камней

- ✅ **Побочные эффекты:** grep по `SparsityPattern` в `.cpp/.h` — класс используется только в тестах (`tests/unit/math/test_SparsityPattern.cpp`). Нет production-потребителей. Изменение конструктора безопасно.
- ✅ **Потокобезопасность:** нет `#pragma omp`, `thread`, `mutex` в SparsityPattern. Код однопоточный.
- ✅ **Граничные случаи:** тесты покрывают: 0 ячеек (default конструктор), 1 ячейка без соседей, 2 ячейки, 3 ячейки, partial blockPattern. Фикс добавляет guard для пустых контейнеров — покрыто.
- ✅ **Производительность:** конструктор вызывается один раз при инициализации. Удаление print-вызовов *ускоряет* — нет файлового I/O.
- ✅ **Обратная совместимость:** print-методы остаются в классе. Единственное изменение поведения: конструктор больше не пишет файлы. Это улучшение, не regression.
- ✅ **Порядок вызовов:** print-вызовы стояли в конце конструктора после всей инициализации. Их удаление не влияет на порядок.
- ✅ **Состояние при ошибке:** если `std::erase` удаляет элементы, контейнер остаётся в валидном состоянии (стандартная гарантия).
- ✅ **Численная устойчивость:** неприменимо — дефект в отладочном выводе, не в вычислениях.
- ⚠️ **Связь с другими задачами:** BUG-013 (перепутаны аргументы `vector<bool>` в делегирующем конструкторе, строка 166) — в том же файле, но независимый баг. Не конфликтует. Адресован в разделе «Обнаруженные проблемы».
- ✅ **Зависимости сборки:** нет изменений в CMakeLists.txt, includes, линковке.

## Обнаруженные проблемы

Ни одна не блокирует BUG-019:

1. **BUG-013** (уже зарегистрирован): строка 166, `std::vector<bool>(true, eqNmbr_ * eqNmbr_)` — перепутаны аргументы. `true` → implicit conversion to `size_t` = 1. Результат: `blockPattern.size() == 1` вместо `eqNmbr²`. Конструктор с 3 аргументами не используется в тестах напрямую (тесты передают 4 аргумента), но BUG-013 остаётся латентным.

---

## Этапы

Минимальный фикс = архитектурное решение (вариант B объединяет оба), поэтому этапы объединены.

---

### Шаг 1: Заменить erase-цикл на `std::erase`

**Цель:** устранить UB от инвалидации итератора при удалении нулевых элементов из `offDiagBlocks_raw`.

**Файлы:** `HydroSolver/Solver/Math/SparsityPattern.cpp`

**Контекст:**
Конструктор `SparsityPattern` (строка 57) заполняет `offDiagBlocks_raw` индексами ненулевых блоков. После заполнения цикл (строки 143–149) удаляет элементы со значением 0. Цикл использует `offDiagBlocks_raw.erase(l)` без сохранения возвращённого итератора — это инвалидирует `l`, и дальнейшее использование `l` (сравнение с `end()` и инкремент) — UB.

Проект использует C++23 (`CMAKE_CXX_STANDARD 23`), поэтому доступен `std::erase(container, value)` из `<vector>` (добавлен в C++20).

**Что сделать:**
1. В файле `SparsityPattern.cpp`, строки 141–150: заменить весь erase-цикл на одну строку `std::erase(offDiagBlocks_raw, static_cast<size_t>(0));`

**Изменения (старый → новый код):**

До:
```cpp
			\ if there are false-values in the blockPattern, then corresponding elements should be erased from the offDiagBlocks_raw std::vector
			//////////////////
			for (auto l = offDiagBlocks_raw.begin(); l != offDiagBlocks_raw.end();)
			{// but hopefully, we never enter this loop
				if (*l == 0)
					offDiagBlocks_raw.erase(l);
				else
					++l;
			}
			//////////////////
```

После:
```cpp
			std::erase(offDiagBlocks_raw, static_cast<size_t>(0));
```

Примечание: `static_cast<size_t>(0)` нужен, потому что `offDiagBlocks_raw` имеет тип `vector<size_t>`, а литерал `0` — `int`. Без каста компилятор может выбрать неверную перегрузку или предупредить о сужающем преобразовании.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Сборка Debug: `cmake --build build --config Debug`
- Тест: `ctest --test-dir build -C Release -R "SparsityPattern"` — все 6 тестов зелёные
- Debug всё ещё будет падать (print-проблема не исправлена), это ожидаемо

**Подводные камни:**
- `std::erase` доступен в C++20+. Проект на C++23 — ✅.
- `std::erase` уже включён через `<vector>` (который включается в `SparsityPattern.h`) — дополнительных include не нужно.

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 3 (верификация Debug)

**Оценка:** ~1 строка замены, ~2 минуты

---

### Шаг 2: Убрать print-вызовы из конструктора и добавить guard в print-методы

**Цель:** устранить UB от `*max_element` на пустом контейнере. Сделать конструктор чистым (без I/O). Закрыть DEBT-044.

**Файлы:**
- `HydroSolver/Solver/Math/SparsityPattern.cpp`
- `HydroSolver/Solver/Math/SparsityPattern.h`

**Контекст:**
Конструктор `SparsityPattern` (строки 159–161 после шага 1, номера могут сдвинуться ~на -9 строк) безусловно вызывает `printPattern()`, `printDiagonalBlocks()`, `printOffDiagBlocks()`. Эти методы:
1. Открывают файлы `test_SparsityPattern*.txt` в cwd (DEBT-044)
2. Вызывают template-методы `print*2Stream()` из `.h`, которые содержат `*std::max_element(vec.begin(), vec.end())` без проверки на пустоту

При 1 ячейке без соседей `offDiagBlocks_raw` пуст → `max_element` возвращает `end()` → разыменование = UB → Debug abort.

Решение: (A) удалить print-вызовы из конструктора, (B) добавить early return в каждый `print*2Stream()` для пустых данных (защита на случай ручного вызова).

**Что сделать:**

1. В `SparsityPattern.cpp`, удалить три строки print-вызовов из конструктора. После шага 1 erase-цикл уже заменён на одну строку, и print-вызовы идут после `totalNmbrOfBlocks = ...`. Удалить:
   ```cpp
   printPattern();
   printDiagonalBlocks();
   printOffDiagBlocks();
   ```

2. В `SparsityPattern.h`, метод `printPattern2Stream` (строка ~58): добавить guard в начало тела функции:
   ```cpp
   if (col_raw.empty()) return s;
   ```

3. В `SparsityPattern.h`, метод `printDiagBlocks2Stream` (строка ~84): добавить guard в начало тела функции:
   ```cpp
   if (diagBlocks_raw.empty()) return s;
   ```

4. В `SparsityPattern.h`, метод `printOffDiagBlocks2Stream` (строка ~106): добавить guard в начало тела функции:
   ```cpp
   if (offDiagBlocks_raw.empty()) return s;
   ```

**Изменения (старый → новый код):**

**SparsityPattern.cpp — конструктор (конец):**

До:
```cpp
			totalNmbrOfBlocks = std::accumulate(blocksPerRow.begin(), blocksPerRow.end(), 0);

			printPattern();
			printDiagonalBlocks();
			printOffDiagBlocks();
		}
```

После:
```cpp
			totalNmbrOfBlocks = std::accumulate(blocksPerRow.begin(), blocksPerRow.end(), 0);
		}
```

**SparsityPattern.h — printPattern2Stream:**

До:
```cpp
		stream& printPattern2Stream(stream& s,
			const std::vector<size_t>& row_raw,
			const std::vector<size_t>& col_raw)
		{
			size_t width = 2 + (size_t)std::log10(
				*std::max_element(col_raw.begin(), col_raw.end())
				+ 1);
```

После:
```cpp
		stream& printPattern2Stream(stream& s,
			const std::vector<size_t>& row_raw,
			const std::vector<size_t>& col_raw)
		{
			if (col_raw.empty()) return s;
			size_t width = 2 + (size_t)std::log10(
				*std::max_element(col_raw.begin(), col_raw.end())
				+ 1);
```

**SparsityPattern.h — printDiagBlocks2Stream:**

До:
```cpp
		stream& printDiagBlocks2Stream(stream& s,
			const std::vector<bool>& blockPattern,
			const std::vector<size_t>& diagBlocks_raw,
			size_t size)
		{
			size_t blockSize = NmbrOfNonzerosPerUnitBlock();
			size_t width = 2 + (size_t)std::log10(
				*std::max_element(diagBlocks_raw.begin(), diagBlocks_raw.end())
				+ 1);
```

После:
```cpp
		stream& printDiagBlocks2Stream(stream& s,
			const std::vector<bool>& blockPattern,
			const std::vector<size_t>& diagBlocks_raw,
			size_t size)
		{
			if (diagBlocks_raw.empty()) return s;
			size_t blockSize = NmbrOfNonzerosPerUnitBlock();
			size_t width = 2 + (size_t)std::log10(
				*std::max_element(diagBlocks_raw.begin(), diagBlocks_raw.end())
				+ 1);
```

**SparsityPattern.h — printOffDiagBlocks2Stream:**

До:
```cpp
		stream& printOffDiagBlocks2Stream(stream& s,
			const std::vector<bool>& blockPattern,
			const std::vector<size_t>& offDiagBlocks_raw)
		{
			size_t blockSize = NmbrOfNonzerosPerUnitBlock();
			size_t width = 2 + (size_t)std::log10(
				*std::max_element(offDiagBlocks_raw.begin(), offDiagBlocks_raw.end())
				+ 1);
```

После:
```cpp
		stream& printOffDiagBlocks2Stream(stream& s,
			const std::vector<bool>& blockPattern,
			const std::vector<size_t>& offDiagBlocks_raw)
		{
			if (offDiagBlocks_raw.empty()) return s;
			size_t blockSize = NmbrOfNonzerosPerUnitBlock();
			size_t width = 2 + (size_t)std::log10(
				*std::max_element(offDiagBlocks_raw.begin(), offDiagBlocks_raw.end())
				+ 1);
```

**Проверка после этого шага:**
- Сборка Release: `cmake --build build --config Release`
- Сборка Debug: `cmake --build build --config Debug`
- Тесты Release: `ctest --test-dir build -C Release -R "SparsityPattern"` — все 6 зелёные
- Тесты Debug: `ctest --test-dir build -C Debug -R "SparsityPattern"` — все 6 зелёные (ранее 2 падали)
- Проверить, что `test_SparsityPattern*.txt` НЕ создаются в cwd при прогоне тестов

**Подводные камни:**
- Print-методы остаются в классе. Если кто-то вызовет их вручную с пустыми данными — guard защитит. Если с непустыми — всё работает как раньше.
- `printPattern()`, `printDiagonalBlocks()`, `printOffDiagBlocks()` (обёртки в `.cpp`) остаются. Они больше не вызываются автоматически, но доступны для отладки. Удалять их не нужно — это выходит за scope BUG-019.

**Зависимости:**
- Требует: шаг 1 (erase уже исправлен)
- Блокирует: шаг 3 (верификация)

**Оценка:** ~7 строк изменений, ~5 минут

---

### Шаг 3: Полная верификация

**Цель:** убедиться, что фикс не сломал ничего. Прогнать все тесты в обоих конфигурациях.

**Файлы:** нет изменений

**Что сделать:**

1. Release-тесты:
   ```powershell
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure
   ```
   Ожидаемо: все тесты зелёные, количество = baseline

2. Debug-тесты:
   ```powershell
   cmake --build build --config Debug
   ctest --test-dir build -C Debug --output-on-failure
   ```
   Ожидаемо: все тесты зелёные. Тесты "single cell" и "partial block pattern" проходят (ранее abort).

3. Проверить cwd:
   ```powershell
   Get-ChildItem test_SparsityPattern*.txt -ErrorAction SilentlyContinue
   ```
   Ожидаемо: файлы НЕ создаются (print-вызовы убраны из конструктора).

**Проверка после этого шага:**
- Все тесты зелёные в Release и Debug
- Нет побочных файлов `test_SparsityPattern*.txt`
- Количество тестов = baseline (нет пропущенных, нет лишних)

**Зависимости:**
- Требует: шаги 1, 2

**Оценка:** ~5 минут

---

### Шаг 4: Обновить vault и GitHub issue

**Цель:** зафиксировать результат в системе знаний.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md`
- `vault/GDM/knowledge/debugging/SparsityPattern abort в Debug при пустых массивах.md`

**Что сделать:**

1. В `известные баги и технический долг.md`, запись BUG-019: изменить статус `🔴 ОТКРЫТ` → `✅ исправлено <дата>`
2. В записи DEBT-044: изменить статус, добавить ссылку на BUG-019 фикс (print-вызовы убраны из конструктора)
3. В `SparsityPattern abort в Debug при пустых массивах.md`: добавить раздел «Решение» с описанием того, что было сделано
4. Прокомментировать GitHub issue #3:
   ```powershell
   gh issue comment 3 --repo ArturSalamatin/GDM --body "Исправлено в ветке fix/bug-019/sparsity-pattern-debug-abort. Erase-цикл заменён на std::erase, print-вызовы убраны из конструктора, guard добавлен в print-методы."
   ```
5. Если пользователь подтверждает — закрыть issue

**Зависимости:**
- Требует: шаг 3 (верификация пройдена)

**Оценка:** ~5 минут

---

## Тестовая стратегия

Новых тестов не требуется. Существующие тесты уже покрывают оба дефекта:

**Тест:** `SparsityPattern: single cell has 1 block`
**Тег:** `[unit][level2][math][SparsityPattern]`
**Файл:** `tests/unit/math/test_SparsityPattern.cpp:50-58` (существующий)
**Сценарий:** 1 ячейка без соседей — `offDiagBlocks_raw` пуст
**Setup:** `graph = {{}}`, `blockPattern = {true, true, true, true}`, `eqNmbr=2`, `cellNmbr=1`
**Ожидание:** `TotalNmbrOfBlocks == 1`, `Row().size() == 3`
**Предотвращает:** `*max_element` на пустом `offDiagBlocks_raw`

**Тест:** `SparsityPattern: partial block pattern (diagonal only)`
**Тег:** `[unit][level2][math][SparsityPattern]`
**Файл:** `tests/unit/math/test_SparsityPattern.cpp:60-70` (существующий)
**Сценарий:** 2 ячейки, blockPattern с false-элементами — erase-цикл активируется
**Setup:** `graph = {{1}, {0}}`, `blockPattern = {true, false, false, true}`, `eqNmbr=2`, `cellNmbr=2`
**Ожидание:** `NmbrOfNonzerosPerUnitBlock == 2`, `TotalNmbrOfBlocks == 4`, `Row().back() == 8`
**Предотвращает:** инвалидация итератора при `erase` + `*max_element` на потенциально пустых/модифицированных контейнерах

**Regression:** все 6 тестов SparsityPattern + все остальные тесты проекта.

## Критерии завершения

- [x] Тест-воспроизводитель "single cell" зелёный в Debug
- [x] Тест "partial block pattern" зелёный в Debug
- [ ] Все 6 тестов SparsityPattern зелёные в Release
- [ ] Все 6 тестов SparsityPattern зелёные в Debug
- [ ] Все остальные тесты проекта зелёные в Release
- [ ] Файлы `test_SparsityPattern*.txt` не создаются при прогоне тестов
- [ ] Vault обновлён: BUG-019 → статус ✅, DEBT-044 → обновлён
- [ ] Заметка в debugging обновлена решением
- [ ] GitHub issue #3 прокомментирован

## Связанные задачи

| ID | Описание | Статус | Влияние на BUG-019 |
|---|---|---|---|
| DEBT-044 | SparsityPattern конструктор пишет файлы в cwd | ОТКРЫТ | Закрывается этим фиксом (print убраны) |
| DEBT-009 | test*.txt дамп в cwd | ОТКРЫТ | Частично (SparsityPattern больше не дампит) |
| BUG-013 | SparsityPattern аргументы `vector<bool>` перепутаны | ОТКРЫТ | Не блокирует, независимый баг |

## Оценка

- **Файлов:** 2 (`SparsityPattern.cpp`, `SparsityPattern.h`)
- **Строк изменений:** ~12
- **Шагов:** 4 (2 кода + 1 верификация + 1 vault)
- **Время:** ~15 минут
