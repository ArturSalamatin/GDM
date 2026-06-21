---
tags:
  - промпт
  - производительность
  - amgcl
  - архитектура
date: 2026-06-20
updated: 2026-06-21
---

# Фаза 2: Структурные оптимизации

## Цель

Архитектурные изменения, дающие ускорение помимо выбора конфигурации AMGCL. Профилирование через `amgcl::profiler` (`AMGCL_PROFILING` ON в CMake).

## Текущий профиль (3D smoke, 11×11×4, 600 дней)

```
[Profile:                  0.943 s] (100.00%)
[  total:                  0.806 s] ( 85.47%)
[    assemble:             0.388 s] ( 41.15%)
[    setup:                0.238 s] ( 25.24%)
[      CSR copy:           0.054 s] (  5.73%)
[      coarsest level:     0.140 s] ( 14.85%)
[    solve:                0.088 s] (  9.33%)
[    update:               0.045 s] (  4.77%)
```

На fine grid (51×51×4, 730 дней) solve доминирует (~80%), assemble ~14%, setup ~4%.

---

## 2.1: Reuse AMG-иерархии — unique_ptr + rebuild по требованию

### Проблема

`Solver_AMG` создаётся заново в каждом `Solve()`. Setup AMG-иерархии — дорогая операция (coarsening + transfer operators), не нужна при каждой Newton-итерации.

### Решение

```cpp
std::unique_ptr<Solver_AMG<B>> solver_;
bool needs_rebuild_ = true;

SolveResult Solve(int maxIter) {
    auto A = amgcl::adapter::block_matrix<value_type<B>>(...);
    if (needs_rebuild_ || !solver_) {
        prof.tic("setup");
        solver_ = std::make_unique<Solver_AMG<B>>(A, prm);
        prof.toc("setup");
        needs_rebuild_ = false;
    }
    prof.tic("solve");
    auto [iters, error] = (*solver_)(A, F, X);
    prof.toc("solve");
    return { iters, error, true };
}
```

`operator()(A, rhs, x)` — использует A для SpMV, но прекондиционер от конструирования.

`InvalidateSetup()` вызывать при начале нового временного шага, не при каждой Newton-итерации.

### Ожидаемый эффект

На fine grid setup ~4% → экономия ~3.5% (все Newton-итерации кроме первой в каждом шаге). Небольшой абсолютный выигрыш, но архитектурно правильно для CPR (см. фаза 3).

---

## 2.2: std::fill вместо аллокации в ResetProblem — ✅ частично сделано

`MatrixCSR::ResetMatrix()` уже переведён на `std::fill`. Проверить `rhs` и `solutionCorrections`:

```cpp
void LinearProblem::ResetProblem() {
    matrix->ResetMatrix();                                          // ✅ std::fill
    std::fill(rhs.begin(), rhs.end(), 0.0);                        // проверить
    std::fill(solutionCorrections.begin(), solutionCorrections.end(), 0.0);  // проверить
}
```

---

## 2.3: OpenMP для assembly

`AssembleMyProblem` имеет `#pragma omp parallel for` под `#ifdef USE_PARALLEL`. Проверить:
1. `USE_PARALLEL` определён в CMake?
2. OpenMP подключён?
3. Если нет — добавить:
```cmake
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_compile_definitions(gdm_core PRIVATE USE_PARALLEL)
    target_link_libraries(gdm_core PRIVATE OpenMP::OpenMP_CXX)
endif()
```

Assembly — embarrassingly parallel. На fine grid assemble ~14% total → ускорение 2–4× на 4–8 ядрах → экономия ~7–10% total.

---

## 2.4: PI-контроллер адаптивного шага

Текущая стратегия: `tau *= 1.15` при успехе, `tau *= 0.70` при откате. Грубая.

PI-контроллер: `tau_new = tau * (target/actual)^kP * (prev/actual)^kI`

Проверить долю wasted trials на fine grid. Если > 5% — реализовать.

---

## Итоговый чеклист

- [ ] 2.1: Reuse AMG setup (unique_ptr + InvalidateSetup)
- [x] 2.2: std::fill в MatrixCSR::ResetMatrix (сделано 2026-06-20)
- [ ] 2.2b: Проверить rhs и solutionCorrections в ResetProblem
- [ ] 2.3: OpenMP для assembly
- [ ] 2.4: PI-контроллер (если wasted > 5%)

## Порядок

2.2b → 2.1 → 2.3 → 2.4

## Связанные заметки

- [[2026-06-21 переход на amgcl profiler]]
- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[prompt-оптимизация-03-CPR-прекондиционер]]
