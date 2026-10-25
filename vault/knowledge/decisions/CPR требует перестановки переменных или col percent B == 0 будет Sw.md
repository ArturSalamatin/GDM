---
tags:
  - решение
  - amgcl
  - cpr
  - переменные
  - layout
date: 2026-06-21
updated: 2026-06-23
---

# CPR требует перестановки переменных или col%B==0 будет Sw

## Факт

`amgcl::preconditioner::cpr` и `cpr_drs` жёстко используют `col % B == 0` для выбора «давленческих» столбцов (cpr.hpp:341, cpr_drs.hpp:387). Нет параметра маски или выбора переменной.

## Наш порядок

`VariableFieldProperties[0] = Sw`, `VariableFieldProperties[1] = P` → в скалярной CRS столбцы `2l` = Sw, `2l+1` = P. CPR берёт `col % 2 == 0` = Sw.

## Решение (обновлено 2026-06-23)

~~Runtime swap строк/столбцов при передаче в CPR~~ → заменено layout-абстракцией.

Новый подход: `CRSStructure` строит CRS-матрицу с учётом `Layout`. Для CPR используется `Layout::InterleavedPSw`, при котором CRS-столбцы `2l` = P, `2l+1` = Sw. `col % 2 == 0` правильно указывает на P.

Преимущество:
- Нет runtime swap (zero overhead vs ~0.7 с per benchmark)
- `fillMatrixBlockRow` не меняется — физический порядок блока сохранён
- Перестановка «запечена» в `diagBlocks_`/`offDiagBlocks_` при конструировании
- Переключение layout — одна строка: `Layout::InterleavedSwP` → `Layout::InterleavedPSw`

## Альтернативы (отклонены)

1. ~~Runtime swap~~ — работало бы, но layout-абстракция чище и без overhead
2. Поменять порядок VariableFieldProperties → 10+ файлов, высокий риск
3. Форкнуть amgcl → потеря обновлений
4. Использовать CPR с Sw как «давление» → проверить в бенчмарке для полноты

## Связанные заметки

- [[layout абстракция отделяет топологию сетки от CRS маппинга]]
- [[план-сессия-6-CPR-прекондиционер]]
- [[prompt-оптимизация-06-CPR-прекондиционер]]
- [[2026-06-23 сессия 6a layout абстракция]]
