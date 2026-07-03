---
tags:
  - план
  - баг
date: 2026-07-02
issue: BUG-018
github: 14
branch: fix/bug-018/snprintf-size-t-format
status: готов к реализации
audit:
  date: 2026-07-02
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# План: BUG-018 — `snprintf` с `%u` для `size_t` — UB на x64

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[известные баги]] (BUG-018)
3. Создай ветку: `git checkout -b fix/bug-018/snprintf-size-t-format experimental`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

**Baseline (experimental, 2026-07-02):** 290 тестов, ~61 сек Release.

---

## Описание проблемы

### Что происходит

Две функции вывода grid metadata используют `snprintf` с форматом `%u` (ожидает `unsigned int`, 4 байта) для аргументов типа `size_t` (8 байт на x64). Это Undefined Behavior по стандарту C: аргументы на стеке сдвигаются, и все значения после первого `size_t`-аргумента читаются по неправильным смещениям. Результат — мусор в выходных файлах `grid_descriptor.txt` и `MatLab/testPlanarMesh.txt`.

### Цепочка причинно-следственных связей

1. **Триггер:** вызов `RawHorizon::PrintModelData(frames)` из `CalculationManager.cpp:25` или `DataPrinter::PrintPlanarMesh()` из `DataPrinter.h:67`
2. **Место ошибки 1:** `RawHorizon.h:43` — `snprintf(buffer, 1000, "%u;%u;%u;%u\n...", grid_size.Nx, grid_size.Ny, grid_size.Nz, frames, ...)`. Все 4 аргумента — `size_t` (GridDescriptors.h:12-14, параметр `size_t frames`)
3. **Место ошибки 2:** `DataPrinter.h:54` — `snprintf(buffer, 1000, "%u;%u;%u;%u\n...", nx(), ny(), nz(), 3.0, ...)`. Первые 3 — `size_t` (ReservoirFactory.h:18-20), четвёртый — `double` литерал `3.0` (ещё хуже: `double` в vararg при ожидании `unsigned int`)
4. **Распространение:** `snprintf` читает 4 байта на каждый `%u`, но `size_t` кладёт 8 байт. Первый `%u` читает младшие 4 байта `Nx` (часто корректно при малых значениях). Второй `%u` читает старшие 4 байта `Nx` (ноль/мусор) вместо `Ny`. Все дальнейшие аргументы сдвинуты, включая `double` для `%E`
5. **Проявление:** мусорные значения в `grid_descriptor.txt` и `MatLab/testPlanarMesh.txt`. Не влияет на расчёты (вывод для визуализации), но делает файлы невалидными для MATLAB/Python-парсеров

### Затронутый код

```cpp
// RawHorizon.h:43 — PrintModelData
snprintf(buffer, 1000, "%u;%u;%u;%u\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
    grid_size.Nx, grid_size.Ny,    // size_t, size_t
    grid_size.Nz, frames,          // size_t, size_t
    grid_bounds.x_min, grid_bounds.y_min,
    grid_bounds.x_max, grid_bounds.y_max,
    block_size.step_x, block_size.step_y,
    0.0, 0.0);
```

```cpp
// DataPrinter.h:54 — PrintPlanarMesh
snprintf(buffer, 1000, "%u;%u;%u;%u\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
    reservoirIntantiator->nx(), reservoirIntantiator->ny(),  // size_t, size_t
    reservoirIntantiator->nz(), 3.0,                        // size_t, double(!)
    reservoirIntantiator->xmin(), reservoirIntantiator->ymin(),
    reservoirIntantiator->xmax(), reservoirIntantiator->ymax(),
    reservoirIntantiator->xstep(), reservoirIntantiator->ystep(), 0.0, 0.0);
```

Типы:
- `grid_size.Nx, Ny, Nz` — `size_t` (`GridDescriptors.h:12-14`)
- `nx(), ny(), nz()` — возвращают `size_t` (`ReservoirFactory.h:18-20`)
- `frames` — параметр `size_t` (`RawHorizon.h:37`)
- `3.0` — литерал `double` (закомментирован `reservoirIntantiator->nt()`, метод не существует)

### Целевое состояние

`snprintf` использует `%zu` — стандартный формат для `size_t` (C99+, поддерживается MSVC с VS2015). В `DataPrinter.h` четвёртый аргумент `3.0` заменён на `static_cast<size_t>(3)`. Grid metadata файлы содержат корректные значения.

---

## Варианты решения

