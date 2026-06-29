---
tags:
  - литература
  - cpr
  - прекондиционер
  - линейная-алгебра
date: 2026-06-29
---

# Wallis et al. 1983 — Incomplete Gaussian Elimination as a Preconditioning for Generalized Conjugate Gradient Acceleration

- **Авторы:** Wallis J.R., Kendall R.P., Little T.E.
- **Год:** 1983
- **Издание:** SPE 12265, Seventh SPE Symposium on Reservoir Simulation, San Francisco
- **DOI/SPE:** SPE-12265-MS

## Основная идея

Предложен ILU-based прекондиционер для ускорения GCG (Generalized Conjugate Gradient) при решении линейных систем, возникающих в полностью неявных (FIM) симуляторах пласта. Ключевое наблюдение: incomplete Gaussian elimination (ILU(0)) на блочном Якобиане FIM-системы — эффективный и дешёвый прекондиционер, который можно строить без обращения полных диагональных блоков.

Эта работа заложила основу для более поздних CPR-прекондиционеров (Constrained Pressure Residual), где эллиптическая подзадача по давлению решается отдельно (AMG), а транспортная часть сглаживается ILU. Сам Wallis 1983 ещё не использовал AMG — двухступенчатая CPR-структура оформилась в последующих работах.

## Decoupling и pressure extraction

Wallis предложил способ выделения pressure equation из блочной системы. Терминология **True-IMPES weights** vs **Quasi-IMPES weights** оформилась позднее — в частности у Cao, Aziz (2002–2005). Суть подхода: для B уравнений на ячейку выбираются weights w из условия:

```
wᵀ · ∂F/∂S = 0
```

где S — вектор насыщенностей. Это означает: линейная комбинация wᵀ·F зависит только от давления P, а не от насыщенностей. Для двухфазной задачи (B=2, переменные P и Sw):

```
w = (∂F_water/∂Sw, −∂F_oil/∂Sw)    (с нормировкой)
```

Скалярная pressure-pressure подматрица:

```
App[i,j] = wᵀ · K[i,j] · e_P
```

где e_P — единичный вектор в направлении давления.

## Отличие от quasi-IMPES

Quasi-IMPES (используется в amgcl) вычисляет weights как первый столбец A⁻¹ диагонального блока — это требует обращения B×B матрицы и ломается при singular блоках. True-IMPES вычисляет weights из nullspace столбца ∂F/∂S — обращение не нужно, singular blocks обрабатываются корректно. Терминология и формализация — Cao, Aziz (2005), но идея extraction восходит к этой работе.

## Релевантность для GDM

Наш симулятор: несжимаемые фазы, нет капиллярного давления, B=2. При Sw → 0 диагональный блок Якобиана вырождается → quasi-IMPES (`invert()` в amgcl) даёт zero/near-zero pivot → crash или плохая сходимость. True-IMPES weights решают эту проблему принципиально.

Запланировано в FEAT-011.

## Связанные заметки

- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[переход с блочного AMG на скалярный CPR в production]]
