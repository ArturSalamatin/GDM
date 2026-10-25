---
tags:
  - промпт
  - производительность
  - инструментирование
date: 2026-06-20
updated: 2026-06-20
---

# Фаза 0: Убрать PrintCRS + добавить детальные замеры времени

## Цель

Убрать debug-вывод матрицы из hot path и добавить замеры wall-clock time по фазам, чтобы получить чистый baseline.

## Шаг 0.1: Устранение PrintCRS/PrintDiagBlocks из Solve()

Файл `LinearProblem.cpp`, метод `Solve()`, строки 110–111:
```cpp
Matrix().PrintCRS();       // ← убрать
Matrix().PrintDiagBlocks(); // ← убрать
```

Эти вызовы **не под #ifdef** — они пишут матрицу в файл при каждом решении СЛАУ. Обернуть в `#ifdef DEBUG_SALAMATIN` или закомментировать. Метод `Print()` (строка 36) оставить нетронутым — он для явной диагностики.

## Шаг 0.2: SolveStats в LinearProblem

В `LinearProblem.h` добавить публичные аккумуляторы:
```cpp
struct SolveStats {
    double setup_s = 0, solve_s = 0, rhs_copy_s = 0;
    size_t total_iters = 0, solve_count = 0;
    size_t max_iters = 0;
    double max_error = 0;
    // AMG-иерархия (заполняется при первом solve)
    size_t amg_levels = 0;
    double operator_complexity = 0;
    double grid_complexity = 0;
};
SolveStats stats;
```

В `LinearProblem::Solve()` заменить `prof.tic/toc` на `std::chrono::steady_clock` и накапливать в `stats`. Для получения AMG-иерархии: после создания `Solver_AMG<B> solve(A, prm)` вывести информацию через `solve.precond()` — у `amg` есть `operator<<`, который печатает levels, operator complexity, grid complexity. Для программного доступа:

```cpp
// После Solver_AMG<B> solve(A, prm):
if (stats.solve_count == 0) {
    // Захватить из ostringstream или напрямую из amg
    std::ostringstream oss;
    oss << solve;  // make_solver::operator<< выводит solver + preconditioner
    // Парсить оттуда levels/complexity или добавить прямой доступ
}
```

Лучший вариант — добавить прямой доступ к полям amg через `solve.precond()`. Класс `amg` хранит `std::vector<level> levels` — нужно проверить, есть ли public accessor. Если нет — использовать `operator<<` через ostringstream при первом solve и парсить оттуда.

Альтернатива: сделать `stats.amg_info_str` — строку из `operator<<`, записывать в профиль как есть.

## Шаг 0.3: Счётчики в ReservoirSimulator

Добавить счётчики (всегда активные, не #ifdef):
- **n_timesteps** — количество принятых временных шагов
- **n_wasted_trials** — количество отброшенных шагов (уже есть: `numPrm.WastedTrialsCount()`)
- **n_newton_total** — суммарное число Ньютоновских итераций
- **n_amg_solves** — суммарное число вызовов LinearProblem::Solve
- **t_assemble_s** — суммарное время `AssembleMyProblem`
- **t_amg_total_s** — суммарное время `MyProblem.Solve()` (весь AMG, включая setup)

В `ReservoirSimulator.h` добавить структуру `SolverProfile` и метод `GetSolverProfile()`.

## Шаг 0.4: Экспорт в test_3d_completions.cpp

В `run_case_3d` после цикла по шагам вызвать `sim.GetSolverProfile()` и записать CSV.

Формат `results/3d_fine_51x51/profile.csv`:
```
metric,value
total_wall_s,<число>
n_timesteps,<число>
n_wasted_trials,<число>
n_newton_total,<число>
n_amg_solves,<число>
t_assemble_s,<число>
t_amg_total_s,<число>
t_amg_setup_s,<число>
t_amg_solve_s,<число>
t_amg_rhs_copy_s,<число>
avg_amg_iters,<число>
max_amg_iters,<число>
avg_amg_error,<число>
amg_levels,<число>
operator_complexity,<число>
grid_complexity,<число>
```

## Шаг 0.5: Baseline — три запуска

Собрать Release, запустить тест 7.5 три раза:
```powershell
cmake --build build --config Release
ctest --test-dir build -C Release -R "fine grid 51x51x4" --output-on-failure
```

Переименовать `profile.csv` → `profile_baseline_runN.csv` после каждого запуска.

## Проверка

- `results/3d_fine_51x51/profile.csv` создан и содержит осмысленные числа
- Баланс масс по-прежнему проходит (< 1e-3)
- Файлы `test_Matrix.txt`, `test_diagValues.txt` больше не создаются при запуске теста

## Ключевые вопросы для анализа baseline

1. **assembly vs AMG**: если assembly > 50%, оптимизировать AMGCL малоэффективно
2. **setup vs solve внутри AMG**: если setup > 50%, нужен reuse иерархии
3. **avg GMRES iters ≈ maxiter (5)**: GMRES не сходится → нужно увеличить maxiter или улучшить precond
4. **wasted trials**: каждый — потеря assembly + solve
5. **operator complexity**: если > 2.0, AMG тратит слишком много на грубые уровни