### Вариант A: `%u` → `%zu` (рекомендуемый)

**Суть:** заменить `%u` на `%zu` в format string. В DataPrinter.h дополнительно `3.0` → `static_cast<size_t>(3)`.

```cpp
// RawHorizon.h
snprintf(buffer, 1000, "%zu;%zu;%zu;%zu\n...", grid_size.Nx, ...);

// DataPrinter.h
snprintf(buffer, 1000, "%zu;%zu;%zu;%zu\n...", nx(), ny(), nz(), static_cast<size_t>(3), ...);
```

**Плюсы:** минимальный diff, стандартный подход, `%zu` = формат для `size_t` в C99/C11/C17. Полностью поддерживается MSVC с VS2015
**Минусы:** нет
**Риски:** нулевые — `%zu` корректен для `size_t` на любой платформе
**Совместимость:** формат выходного файла меняется: вместо мусора будут корректные числа. Это исправление, не поломка
**Трудоёмкость:** 2 файла, ~3 строки каждый

### Вариант B: `std::format` (C++20)

**Суть:** заменить `snprintf` + `char buffer[1000]` на `std::format`:

```cpp
auto result = std::format("{};{};{};{}\n...", grid_size.Nx, grid_size.Ny, ...);
```

**Плюсы:** type-safe, нет возможности несовпадения формата и типа
**Минусы:** больший diff, другой стиль (остальной проект использует `snprintf`), нужен `#include <format>`, разный формат чисел (нужен explicit `{:+19.11E}`)
**Риски:** низкие, но больше шансов опечатки при переписывании format string
**Совместимость:** полная
**Трудоёмкость:** 2 файла, ~10 строк каждый

### Выбор: Вариант A

Минимальный diff, стандартный подход, нулевой риск. Вариант B — хороший рефакторинг, но выходит за скоуп BUG-018 (можно добавить как DEBT если нужно).

---

## Поиск подводных камней

- ✅ **Побочные эффекты:** `PrintModelData` вызывается из `CalculationManager.cpp:25`. `PrintPlanarMesh` — из `DataPrinter.h:67` (метод `PrintDefault`). Оба метода — вывод в файл, не в pipeline расчётов. Изменение формата не влияет на симуляцию
- ✅ **Потокобезопасность:** обе функции вызываются при инициализации, до OpenMP-секций. Локальный `char buffer[1000]` — на стеке. Безопасно
- ✅ **Граничные случаи:** `Nx/Ny/Nz = 0` — `%zu` выведет `0`, корректно. `Nx > UINT_MAX` — `%zu` корректен, `%u` бы усёк. `frames = 0` — `%zu` выведет `0`
- ✅ **Производительность:** вызывается один раз при инициализации. Не в горячем цикле
- ✅ **Обратная совместимость:** формат выходного файла: числа будут корректными вместо мусорных. Парсеры MATLAB/Python получат валидные данные. Это fix, не breaking change
- ✅ **Порядок вызовов:** не зависит от порядка инициализации
- ✅ **Состояние при ошибке:** нет throw/early return
- ✅ **Численная устойчивость:** не применимо (форматирование, не вычисления)
- ✅ **Связь с другими задачами:** нет конфликтов. Ни один BUG/DEBT не затрагивает `DataPrinter.h` или `RawHorizon.h::PrintModelData`
- ✅ **Зависимости сборки:** `%zu` — стандартный C99, не требует новых include. Нет CMake-изменений

---

## Обнаруженные проблемы

Нет.

---

## Затронутые файлы

| Файл | Роль |
|---|---|
| `HydroSolver/Helpers/DataPrinter.h` | `%u` → `%zu` в `PrintPlanarMesh()`, `3.0` → `static_cast<size_t>(3)` |
| `HydroSolver/Solver/Grids/RawHorizon.h` | `%u` → `%zu` в `PrintModelData()` |

---

## Шаги реализации

### Шаг 1: Исправить format string в `RawHorizon.h` и `DataPrinter.h`

**Цель:** устранить UB — заменить `%u` на `%zu` для аргументов `size_t`, исправить `double` литерал `3.0` на `static_cast<size_t>(3)` в DataPrinter.h.

**Файлы:** `HydroSolver/Solver/Grids/RawHorizon.h`, `HydroSolver/Helpers/DataPrinter.h`

