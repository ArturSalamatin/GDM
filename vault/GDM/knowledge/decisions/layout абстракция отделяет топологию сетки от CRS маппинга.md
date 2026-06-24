---
tags:
  - решение
  - архитектура
  - матрица
  - layout
date: 2026-06-23
---

# Layout абстракция отделяет топологию сетки от CRS маппинга

## Проблема

CPR-прекондиционер в amgcl жёстко использует `col % B == 0` для выбора давленческих столбцов. Наш порядок переменных `[Sw, P]` означает, что CPR выбирает Sw вместо P.

Первоначальный план — runtime swap строк/столбцов при передаче в CPR — работал бы, но создавал хрупкую связь между порядком в матрице и выбором солвера.

## Решение

Новый класс `CRSStructure` инкапсулирует построение CRS-структуры (`row`, `col`, `diagBlocks`, `offDiagBlocks`) из топологии сетки и `Layout`:

```
enum class Layout { InterleavedSwP, InterleavedPSw, Blocked };
```

- **InterleavedSwP** — текущий порядок `[Sw₀,P₀, Sw₁,P₁, ...]`
- **InterleavedPSw** — для CPR `[P₀,Sw₀, P₁,Sw₁, ...]` (col%B==0 → P)
- **Blocked** — `[P₀,...,Pₙ, Sw₀,...,Swₙ]` (заглушка для будущих задач)

Ключевой контракт: `fillMatrixBlockRow` всегда заполняет блоки в физическом порядке `[d(oil)/d(Sw), d(oil)/d(P), d(water)/d(Sw), d(water)/d(P)]`. `diagBlocks_` и `offDiagBlocks_` индексируются в физическом порядке, но указывают на CRS-позиции с учётом layout. `CopyBlock(data[i]) → val[diagBlocks[offset + i]]` работает без изменений.

## Архитектура

- `SparsityPattern` — старый класс, не используется (осиротел)
- `CRSStructure` — новый, хранит row/col/diagBlocks/offDiagBlocks + permutation tables
- `MatrixCSR` — владеет `unique_ptr<CRSStructure>`, нешаблонный
- `LinearProblem(Layout, ...)` — передаёт Layout при создании MatrixCSR
- `LinearProblem::AddDiagBlock` — пишет rhs через `crs.GlobalIndex(cell, var)`
- `LinearProblem::UnpackCellCorrections` — обратный маппинг CRS→физический
- `UpdateGrid` — вызывает `UnpackCellCorrections` вместо прямого `Corrections()[B*l + i]`

## Blast radius

Затронуты: CRSStructure (новый), MatrixCSR.h/cpp, LinearProblem.h/cpp, ReservoirSimulator.cpp, AbstractGrid.h (non-const operator[]), AbstractCells.h (UpdateState(const double*)).

Не затронуты: fillMatrixBlockRow, Wells.cpp, boundary conditions, TwoPhaseFlowCell, Solve().

## Связанные заметки

- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[план-сессия-6-CPR-прекондиционер]]
