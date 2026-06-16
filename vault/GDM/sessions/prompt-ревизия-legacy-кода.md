---
tags:
  - промпт
  - ревизия
  - legacy
date: 2026-06-16
---

# Промпт: ревизия legacy-кода HydroSolver

Скопируй этот промпт в новую сессию Claude Code целиком. Проект находится в `d:\Lessons\Grants\2027\GDM`.

---

## Контекст

GDM — наукоёмкий гидродинамический симулятор для моделирования работы нефтяного месторождения. Двухфазная фильтрация нефть–вода, обе фазы несжимаемые. Язык — C++23, сборка CMake + Visual Studio 2022.

В корне проекта лежит legacy-код, восстановленный из архивных снимков (конец 2022 года). Код на момент заморозки был в процессе рефакторинга: часть модулей может быть незавершена, часть — заглушки. Задача этой сессии — провести полную ревизию и создать карту кода.

**Правила работы описаны в `CLAUDE.md` — прочитай его перед началом.**

## Структура проекта

```
GDM/
├── HydroSolver/           # основной солвер (126 собственных .cpp/.h + AMGSolver)
│   ├── HydroSolver.cpp    # точка входа (21 строка)
│   ├── stdafx.h/cpp       # precompiled header
│   ├── defines.h          # общие определения (65 строк)
│   ├── Anomaly/           # модель аномалий / траектории
│   ├── Data/              # фабрики данных (Factory pattern)
│   │   └── wells/         # работа со скважинными данными
│   ├── Descriptors/       # MER-дескрипторы
│   ├── Helpers/           # Config, LogFile, DataPrinter
│   ├── Reservoir/         # CalculationManager, ReservoirSimulator, NumericalParameters
│   │   └── Well/          # скважины (SomeWell, Wells, SetOfPoints, WellJobs)
│   ├── Solver/
│   │   ├── Grids/         # сетка (OilField, AbstractGrid, Cells/)
│   │   └── Math/          # линейная алгебра (LinearProblem, MatrixCSR, SparsityPattern)
│   ├── Temp/              # CacheFile, INIReader
│   ├── Utils/             # BinaryFileHandler, IRCGEngine, JSON, SVParser/Writer
│   ├── tests/             # юнит-тесты (create_*, simulate_reservoir)
│   └── AMGSolver/         # amgcl — сторонняя библиотека AMG-солвера
├── Shared0/               # утилиты (18 файлов, частично дублируют HydroSolver/Utils)
├── GridEngine/            # парсер GRDECL-сеток (12 файлов)
├── MatLab/                # скрипты визуализации (14 файлов)
├── HydroSolver.sln        # VS solution (1 проект: HydroSolver)
├── CMakeLists.txt         # новый CMake (пока пустой, без исходников)
└── Support/legacy/        # архив — не трогать
```

## Задание: полная ревизия кода

Проведи ревизию в 6 этапов. По каждому этапу запиши результаты в vault.

### Этап 1. Точка входа и поток управления

Прочитай файлы в следующем порядке:
1. `HydroSolver/HydroSolver.cpp` (21 строка) — main, точка входа
2. `HydroSolver/stdafx.h` (26 строк) — что подключается глобально
3. `HydroSolver/defines.h` (65 строк) — типы, константы, макросы

Определи:
- Что делает main()? Какие объекты создаёт, что вызывает?
- Есть ли аргументы командной строки?
- Какой путь выполнения: чтение данных → построение сетки → решение → вывод?

### Этап 2. Ядро симулятора — Reservoir

Прочитай файлы (по убыванию важности):
1. `HydroSolver/Reservoir/ReservoirSimulator.cpp` (1184 строки) — **самый большой файл, ядро**
2. `HydroSolver/Reservoir/ReservoirSImulator.h` (314 строк) — интерфейс симулятора
3. `HydroSolver/Reservoir/CalculationManager.h` (15 строк) + `.cpp` (62 строки)
4. `HydroSolver/Reservoir/NumericalParameters.h` (97 строк) + `.cpp` (81 строка)
5. `HydroSolver/Reservoir/AnomalySimulator.h` (8 строк)
6. `HydroSolver/Reservoir/FluxReader.h` (78 строк)

