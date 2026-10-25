---
tags: [decisions, build, architecture]
date: 2026-06-16
---

# Отвязка от PostgreSQL и GEOS через заглушки и синтетический main

## Контекст

Legacy-код HydroSolver зависел от PostgreSQL (libpqxx) для ввода данных и GEOS для геометрии контуров.
Обе зависимости не нужны для ядра солвера.

## Решение

1. **PostgreSQL (pqxx)**: удален `#include <pqxx/pqxx>` из `stdafx.h`. Весь слой `Data/` (фабрики, конфиги) исключен из сборки. Вместо `HorizonFactory` создан класс `DevelopedHorizon` с прямой инициализацией полей.
2. **GEOS**: `Point.h` переписан с заглушками `Coordinate` и `geos_polygon`. `FlowField.h` переключен на локальные `Coordinate`. `GeosPoint.h` заменен на `SimplePoint`.
3. **UniversalSVWriter/Parser**: файлы в кодировке cp1251, содержат wchar_t литералы с кириллицей — несовместимы с `/utf-8`. Функции `SaveFlowField2File` и `LoadFlowFieldFromFile` заглушены.
4. **Синтетический main**: `src/main.cpp` строит 20x20x1 однородный пласт без скважин, запускает `Solve()`.

## Что исключено из сборки

- `Data/*.cpp` — все фабрики (DB-зависимые)
- `Utils/*.cpp` — проблемы с кодировкой
- `HydroSolver.cpp` — старый main
- `Config*.cpp`, `Anomaly/Anomaly.cpp`, `Anomaly/Trajectory.cpp`

## Почему не рефакторинг

Рефакторинг legacy I/O-слоя — отдельная задача. Сейчас цель — запуск солвера. Заглушки минимально-инвазивны: код скомпилируется и при восстановлении оригинальных функций.

## Связанные заметки

- [[линейный солвер — AMG через amgcl]]
- [[схема дискретизации полностью неявная а не IMPES]]
