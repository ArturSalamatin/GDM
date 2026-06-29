---
tags:
  - литература
  - cpr
  - прекондиционер
  - decoupling
  - композиционное-моделирование
date: 2026-06-29
---

# Lacroix 2003 — Decoupling Preconditioners in the Implicit Parallel Accurate Reservoir Simulator (IPARS)

- **Авторы:** Lacroix S., Vassilevski Y.V., Wheeler J.A., Wheeler M.F.
- **Год:** 2003
- **Издание:** SPE Reservoir Simulation Symposium, Houston, TX, February 2003
- **DOI/SPE:** SPE-79684-MS

**Примечание:** ранняя версия материала опубликована в Numerical Linear Algebra with Applications, 2001, Vol. 8, No. 8, pp. 517–532 (DOI: 10.1002/nla.255) под тем же названием.

## Основная идея

Расширение CPR-decoupling на многокомпонентные (compositional) задачи. Если Cao 2002 работал преимущественно с black-oil (B ≤ 3), Lacroix показал, как строить pressure equation для произвольного числа компонент и фаз через обобщённые decoupling operators.

## Ключевые вклады

1. **Обобщённый decoupling operator D:** матрица n_cells × n_eq, действующая слева на блочный Якобиан. D·A даёт первую строку — скалярное pressure equation, остальные — transport. Выбор D определяет качество pressure approximation.

2. **Связь с IMPES:** для идеального decoupling D·A должно давать в первой строке матрицу, зависящую только от давления (∂/∂S ≡ 0). В реальности — приближённо.

3. **Практические рекомендации:** для black-oil (B=3) True-IMPES weights вычисляются аналитически. Для compositional (B > 3) нужна SVD или QR-разложение блока ∂F/∂C для нахождения nullspace.

## Отличие от Cao 2002

Cao работал с конкретными формулами для B=2 и B=3. Lacroix дал общую algebraic framework, применимую к произвольному числу компонент без вывода формул для каждого случая.

## Релевантность для GDM

Сейчас — низкая (GDM = двухфазный, B=2). Станет ключевой при расширении до трёхфазной/композиционной модели (FEAT-004, FEAT-005). Даёт алгоритм вычисления decoupling weights через SVD блока Якобиана, не требующий аналитических формул.

## Связанные заметки

- [[Cao 2002 Development of Techniques for General Purpose Simulators]]
- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]]
- [[планируемые фичи]] — FEAT-004 (трёхфазная модель), FEAT-005 (полимерное заводнение)
