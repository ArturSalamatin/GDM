---
tags:
  - сессия
  - профилирование
  - amgcl
  - оптимизация
date: 2026-06-20
---

# 2026-06-20 Инструментация AMGCL и baseline-профиль

## Что сделано

### Фаза 0: Инструментирование

1. **SolveResult** — новая структура возвращаемого значения из `LinearProblem::Solve()`:
   - `size_t iters`, `double error`, `bool converged`, `double setup_ms`, `double solve_ms`
   - Замер через `std::chrono::steady_clock` (заменил `amgcl::profiler`)

2. **SolverProfile** — агрегирующая структура в `ReservoirSimulator`:
   - Счётчики: `n_time_steps`, `n_newton_iters`, `n_amg_solves`, `n_wasted_trials`, `total_amg_iters`
   - Таймеры: `t_assemble_ms`, `t_amg_setup_ms`, `t_amg_solve_ms`, `t_update_grid_ms`, `t_total_ms`
   - Экспорт в `solver_profile.csv` из теста 7.5

3. **Удалены PrintCRS/PrintDiagBlocks** из hot path (`Solve()`)

4. **std::fill** вместо аллокации в `ResetMatrix()` / `ResetProblem()`

5. **Исправлен SIGSEGV** в `MatrixCSR` — `value.resize(nnz)` в теле конструктора
   - См. [[SIGSEGV в MatrixCSR ResetMatrix вызванном из конструктора]]

### Baseline-профиль (тест 7.5: 51×51×4, 7 скважин, 730 дней)

| Компонента | Время (мс) | Доля |
|---|---|---|
| AMG solve | 135 596 | **79.6%** |
| Assemble | 24 449 | 14.3% |
| AMG setup | 7 108 | 4.2% |
| Update grid | 3 256 | 1.9% |
| **Итого** | **170 409** | 100% |

- 200 time steps, 1355 Newton iterations, 42857 AMG iterations
- Среднее: 31.6 AMG-итер/solve, ~100 мс/solve, 5.2 мс/setup
- 0 wasted trials

### Выводы для следующих этапов

1. **AMG solve — 80% времени** → оптимизация конфигурации (smoother, coarsening, solver) — главный рычаг
2. **AMG setup — 4.2%** → reuse setup (Phase 2) даёт потолок ≤4% ускорения, низкий приоритет
3. **Assemble — 14.3%** → вторичный bottleneck, но пока не трогаем
4. Следующий шаг: Phase 1 — перебор конфигураций AMGCL в `test_amgcl_benchmark.cpp`

## Связанные заметки

- [[план профилирования и оптимизации AMGCL]]
- [[SIGSEGV в MatrixCSR ResetMatrix вызванном из конструктора]]
- [[возможности amgcl для блочных СЛАУ]]
