---
tags:
  - литература
  - cpr
  - прекондиционер
  - amg
  - system-amg
date: 2026-06-29
---

# Gries 2014 — Preconditioning for Efficiently Applying Algebraic Multigrid in Fully Implicit Reservoir Simulations

- **Авторы:** Gries S., Stüben K., Brown G.L., Chen D., Collins D.A.
- **Год:** 2014
- **Издание:** SPE Journal, Vol. 19, No. 4, pp. 726–736
- **DOI/SPE:** SPE-163608-PA

## Основная идея

Альтернатива классическому CPR (decoupling → скалярный AMG для pressure → ILU для всей системы). Вместо decoupling используется System-AMG, который работает с блочной системой напрямую: coarsening и интерполяция учитывают блочную структуру Якобиана. Pressure equation не выделяется явно — AMG «видит» сильные связи по давлению через анализ блочной матрицы.

## Ключевые вклады

1. **System-AMG coarsening:** для каждой ячейки блок B×B анализируется целиком. Сильные связи определяются по спектральному радиусу off-diagonal блоков относительно diagonal (а не поэлементно).

2. **Block smoothers:** Gauss-Seidel или ILU на уровне блоков (B×B), не скаляров. Это точнее чем pointwise ILU для систем с сильной связью между переменными.

3. **Сравнение с CPR:** на стандартных SPE benchmark (SPE10, Norne) System-AMG показывает сопоставимое или лучшее число итераций, но с более высокой стоимостью одной итерации (блочная арифметика). Выигрыш — в робастности: нет проблемы выбора decoupling strategy.

4. **Гибрид:** Gries предлагает и гибридный вариант — System-AMG для coarsening + скалярный AMG на грубых уровнях. Компромисс между робастностью и стоимостью.

## Сравнение подходов

| Подход | Decoupling | AMG | Робастность | Стоимость итерации |
|---|---|---|---|---|
| CPR (Wallis/Cao) | Явный (weights) | Скалярный | Зависит от weights | Низкая |
| System-AMG (Gries) | Нет | Блочный | Высокая | Высокая |
| Гибрид | Нет | Блочный → скалярный | Высокая | Средняя |

## Релевантность для GDM

GDM использует amgcl, который поддерживает блочный AMG (`static_matrix<B,B>`) и скалярный CPR. System-AMG в чистом виде в amgcl не реализован — нет block-aware coarsening. Но идея гибридного подхода может быть полезна при расширении до трёхфазной модели, когда выбор decoupling weights становится нетривиальным.

Для текущей двухфазной задачи (B=2) классический CPR с True-IMPES weights (FEAT-011) — оптимальный выбор: простой, робастный, дешёвый.

## Связанные заметки

- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]]
- [[Cao 2002 Development of Techniques for General Purpose Simulators]]
- [[возможности amgcl для блочных СЛАУ]]
- [[переход с блочного AMG на скалярный CPR в production]]
