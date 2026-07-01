---
tags:
  - баг
  - солвер
  - amgcl
date: 2026-07-01
---

# BUG-006: Solve() всегда возвращал converged = true

## Суть

`LinearProblem::Solve()` возвращал захардкоженный `converged = true` независимо от результата AMGCL lgmres. Newton loop не получал информации о расхождении линейного солвера.

## Корневая причина

Строка `return { iters, error, true };` в `LinearProblem.cpp:143`.

Дополнительно: `CurrentANG_IsAccuracyReached()` использовал `error == 1.0 || error == 0.0` — бессмысленный критерий, маскировавший проблему.

## Подводный камень lgmres

При `norm_rhs < abstol` но `norm_rhs > eps(1)`, lgmres выходит на iter=0 с `error = norm_rhs/norm_rhs = 1.0`. Абсолютный критерий удовлетворён, но относительная ошибка = 1.0 — формально «не сошёлся» по relative tolerance. Старый код `error == 1.0` случайно обрабатывал этот случай правильно.

## Решение

1. `LinearProblem::Solve()`: `converged = std::isfinite(error)` — NaN/Inf означает дивергенцию
2. `CurrentANG_IsAccuracyReached()`: `error <= AMG_RelTol || iters == 0` — покрывает edge case lgmres
3. Аналогичный фикс в `test_amgcl_benchmark.cpp`

## Отклонение от плана

Исходный план предлагал `converged = isfinite(error) && error <= tol`. Это вызвало бесконечные wasted trials: Newton loop немедленно ломался на `converged=false`, делал `ReverseState(); break;`, уменьшал шаг, и снова. Решено разделить ответственность: `converged` в `SolveResult` = «конечный результат», а проверка точности — в `CurrentANG_IsAccuracyReached()`.

## Связанные заметки

- [[code-review-2026-06-28-числовая-устойчивость]] — обнаружение бага при code review
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — BUG-002, контекст CPR

## Тесты

- `LinearProblem::Solve returns finite error and converged=true` — проверяет converged и isfinite
- `LinearProblem::Solve converged=true when maxIter sufficient` — проверяет error <= tol при достаточном maxIter
