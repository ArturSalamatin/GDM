---
tags:
  - архитектура
  - тестирование
  - зависимости
date: 2026-06-24
---

# Граф зависимостей классов gdm_core

Граф построен для библиотеки `gdm_core` (CMakeLists.txt). Исключены `GridEngine/` и `HydroSolver/AMGSolver/`. Уровень класса = максимальная глубина его зависимостей от других классов проекта. Уровень 0 — классы без внутрипроектных зависимостей.

## Обзор уровней

| Уровень | Классы | Характеристика |
|---------|--------|----------------|
| 0 | [[юнит-тесты уровень 0 нулевые зависимости]] | Value-types и утилиты, зависят только от STL |
| 1 | [[юнит-тесты уровень 1 зависимости от уровня 0]] | Контейнеры и агрегаторы, используют типы уровня 0 |
| 2 | [[юнит-тесты уровень 2 средняя глубина зависимостей]] | Алгоритмические классы, CRS-структуры, иерархия ячеек |
| 3 | [[юнит-тесты уровень 3 составные классы]] | MatrixCSR, TwoPhaseFlowCell, WellJobs — зависят от нескольких уровней |
| 4+ | OilField, LinearProblem, SomeWell, ReservoirSimulator | Интеграционные — не для юнит-тестов в чистом виде |

## Диаграмма зависимостей (упрощённая)

```
Уровень 0 (STL only)
  GeosPoint/SimplePoint
  GridDescriptors (GridSize, GridShift, GridBounds, BlockSize)
  PropertyDescriptor (PhaseProperties, CustomUnitConverter, OtherProperties)
  PIController + PIControllerParams
  Point, trPoint, Contour, Coordinate, geos_polygon
  Segment, WellJob, WellJobTime

Уровень 1 (→ Уровень 0)
  SetOfPoints           → Segment, WellJob
  SetOfPerforations     → SetOfPoints, WellJobTime
  RawHorizon            → GridDescriptors, PropertyDescriptor
  SchemeParameters      → GridDescriptors, PropertyDescriptor
  FluidDebit, TimeFrame → (defines.h, через GeosPoint + SetOfPoints)
  MathRoutines (static) → Point
  DebitConversion2SI    → (standalone)

Уровень 2 (→ Уровни 0–1)
  AccumulatedPerforations → SetOfPerforations, WellJobTime
  MER_Data                → FluidDebit, TimeFrame, defines.h
  NumericalParameters     → PIController, defines.h
  CRSStructure            → STL only (но логически связан с SparsityPattern)
  SparsityPattern         → STL only
  AbstractCells hierarchy → STL only (SomeDimCell → Dim1Cell → Dim2Cell → Dim3Cell)
    TimeDependentCell     → STL only
    PhysPropCell          → STL only
    SomeProcessCell_TimeDependent<Dim3Cell> → все три выше

Уровень 3 (→ Уровни 0–2)
  MatrixCSR             → CRSStructure
  TwoPhaseFlowCell      → SomeProcessCell_TimeDependent<Dim3Cell>
  WellJobs              → SetOfPoints, WellJobTime, AccumulatedPerforations, defines.h

Уровень 4+ (интеграция)
  AbstractGrid/SomeStructuredGrid3Dim → AbstractCells, GridDescriptors
  OilField              → RawHorizon, AbstractGrid, TwoPhaseFlowCell, PropertyDescriptor
  LinearProblem         → MatrixCSR, CRSStructure, amgcl (CPRSolver)
  WellTrajectory        → TwoPhaseFlowCell, defines.h
  SomeWell              → WellTrajectory, MER_Data, SetOfPoints, ExceptionFactory
  WellFixedProduction   → SomeWell
  ReservoirSimulator    → OilField, LinearProblem, Wells, NumericalParameters
```

## Файлы в сборке gdm_core

Из CMakeLists.txt (`add_library(gdm_core STATIC ...)`):

### Reservoir
- `Reservoir/ReservoirSimulator.cpp` — главный класс симулятора
- `Reservoir/NumericalParameters.cpp` — параметры численной схемы
- `Reservoir/PIController.cpp` — адаптивный шаг по времени

### Grid / Cells
- `Solver/Grids/OilField.cpp` — сетка с ячейками двухфазного потока
- `Solver/Grids/Cells/AbstractCells.cpp` — иерархия ячеек (Dim1..3, PhysPropCell)
- `Solver/Grids/Cells/TwoPhaseFlowCell.cpp` — ячейка с Corey relperm

### Math / Linear algebra
- `Solver/Math/LinearProblem.cpp` — СЛАУ с CPR-солвером (amgcl)
- `Solver/Math/MatrixCSR.cpp` — CRS-матрица с блочной структурой
- `Solver/Math/SparsityPattern.cpp` — паттерн разреженности
- `Solver/Math/CRSStructure.cpp` — layout-абстракция (Interleaved/Blocked)
- `Solver/Math/MathRoutines.cpp` — интерполяция, ODE-интеграторы

### Wells
- `Reservoir/Well/SomeWell.cpp` — базовый класс скважины
- `Reservoir/Well/Wells.cpp` — WellFixedProduction
- `Reservoir/Well/WellJobs.cpp` — агрегация перфораций
- `Reservoir/Well/SetOfPoints.cpp` — операции над отрезками (sweep-line)
- `Reservoir/Well/WellTrajectory.cpp` — траектория скважины в сетке

### Flow field / Streamlines
- `Anomaly/FlowField/SomeFlowField.cpp` — абстрактное поле скоростей
- `Anomaly/FlowField/FlowField.cpp` — дискретное поле на сетке
- `Anomaly/FlowField/Point.cpp` — 2D-точка, trPoint
- `Anomaly/Trajectory.cpp` — трассировка линий тока

### Descriptors
- `Descriptors/Descriptors.cpp` — SchemeParameters, AnomalyDetectionProperties
- `Descriptors/MER_Descriptor.cpp` — MER_Data (дебиты скважин)

### Helpers
- `Helpers/LogFile.cpp` — статический лог

## Порядок покрытия тестами

1. Уровень 0 → 2. Уровень 1 → 3. Уровень 2 → 4. Уровень 3
5. Уровень 4+ только как интеграционные тесты (уже частично покрыты в test_smoke, test_components, и др.)

## Связанные заметки

- [[юнит-тесты уровень 0 нулевые зависимости]]
- [[юнит-тесты уровень 1 зависимости от уровня 0]]
- [[юнит-тесты уровень 2 средняя глубина зависимостей]]
- [[юнит-тесты уровень 3 составные классы]]
- [[текущие приоритеты]]
