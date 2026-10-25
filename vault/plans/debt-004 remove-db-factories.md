---
tags:
  - план
  - удаление
date: 2026-07-15
issue: DEBT-004
github: 28
branch: refactor/debt-004/remove-db-factories
status: реализован
audit:
  date: 2026-07-15
  pass: 2
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-004: Удаление мёртвых DB-фабрик и PostgreSQL-зависимого слоя

## Контекст

Legacy-код HydroSolver зависел от PostgreSQL (libpqxx) для ввода данных из БД и GEOS для геометрии контуров. В июне 2026 обе зависимости были отвязаны: весь Data-слой (фабрики, конфиги) исключён из сборки за `if(PostgreSQL_FOUND)` в CMakeLists.txt. Вместо `HorizonFactory` создан `DevelopedHorizon` с прямой инициализацией через `test_helpers`.

Подробности: [[отвязка от PostgreSQL и GEOS через заглушки и синтетический main]]

Сейчас PostgreSQL не установлен, `find_package(PostgreSQL QUIET)` не находит его — весь блок `gdm_data` + `gdm_app` пропускается. Файлы физически присутствуют в репозитории, но не компилируются. Это мёртвый код, создающий путаницу при навигации.

## Подтип: удаление мёртвого кода

## Гипотеза мёртвости (доказательство)