Определи:
- Какая численная схема реализована? (IMPES, полностью неявная, sequential implicit?)
- Какие уравнения решаются? (давление, насыщенность, оба одновременно?)
- Как устроен временной цикл? (фиксированный шаг, адаптивный, CFL?)
- Что делает CalculationManager — обёртка над симулятором или что-то большее?
- Есть ли нелинейная итерация (Ньютон)?
- Какие граничные условия заданы?

### Этап 3. Солвер и сетка

Прочитай:
1. `HydroSolver/Solver/Grids/OilField.h` (63 строки) + `.cpp` (100 строк)
2. `HydroSolver/Solver/Grids/AbstractGrid.h` (265 строк)
3. `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.h` (99 строк) + `.cpp` (101 строка)
4. `HydroSolver/Solver/Grids/Cells/AbstractCells.h` (193 строки)
5. `HydroSolver/Solver/Grids/GridDescriptors.h` (44 строки)
6. `HydroSolver/Solver/Grids/PropertyDescriptor.h` (50 строк)
7. `HydroSolver/Solver/Grids/RawHorizon.h` (68 строк)
8. `HydroSolver/Solver/Math/LinearProblem.h` (77 строк) + `.cpp` (121 строка)
9. `HydroSolver/Solver/Math/MatrixCSR.h` (110 строк) + `.cpp` (76 строк)
10. `HydroSolver/Solver/Math/SparsityPattern.h` (115 строк) + `.cpp` (148 строк)
11. `HydroSolver/Solver/Math/MathRoutines.h` (323 строки) + `.cpp` (67 строк)

Определи:
- Тип сетки: структурированная/неструктурированная? Какая размерность?
- Как хранятся ячейки и их свойства (пористость, проницаемость, насыщенность)?
- Какие физические свойства есть у TwoPhaseFlowCell?
- Как устроена матрица системы (CSR)? Как строится шаблон разреженности?
- Как подключается amgcl для решения СЛАУ?
- Что делает LinearProblem — сборка матрицы и правой части?
- MathRoutines — что там? (интерполяция, относительные проницаемости, PVT?)

### Этап 4. Скважины

Прочитай:
1. `HydroSolver/Reservoir/Well/SomeWell.h` (232 строки) + `.cpp` (353 строки)
2. `HydroSolver/Reservoir/Well/Wells.h` (25 строк) + `.cpp` (81 строка)
3. `HydroSolver/Reservoir/Well/WellJobs.h` (72 строки) + `.cpp` (105 строк)
4. `HydroSolver/Reservoir/Well/SetOfPoints.h` (134 строки) + `.cpp` (424 строки)
5. `HydroSolver/Reservoir/Well/WellTrajectory.h` (28 строк) + `.cpp` (1 строка — заглушка?)

Определи:
- Как задаётся скважина? (координаты, перфорации, траектория)
- Какие режимы работы скважины? (фиксированный дебит, фиксированное давление?)
- Как вычисляется индекс продуктивности? (Peaceman?)
- Что делает SetOfPoints — геометрия пересечения скважины с сеткой?
- WellJobs — расписание работы скважины?

### Этап 5. Фабрики данных и ввод/вывод

Прочитай:
1. `HydroSolver/Data/ReservoirFactory.h` (146 строк) + `.cpp` (123 строки)
2. `HydroSolver/Data/Wellfactory.h` (38 строк) + `.cpp` (96 строк)
3. `HydroSolver/Data/HorizonFactory.h` (35 строк) + `.cpp` (117 строк)
4. `HydroSolver/Data/RawWellFactory.h` (119 строк) + `.cpp` (155 строк)
5. `HydroSolver/Data/ConnectionFactory.h` (23 строки) + `.cpp` (13 строк — заглушка?)
6. `HydroSolver/Data/PhaseFactory.hpp` (30 строк)
7. `HydroSolver/Data/ExceptionFactory.h` (429 строк)
8. `HydroSolver/Data/DBUtils.h` (302 строки) + `.cpp` (187 строк)
9. `HydroSolver/Helpers/Config.h` (69 строк) + `.cpp` (1 строка — заглушка?)
10. `HydroSolver/Helpers/Config_JSON.h` (159 строк) + `.cpp` (2 строки)
11. `HydroSolver/Helpers/DataPrinter.h` (204 строки)

