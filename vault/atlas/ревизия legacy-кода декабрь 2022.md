---
tags:
  - atlas
  - ревизия
  - архитектура
date: 2026-06-16
---

# Ревизия legacy-кода HydroSolver (снимок декабрь 2022)

## 1. Общая архитектура

### Поток данных

```
PostgreSQL (pqxx)
  ↓
ConnectionFactory → DBLoader (grdecl_memory)
  ↓
ReservoirFactory (сетка, кубы свойств)   WellFactory (МЭР, перфорации, координаты)
  ↓                                         ↓
HorizonFactory → DevelopedHorizon (агрегат всех данных)
  ↓
ReservoirSimulator
  ├── OilField (структурированная 3D-сетка из TwoPhaseFlowCell)
  ├── LinearProblem → MatrixCSR → SparsityPattern → amgcl (GMRES + AMG)
  └── Wells (WellFixedProduction — фиксированный дебит)
       ├── MER_Data (помесячная добыча)
       ├── WellJobs → AccumulatedPerforations (история перфораций)
       └── WellTrajectory (геометрия в сетке)
  ↓
Solve() → PerformNewtonLoop() → SingleIteration()
  ├── AssembleMyProblem() — сборка якобиана и RHS
  ├── AccountForBoundaryConditions() — ГУ на контуре
  ├── MyProblem.Solve() — amgcl (GMRES + AMG с block-матрицами 2×2)
  └── UpdateGrid() — проверка сходимости Ньютона, обновление состояния
  ↓
PrintReservoirState() → oil_saturation.txt, pressure.txt, overall_balance.txt
Anomaly/FlowField — постпроцессинг: трассировка частиц в поле скоростей
```

### Диаграмма зависимостей модулей

```
main() → tests/simulate_reservoir → CalculationManager → ReservoirSimulator
                                                              │
                    ┌─────────────────┬──────────────────┬────┘
                    ↓                 ↓                  ↓
              Solver/Grids     Solver/Math          Reservoir/Well
              (OilField,       (LinearProblem,      (SomeWell,
               AbstractGrid,   MatrixCSR,            WellFixedProduction,
               TwoPhaseFlowCell) SparsityPattern)    SetOfPoints, WellJobs)
                    ↓                 ↓                  ↓
              Data/Factories   amgcl (AMGSolver/)   Descriptors/MER_Descriptor
              (ReservoirFactory,                    
               WellFactory,                         
               HorizonFactory)                      
                    ↓
              DBUtils (pqxx → PostgreSQL)
              GridEngine (парсер GRDECL → DB)
```

## 2. Реализованная физика

### Уравнения
Двухфазная фильтрация (нефть–вода), **полностью неявная схема (Fully Implicit Method)**.

Переменные задачи: **нормированная водонасыщенность S̃_w** и **давление P** — решаются одновременно как блочная система 2×2.

Уравнения неразрывности для каждой фазы:
```
∂(φ ρ_α S_α) / ∂t + ∇·(ρ_α f_α λ_total ∇P) = q_α
```
где α ∈ {oil, water}, f_α — доля фазы в потоке, λ_total — суммарная подвижность.

### Относительные проницаемости
Степенная модель (Corey) с показателем `permPower = 3`:
```
k_rw(S̃_w) = (S̃_w)³
k_ro(S̃_w) = (1 - S̃_w)³
```

### Подвижности
```
λ_α = k · k_rα / μ_α
λ_total = λ_oil + λ_water
f_oil = λ_oil / λ_total
```

### Плотности фаз
Слабая сжимаемость через линейную зависимость от давления:
```
ρ_α(P) = ρ_α^ref · (1 + c_α · (P - P^ref))
```
Но в `PhaseFactory` компрессибельности выставлены в **0.0** — фактически несжимаемые фазы (с опцией включения).

### Вязкости
Постоянные: μ_oil = 4.3 мПа·с, μ_water = 2.0 мПа·с (преобразуются в Па·сут).

### Дискретизация
- **Пространство:** метод конечных объёмов на структурированной 3D-сетке (стандартная 5/7-точечная схема).
- **Подвижности на гранях:** гармоническое среднее для суммарной подвижности, upstream-взвешивание для доли фазы.
- **Время:** полностью неявная схема, линеаризованная методом Ньютона.

### Линейный солвер
amgcl: GMRES + AMG (агрегация + демпфированный Якоби) для блочных матриц 2×2.

### Адаптивный шаг по времени
Адаптивный: увеличивается на 15% при успешной итерации, уменьшается на 30% при провале Ньютона. Ограничен сверху шагом до ближайшего события (МЭР, перфорация). **CFL-ограничение отсутствует** (не нужно для полностью неявной схемы).

