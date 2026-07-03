---
tags:
  - debugging
  - баг
  - UB
date: 2026-07-03
issue: BUG-018
---

# BUG-018: snprintf с %u для size_t — UB на x64

## Симптом

`PrintModelData()` (RawHorizon.h) и `PrintPlanarMesh()` (DataPrinter.h) записывают grid metadata в текстовые файлы (`grid_descriptor.txt`, `MatLab/testPlanarMesh.txt`). Grid dimensions (`Nx`, `Ny`, `Nz`) выводятся как мусор на x64.

## Причина

`snprintf` с форматом `%u` (ожидает `unsigned int` = 4 байта) получает аргументы типа `size_t` (= 8 байт на x64). Аргументы на стеке сдвигаются — все значения после первого `size_t` читаются по неправильным смещениям. Дополнительно в `DataPrinter.h` четвёртый аргумент — литерал `3.0` (double) при формате `%u` — двойное UB.

## Фикс

- `%u` → `%zu` (стандартный формат для `size_t`, C99+, MSVC с VS2015)
- `3.0` → `static_cast<size_t>(3)` в DataPrinter.h

## Файлы

- `HydroSolver/Solver/Grids/RawHorizon.h:43` — `PrintModelData()`
- `HydroSolver/Helpers/DataPrinter.h:54` — `PrintPlanarMesh()`

## Связанные заметки

- [[bug-018 snprintf-size-t-format]]
