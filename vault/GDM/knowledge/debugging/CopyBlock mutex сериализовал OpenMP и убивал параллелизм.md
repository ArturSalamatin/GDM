---
tags:
  - баг
  - производительность
  - openmp
  - матрица
date: 2026-06-21
---

# CopyBlock mutex сериализовал OpenMP и убивал параллелизм

## Проблема

`MatrixCSR::CopyBlock` содержал `static std::mutex CopyBlockMutex` с `std::lock_guard`. Мьютекс был добавлен для thread-safety при OpenMP-параллелизации `fillMatrixBlockRow`.

При 10404 ячейках × 7 CopyBlock/ячейку × 1112 Newton-итераций = ~81М lock/unlock операций. Мьютекс полностью сериализовал запись в матрицу — assembly занимала 21 с вместо 2.7 с.

## Почему мьютекс не нужен

`#pragma omp parallel for` разбивает цикл по ячейкам `l`. Каждый поток обрабатывает свой набор ячеек.

`fillMatrixBlockRow(l, ...)` вызывает:
- `AddDiagBlock(l, ...)` — пишет в строку `l` CRS-матрицы
- `AddOffDiagBlock(l, neibCount, ...)` — пишет в строку `l` CRS-матрицы (столбец `neibCount`)

Каждый поток пишет **только в свою строку** матрицы. Строки CRS не перекрываются в массиве `value[]`. Data race невозможен.

## Исправление

Удалён `static std::mutex` и `std::lock_guard`. `CopyBlock` стал простым циклом `dest[blockPosInValArray[offset + i]] += data[i]`.

## Эффект

Assembly: 21 с → 2.7 с (–87%). Полное время: 63.8 → 43.7 с (–32%).

## Связанные заметки

- [[2026-06-21 сессия 5 оптимизация assembly]]
