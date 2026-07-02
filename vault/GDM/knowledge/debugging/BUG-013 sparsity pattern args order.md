---
tags:
  - debugging
  - баг
  - исправлено
date: 2026-07-02
issue: BUG-013
github: https://github.com/ArturSalamatin/GDM/issues/9
---

**GitHub issue:** [#9](https://github.com/ArturSalamatin/GDM/issues/9)

# BUG-013: SparsityPattern 3-arg конструктор — аргументы vector\<bool\> перепутаны

## Симптом

3-arg convenience конструктор `SparsityPattern(eqNmbr, cellNmbr, graph)` создаёт `blockPattern` размера 1 вместо `eqNmbr²`. При `eqNmbr > 1` — немедленный OOB в теле 4-arg конструктора (crash в Debug, silent corruption в Release).

## Корневая причина

`std::vector<bool>(true, eqNmbr_ * eqNmbr_)` — первый аргумент `true` неявно преобразуется в `size_t(1)`, создавая вектор размера 1. Правильно: `(eqNmbr_ * eqNmbr_, true)`.

Дополнительно: тип `cellNmbr` был `int` (в отличие от `size_t` в 4-arg конструкторе) — implicit conversion при делегировании.

## Решение

1. `std::vector<bool>(static_cast<size_t>(eqNmbr_) * eqNmbr_, true)` — аргументы в правильном порядке, `static_cast` исключает повторение ошибки
2. Тип `cellNmbr`: `int` → `size_t` — согласован с 4-arg конструктором
3. Добавлены 3 теста: воспроизводитель, сравнение 3-arg/4-arg, граничный случай eqNmbr=1

## Связанные заметки

- [[SparsityPattern abort в Debug при пустых массивах]] — BUG-019 (исправлен ранее)
- [[bug-013 sparsity-pattern-args-order]] — план исправления
