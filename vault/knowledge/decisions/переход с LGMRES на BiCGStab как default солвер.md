---
tags:
  - решение
  - солвер
  - bicgstab
  - lgmres
  - amgcl
date: 2026-07-15
issue: RES-007
---

# Переход с LGMRES на BiCGStab как default солвер

## Решение

Default `GDM_SOLVER` изменён с `CPR` (LGMRES) на `CPR_BICGSTAB` (BiCGStab).

## Почему

RES-007 показал ускорение 12-90% при идентичной точности (Newton iterations побитово совпадают, баланс O(1e-14)). Все 310 тестов зелёные.

BiCGStab O(n) памяти vs LGMRES O(n·20). На малых сетках (текущие тесты 5×5..21×21) фиксированные затраты LGMRES на Krylov-базис доминируют.

## Когда пересмотреть

При переходе на крупные сетки (>100k DOF). На больших системах:
- LGMRES augmentation amortизируется
- BiCGStab может дать near-breakdown на плохо обусловленных системах
- Superlinear convergence LGMRES может компенсировать overhead

Переключение: `-DGDM_SOLVER=CPR` (одна строка CMake, compile-time).

## Связанные заметки

- [[RES-007 результаты bicgstab vs lgmres]]
- [[переход с блочного AMG на скалярный CPR в production]]
- [[amgcl конфигурация lgmres ilu0 aggregation]]