Определи:
- Откуда читаются данные? (файлы, БД SQLite, JSON?)
- Какой формат входных данных? (GRDECL, CSV, JSON, ini?)
- Как фабрики связаны друг с другом? Какая фабрика создаёт что?
- ConnectionFactory — 13 строк, это заглушка?
- Config.cpp — 1 строка, это заглушка? Используется Config_JSON вместо него?
- DBUtils — работа с SQLite? Какие таблицы?
- ExceptionFactory — 429 строк, что это? Типизированные исключения?

### Этап 6. Вспомогательные модули

Бегло просмотри (не нужно читать построчно, достаточно заголовки + ключевые функции):

**Anomaly (модель аномалий):**
- `Anomaly/Anomaly.h` (171 строка) + `.cpp` (275 строк)
- `Anomaly/Trajectory.h` (65 строк) + `.cpp` (26 строк)
- `Anomaly/FlowField/` — Point, SomeFlowField, FlowField

**Descriptors:**
- `Descriptors/Descriptors.h` (77 строк) + `.cpp` (44 строки)
- `Descriptors/MER_Descriptor.h` (143 строки) + `.cpp` (191 строка)

**Tests:**
- `tests/simulate_reservoir.h` (9 строк) + `.cpp` (42 строки)
- Все `create_*.h/.cpp` — что они создают, запускаются ли?

**GridEngine** (отдельный проект):
- `GridEngine/GridEngine/grdecl.h` (562 строки) + `.cpp` (849 строк)
- Как связан с HydroSolver? Используется ли из HydroSolver или это самостоятельная утилита?

**Shared0 vs HydroSolver/Utils:**
- Сравни файлы в `Shared0/Utils/` и `HydroSolver/Utils/` — они идентичны или различаются?

Определи:
- Что такое «аномалия» в контексте симулятора?
- MER_Descriptor — что описывает? (ежемесячная отчётность?)
- Тесты — это юнит-тесты или скрипты создания тестовых данных?
- GridEngine — нужен ли для работы HydroSolver или это отдельная утилита предобработки?

---

## Формат результата

Создай заметку `vault/GDM/atlas/ревизия legacy-кода декабрь 2022.md` со следующей структурой:

```markdown
---
tags:
  - atlas
  - ревизия
  - архитектура
date: 2026-06-XX
---

# Ревизия legacy-кода HydroSolver (снимок декабрь 2022)

## 1. Общая архитектура
[Диаграмма зависимостей модулей в текстовом виде]
[Поток данных: ввод → сетка → сборка → решение → вывод]

## 2. Реализованная физика
[Какие уравнения, какая схема, какие допущения]

## 3. Карта модулей
[Таблица: модуль | статус (работает / заглушка / незавершён) | назначение | ключевые классы]

## 4. Зависимости
[Внешние: amgcl, SQLite?, nlohmann::json?, geos?]
[Внутренние: кто от кого зависит]

## 5. Проблемы и технический долг
[Что сломано, что незавершено, что нужно переписать]
[Дубликаты кода (Shared0 vs Utils)]
[Заглушки (файлы с 1-2 строками)]

## 6. Рекомендации
[Что переиспользовать как есть]
[Что требует рефакторинга]
[Что нужно писать с нуля]
[Приоритет: какой модуль поднимать первым]
```

Также обнови `vault/GDM/00-home/текущие приоритеты.md` с результатами ревизии.

---

## Важные замечания

- **Не редактируй исходники.** Это ревизия, не рефакторинг.
- **Не пытайся собрать проект.** Сборка — задача следующей сессии.
- Если файл — заглушка (1-2 строки), отметь это явно.
- Если видишь явную ошибку в физике или математике — отметь это явно.
- Если модуль выглядит незавершённым — отметь, что именно не реализовано.
- Читай vault (`00-home/index.md`, `текущие приоритеты.md`) перед началом работы.
- Файлы в `Support/legacy/` — архив, не трогать.
