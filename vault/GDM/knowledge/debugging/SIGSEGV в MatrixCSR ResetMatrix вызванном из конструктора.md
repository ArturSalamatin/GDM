---
tags:
  - debugging
  - amgcl
date: 2026-06-20
---
# Промежуточный SIGSEGV при оптимизации MatrixCSR::ResetMatrix()

## Суть

Не баг legacy-кода, а артефакт промежуточного состояния при рефакторе.

## Что произошло

`ResetMatrix()` вызывался на каждом Newton step и пересоздавал вектор:
```cpp
void MatrixCSR::ResetMatrix() {
    value = std::vector<double>(nnz, 0.0);  // аллокация каждый раз
}
```

При оптимизации заменён на `std::fill` (без аллокации):
```cpp
void MatrixCSR::ResetMatrix() {
    std::fill(value.begin(), value.end(), 0.0);
}
```

Но конструктор вызывал `ResetMatrix()` для начальной инициализации `value`. После замены на `std::fill` вектор оставался пуст → краш при первом обращении к `value[i]`.

## Исправление

Конструктор: `ResetMatrix()` → `value.resize(nnz, 0.0)`.

## Связанные заметки

- [[план профилирования и оптимизации AMGCL]]
