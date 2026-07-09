---
tags:
  - debugging
  - cpr
  - ilu
  - численные-методы
date: 2026-06-24
github: https://github.com/ArturSalamatin/GDM/issues/4
---

**GitHub issue:** [#4](https://github.com/ArturSalamatin/GDM/issues/4) (BUG-002), [#21](https://github.com/ArturSalamatin/GDM/issues/21) (FEAT-010)

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

## Проявление 3: assert в cpr.hpp:522 (block-LU)

Обнаружено 2026-06-29 при верификации BUG-001. Тест `Grid convergence: single injector` падает с:

```
Assertion failed: !math::is_zero(d), file .../amgcl/preconditioner/cpr.hpp, line 522
```

Это LU-факторизация B×B блока (B=2, двухфазная задача) внутри CPR:

```cpp
// cpr.hpp:520-526
for(int k = 0; k < B; ++k) {
    scalar_type d = A[k*B+k];
    assert(!math::is_zero(d));  // ← abort()
    for(int i = k+1; i < B; ++i) {
        A[i*B+k] /= d;
```

Падает в **обоих** конфигах (Release и Debug) — `assert` в amgcl не зависит от `NDEBUG`. Патч ilu0.hpp (fallback `D[i]=1`) не покрывает этот путь — это другой уровень факторизации (block-LU в CPR, не ILU0).

Корневая причина та же: near-singular 2×2 блок Якобиана при определённых Sw → zero pivot при LU-разложении блока.

**Решение проявления 3 (реализовано 2026-06-29):**

```cpp
// было:
assert(!math::is_zero(d));
// стало:
if (math::is_zero(d)) d = static_cast<scalar_type>(1);
A[k*B+k] = d;
```

Запись `A[k*B+k] = d` обязательна: `d` — локальная копия, обратный ход (строка 543: `y[i] /= A[i*B+i]`) читает из массива. Без записи — деление на ноль.

Верификация: Release 277/277, Debug 277/277. Grid convergence подтверждён (dSw_fine < dSw_coarse, dP_fine < dP_coarse). Массовый баланс < 1e-11.

**Три уровня решения:**

| Уровень | Задача | Суть | Покрытие |
|---|---|---|---|
| 1 | BUG-002 | Fallback `d = 1` при `math::is_zero(d)` в `cpr.hpp:522` + запись `A[k*B+k] = d` для обратного хода | exact zero pivot → crash устранён |
| 2 | FEAT-010 | Threshold-based: `\|d\| < τ·max(\|A_row\|)` → weight = (1, 0) напрямую | near-zero pivot → overflow/NaN устранён |
| 3 | FEAT-011 | True-IMPES / ABF weights: вычисление weights из nullspace ∂F/∂Sw без обращения блока | принципиальное решение, PR в upstream amgcl |

**Проблема near-zero pivot (не покрыта уровнем 1):**
При `d = 1e-15` (near-zero, не exact zero) fallback уровня 1 не срабатывает (`math::is_zero` — exact comparison). LU-разложение: `A[i*B+k] /= 1e-15` → элементы ~ 1e+15 → обратный ход: weights ≈ (0, 0) → строка App ≈ 0 → AMG получает near-singular pressure matrix. Это **хуже**, чем exact zero с fallback (weights = (1,0) → Kpp напрямую).

**True-IMPES weights (уровень 3, Wallis 1983):**
Вместо обращения блока K_diag[i] вычисляют weights w из условия wᵀ·∂F/∂Sw = 0. Для B=2: w = (∂Fw/∂Sw, −∂Fo/∂Sw) с нормировкой. Не требует обращения матрицы. При Sw → 0 отношение ∂Fw/∂Sw к ∂Fo/∂Sw остаётся определённым. При оба = 0 → w = (1, 0) (давление decoupled тривиально).

## Связанные заметки

- [[переход с блочного AMG на скалярный CPR в production]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[prompt-оптимизация-09-CPR-в-production]]
