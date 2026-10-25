---
tags:
  - сессия
  - производительность
  - amgcl
  - openmp
date: 2026-06-21
---

# Сессия 4: iluk в production, комбинации, OpenMP

## Выполнено

### Шаг 1: ilu0 → iluk(k=1) в production ✅
- `LinearProblem.h`: include `iluk.hpp`, typedef `Solver_AMG` с `relaxation::iluk`
- 35/35 тестов пройдены

### Шаг 2: серия H — комбинации iluk ✅

| Config | iters/solve | Profile (с) |
|--------|-------------|-------------|
| H1 iluk(k=1) baseline | 8.3 | 73.6 |
| H2 + npre=2 | 6.8 | 74.2 |
| H3 + W-cycle | 6.2 | 79.3 |
| H4 + npre=2, npost=2 | 5.9 | 76.1 |
| **H5 + K=5** | 8.3 | **71.8** |
| H6 iluk(k=2) | 7.0 | 74.1 |

**Решение:** K=5 применён в production (конструктор LinearProblem). Выигрыш –2.4%.
Увеличение сглаживания снижает итерации, но iluk-apply дорогой — по времени хуже.

### Шаг 3: reuse AMG — ❌ отложен

`make_solver::operator()(A, F, X)` вызывает lgmres с `A` для SpMV. Но `A` — `block_matrix_adapter` (lazy view), а lgmres ожидает `build_matrix`. Результат — SIGSEGV.

Reuse AMG с блочным backend требует конвертации adapter → build_matrix и хранения копии. Выигрыш < 5%, не оправдывает сложности. Reuse AMG будет доступен через CPR.partial_update() (сессия 6).

### Шаг 4: OpenMP ✅
- `find_package(OpenMP)` + `target_link_libraries(gdm_core PUBLIC OpenMP::OpenMP_CXX)`
- `USE_PARALLEL` уже определён в `Defines.h`, прагмы стояли в assembly
- Fix: `size_t l` → `int l` в AssembleMyProblem (MSVC OpenMP требует signed)
- 35/35 тестов пройдены

## Итог

Текущий production-солвер: `amg<aggregation, iluk(k=1)> + lgmres(M=15, K=5)` + OpenMP.

## Связанные заметки

- [[prompt-оптимизация-04-iluk-reuse-openmp-adaptive]]
- [[amgcl конфигурация iluk k1 новый оптимум]]
- [[2026-06-21 серии DFGE перебор параметров AMGCL]]
