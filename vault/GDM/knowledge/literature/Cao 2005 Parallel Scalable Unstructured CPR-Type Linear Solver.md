---
tags:
  - литература
  - cpr
  - прекондиционер
  - линейная-алгебра
  - amg
date: 2026-06-29
---

# Cao 2005 — Parallel Scalable Unstructured CPR-Type Linear Solver for Reservoir Simulation

- **Авторы:** Cao H., Tchelepi H.A., Walecka-Hutchison J., Buckley S.E.
- **Год:** 2005
- **Издание:** SPE 96030, SPE Annual Technical Conference and Exhibition, Dallas, TX
- **DOI/SPE:** SPE-96030-MS

## Основная идея

Обобщение CPR-прекондиционера Wallis (1983) на произвольное число компонент и фаз. Систематическое сравнение стратегий decoupling: quasi-IMPES, True-IMPES, ABF (Analytical Block Factorization). Показано, что выбор decoupling strategy критически влияет на робастность и скорость сходимости CPR, особенно для задач с фазовыми переходами и near-singular блоками Якобиана.

## Три стратегии decoupling

### 1. Quasi-IMPES (текущий amgcl)

Weights = первый столбец A_diag⁻¹. Требует обращения B×B блока. Ломается при singular блоках.

```
d = A_diag⁻¹ · e₁
App[i,j] = dᵀ · K[i,j] · e₁
```

### 2. True-IMPES (Wallis 1983)

Weights из nullspace столбца ∂F/∂S. Не требует обращения.

```
wᵀ · ∂F/∂S = 0
App[i,j] = wᵀ · K[i,j] · e_P
```

Для B=2: w = (∂Fw/∂Sw, −∂Fo/∂Sw), нормировка по max(|w|).

### 3. ABF (Analytical Block Factorization)

Точное блочное LDU-разложение с аналитическими формулами для Шур-дополнения. Наиболее точное приближение pressure equation, но сложнее в реализации (зависит от числа компонент).

## Результаты сравнения (из статьи)

| Стратегия | Робастность | Итерации GMRES | Сложность |
|---|---|---|---|
| Quasi-IMPES | Низкая (singular blocks) | Средние | Простая |
| True-IMPES | Высокая | Средние | Простая |
| ABF | Высокая | Наименьшие | Средняя |

Для двухфазных задач True-IMPES и ABF дают одинаковый результат (B=2 → ABF сводится к True-IMPES).

## Ключевые выводы для GDM

1. **Quasi-IMPES (текущее)** — простейший, но наименее робастный. При singular blocks → crash (BUG-002)
2. **True-IMPES** — та же простота, но robustness. Для B=2 реализация тривиальна (~20 строк)
3. **ABF** — избыточен для двухфазной задачи (совпадает с True-IMPES при B=2), станет актуален при расширении до 3+ компонент (FEAT-004, FEAT-005)
4. Cao показал, что на near-singular blocks True-IMPES даёт корректные weights даже при det(A_diag) → 0, потому что weights зависят от **отношения** производных, а не от абсолютных значений

## Формулы для двухфазного случая (B=2)

Якобиан в ячейке i (block row):

```
K_diag = | ∂F_oil/∂P     ∂F_oil/∂Sw   |
         | ∂F_water/∂P   ∂F_water/∂Sw |
```

True-IMPES weights:

```
w₁ =  ∂F_water/∂Sw
w₂ = −∂F_oil/∂Sw
```

Нормировка: `w /= max(|w₁|, |w₂|)` (или `w /= ||w||₂`).

Специальные случаи:
- **Sw → 0:** ∂Fw/∂Sw → 0, ∂Fo/∂Sw → −n_o·(1−Sw)^(n_o−1)/μ_o → ненулевое. w = (0, −∂Fo/∂Sw) → w_norm = (0, −1). App берёт только water equation для pressure (корректно: oil equation не зависит от Sw при Sw=0)
- **Оба = 0:** w = (0, 0) → fallback w = (1, 0) — берём pressure-pressure элемент напрямую

## Релевантность для GDM

Запланировано в FEAT-011: реализация True-IMPES weights в `cpr.hpp:invert()` (замена quasi-IMPES). После верификации — PR в upstream amgcl.

При расширении до трёхфазной/композиционной модели (FEAT-004, FEAT-005) — переход на ABF.

## Связанные заметки

- [[Wallis 1983 Incomplete Gaussian Elimination as Preconditioning for CPR]]
- [[Cao 2002 Development of Techniques for General Purpose Simulators]]
- [[Lacroix 2003 Decoupling Preconditioners in IPARS]]
- [[Cao 2009 A Fully Coupled Two-Phase Flow CPR Preconditioner]]
- [[Gries 2014 System-AMG Approach for Fully Coupled CPR]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[переход с блочного AMG на скалярный CPR в production]]
- [[возможности amgcl для блочных СЛАУ]]
