---
tags:
  - баг
  - debug
  - SparsityPattern
  - assertion
date: 2026-06-28
issue: BUG-019
github: https://github.com/ArturSalamatin/GDM/issues/3
---

**GitHub issue:** [#3](https://github.com/ArturSalamatin/GDM/issues/3)

# SparsityPattern abort в Debug при пустых массивах

## Симптомы

При запуске Debug-тестов `gdm_unit_level2.exe` два теста вызывают `abort()`:

1. **`SparsityPattern: single cell has 1 block`** — `Debug Assertion Failed: can't dereference out of range vector iterator` (`<vector>` line 56)
2. **`SparsityPattern: partial block pattern (diagonal only)`** — `Debug Assertion Failed: vector iterators incompatible` (`<vector>` line 202)

В Release оба теста проходят — `assert` отключён, UB молчит.

## Воспроизведение

Ветка: любая (pre-existing, существует на `experimental`).

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug -R "SparsityPattern: single cell"
# → abort(), диалог Microsoft Visual C++ Runtime Library
```

### Тестовый сценарий 1 (single cell)

```cpp
// tests/unit/math/test_SparsityPattern.cpp:50-58
std::vector<std::vector<int>> graph = {{}};  // 1 ячейка, 0 соседей
std::vector<bool> blockPattern = {true, true, true, true};
SparsityPattern sp(2, 1, graph, blockPattern);  // → abort
```

### Тестовый сценарий 2 (partial block)

```cpp
// tests/unit/math/test_SparsityPattern.cpp:60-70
std::vector<std::vector<int>> graph = {{1}, {0}};
std::vector<bool> blockPattern = {true, false, false, true};  // только диагональ
SparsityPattern sp(2, 2, graph, blockPattern);  // → abort
```

## Корневая причина

### Проблема 1: `max_element` на пустом контейнере

Конструктор `SparsityPattern` (строки 159–161 в `SparsityPattern.cpp`) безусловно вызывает:

```cpp
printPattern();         // → printPattern2Stream → *max_element(col_raw.begin(), col_raw.end())
printDiagonalBlocks();  // → printDiagBlocks2Stream → *max_element(diagBlocks_raw.begin(), diagBlocks_raw.end())
printOffDiagBlocks();   // → printOffDiagBlocks2Stream → *max_element(offDiagBlocks_raw.begin(), offDiagBlocks_raw.end())
```

Для 1 ячейки без соседей `offDiagBlocks_raw` пуст. `std::max_element` на пустом range возвращает `end()`, его разыменование — UB. В Debug MSVC это assertion.

Место: `SparsityPattern.h:60` (`printPattern2Stream`), `:86` (`printDiagBlocks2Stream`), `:108` (`printOffDiagBlocks2Stream`).

### Проблема 2: erase во время итерации

Строки 143–149 в `SparsityPattern.cpp`:

```cpp
for (auto l = offDiagBlocks_raw.begin(); l != offDiagBlocks_raw.end();)
{
    if (*l == 0)
        offDiagBlocks_raw.erase(l);  // инвалидирует итераторы!
    else
        ++l;
}
```

`vector::erase` инвалидирует все итераторы начиная с точки удаления. Код рассчитывает на то, что `erase` возвращает валидный итератор на следующий элемент (это верно), но присваивает результат `l` только неявно (через fact, что `erase` возвращает новый итератор, а код его игнорирует — `l` остаётся прежним). В MSVC Debug iterator debugging ловит использование инвалидированного итератора.

Правильно: `l = offDiagBlocks_raw.erase(l);`

## Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `HydroSolver/Solver/Math/SparsityPattern.cpp` | 143–149, 159–161 | erase без сохранения итератора, безусловный print |
| `HydroSolver/Solver/Math/SparsityPattern.h` | 60, 86, 108 | `*max_element` без проверки пустоты |

## Варианты решения

### Вариант A: Минимальный фикс

1. Добавить `if (!vec.empty())` перед каждым `*max_element`
2. Заменить `offDiagBlocks_raw.erase(l)` на `l = offDiagBlocks_raw.erase(l)`
3. Обернуть print-вызовы в `#ifdef GDM_DEBUG_SPARSITY` или убрать из конструктора

### Вариант B: Правильное решение (рекомендуется)

1. Убрать print-вызовы из конструктора полностью — конструктор не должен делать I/O
2. Исправить erase-цикл: `l = offDiagBlocks_raw.erase(l)` или заменить на `std::erase_if` (C++20) / `erase-remove idiom`
3. Если print нужен для отладки — вынести в отдельный метод `void dump(const std::string& dir) const`

## Связанные задачи

- [[DEBT-009]]: test*.txt дамп в cwd — та же категория проблем
- [[DEBT-044]]: SparsityPattern конструктор пишет файлы в cwd
- [[BUG-013]]: SparsityPattern конструктор — аргументы `vector<bool>` перепутаны (тот же класс)

## Рабочая ветка

Исправление должно выполняться от `experimental` в ветке `fix/bug-019/sparsity-pattern-debug-abort`.
