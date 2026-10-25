---
tags:
  - литература
  - cpr
  - прекондиционер
  - decoupling
  - amg
date: 2026-06-29
---

# Cao 2009 — A Fully Coupled Two-Phase Flow CPR Preconditioner

- **Авторы:** Cao H., Tchelepi H.A.
- **Год:** 2009 (опубликовано)
- **Издание:** не опубликовано как отдельная SPE-статья; материал вошёл в Tchelepi et al., "Convergence of Sequential Methods for Compositional Flow", а также частично в обзорную часть Stueben et al. 2007 и Gries 2014

**Примечание:** название "A General Framework for CPR Preconditioners" часто приписывается этой работе, но формально она не вышла как самостоятельная публикация. Обобщающий CPR-framework Cao–Tchelepi изложен в комбинации:
- Cao 2002 (PhD thesis) — полный вывод
- Cao et al. 2005 (SPE-96030) — параллельная реализация
- Различных обзорных секций в работах Stueben, Gries и др.

## Основная идея

Итоговая формализация CPR-framework от Cao–Tchelepi. Ключевой вклад — доказательство, что для двухфазной задачи True-IMPES и ABF дают математически эквивалентный pressure operator (при B=2 Шур-дополнение блока 2×2 совпадает с True-IMPES decoupled pressure equation).

## Почему важна эта заметка

Многие ссылки в литературе (включая amgcl documentation, MRST source, OPM) цитируют «Cao & Tchelepi CPR framework» без точного указания, какая именно публикация имеется в виду. Эта заметка фиксирует: первоисточник — PhD thesis (2002), основная техническая публикация — SPE-96030 (2005).

## Связанные заметки

- [[Cao 2002 Development of Techniques for General Purpose Simulators]]
- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]]
- [[Lacroix 2003 Decoupling Preconditioners in IPARS]]
- [[Gries 2014 System-AMG Approach for Fully Coupled CPR]]
- [[Wallis 1983 Incomplete Gaussian Elimination as Preconditioning for CPR]]
