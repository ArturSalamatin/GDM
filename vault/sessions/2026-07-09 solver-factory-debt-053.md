---
tags:
  - сессия
  - рефакторинг
  - amgcl
date: 2026-07-09
---

# 2026-07-09: SolverFactory — compile-time выбор конфигурации СЛАУ (DEBT-053)

## Что сделано

1. Завершён повторный аудит плана DEBT-053 (раунды 2 и 3)
   - Раунд 2 нашёл пропущенный файл `examples/ex_benchmark_series_cpr.cpp` — добавлен в план
   - Раунд 3 подтвердил: проблем не найдено
2. Реализован DEBT-053:
   - Создан `HydroSolver/Solver/Math/SolverConfig.h` — 5 конфигураций под `#ifdef`
   - CMake-опция `GDM_SOLVER` (CPR, CPR_BICGSTAB, CPR_SA, CPR_DRS, ILU0)
   - `LinearProblem.h/cpp` переключены на `SolverType`/`PrecondType` из SolverConfig.h
   - `#ifdef`-guards для несовместимых параметров (`block_size` для ILU0, `K` для BiCGStab)
   - Бенчмарк и example рефакторены — include-ят SolverConfig.h
3. Верифицированы все 5 конфигураций

## Результаты верификации

| Config | Тесты | Время Release | Примечание |
|---|---|---|---|
| CPR (default) | 294/294 | ~105s | baseline |
| CPR_BICGSTAB | 294/294 | ~76s | **на 30% быстрее baseline** |
| CPR_SA | 294/294 | ~110s | сопоставимо |
| CPR_DRS | 293/294 | timeout | Series TS benchmark timeout |
| ILU0 | 293/294 | ~438s | Series TS benchmark timeout, 3.5× медленнее |

**Ключевое наблюдение:** CPR_BICGSTAB проходит все 294 теста и на ~30% быстрее CPR+lgmres. Требует отдельного исследования — может ли стать новым default.

## Коммиты

- `61e8f39` vault: DEBT-053 начало реализации
- `0e2800d` refactor: SolverConfig.h + CMake опция GDM_SOLVER
- `144904f` refactor: бенчмарк и example используют SolverConfig.h
- `98b278e` vault: DEBT-053 реализован

## Ветка

`refactor/debt-053/solver-factory` — готова к merge в `experimental`

## Связанные задачи

- [[debt-053 solver-factory]] — план
- [[debt-051 amgcl-submodule-and-solvers]] — общий план DEBT-051..053
- DEBT-051 ✅, DEBT-052 ✅, DEBT-053 ✅ — инфраструктурная цепочка завершена
- Следующий шаг: FEAT-010 (threshold-based fallback)

## Snapshot

- Тесты: 294/294 (Release ~105s, Debug ~309s)
- Warnings: 1 pre-existing C4267 в test_JacobianAssembly.cpp
- Submodule amgcl: `experimental/patches`
