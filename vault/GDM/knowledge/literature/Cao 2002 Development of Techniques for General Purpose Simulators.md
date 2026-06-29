---
tags:
  - литература
  - cpr
  - прекондиционер
  - линейная-алгебра
  - amg
  - decoupling
date: 2026-06-29
---

# Cao 2002 — Development of Techniques for General Purpose Simulators

- **Авторы:** Cao H.
- **Год:** 2002
- **Издание:** PhD Thesis, Stanford University, Department of Petroleum Engineering
- **Advisor:** Aziz K.

## Основная идея

Диссертация — фундамент серии Cao–Tchelepi по CPR. Первое систематическое изложение CPR-framework с формализацией decoupling strategies. Введены три стратегии: quasi-IMPES, True-IMPES (обобщение Wallis 1983), ABF (Analytical Block Factorization). Показано, что выбор decoupling определяет робастность CPR, а не детали AMG-солвера на pressure stage.

## Ключевые вклады

1. **Формализация двухступенчатого CPR:**
   - Stage 1: AMG (или прямой метод) на скалярной pressure equation, полученной через decoupling
   - Stage 2: ILU(k) на полной блочной системе
   - Порядок: pressure-correction → global smoother (аддитивный или мультипликативный)

2. **Три decoupling strategy:**
   - **Quasi-IMPES:** weights = первый столбец inv(A_diag). Простой, но ломается при singular блоках
   - **True-IMPES:** weights из nullspace ∂F/∂S. Робастный, не требует обращения
   - **ABF:** точное блочное LDU-разложение, Шур-дополнение для pressure. Наиболее точный, но сложнее при > 2 компонент

3. **Анализ near-singular случаев:** при фазовых переходах (появление/исчезновение фазы) диагональные блоки Якобиана вырождаются. Quasi-IMPES → zero pivot. True-IMPES и ABF корректны.

4. **Параллельная реализация:** domain decomposition, overlap для ILU, scalability tests на O(10⁶) ячеек.

## Формулы (двухфазный случай)

Совпадают с [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]], раздел «Формулы для двухфазного случая».

## Релевантность для GDM

Диссертация — первоисточник для понимания, почему quasi-IMPES (текущий amgcl `invert()`) ломается на наших задачах (BUG-002). Глава 4 содержит подробный вывод True-IMPES weights для произвольного числа компонент.

## Связанные заметки

- [[Wallis 1983 Incomplete Gaussian Elimination as Preconditioning for CPR]]
- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]]
- [[Lacroix 2003 Decoupling Preconditioners in IPARS]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
