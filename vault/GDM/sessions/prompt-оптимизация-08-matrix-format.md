---
tags:
  - промпт
  - производительность
  - рефакторинг
  - матрица
date: 2026-06-21
---

# Сессия 8: Блочный матричный формат и устранение копирования

## Предварительно

Прочитай `vault/GDM/00-home/текущие приоритеты.md`.

## Контекст

Текущий `MatrixCSR` хранит скалярную CRS-матрицу:
- `value: std::vector<double>` — все элементы развёрнуты (для блока 2×2 → 4 double подряд)
- `Row(), Col()` — скалярные индексы (размерность `rhsSize = cellNmbr × B`)

При каждом вызове `Solve()` AMGCL перепаковывает скалярную матрицу в блочный формат через `block_matrix<value_type<B>>`:
```cpp
auto A = amgcl::adapter::block_matrix<value_type<B>>(
    std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()));
```

Это создаёт новую CRS-матрицу с `static_matrix<2,2>` элементами. На 10404 ячейках (~73K ненулевых блоков) — ~2.3 MB данных. Копирование ~0.5–1 мс, пренебрежимо на текущей сетке. Но:

1. На крупных сетках (100×100×10 = 100K ячеек) — ~25 MB, копирование ~5–10 мс × 1000 solve = 5–10 с.
2. Если принят CPR (сессия 6), то CPR работает со скалярной матрицей → копирование не нужно вообще.
3. Если reuse AMG (шаг 3 из сессии 4) работает — всё равно нужна перепаковка для `operator()(A, rhs, x)`.

## Зависимость от архитектурных решений

Эта оптимизация зависит от того, какой солвер в production:

### Вариант A: Блочный AMG остаётся в production

Нужно хранить матрицу сразу в блочном формате `static_matrix<B,B>`. Это большой рефакторинг:
- `MatrixCSR` → `BlockMatrixCSR<B>`
- `value: std::vector<amgcl::static_matrix<double, B, B>>`
- `Row(), Col()` — блочные индексы (размерность `cellNmbr`)
- `AddDiagBlock`, `AddOffDiagBlock` — писать напрямую в блочные элементы
- `fillMatrixBlockRow` — адаптировать под блочный формат

Выигрыш: устранение перепаковки + cache locality (блочные элементы хранятся подряд).

### Вариант B: CPR принят (скалярный backend)

CPR работает со скалярной матрицей → текущий формат MatrixCSR идеален. Никакого рефакторинга не нужно. Устранение перепаковки — автоматически.

### Вариант C: Гибрид — хранить оба формата

Хранить скалярную CRS как primary и лениво создавать блочную при необходимости. Кэшировать блочную версию до `InvalidateSetup()`.

## Этап 1: Измерение стоимости перепаковки

### Инструментация

В `LinearProblem::Solve()` уже есть `prof.tic("setup")`. Разделить:
```cpp
prof.tic("block_matrix");
auto A = amgcl::adapter::block_matrix<value_type<B>>(...);
prof.toc("block_matrix");

prof.tic("amg_construct");
solver_ = std::make_unique<Solver_AMG<B>>(A, prm);
prof.toc("amg_construct");
```

Запустить бенчмарк, измерить долю `block_matrix` в setup.

### Масштабирование

Запустить на разных сетках:
- 21×21×4 (1764 ячейки)
- 51×51×4 (10404 ячейки)
- 101×101×4 (40804 ячейки) — если хватит памяти

Измерить `block_matrix` время на каждой сетке. Если линейно по nnz и < 1% от total — не стоит рефакторинга.

## Этап 2: Блочный MatrixCSR (только если оправдано)

### Архитектура

