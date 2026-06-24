---
tags:
  - debugging
  - cpr
  - ilu
  - численные-методы
date: 2026-06-24
---

# Zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане

## Проблема

При переходе с блочного AMG на скалярный CPR два теста падали с `Zero pivot in ILU` (`ilu0.hpp:156`):
- **Grid convergence 41×41** — при Sw_init = 0.2 (ILU factorization порождает zero pivot через elimination на тонкой сетке)
- **Five-spot** — при Sw_init = 0 (вся строка Якобиана для водной фазы нулевая)

## Причина

### ILU-produced zero pivots (Grid convergence)

Даже если начальные диагонали ненулевые, ILU(0) factorization может обнулить pivot через elimination: `D[i] -= tl * U[k]`. На тонких сетках (41×41) это происходит чаще — больше fill-in paths. Регуляризация начальной матрицы не предотвращает такие produced zeros.

Блочный AMG обходил проблему: ILU работала с 2×2 блоками, где inverse не требует ненулевого скалярного pivot.

### Полностью вырожденный Якобиан (Five-spot, Sw=0)

При Sw = 0: dFlux_water/dSw = 0, dMass_water/dSw = 0, dMass_water/dP = 0 — вся строка Якобиана водной фазы нулевая. CPR block inversion (2×2 → pressure restrictor) создаёт near-singular pressure matrix → AMG pressure solve → NaN → GMRES diverges.

Проверено: регуляризация Sw-диагоналей, `D[i]=1` fallback, iluk(k=0), smoothed_aggregation — ни один метод не решает проблему CPR при **точно** Sw=0. Работает начиная с Sw ≥ 0.0001.

## Решение — два уровня

### 1. Патч `ilu0.hpp` (обязателен)

```cpp
// было:
precondition(!math::is_zero((*D)[i]), "Zero pivot in ILU");
// стало:
if (math::is_zero((*D)[i]))
    (*D)[i] = static_cast<value_type>(1);  // identity для вырожденной строки
```

Файл не попадает в git (`.gitignore` исключает `AMGSolver/`), добавляется через `git update-index --add`.

Без патча Grid convergence 41×41 crash-ит даже при Sw_init = 0.2.

### 2. Регуляризация Sw-строк в `LinearProblem::Solve()` (дополнительная)

Перед созданием CPRSolver: для нечётных строк (Sw в InterleavedPSw) с |diag| < 1e-20 ставим diag = 1e-6. Страхует CPR block inversion от вырожденных 2×2 блоков.

### 3. Five-spot: `oil_saturation = 0.999` (Sw_init = 0.001)

При точно Sw=0 CPR не работает принципиально. В реальных пластах connate water Sw_c ≥ 10–20%.

## Верификация

| Тест | Без патча | С патчем |
|---|---|---|
| Grid convergence 21×21 | ✅ | ✅ |
| Grid convergence 41×41 | ❌ Zero pivot | ✅ 2.99 с |
| Five-spot (Sw=0.001) | ❌ Zero pivot | ✅ 1.65 с |
| Variable debit (4 шт) | ✅ | ✅ |
| 3D completions (4 шт) | ✅ | ✅ |

4 прогона полного набора (45 тестов) — стабильно.

## Связанные заметки

- [[переход с блочного AMG на скалярный CPR в production]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[prompt-оптимизация-09-CPR-в-production]]