**Контекст:**
`RawHorizon::PrintModelData(size_t frames)` (RawHorizon.h:37-53) записывает grid metadata в текстовый файл `grid_descriptor.txt`. Формат файла: первая строка — `Nx;Ny;Nz;Nt` (целые, разделённые `;`), вторая — `x_min;y_min;x_max;y_max` (double), третья — `step_x;step_y;0;0` (double). Вызывается из `CalculationManager.cpp:25` при инициализации.

`DataPrinter::PrintPlanarMesh()` (DataPrinter.h:48-63) — аналогичная функция, записывает в `MatLab/testPlanarMesh.txt` тот же формат. Вызывается из `PrintDefault()` (DataPrinter.h:67).

Проблема: `snprintf` с `%u` (ожидает `unsigned int` = 4 байта) получает `size_t` (= 8 байт на x64). В `DataPrinter.h` четвёртый аргумент — литерал `3.0` (double), что ещё хуже: `double` = 8 байт, но `%u` читает 4 байта с неправильной интерпретацией IEEE 754 → совсем мусор.

**Что сделать:**

1. В файле `HydroSolver/Solver/Grids/RawHorizon.h`, строка ~43:
   заменить `"%u;%u;%u;%u\n` на `"%zu;%zu;%zu;%zu\n`

2. В файле `HydroSolver/Helpers/DataPrinter.h`, строка ~54:
   заменить `"%u;%u;%u;%u\n` на `"%zu;%zu;%zu;%zu\n`

3. В файле `HydroSolver/Helpers/DataPrinter.h`, строка ~56:
   заменить `3.0,//reservoirIntantiator->nt(),` на `static_cast<size_t>(3),//reservoirIntantiator->nt(),`

**Изменения (старый → новый код):**

**RawHorizon.h:**

До:
```cpp
			snprintf(buffer, 1000, "%u;%u;%u;%u\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				grid_size.Nx, grid_size.Ny,
				grid_size.Nz, frames,
```

После:
```cpp
			snprintf(buffer, 1000, "%zu;%zu;%zu;%zu\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				grid_size.Nx, grid_size.Ny,
				grid_size.Nz, frames,
```

**DataPrinter.h:**

До:
```cpp
			snprintf(buffer, 1000, "%u;%u;%u;%u\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				reservoirIntantiator->nx(), reservoirIntantiator->ny(),
				reservoirIntantiator->nz(), 3.0,//reservoirIntantiator->nt(),
```

После:
```cpp
			snprintf(buffer, 1000, "%zu;%zu;%zu;%zu\n%+19.11E;%+19.11E;%+19.11E;%+19.11E\n%+19.11E;%+19.11E;%+19.11E;%+19.11E",
				reservoirIntantiator->nx(), reservoirIntantiator->ny(),
				reservoirIntantiator->nz(), static_cast<size_t>(3),//reservoirIntantiator->nt(),
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — все 290 тестов зелёные
- Debug: `cmake --build build --config Debug` + тесты — все зелёные

**Подводные камни:** нет (см. чеклист выше)

**Зависимости:**
- Требует: нет
- Блокирует: нет

**Оценка:** ~3 строки изменений в 2 файлах, ~3 минуты

---

## Тестовая стратегия

### Тест-воспроизводитель

`PrintModelData` и `PrintPlanarMesh` не покрыты unit-тестами. Тест-воспроизводитель не создаётся — UB в `snprintf` не ловится Catch2 без специальной инфраструктуры (sanitizers). Фикс верифицируется инспекцией кода (`%zu` = стандартный формат для `size_t`) и полным прогоном регрессионных тестов.

### Regression

Все 290 существующих тестов. `PrintModelData` вызывается косвенно через `CalculationManager`, но ни один тест не парсит выходной файл `grid_descriptor.txt`.

### Visual

Не применимо.

---

## Критерии завершения

- [ ] `%u` заменён на `%zu` в `RawHorizon.h:43`
- [ ] `%u` заменён на `%zu` в `DataPrinter.h:54`
- [ ] `3.0` заменён на `static_cast<size_t>(3)` в `DataPrinter.h:56`
- [ ] Все существующие тесты зелёные в Release (290)
- [ ] Все тесты зелёные в Debug
- [ ] Ноль новых warnings при сборке (оба конфига)
- [ ] Vault обновлён: запись в [[известные баги]] → статус ✅
- [ ] Заметка в `knowledge/debugging/` создана
- [ ] GitHub issue #14 прокомментирован с результатом
- [ ] `vault/GDM/00-home/index.md` обновлён
