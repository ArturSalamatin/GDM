---
tags:
  - сессия
  - рефакторинг
  - удаление
  - DEBT-004
date: 2026-07-15
---

# 2026-07-15: DEBT-004 удаление DB-фабрик и PostgreSQL-слоя

## Что сделано

1. **`/issue DEBT-004`** — создан GitHub issue [#28](https://github.com/ArturSalamatin/GDM/issues/28)
2. **`/plan-improve DEBT-004`** — план удаления мёртвого кода, 5 шагов
3. **`/audit-plan DEBT-004`** — два прохода аудита:
   - Pass 1: 🔴 1 критическая (GeosPoint.h в списке на удаление — автофикс)
   - Pass 2: 0/0/0, план чистый
4. **`/implement DEBT-004`** — реализация, 5 шагов, 6 коммитов кода + 1 vault

## Удалённые файлы (43 файла, 3479 строк)

### Группа 1: Data/wells/ (12 файлов)
- GEOSObject.h/.cpp — GEOS-зависимая геометрия контуров скважин
- Maps.h/.cpp — утилиты для map-контейнеров (legacy)
- MessageQueue.h/.cpp — очередь сообщений (legacy)
- WellDataHandler.h/.cpp — загрузка скважинных данных из PostgreSQL (не путать с Utils/WellDataHandler.*)
- WellsBase.h/.cpp — базовые классы скважин (legacy)
- LayeredData.h — дескриптор послойных данных
- PathUtils.h — утилиты путей

### Группа 2: DB-фабрики (12 файлов)
- ConnectionFactory.h/.cpp — обёртка pqxx::connection
- ReservoirFactory.h/.cpp — GridGeometryFactory, RawReservoirFactory, ReservoirFactory<T>, UnitsConversionFactory
- HorizonFactory.h/.cpp — создание DevelopedHorizon из PostgreSQL
- Wellfactory.h/.cpp — MERFactory, WellFactory
- RawWellFactory.h/.cpp — LayerFactory, RawWellFactory, RawMERFactory, PerforationFactory
- DBUtils.h/.cpp — pqxx-based DBLoader

### Группа 3: CalculationManager (2 файла)
- CalculationManager.h/.cpp — DB-backed entry point симуляции (содержал hardcoded connection string)

### Группа 4: Legacy main + тесты (17 файлов)
- HydroSolver.cpp — старый main (вызывал run_simulations через CalculationManager)
- 8 пар create_*.h/.cpp + simulate_reservoir.h/.cpp — plain functions (не Catch2), использовали HorizonFactory

### Группа 5: CMakeLists.txt (64 строки)
- Блок find_package(PostgreSQL) + if(PostgreSQL_FOUND): FetchContent для GEOS и libpqxx, targets gdm_data и gdm_app

## Сохранённые файлы (живые зависимости в Data/)

- **PhaseFactory.hpp** — используется в main.cpp, test_helpers.h, test_JacobianAssembly.cpp
- **ExceptionFactory.h** — используется в ReservoirSimulator.cpp, SomeWell.h, SetOfPoints.cpp, MER_Descriptor.cpp, main.cpp, test_helpers.h
- **GeosPoint.h** — используется в defines.h → WellPosition по всему проекту (найден аудитом)

## ⚠️ Замечание пользователя

Пользователь отметил: «возможно эти файлы придётся восстанавливать на следующих этапах работы; пока неочевидно, почему такое количество кода объявлено мёртвым».

**Контекст мёртвости:** весь удалённый код находился за `if(PostgreSQL_FOUND)` в CMakeLists.txt. PostgreSQL не установлен — targets `gdm_data` и `gdm_app` не собирались. Ни один удалённый файл не включался (#include) из living targets (`gdm_core`, `gdm_tests`, `gdm_benchmark`). Удалённый код можно восстановить из git:

```
git show e05d43a:HydroSolver/Data/ReservoirFactory.h   # любой файл по имени
git log --diff-filter=D -- HydroSolver/Data/            # все удалённые файлы
git checkout e05d43a -- HydroSolver/Data/                # восстановить всю папку
```

Коммит до удаления: `e05d43a` (на ветке experimental).

**Что может потребовать восстановления:**
- `ReservoirFactory` содержит `UnitsConversionFactory` — может быть полезен при реализации файлового ввода (фаза 7)
- `HorizonFactory` содержит логику сборки `DevelopedHorizon` из фабрик — может быть образцом для нового builder
- `WellDataHandler` (Data/) содержит логику парсинга MER-данных из PostgreSQL — если вернётся DB-ввод
- `GEOSObject` — если потребуется геометрия контуров скважин (пересечение полигонов)

## Коммиты

- `e05d43a` vault: DEBT-004 план удаления DB-фабрик и аудит (2 прохода)
- `d8d45c1` vault: DEBT-004 начало реализации
- `d2def51` refactor: удалить мёртвые файлы Data/wells/ (кроме GeosPoint.h)
- `6779b08` refactor: удалить DB-фабрики
- `16e8016` refactor: удалить CalculationManager
- `371c2e2` refactor: удалить legacy main и legacy тесты
- `24ec788` refactor: удалить CMake-блок PostgreSQL
- `306895b` vault: DEBT-004 реализован

## Ветка

`refactor/debt-004/remove-db-factories` — merged в experimental.

## Baseline

- 310 тестов, 310 pass
- Release: ~109s (до) → ~99s (после)
- Debug: ~417s (до) → ~400s (после)
- Warning: 1 pre-existing C4267 в test_JacobianAssembly.cpp:34

## Связанные задачи

- DEBT-005: UniversalSVParser/Writer — упомянуты в удалённом gdm_data, но живут в Utils/. Отдельная задача
- Inbox: ExceptionFactory broken include path — существовала до DEBT-004
