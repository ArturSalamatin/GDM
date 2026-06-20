---
tags:
  - debugging
  - amgcl
  - crash
date: 2026-06-20
---

# SIGSEGV в MatrixCSR::ResetMatrix(), вызванном из конструктора

## Симптом

Нестабильный SIGSEGV при добавлении любых полей в `LinearProblem.h`. Проявляется при clean build, пропадает при инкрементальной сборке.

## Причина

`MatrixCSR` конструктор вызывал `ResetMatrix()`, который делал `value = std::vector<double>(nnz, 0.0)`. После рефактора `ResetMatrix()` стал делать `std::fill(value.begin(), value.end(), 0.0)` — на пустом `value` это корректно (no-op), но вектор остаётся пустым.

Дополнительный фактор: в `MatrixCSR.h` поле `value` объявлено **до** `nnz`. По стандарту C++ члены инициализируются в порядке объявления, а не в порядке initializer-list. Поэтому `value(nnz, 0.0)` в member-init-list — UB: `nnz` ещё не инициализирован.

## Исправление

```cpp
// Конструктор — value.resize() в теле, после инициализации nnz
MatrixCSR::MatrixCSR(...) noexcept :
    pattern{ ... },
    nnz{ sparsity_pattern().TotalNmbrOfBlocks() * sparsity_pattern().NmbrOfNonzerosPerUnitBlock() }
{
    value.resize(nnz, 0.0);  // nnz уже валиден
}

// ResetMatrix() — std::fill, не аллокация
void MatrixCSR::ResetMatrix()
{
    std::fill(value.begin(), value.end(), 0.0);
}
```

## Связь

Ранее подозревались `virtual ~LinearProblem()`, move-конструктор, `noexcept` в `ReservoirSimulator` — всё это были ложные следы. Баг сидел в `MatrixCSR` и проявлялся при любом изменении, вызывающем full recompile.

## Связанные заметки

- [[план профилирования и оптимизации AMGCL]]
