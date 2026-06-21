---
tags: [session, amgcl, optimization, benchmark]
date: 2026-06-20
---
# Оптимизация AMGCL солвера — lgmres + ilu0

## Результат

Замена `amg<aggregation, damped_jacobi> + gmres` на `amg<aggregation, ilu0> + lgmres` даёт **2.5× ускорение солвера** на тесте 7.5 (51×51×4, 7 скважин, 730 дней).

| Метрика | До | После |
|---------|-----|-------|
| t_amg_solve | 135.6 с | 53.8 с |
| avg_iters/solve | 31.6 | 13.6 |
| total_amg_iters | 42857 | 16082 |

## Методология

Три серии бенчмарков на укороченном сценарии (200 дней):

**Series A — Krylov solver** (AMG<aggregation, damped_jacobi>):
- gmres M=5/15/30, bicgstab, bicgstabl L=2, lgmres M=15, fgmres M=15, idrs s=4
- Победитель: **lgmres M=15** (45.4s), на 5% лучше baseline
- bicgstab/bicgstabl: меньше итераций на solve, но дороже каждая — итого медленнее

**Series B — Relaxation** (lgmres + aggregation):
- damped_jacobi, spai0, ilu0, gauss_seidel, chebyshev
- Победитель: **ilu0** (28.1s), на 37% лучше lgmres+damped_jacobi
- spai0 и chebyshev не сходятся с блочным 2×2 бэкендом

**Series C — Coarsening** (lgmres + ilu0):
- aggregation vs smoothed_aggregation
- Победитель: **aggregation** (28.1s vs 31.0s)
- ruge_stuben несовместим с блочным бэкендом (нет operator<= для static_matrix)

## Баг benchmark: отсутствие abstol

`SolveWith` не копировал `tol`/`abstol` из production `LinearProblem::prm`. Production использует `abstol = 1e-5` (из NumericalParameters), а benchmark default = 0. Без абсолютного критерия солвер не мог остановиться на поздних Newton итерациях, когда RHS уже мал. Исправлено: benchmark теперь копирует `tol`/`abstol` из `sim.numPrm`.

## Несовместимые компоненты AMGCL с блочным бэкендом

- `ruge_stuben` — нет `operator<=` для `static_matrix<2,2>`
- `spai1` — `QR::solve` не поддерживает блочные типы
- `spai0` — не сходится (45 iter на каждом Newton step)
- `chebyshev` — не сходится

## Файлы

- `LinearProblem.h` — `Solver_AMG` typedef: `lgmres + amg<aggregation, ilu0>`
- `LinearProblem.cpp` — `SolveWith` template definition + explicit instantiations
- `tests/test_amgcl_benchmark.cpp` — benchmark framework (Series A/B/C)
- `CMakeLists.txt` — `gdm_benchmark` target

## Связанные заметки

- [[2026-06-20 инструментация AMGCL и baseline профиль]]
- [[SIGSEGV в MatrixCSR ResetMatrix вызванном из конструктора]]
