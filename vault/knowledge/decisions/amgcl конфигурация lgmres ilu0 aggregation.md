---
tags: [decision, amgcl, solver, performance]
date: 2026-06-20
---
# AMGCL: lgmres + ilu0 + aggregation

## Решение

Оптимальная конфигурация AMGCL для двухфазной FIM-системы (блочный 2×2 бэкенд, `static_matrix<double,2,2>`):

```
amgcl::make_solver<
    amgcl::amg<Backend, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>,
    amgcl::solver::lgmres<Backend>
>
```

## Почему

Систематический бенчмарк 15 конфигураций на сетке 51×51×4 (10404 ячейки, 7 скважин):

- **ilu0** даёт 2.3× меньше AMG-итераций на solve (13.6 vs 31.6) по сравнению с damped_jacobi — лучшее сглаживание для седловой 2×2 структуры
- **lgmres** на 5% быстрее gmres при том же числе итераций — экономия на рестартах Krylov subspace
- **aggregation** на 10% быстрее smoothed_aggregation

Общее ускорение солвера: **2.5×** (t_amg_solve: 135.6 → 53.8 сек на полном 730-дневном тесте).

## Ограничения

- Тестировано на fine grid (51×51×4). На coarse grid (11×11) разница может быть меньше.
- Блочный бэкенд ограничивает выбор: ruge_stuben, spai0, spai1, chebyshev несовместимы.
- При переходе на трёхфазную модель (B=3) — перетестировать.

## Связанные заметки

- [[2026-06-20 оптимизация AMGCL солвера lgmres ilu0]]