### Граничные условия
Условие Дирихле на всех боковых гранях области: P = RefPressure (100 атм по умолчанию). Приток извне — чистая вода (f_oil_in = 0). Z-направление: граничные условия **отключены** (`if (false && (nz > 1))`).

### Скважины
Единственный реализованный режим — **фиксированный суммарный дебит** (WellFixedProduction). Давление на забое вычисляется из баланса дебитов по перфорациям. Индекс продуктивности Писмана: `factor = l_perf · 2π / ln(r_app / r_well)`.

## 3. Карта модулей

| Модуль | Статус | Назначение | Ключевые классы |
|--------|--------|------------|-----------------|
| `HydroSolver.cpp` | ✅ работает | Точка входа, вызывает тесты | `main()` |
| `Reservoir/ReservoirSimulator` | ✅ работает | Ядро симулятора: сборка, решение, обновление | `ReservoirSimulator` |
| `Reservoir/CalculationManager` | ✅ работает | Обёртка: временная сетка + цикл по шагам | `CalculationManager` |
| `Reservoir/NumericalParameters` | ✅ работает | Адаптивный шаг, контроль Ньютона и AMG | `NumericalParameters` |
| `Reservoir/AnomalySimulator` | ⬜ заглушка | Пустой класс-наследник | `AnomalySimulator` |
| `Reservoir/FluxReader` | ✅ работает | Чтение потоков из текстовых файлов | `FluxReader` |
| `Reservoir/Well/SomeWell` | ✅ работает | Базовый класс скважины | `SomeWell`, `WellEnvironment` |
| `Reservoir/Well/Wells` | ✅ работает | WellFixedProduction — единственная реализация | `WellFixedProduction` |
| `Reservoir/Well/WellJobs` | ✅ работает | Агрегация перфораций по слоям | `WellJobs` |
| `Reservoir/Well/SetOfPoints` | ✅ работает | Геометрия перфораций (отрезки, объединение) | `SetOfPoints`, `AccumulatedPerforations` |
| `Reservoir/Well/WellTrajectory` | ⬜ заглушка | Только заголовок, .cpp = 1 строка | `WellTrajectory` (inline в .h) |
| `Solver/Grids/OilField` | ✅ работает | Конкретная сетка для нефтяного месторождения | `OilField` |
| `Solver/Grids/AbstractGrid` | ✅ работает | Шаблонная структурированная 3D-сетка | `SomeGrid`, `SomeStructuredGrid3Dim` |
| `Solver/Grids/Cells/TwoPhaseFlowCell` | ✅ работает | Ячейка с двухфазными свойствами | `TwoPhaseFlowCell` |
| `Solver/Grids/Cells/AbstractCells` | ✅ работает | Иерархия ячеек (Dim1/2/3, PhysPropCell) | `SomeProcessCell_TimeDependent` |
| `Solver/Grids/GridDescriptors` | ✅ работает | Типы-алиасы для данных сетки | `GridSize`, `GridBounds`, `BlockSize` |
| `Solver/Grids/PropertyDescriptor` | ✅ работает | Свойства фаз и конвертер единиц | `PhaseProperties`, `OtherProperties` |
| `Solver/Grids/RawHorizon` | ✅ работает | Агрегат данных горизонта (слоя) | `RawHorizon` |
| `Solver/Math/LinearProblem` | ✅ работает | Сборка/решение СЛАУ через amgcl | `LinearProblem` |
| `Solver/Math/MatrixCSR` | ✅ работает | CSR-матрица с блочной структурой | `MatrixCSR` |
| `Solver/Math/SparsityPattern` | ✅ работает | Шаблон разреженности блочной матрицы | `SparsityPattern` |
| `Solver/Math/MathRoutines` | ✅ работает | Интерполяция, интеграция ОДУ (Euler2, RK4) | `MathRoutines` |
| `Data/ReservoirFactory` | ✅ работает | Чтение сетки и кубов из DB | `GridGeometryFactory`, `ReservoirFactory` |
| `Data/WellFactory` | ✅ работает | Чтение скважинных данных из DB | `WellFactory`, `MERFactory` |
| `Data/HorizonFactory` | ✅ работает | Композиция: сетка + скважины + фазы | `HorizonFactory`, `DevelopedHorizon` |
| `Data/ConnectionFactory` | ✅ работает | Обёртка DB-соединения | `ConnectionFactory` |
| `Data/RawWellFactory` | ✅ работает | Сырые скважинные данные из DB | `RawWellFactory`, `PerforationFactory`, `RawMERFactory` |
| `Data/PhaseFactory` | ✅ работает | Захардкоженные свойства фаз | `PhaseFactory`, `OtherFactory` |
| `Data/ExceptionFactory` | ✅ работает | Типизированные исключения и предупреждения (429 строк) | `MessageFactory`, `WarningFactory` |
| `Data/DBUtils` | ✅ работает | Работа с PostgreSQL через pqxx (Large Objects) | `DBSaver`, `DBLoader` |
| `Helpers/Config` | ⬜ заглушка | Config.cpp = 1 строка (пустой) | Абстрактный `Config` |
| `Helpers/Config_JSON` | ⚠️ незавершён | Парсинг JSON-конфига (заголовок 159 строк, .cpp = 2 строки) | `Config_JSON` |
| `Helpers/DataPrinter` | ✅ работает | Вывод данных (header-only) | `DataPrinter` |
| `Anomaly/Anomaly` | ⚠️ незавершён | Обнаружение аномалий через фазовые портреты | `SingleWellDomain` |
| `Anomaly/Trajectory` | ⬜ заглушка | Почти пустой | `Trajectory` |
| `Anomaly/FlowField/` | ✅ работает | Поле скоростей, трассировка частиц | `FlowField`, `SomeFlowField`, `Point` |
| `Descriptors/Descriptors` | ✅ работает | SchemeParamaters, AnomalyDetectionProperties | |
| `Descriptors/MER_Descriptor` | ✅ работает | Данные МЭР (помесячная эксплуатация) | `MER_Data` |
| `tests/simulate_reservoir` | ✅ работает | Точка запуска симуляции | `simulate_reservoir()`, `run_simulations()` |
| `tests/create_*` | ✅ работает | Создание отдельных объектов для отладки | |
| `Utils/` | ✅ работает | SVParser, BinaryFileHandler, IRCGEngine, JSON | |
| `GridEngine/` | ✅ работает | Парсер GRDECL-файлов → PostgreSQL | `grdecl::Parser` |
| `Shared0/` | 🔴 дубликат | **Полная копия** HydroSolver/Utils/ | |

