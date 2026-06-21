---
tags: [session, amgcl, profiler, refactor]
date: 2026-06-21
---
# Переход на amgcl::profiler — замена ручного chrono

## Результат

Профилирование переведено с ручного `std::chrono` на встроенный `amgcl::profiler<>`. Макрос `AMGCL_PROFILING` включён через CMake-опцию (по умолчанию ON). Иерархический вывод с детализацией внутренних операций amgcl.

## Что сделано

1. CMakeLists.txt — добавлена опция `AMGCL_PROFILING` (ON по умолчанию), определяет макрос `AMGCL_PROFILING`
2. `LinearProblem.h` — убран `#include <chrono>`, из `SolveResult` убраны `setup_ms`/`solve_ms`
3. `LinearProblem.cpp` — `Solve()` и `SolveWith()` используют `prof.tic("setup")`/`prof.toc("setup")`, `prof.tic("solve")`/`prof.toc("solve")` вместо chrono
4. `ReservoirSimulator.h` — убран `#include <chrono>`, из `SolverProfile` убраны все `double t_*` поля (остались только счётчики)
5. `ReservoirSimulator.cpp` — `Solve()`, `PerformNewtonLoop()`, `SingleIteration()` используют `prof.tic()`/`prof.toc()` для секций total, assemble, update
6. `test_3d_completions.cpp` — вместо CSV с chrono-полями пишет `amgcl_profile.txt` через `std::cout << prof`
7. `test_amgcl_benchmark.cpp` — `prof.reset()` перед каждым бенчмарком, `report()` выводит `prof`

## Пример вывода (3D smoke, 11×11×4)

```
[Profile:                  0.943 s] (100.00%)
[  total:                  0.806 s] ( 85.47%)
[    assemble:             0.388 s] ( 41.15%)
[    setup:                0.238 s] ( 25.24%)
[      CSR copy:           0.054 s] (  5.73%)
[      coarsest level:     0.140 s] ( 14.85%)
[    solve:                0.088 s] (  9.33%)
[      coarse:             0.049 s] (  5.20%)
[      residual:           0.027 s] (  2.86%)
[      spmv:               0.008 s] (  0.85%)
[    update:               0.045 s] (  4.77%)
```

## Как работает amgcl::profiler

- Глобальный объект `amgcl::prof` (определён в LinearProblem.h через `__declspec(selectany)`)
- `AMGCL_PROFILING` макрос активирует `AMGCL_TIC`/`AMGCL_TOC` внутри amgcl (amg.hpp, coarsening, relaxation)
- Наши `prof.tic("assemble")` и внутренние amgcl-замеры складываются в одно дерево
- `prof.reset()` — сброс для нового прогона (benchmark)

## Также

- Исправлена заметка про SIGSEGV в MatrixCSR — это был артефакт рефактора, не баг legacy
- Создан промпт для очистки benchmark-артефактов из production-кода (отложено)

## Связанные заметки

- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[2026-06-20 оптимизация AMGCL солвера lgmres ilu0]]
- [[prompt-очистка-benchmark-артефактов]]
