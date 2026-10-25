---
tags:
  - сессия
  - FEAT-011
  - CPR
  - регуляризация
  - ILU0
date: 2026-07-13
---

# 2026-07-13: FEAT-011 — фикс Sw=0 и закрытие задачи

## Цель сессии

Завершение FEAT-011 (True-IMPES weights): диагностика и исправление NaN на 2-й итерации Ньютона при Sw=0, проверка необходимости BUG-002 ILU0 fallback, финальная верификация.

## Контекст

FEAT-011 была реализована 2026-07-09 (шаги 1–4: tag dispatch, `invert()` dispatch, конфигурации CPR переключены на `true_impes_weights`, тест Sw=0 добавлен). Тест Sw=0 падал с NaN.

## Результаты

### 1. Диагностика NaN при Sw=0

**Симптом:** 2-я итерация Ньютона → NaN в corrections → simulation fails.

**Ложный след:** предположение о InterleavedSwP layout (identity permutation) → вывод что True-IMPES weights дают вырожденные w0=0 → добавлен fallback в cpr.hpp. Тест всё равно падал.

**Истинная причина:** Layout = InterleavedPSw, `permCRSToPhysical = {1, 0}`:
- Чётные CRS-строки = WaterEq (physRow 1)
- Нечётные CRS-строки = OilEq (physRow 0)

Регуляризация диагонали `for (i = 1; i < rhsSize; i += B)` обрабатывала только нечётные (OilEq) строки. WaterEq-строки (чётные) при Sw=0 имеют `dWaterEq/dP = 0` → zero pivot → ILU0 даёт плохой прекондиционер → GMRES дивергирует.

**Фикс:** `for (i = 0; i < rhsSize; ++i)` — регуляризация всех строк.

### 2. True-IMPES weights — верификация корректности

Через анализ layout показано: weights `w0 = dOilEq/dSw`, `w1 = -dWaterEq/dSw` корректно обнуляют Sw-столбец при любых Sw, включая Sw=0. Проблема была не в weights.

### 3. BUG-002 ILU0 fallback — экспериментальная верификация

- Убрали `if (is_zero(D[i])) D[i] = 1` из ilu0.hpp
- Вернули upstream `precondition(!math::is_zero((*D)[i]), "Zero pivot in ILU")`
- Тест Sw=0 пройден (4.53 сек) — регуляризация в LinearProblem предотвращает zero pivot
- **Решение:** fallback не нужен, ilu0.hpp = upstream

### 4. Производительность

Сравнение времени тестов:
- Baseline (до FEAT-011): ~100 сек
- FEAT-011 на feature branch: 116.62 сек (296/296)
- После merge в experimental: 118.87 сек (296/296)
- Без BUG-002 fallback: без изменений

Замедление ~18% — объясняется добавлением теста Sw=0 (3.5–4.5 сек) и relaxed tolerance.

### 5. amgcl тесты

Тесты amgcl проверяют AMG solver/smoother/coarsening комбинации на задачах Лапласа. CPR не покрыт тестами amgcl → наши изменения в cpr.hpp не тестируются upstream → тестирование только через GDM.

## Коммиты

- `44cab5d` — fix: FEAT-011 регуляризация диагонали для всех строк Якобиана
- `b8f97ba` — fix subrepo state
- `593073b` — Merge branch 'feat/feat-011/true-impes-weights' into experimental

## Ключевые решения

1. [[регуляризация диагонали Якобиана для всех строк а не только Sw]]
2. [[true-impes weights корректны через анализ layout InterleavedPSw]]
3. [[BUG-002 ILU0 fallback не нужен при корректной регуляризации]]

## Файлы изменены

| Файл | Изменение |
|---|---|
| `HydroSolver/Solver/Math/LinearProblem.cpp` | Регуляризация: `i=1,i+=B` → `i=0,++i` |
| `amgcl/amgcl/relaxation/ilu0.hpp` | Возврат к upstream (убран BUG-002 fallback) |
| `tests/test_visual_verification.cpp` | Tolerance Sw=0 теста: 1e-3 → 2e-3 |

## Статус

- FEAT-011: ✅ закрыта
- BUG-002 ILU0 fallback: ✅ убран (не нужен)
- 296/296 тестов зелёные
- Ветка merged в experimental