## 4. Зависимости

### Внешние
| Библиотека | Назначение | Критичность |
|-----------|-----------|-------------|
| **amgcl** (AMGSolver/) | Линейный солвер (AMG + GMRES, блоки 2×2) | Критичная, ядро |
| **pqxx** (libpqxx) | Клиент PostgreSQL — всё чтение данных | Критичная, I/O |
| **GEOS** (GeosPoint) | Геометрия (координаты скважин, контуры) | Средняя |
| Стандартная библиотека C++17 | `<filesystem>`, `<execution>`, structured bindings | Критичная |

**Примечание:** Зависимость от PostgreSQL — архитектурная. Все входные данные (сетка, кубы свойств, координаты скважин, МЭР, перфорации) читаются исключительно из PostgreSQL через pqxx. Нет альтернативного ввода из файлов.

### Внутренние зависимости
- `ReservoirSimulator` → `OilField`, `LinearProblem`, `SomeWell`, `FlowField`
- `OilField` → `AbstractGrid<TwoPhaseFlowCell>`, `RawHorizon`
- `LinearProblem` → `MatrixCSR` → `SparsityPattern`, `amgcl`
- `SomeWell` → `WellTrajectory`, `MER_Data`, `AccumulatedPerforations`, `TwoPhaseFlowCell`
- `HorizonFactory` → `ReservoirFactory`, `WellFactory`, `PhaseFactory`
- `ReservoirFactory` → `ConnectionFactory` → `DBLoader` (pqxx)
- `GridEngine` → `DBSaver` (pqxx) — **не** используется из HydroSolver; это утилита предобработки, заливающая GRDECL в PostgreSQL

## 5. Проблемы и технический долг

### Критические
1. **Жёсткая привязка к PostgreSQL.** Все данные читаются только из БД через pqxx. Для работы симулятора нужна запущенная PostgreSQL с данными в специфическом формате. Тестовая строка подключения: `"ljihnlsqjfybogpnpddrvrmvwgfyxefj"`. Это делает проект невозможным для запуска без инфраструктуры.

2. **Z-направление отключено.** В `SetConnectivityGraph_3D` связи по Z закомментированы. В `AccountForBoundaryConditions` условие `if (false && (nz > 1))`. Фактически сетка работает как **набор независимых 2D-слоёв**, а не как 3D.

3. **Утечка памяти в Wells.** `std::map<WellName, wells::SomeWell*> Wells` — raw pointers, создаются через `new`, удаляются в деструкторе. Нет copy/move конструкторов — нарушение Rule of Five.

4. **Захардкоженные свойства в RawReservoirFactory.** В строках 122–127 файла ReservoirFactory.cpp все прочитанные из БД значения **перезаписываются** константами: проницаемость = 400 мД, пористость = 0.15, все ячейки активны. Это отладочная заглушка, которая аннулирует реальные данные.

