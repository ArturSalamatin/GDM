---
tags: [prompt, cleanup, amgcl, benchmark]
date: 2026-06-21
---
# Очистка benchmark-артефактов из production-кода

После фазы 3.1 (оптимизация AMGCL) в production-коде остались артефакты экспериментальной инфраструктуры. Нужно вычистить, не ломая benchmark-тесты.

## Задачи

### 1. Убрать лишние includes из `LinearProblem.h`

Production-код использует только `Solver_AMG<B>` = `amg<aggregation, ilu0> + lgmres`.

**Оставить** (нужны production):
```
amgcl/adapter/block_matrix.hpp
amgcl/adapter/crs_tuple.hpp
amgcl/value_type/static_matrix.hpp
amgcl/make_solver.hpp
amgcl/amg.hpp
amgcl/coarsening/aggregation.hpp
amgcl/relaxation/ilu0.hpp
amgcl/solver/lgmres.hpp
amgcl/io/mm.hpp
amgcl/profiler.hpp
```

**Удалить** (нужны только benchmark):
```
amgcl/coarsening/smoothed_aggregation.hpp
amgcl/solver/gmres.hpp
amgcl/solver/bicgstab.hpp
amgcl/solver/bicgstabl.hpp
amgcl/solver/fgmres.hpp
amgcl/solver/idrs.hpp
amgcl/relaxation/damped_jacobi.hpp
amgcl/relaxation/spai0.hpp
amgcl/relaxation/chebyshev.hpp
amgcl/relaxation/gauss_seidel.hpp
amgcl/relaxation/as_preconditioner.hpp
```

Эти includes нужно перенести в `tests/test_amgcl_benchmark.cpp` (добавить в начало файла).

### 2. Убрать `SolverParams()` из `LinearProblem.h`

Строка 81: `Solver_AMG<B>::params& SolverParams() { return prm; }` — использовался только в benchmark. Удалить.

В `test_amgcl_benchmark.cpp` benchmark обращается к `sim.MyProblem` напрямую (friend или public). Проверить, что benchmark не использует `SolverParams()` (grep подтвердил — не использует).

### 3. Перенести `SolveWith` template и explicit instantiations в benchmark

**В `LinearProblem.h`:** удалить объявление `SolveWith` (строка 112–113).

**В `LinearProblem.cpp`:** удалить определение `SolveWith` (после `Solve()`) и все 13 explicit instantiations (строки 191–217).

**В `test_amgcl_benchmark.cpp`:** `SolveWith` уже реализован inline в `run_benchmark` — он напрямую создаёт солвер и вызывает его. Проверить, что benchmark не вызывает `LinearProblem::SolveWith` (grep строка 174 — вызывает!). Значит нужен один из вариантов:
- (a) Оставить `SolveWith` в LinearProblem, но перенести определение и instantiations в benchmark .cpp
- (b) Переписать benchmark: вынести логику SolveWith в локальную функцию benchmark

Вариант (b) предпочтительней — полностью изолирует benchmark от production API. Логика `SolveWith` тривиальна: создать солвер, вызвать `operator()`, вернуть результат. Нужен доступ к `matrix`, `rhs`, `solutionCorrections` — через уже имеющийся friend/public доступ.

### 4. Удалить диагностический тест `[diag]`

В `test_amgcl_benchmark.cpp` есть тест с тегом `[diag]` — сравнивал `SolveWith` с production `Solve()`. Свою задачу выполнил (нашёл баг с abstol). Удалить.

### 5. Проверить `gmres.hpp` в production

`amgcl/solver/gmres.hpp` был в оригинальном коде до оптимизации. Сейчас production использует `lgmres`. Убедиться, что gmres.hpp не включается из другого места (grep по всему проекту). Если нет — удалить include.

## Порядок выполнения

1. Перенести includes из LinearProblem.h в test_amgcl_benchmark.cpp
2. Переписать benchmark: локальная функция вместо SolveWith
3. Удалить SolveWith (объявление + определение + instantiations) из LinearProblem.h/.cpp
4. Удалить SolverParams() из LinearProblem.h
5. Удалить тест [diag]
6. Собрать Release: `cmake --build build --config Release`
7. Прогнать тесты: `ctest --test-dir build -C Release --output-on-failure`
8. Прогнать benchmark (Series A хватит): `ctest --test-dir build -C Release -R benchmark -L seriesA`

## Связанные заметки

- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[2026-06-20 оптимизация AMGCL солвера lgmres ilu0]]
