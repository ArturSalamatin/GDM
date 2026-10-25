---
tags:
  - архитектура
  - линии-тока
  - streamlines
date: 2026-06-18
---

# Модуль линий тока (Anomaly/FlowField)

## Расположение

`HydroSolver/Anomaly/` — расчёт траекторий частиц жидкости (линий тока) по полю скоростей.

## Ключевые классы

### FlowField/SomeFlowField
- `SomeFlowField` — хранит последовательность `FlowFieldSnapshot` (vx, vy на staggered-сетке) + поле пористости
- Интерполяция: билинейная по пространству (`InterpFieldConstTime`), линейная по времени между снапшотами
- Инициализируется в конструкторе `ReservoirSimulator`, снапшоты добавляются через `AddFlowFieldSnapShot()` после каждого принятого шага
- Компоненты скорости — суммарный поток (нефть + вода) через грани ячеек (`OverallFluxes()`)

### FlowField/Point
- `Point` — (x, y)
- `trPoint` — (t, s, x, y) — точка траектории с временем и пройденным расстоянием
- `geos_polygon` — заглушка (заменяет GEOS), геометрические операции `contains`/`intersects`/`Union` пока тривиальны

### Trajectory
- Единичная траектория = вектор `trPoint`
- `FollowTrajectory()` — интегрирование по нестационарному полю (интерполяция по t)
- `FollowTrajectory_fixed_time()` — интегрирование при фиксированном t (мгновенная картина поля)
- ODE-интегратор: `MathRoutines::IntegrateODE` — модифицированный Эйлер (Heun), O(h²), адаптивный шаг

### PhasePortrait (FlowField.h)
- Ансамбль траекторий с общей точкой старта
- `DistributeStartPoints_radial()` / `_rectangle()` — расстановка начальных точек
- `improve_discretization()` — адаптивное добавление траекторий при расхождении

### Anomaly (Anomaly.h/cpp)
- `WellSignals`, `SingleWellDomain`, `Anomalies` — legacy-обвязка для детекции аномалий в работе скважин
- **Не включён в CMake**, зависит от `WellDataHandler`, `GeoJsonEngine`, `LogFile`
- Для базовых линий тока НЕ нужен

## Boundary handling

За пределами сетки `InterpFieldConstTime` → NaN → `f_const` возвращает {0,0,0} → траектория останавливается.

## Тестирование

- `tests/test_streamlines.cpp` — два тест-кейса (single injector, two wells)
- Экспорт CSV: `streamlines.csv`, `velocity_field.csv`, `metadata.json`
- Визуализация: `scripts/plot_streamlines.py`

## Связи

- [[численные методы решения уравнений фильтрации]] — поле скоростей из решения уравнения давления
- [[структура проекта и конвенции кода]] — расположение модулей