5. **Физическая ошибка в граничных условиях.** В `AccountForBoundaryConditions` для XY-граней при притоке (dp < 0) плотность нефти умножается на `DensityOil()`, а при оттоке — нет. Формулы по X/Y-граням не идентичны формулам по Z-грани (хотя код Z-грани всё равно отключён). Требуется верификация.

6. **`const_cast` в скважинном модуле.** Множественные `const_cast` в `SomeWell::BringFirstPerforationToFirstMER()` и подобных — признак нарушения const-корректности.

### Заглушки (файлы с минимальным содержанием)
- `AnomalySimulator.h` — пустой наследник `ReservoirSimulator` (0 методов)
- `WellTrajectory.cpp` — 1 строка (include)
- `Config.cpp` — 1 строка (include, пустой)
- `Config_JSON.cpp` — 2 строки (только include + `#include`)
- `ReservoirSimulator::Continue()` — пустое тело метода

### Дубликаты кода
- **Shared0/ ≡ HydroSolver/Utils/** — все .h файлы побайтно идентичны (10 из 10). JSONCreate.h отличается одной строкой (`,` в find). **Shared0 следует удалить.**
- Код `AccountForBoundaryConditions` (350 строк) дублирует структуру для X, Y, Z с минимальными различиями — можно параметризовать.
- `OilContourFlux` (130 строк) дублирует ту же логику.
- `OverallFluxes` (250 строк) — аналогичная ситуация.

### Незавершённые модули
- **Anomaly/** — частично реализован обнаружение аномалий через фазовые портреты (трассировка частиц). Зависит от GEOS для полигонов. Функционально не связан с основным солвером, это постпроцессинг.
- **Config_JSON** — парсинг JSON-конфига вместо PostgreSQL. Заголовок описывает 159 строк интерфейса, реализация отсутствует.
- **WellFixedPressure** — режим скважины с фиксированным давлением **не реализован** (есть только WellFixedProduction).

### Прочие проблемы
- OMP-параллелизм закомментирован повсюду (`#ifdef USE_PARALLEL`)
- `std::wstring` используется повсеместно (Windows-специфика), в том числе для имён файлов и ключей словарей
- Типы-алиасы через `using ... = struct { ... }` вместо нормальных структур (UB до C++20)
- `static` глобальные переменные в заголовках (`PhysPropCell::ConstantPointProperties`) — проблема при многопоточности
- `FluxReader` и `ReservoirSimulator::LoadFlowFieldFromFile_bin` содержат захардкоженные значения (nz = 4)

## 6. Рекомендации

### Переиспользовать как есть
- **amgcl** — зрелая библиотека, header-only, прекрасно работает для блочных систем
- **Solver/Math/** — `LinearProblem`, `MatrixCSR`, `SparsityPattern` — хорошо структурированы, понятная логика CSR-формата
- **Reservoir/Well/SetOfPoints** — нетривиальная геометрия перфораций (объединение/вычитание отрезков), хорошо реализована

### Требует рефакторинга
- **ReservoirSimulator** — монолитный файл (1184 строки). Разделить на: (a) сборка системы, (b) граничные условия, (c) вычисление потоков, (d) ввод/вывод
- **Solver/Grids/AbstractGrid** — шаблонный класс с `std::vector<int>` для connectivityGraph, `int` для индексов. Привести к `size_t` / `std::ptrdiff_t`
- **TwoPhaseFlowCell** — массивы свойств по магическим индексам (DependentFieldProperties[0..9]). Заменить на именованные поля
- **SomeWell** — избавиться от raw pointers и const_cast
- **AccountForBoundaryConditions** — 350 строк копипасты → параметризованная функция по направлению

### Писать с нуля
- **Ввод данных** — заменить pqxx/PostgreSQL на чтение из файлов (JSON, GRDECL, или CSV). Это устраняет критическую зависимость от инфраструктуры
- **CMakeLists.txt** — сейчас пуст, нужно подключить все исходники, amgcl, убрать зависимость от pqxx
- **Конфигурация** — Config_JSON не реализован. Написать простой конфиг (JSON или ini) для параметров задачи
- **Юнит-тесты** — текущие `create_*` и `simulate_reservoir` привязаны к PostgreSQL. Нужны автономные тесты с синтетическими данными (Buckley–Leverett)
- **Режим скважины с фиксированным давлением** — не реализован

### Приоритет подъёма модулей
1. **CMake + сборка** — подключить исходники, amgcl; отключить pqxx
2. **Ввод данных из файлов** — заменить PostgreSQL на файловый ввод (JSON/текст)
3. **Z-связность** — раскомментировать и протестировать
4. **Валидация** — Buckley–Leverett в 1D
5. **Рефакторинг ячейки** — убрать магические индексы
6. **Второй режим скважин** — фиксированное давление