```cpp
template<unsigned char B>
class BlockMatrixCSR
{
    using block_type = amgcl::static_matrix<double, B, B>;

    std::vector<size_t> row_;    // size = cellNmbr + 1
    std::vector<size_t> col_;    // size = nnz_blocks
    std::vector<block_type> val_; // size = nnz_blocks

    // Для совместимости с CPR (скалярный доступ):
    std::vector<size_t> scalar_row_;  // size = cellNmbr*B + 1
    std::vector<size_t> scalar_col_;
    std::vector<double> scalar_val_;
    bool scalar_dirty_ = true;

public:
    void AddDiagBlock(size_t l, const block_type& block, const double* rhs);
    void AddOffDiagBlock(size_t l, size_t neibIdx, const block_type& block);

    // Для AMGCL блочного backend — zero-copy:
    const auto& BlockRow() const { return row_; }
    const auto& BlockCol() const { return col_; }
    const auto& BlockVal() const { return val_; }
    size_t BlockRows() const { return row_.size() - 1; }

    // Для CPR/скалярного доступа — lazy rebuild:
    const std::vector<size_t>& Row() const;
    const std::vector<size_t>& Col() const;
    const std::vector<double>& Val() const;
};
```

### Передача в AMGCL без копирования

Блочный backend AMGCL принимает CRS-кортеж:
```cpp
auto A = std::tie(cellNmbr, block_row, block_col, block_val);
Solver_AMG<B> solve(A, prm);
```

Но `block_val` должен быть `std::vector<static_matrix<B,B>>` — именно то, что хранит `BlockMatrixCSR`.

### Изменения в fillMatrixBlockRow

Вместо `std::vector<double> blDiag(4)` → `static_matrix<2,2> blDiag = {};`.
Вместо `MyProblem.AddDiagBlock(l, blDiag, rhsBlock)` → `MyProblem.AddDiagBlock(l, blDiag, rhsBlock)` с block_type.

Это пересекается с оптимизацией assembly (сессия 5) — координировать.

## Этап 3: rhs и x — блочные vectorы

Сейчас `rhs` и `solutionCorrections` — `std::vector<double>`. AMGCL блочный backend ожидает `std::vector<rhs_type<B>>` = `std::vector<static_matrix<2,1>>`.

Перепаковка rhs/x через `reinterpret_cast` (текущий подход) — zero-copy, если alignment правильный. `static_matrix<2,1>` содержит `double[2]` — alignment совпадает с `double[2]` в `std::vector<double>`.

Но формально `reinterpret_cast` — UB. Чистый подход: хранить rhs как `std::vector<rhs_type<B>>`.

Это тоже рефакторинг с большим blast radius — все обращения к `rhs[2*l]` → `rhs[l](0,0)`.

### Рекомендация

Оставить `reinterpret_cast` — работает, UB формальное (одинаковый layout). Не стоит рефакторинга ради формальной корректности.

## Порядок

1. Измерить стоимость перепаковки (этап 1) → решить, стоит ли
2. Если CPR принят (сессия 6) → не нужен блочный формат → skip этапы 2–3
3. Если блочный AMG остаётся → этап 2 (BlockMatrixCSR) → этап 3 (опционально)

## Риски

1. **Blast radius** — MatrixCSR используется всюду: assembly, boundary conditions, wells, print/debug. Замена формата затрагивает 10+ файлов.
2. **SparsityPattern** — текущий `SparsityPattern` хранит скалярные смещения. Нужен блочный SparsityPattern.
3. **Совместимость** — если будущие расширения (3-phase, compositional) увеличат B, шаблонизация должна оставаться чистой.

## Ожидаемый эффект

На текущей сетке (10404 ячеек): < 1% ускорения. На 100K ячеек: 1–2% ускорения. Основная ценность — архитектурная чистота и подготовка к масштабированию.

## Коммиты

```
refactor: инструментация block_matrix перепаковки
refactor: BlockMatrixCSR<B> с нативным блочным хранением
perf: zero-copy передача блочной матрицы в AMGCL
```

## Связанные заметки

- [[prompt-оптимизация-05-profiling-assembly]]
- [[prompt-оптимизация-06-CPR-прекондиционер]]