Все удаляемые файлы входят **только** в targets `gdm_data` и `gdm_app`, которые находятся за `if(PostgreSQL_FOUND)` (CMakeLists.txt:315-372). PostgreSQL не установлен — эти targets не собираются. Ни один из удаляемых файлов не включается (#include) из `gdm_core`, `gdm_tests` или `gdm_benchmark`.

**Исключения — живые файлы в `Data/`:**
- `HydroSolver/Data/PhaseFactory.hpp` — используется в `src/main.cpp`, `tests/test_helpers.h`, `tests/unit/math/test_JacobianAssembly.cpp`
- `HydroSolver/Data/ExceptionFactory.h` — используется в `ReservoirSimulator.cpp`, `SomeWell.h`, `SetOfPoints.cpp`, `MER_Descriptor.cpp`, `src/main.cpp`, `tests/test_helpers.h`
- `HydroSolver/Data/wells/GeosPoint.h` — используется в `HydroSolver/defines.h:6` (→ `WellPosition` по всему проекту), `tests/test_helpers.h:9`, `tests/unit/wells/test_GeosPoint.cpp`

Эти три файла **не удалять**.

## Инвентарь файлов для удаления

### Группа 1: DB-фабрики (HydroSolver/Data/)

| Файл | Строк | Роль |
|---|---|---|
| `HydroSolver/Data/ConnectionFactory.h` | 25 | Обёртка pqxx::connection |
| `HydroSolver/Data/ConnectionFactory.cpp` | 16 | Реализация |
| `HydroSolver/Data/ReservoirFactory.h` | 167 | GridGeometryFactory, RawReservoirFactory, ReservoirFactory\<T\>, UnitsConversionFactory |
| `HydroSolver/Data/ReservoirFactory.cpp` | 138 | Реализация |
| `HydroSolver/Data/HorizonFactory.h` | 37 | Создание DevelopedHorizon из DB |
| `HydroSolver/Data/HorizonFactory.cpp` | 122 | Реализация |
| `HydroSolver/Data/Wellfactory.h` | 47 | MERFactory, WellFactory |
| `HydroSolver/Data/Wellfactory.cpp` | ~80 | Реализация |
| `HydroSolver/Data/RawWellFactory.h` | 140 | LayerFactory, RawWellFactory, RawMERFactory, PerforationFactory |
| `HydroSolver/Data/RawWellFactory.cpp` | ~200 | Реализация |
| `HydroSolver/Data/DBUtils.h` | ~80 | pqxx-based DBLoader |
| `HydroSolver/Data/DBUtils.cpp` | ~50 | Реализация |

### Группа 2: Data/wells/ (кроме GeosPoint.h)

| Файл | Роль |
|---|---|
| `HydroSolver/Data/wells/GEOSObject.h/.cpp` | GEOS-зависимая геометрия |
| `HydroSolver/Data/wells/Maps.h/.cpp` | Legacy map utilities |
| `HydroSolver/Data/wells/MessageQueue.h/.cpp` | Legacy message queue |
| `HydroSolver/Data/wells/WellDataHandler.h/.cpp` | Legacy well data handler (DB, отличается от `Utils/WellDataHandler.*`) |
| `HydroSolver/Data/wells/WellsBase.h/.cpp` | Legacy well base |
| `HydroSolver/Data/wells/LayeredData.h` | Layered data descriptor |
| `HydroSolver/Data/wells/PathUtils.h` | Path utilities |

**НЕ удалять:** `HydroSolver/Data/wells/GeosPoint.h` — живой, используется через `defines.h` → `WellPosition` во всём проекте

### Группа 3: CalculationManager

| Файл | Роль |
|---|---|
| `HydroSolver/Reservoir/CalculationManager.h` | DB-backed simulation entry point |
| `HydroSolver/Reservoir/CalculationManager.cpp` | Реализация (включает HorizonFactory) |

### Группа 4: Legacy main + legacy тесты

| Файл | Роль |
|---|---|
| `HydroSolver/HydroSolver.cpp` | Старый main (вызывает run_simulations) |
| `HydroSolver/tests/create_horizon.h/.cpp` | Legacy тест |
| `HydroSolver/tests/create_merdata.h/.cpp` | Legacy тест |
| `HydroSolver/tests/create_numericalparameters.h/.cpp` | Legacy тест |
| `HydroSolver/tests/create_oilfield.h/.cpp` | Legacy тест |
| `HydroSolver/tests/create_perforations.h/.cpp` | Legacy тест |
| `HydroSolver/tests/create_reservoirsimulator.h/.cpp` | Legacy тест |
| `HydroSolver/tests/create_well.h/.cpp` | Legacy тест |
| `HydroSolver/tests/simulate_reservoir.h/.cpp` | Legacy тест |

### Группа 5: CMakeLists.txt

Удалить строки 310-372 (весь блок `find_package(PostgreSQL)` + `if(PostgreSQL_FOUND) ... endif()`).

**Не удалять** файлы `Utils/UniversalSVParser.cpp` и `Utils/UniversalSVWriter.cpp` — они упомянуты в `gdm_data`, но физически живут в `Utils/`. Их удаление — отдельная задача DEBT-005.

## Подводные камни

- [x] ✅ **Все call sites найдены:** все include-ы DB-фабрик — только внутри `Data/`, `Data/wells/`, legacy тестов и `CalculationManager.cpp`. Ни одного в `gdm_core`/`gdm_tests`/`gdm_benchmark`
- [x] ✅ **Потокобезопасность:** удаление не затрагивает runtime-код
- [x] ✅ **Зависимости сборки:** `gdm_data`/`gdm_app` не собираются сейчас, удаление CMake-блока ничего не сломает
- [x] ✅ **Тесты:** все 310 Catch2-тестов — в `gdm_tests` и `gdm_benchmark`, не зависят от удаляемого кода
- [x] ✅ **Мёртвый код подтверждён:** PostgreSQL не установлен, targets не собираются, include-ов из living code нет
- [x] ✅ **PhaseFactory.hpp и ExceptionFactory.h** — живые, явно исключены из удаления
- [x] ✅ **GeosPoint.h** — живой (используется `defines.h:6`, `test_helpers.h:9`, `test_GeosPoint.cpp`), явно исключён из удаления. Найден аудитом
- [x] ⚠️ **DEBT-005 (UniversalSVParser/Writer):** файлы `Utils/UniversalSVParser.cpp` и `Utils/UniversalSVWriter.cpp` упомянуты в `gdm_data`, но НЕ удаляются — они в `Utils/`, не в `Data/`. После удаления CMake-блока они потеряют единственную CMake-ссылку, но и так не собирались. DEBT-005 адресует их отдельно
- [x] ⚠️ **Anomaly.h → Data/wells/WellDataHandler.h**: `Anomaly.h` включает мёртвый `WellDataHandler.h`, но `Anomaly.cpp` не компилируется (не в CMakeLists.txt). После удаления `WellDataHandler.h` include в `Anomaly.h` станет битым, но это не влияет на сборку. При будущем оживлении Anomaly потребуется чистка include-ов

## Обнаруженные проблемы

Нет новых проблем. DEBT-005 уже зарегистрирован.

---

## Шаги реализации

### Шаг 1: Удаление мёртвых файлов из `Data/wells/`

**Цель:** удалить legacy well-обработку, зависимую от GEOS и PostgreSQL, оставив живой `GeosPoint.h`

**Файлы для удаления:**
- `HydroSolver/Data/wells/GEOSObject.h`
- `HydroSolver/Data/wells/GEOSObject.cpp`
- `HydroSolver/Data/wells/Maps.h`
- `HydroSolver/Data/wells/Maps.cpp`
- `HydroSolver/Data/wells/MessageQueue.h`
- `HydroSolver/Data/wells/MessageQueue.cpp`
- `HydroSolver/Data/wells/WellDataHandler.h`
- `HydroSolver/Data/wells/WellDataHandler.cpp`
- `HydroSolver/Data/wells/WellsBase.h`
- `HydroSolver/Data/wells/WellsBase.cpp`
- `HydroSolver/Data/wells/LayeredData.h`
- `HydroSolver/Data/wells/PathUtils.h`

**НЕ удалять:**
- `HydroSolver/Data/wells/GeosPoint.h` — **ЖИВОЙ**. Включается из `HydroSolver/defines.h:6`, `tests/test_helpers.h:9`, `tests/unit/wells/test_GeosPoint.cpp:3`. Определяет `GeosShell::SimplePoint` и `GeosShell::GeosPoint`, используемые как `WellPosition` по всему проекту

**Контекст:**
Папка `Data/wells/` содержит GEOS-зависимую обработку скважинных данных из PostgreSQL. Большинство файлов входят только в target `gdm_data` (CMakeLists.txt:344-348), который не собирается. Исключение — `GeosPoint.h`: это заглушка GEOS-координат (не зависит от libgeos), используемая через `defines.h` во всём проекте. В `Utils/` есть одноимённый `WellDataHandler.*` — это текущий файловый парсер, он в `gdm_core` и не затрагивается.

**Что сделать:**
1. Удалить 12 мёртвых файлов (все, кроме `GeosPoint.h`)

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидание: 310/310, всё зелёное (удалённые файлы не участвовали в сборке)

**Подводные камни:**
- `GeosPoint.h` — **НЕЛЬЗЯ удалять**, используется через `defines.h` → `WellPosition`
- `Data/wells/WellDataHandler.*` — НЕ путать с `Utils/WellDataHandler.*` (живой)

**Зависимости:** нет

**Оценка:** ~0 строк кода, ~2 минуты (git rm)

---

### Шаг 2: Удаление DB-фабрик из `Data/`

**Цель:** удалить ConnectionFactory, ReservoirFactory, HorizonFactory, WellFactory, RawWellFactory, DBUtils

**Файлы:**
- `HydroSolver/Data/ConnectionFactory.h`
- `HydroSolver/Data/ConnectionFactory.cpp`
- `HydroSolver/Data/ReservoirFactory.h`
- `HydroSolver/Data/ReservoirFactory.cpp`
- `HydroSolver/Data/HorizonFactory.h`
- `HydroSolver/Data/HorizonFactory.cpp`
- `HydroSolver/Data/Wellfactory.h`
- `HydroSolver/Data/Wellfactory.cpp`
- `HydroSolver/Data/RawWellFactory.h`
- `HydroSolver/Data/RawWellFactory.cpp`
- `HydroSolver/Data/DBUtils.h`
- `HydroSolver/Data/DBUtils.cpp`

**Контекст:**
Эти файлы — ядро DB-зависимого ввода данных. `ConnectionFactory` оборачивает pqxx::connection. `ReservoirFactory` загружает сеточные данные из DB. `HorizonFactory` собирает `DevelopedHorizon` из фабрик. Все входят в `gdm_data` (CMakeLists.txt:338-343), не собираются. Заменены прямой инициализацией через `test_helpers::make_uniform_horizon()`.

**Что сделать:**
1. Удалить все 12 файлов

**НЕ трогать:**
- `HydroSolver/Data/PhaseFactory.hpp` — живой (используется в main.cpp, test_helpers.h, test_JacobianAssembly.cpp)
- `HydroSolver/Data/ExceptionFactory.h` — живой (используется в ReservoirSimulator.cpp, SomeWell.h, SetOfPoints.cpp, MER_Descriptor.cpp)

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидание: 310/310

**Зависимости:**
- Требует: шаг 1 (Data/wells/ уже удалён, иначе RawWellFactory.h всё ещё ссылается на wells/)

**Оценка:** ~0 строк кода, ~2 минуты

---

### Шаг 3: Удаление CalculationManager

**Цель:** удалить DB-backed entry point симуляции

**Файлы:**
- `HydroSolver/Reservoir/CalculationManager.h`
- `HydroSolver/Reservoir/CalculationManager.cpp`

**Контекст:**
`CalculationManager` — единственный production-потребитель `HorizonFactory`. Создаёт `DevelopedHorizon` из PostgreSQL-строки подключения, затем запускает `ReservoirSimulator`. Входит в `gdm_data` (CMakeLists.txt:351). Не вызывается из `gdm_core` или тестов — только из `HydroSolver.cpp` (legacy main) и `simulate_reservoir.cpp` (legacy тест).

**Что сделать:**
1. Удалить `HydroSolver/Reservoir/CalculationManager.h`
2. Удалить `HydroSolver/Reservoir/CalculationManager.cpp`

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидание: 310/310

**Зависимости:**
- Требует: шаг 2 (фабрики уже удалены)

**Оценка:** ~0 строк, ~1 минута

---

### Шаг 4: Удаление legacy тестов и legacy main

**Цель:** удалить старый main и legacy тест-функции

**Файлы:**
- `HydroSolver/HydroSolver.cpp`
- `HydroSolver/tests/create_horizon.h` + `.cpp`
- `HydroSolver/tests/create_merdata.h` + `.cpp`
- `HydroSolver/tests/create_numericalparameters.h` + `.cpp`
- `HydroSolver/tests/create_oilfield.h` + `.cpp`
- `HydroSolver/tests/create_perforations.h` + `.cpp`
- `HydroSolver/tests/create_reservoirsimulator.h` + `.cpp`
- `HydroSolver/tests/create_well.h` + `.cpp`
- `HydroSolver/tests/simulate_reservoir.h` + `.cpp`

Итого: 17 файлов

**Контекст:**
`HydroSolver.cpp` — старый main, вызывает `run_simulations()` через `CalculationManager`. Legacy тесты (`create_*.cpp`, `simulate_reservoir.cpp`) — функции (не Catch2 TEST_CASE), входят в target `gdm_app` (CMakeLists.txt:356-365). Все используют `HorizonFactory` для создания горизонтов из DB. Ни один не вызывается из живого кода.

**Что сделать:**
1. Удалить `HydroSolver/HydroSolver.cpp`
2. Удалить все 16 файлов legacy тестов (8 пар .h/.cpp)

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидание: 310/310

**Зависимости:**
- Требует: шаг 3

**Оценка:** ~0 строк, ~2 минуты

---

### Шаг 5: Удаление CMake-блока PostgreSQL

**Цель:** удалить неиспользуемый блок сборки из CMakeLists.txt

**Файлы:** `CMakeLists.txt`

**Контекст:**
Строки 310-372 содержат: `find_package(PostgreSQL QUIET)`, `if(PostgreSQL_FOUND)` с FetchContent для GEOS и libpqxx, targets `gdm_data` и `gdm_app`, `else()` с message, `endif()`. После удаления файлов на шагах 1-4 этот блок ссылается на несуществующие файлы. Удаляем целиком.

**Что сделать:**
1. Удалить строки 310-372 из CMakeLists.txt (весь блок от комментария `# gdm_data + gdm_app` до `endif()`)

До:
```cmake
# =============================================================================
# gdm_data + gdm_app — Data-слой (PostgreSQL + GEOS)
# =============================================================================
find_package(PostgreSQL QUIET)

if(PostgreSQL_FOUND)
    # --- GEOS ---
    ...
    # --- gdm_data ---
    ...
    # --- gdm_app ---
    ...
    message(STATUS "PostgreSQL found — building gdm_data and gdm_app")
else()
    message(STATUS "PostgreSQL not found — gdm_data and gdm_app will not be built")
endif()
```

После: (строки удалены, ничего не заменяет)

**Проверка после этого шага:**
- Реконфигурация: `cmake -B build -S . -G "Visual Studio 17 2022"`
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Ожидание: 310/310, в выводе cmake нет строки «PostgreSQL not found»
- Проверить: `cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure`

**Подводные камни:**
- `UniversalSVParser.cpp` и `UniversalSVWriter.cpp` упоминались в `gdm_data` — после удаления блока они теряют CMake-ссылку. Но файлы остаются в `Utils/`, и DEBT-005 адресует их отдельно. Сейчас они и так не собирались

**Зависимости:**
- Требует: шаги 1-4 (файлы уже удалены)

**Оценка:** ~63 строки удалены из CMakeLists.txt, ~5 минут (включая реконфигурацию)

---

## Тестовая стратегия

Задача — чистое удаление мёртвого кода. Новых тестов не требуется.

**Regression:** все 310 существующих Catch2-тестов (`gdm_tests` + `gdm_benchmark`) должны оставаться зелёными после каждого шага. Удаляемый код не участвует в их сборке и исполнении.

**Инвариант:** количество тестов (310), время (~123s Release, ~434s Debug), количество warnings (1 pre-existing C4267 в test_JacobianAssembly.cpp:34) — не должны измениться.

---

## Критерии завершения

- [ ] Все файлы из инвентаря удалены (4 группы: wells/, фабрики, CalculationManager, legacy тесты+main)
- [ ] Блок `if(PostgreSQL_FOUND)` удалён из CMakeLists.txt
- [ ] `PhaseFactory.hpp` и `ExceptionFactory.h` на месте и работают
- [ ] Все 310 тестов зелёные (Release + Debug)
- [ ] В выводе cmake нет строки «PostgreSQL»
- [ ] Vault обновлён: статус DEBT-004, заметки
- [ ] GitHub issue #28 прокомментирован

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[отвязка от PostgreSQL и GEOS через заглушки и синтетический main]]
3. Создай ветку: `git checkout -b refactor/debt-004/remove-db-factories experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни: 310 тестов, ~123s Release, 1 warning C4267
7. Начни с шага 1. После каждого шага: сборка + тесты

## Baseline

- Тесты: 310/310 passed
- Release: ~123s
- Debug: ~434s
- Warnings: 1 (C4267 в test_JacobianAssembly.cpp:34 — pre-existing)
- Default solver: CPR_BICGSTAB

## Связанные заметки

- [[отвязка от PostgreSQL и GEOS через заглушки и синтетический main]]
- DEBT-005: UniversalSVParser/Writer — связанная задача
