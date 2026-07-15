---
tags:
  - сессия
  - солвер
  - исследование
  - bicgstab
date: 2026-07-15
---

# 2026-07-15: RES-007 BiCGStab vs LGMRES

## Что сделано

1. **`/issue RES-007`** — создан GitHub issue [#27](https://github.com/ArturSalamatin/GDM/issues/27)
2. **`/plan-research RES-007`** — план сравнения CPR+BiCGStab vs CPR+LGMRES, 6 шагов
3. **`/audit-plan RES-007`** — два прохода аудита, 0 проблем на втором
4. **`/implement RES-007`** — полный цикл реализации:
   - Baseline CPR/LGMRES на 4 сценариях
   - Прогон CPR_BICGSTAB на тех же сценариях
   - Создан per-timestep тест `test_solver_comparison.cpp`
   - Анализ и вывод

5. **Переключение default солвера** на CPR_BICGSTAB в CMakeLists.txt
6. **Документация** — секция Solver Configuration в README.md

## Результаты RES-007

| Сценарий | LGMRES | BiCGStab | Ускорение |
|---|---|---|---|
| S1 benchmark (21×21×4, 6 wells) | 4.42s | 3.88s | −12% |
| S2 five-spot (21×21×1) | 18.7s | 1.84s | −90% |
| S3 variable debit (11×11×1) | 25.2s | 3.16s | −87% |
| S4 все 310 тестов | 516s | 117s | −77% |

**Гипотеза подтверждена.** BiCGStab быстрее на всех сценариях. Newton iterations побитово совпадают на per-timestep уровне. Баланс масс O(1e-14).

Ускорение сильно зависит от размера системы: на малых сетках (5×5..21×21) overhead LGMRES на хранение Krylov-базиса доминирует. При переходе на крупные сетки (>100k DOF) нужно перепроверить.

## Коммиты

- `3779e44` vault: RES-007 план bicgstab-vs-lgmres и аудит
- `f7ef816` vault: RES-007 начало реализации
- `b62dc16` test: RES-007 per-timestep solver comparison benchmark
- `4e3dc64` vault: результаты RES-007 BiCGStab vs LGMRES — гипотеза подтверждена
- `fb67ff7` feat: default solver CPR_BICGSTAB, документация по переключению солверов

## Ветка

`research/res-007/bicgstab-vs-lgmres` — готова к merge в experimental.

## Baseline

- 310 тестов, 310 pass
- Release: ~123s (CPR_BICGSTAB)
- Debug: ~434s
- Warning: 1 pre-existing C4267 в test_JacobianAssembly.cpp:34

## Новые файлы vault

- [[RES-007 результаты bicgstab vs lgmres]]
- [[res-007 bicgstab-vs-lgmres]] (план)

## Следующие шаги

- Merge ветки в experimental
- При переходе на крупные сетки — перепроверить BiCGStab vs LGMRES
- Рассмотреть RES-008 (блочный backend amgcl) или другие задачи из roadmap
