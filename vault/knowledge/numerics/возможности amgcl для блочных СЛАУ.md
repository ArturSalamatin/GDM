---
tags:
  - amgcl
  - солвер
  - AMG
  - справочник
date: 2026-06-20
---

# Возможности amgcl для блочных СЛАУ

Библиотека amgcl (автор — Денис Демидов) — header-only C++ библиотека для решения больших разреженных СЛАУ. Ниже — полный каталог компонентов, доступных в нашей версии, с рекомендациями для задач двухфазной фильтрации (блочная система 2×2, FIM-формулировка).

## 1. Итерационные солверы (Krylov)

Пространство имён: `amgcl::solver`

| Солвер | Файл | Для каких матриц | Хранение | Примечание |
|---|---|---|---|---|
| **CG** | `cg.hpp` | SPD | 1 вектор | Только для симметричных положительно определённых. FIM-система не SPD. |
| **BiCGStab** | `bicgstab.hpp` | Общие | 4 вектора | Стандарт для несимметричных. Может иметь нерегулярную сходимость. |
| **BiCGStab(l)** | `bicgstabl.hpp` | Общие | 2l+5 вект. | Стабильнее BiCGStab, параметр l (обычно 2–4). |
| **GMRES** | `gmres.hpp` | Общие | M+3 вект. | Гарантированная монотонная сходимость. M — размер базиса Krylov. |
| **LGMRES** | `lgmres.hpp` | Общие | M+K+3 вект. | Augmented GMRES — переиспользует K рестартных направлений. Экономит итерации при рестартах. |
| **FGMRES** | `fgmres.hpp` | Общие | 2M+3 вект. | Flexible GMRES — допускает переменный предобуславливатель. Обязателен, если precond зависит от итерации. |
| **IDR(s)** | `idrs.hpp` | Общие | s+3 вект. | Induced Dimension Reduction. Часто быстрее BiCGStab для s=4. Параметр s контролирует quality/cost. |
| **Richardson** | `richardson.hpp` | Общие | 1 вектор | Простая итерация. Для AMG как солвера (а не precond). |
| **Preonly** | `preonly.hpp` | — | 0 | Один шаг предобуславливателя, без Krylov. Для AMG/ILU как прямого метода. |

### Рекомендации для GDM

- **BiCGStab** или **IDR(s)** — оптимальные для FIM-системы. Дешевле GMRES по памяти, не требуют рестартов.
- **GMRES с M=15–30** — безопасный выбор, гарантированная монотонность. Дороже по памяти.
- **LGMRES** — если GMRES рестартует часто, LGMRES может сэкономить 20–30% итераций.
- **Preonly** — интересен, если AMG (W-cycle) сходит за 1 итерацию.

## 2. Relaxation (сглаживатели для AMG / самостоятельные предобуславливатели)

Пространство имён: `amgcl::relaxation`

| Relaxation | Файл | Примечание |
|---|---|---|
| **Damped Jacobi** | `damped_jacobi.hpp` | Параметр ω (damping). По умолчанию 2/3. Дёшево, параллелизуемо. |
| **Gauss–Seidel** | `gauss_seidel.hpp` | Мощнее Jacobi, но не параллелизуемо. Для блочных — проверить поддержку. |
| **SPAI-0** | `spai0.hpp` | Sparse Approximate Inverse, диагональный. Хорош для блочных — каждый блок инвертируется. |
| **SPAI-1** | `spai1.hpp` | SPAI с шаблоном разреженности = строке матрицы. Мощнее SPAI-0, дороже setup. |
| **ILU(0)** | `ilu0.hpp` | Неполная LU-факторизация без fill-in. Мощный сглаживатель, дорогой setup. |
| **ILU(k)** | `iluk.hpp` | ILU с уровнями fill-in k. k=1 часто оптимален. |
| **ILU(p)** | `ilup.hpp` | ILU с порогом отбрасывания p. Адаптивный fill-in. |
| **ILU(t)** | `ilut.hpp` | ILU с порогом + ограничением fill-in. |
| **Chebyshev** | `chebyshev.hpp` | Полиномиальный. Нужна оценка спектрального радиуса. Хорош для SPD. |
| **as_preconditioner** | `as_preconditioner.hpp` | Обёртка: один шаг relaxation как отдельный предобуславливатель (без AMG). |
| **as_block** | `as_block.hpp` | Скалярный relaxation применяется поблочно. |

### Рекомендации для GDM

- **SPAI-0** — наилучший кандидат для замены damped_jacobi. Блочная инверсия 2×2 — дешёвая (аналитическая формула), но качество сглаживания существенно выше.
- **ILU(0)** — если SPAI-0 недостаточно. Дороже setup, но может радикально уменьшить число итераций.
- **Chebyshev** — не рекомендуется для несимметричных задач.
- **as_preconditioner<ilu0>** — голый ILU(0) без AMG. Для 10k ячеек может быть конкурентоспособен.

## 3. Coarsening (стратегии огрубления для AMG)

Пространство имён: `amgcl::coarsening`

| Coarsening | Файл | Примечание |
|---|---|---|
| **Aggregation** | `aggregation.hpp` | Несглаженная агрегация. Быстрый setup, грубая интерполяция. |
| **Smoothed Aggregation** | `smoothed_aggregation.hpp` | Сглаженная агрегация (SA-AMG). Лучшее качество V-cycle, дороже setup. Параметр `over_interp`. |
| **Smoothed Aggr. E-min** | `smoothed_aggr_emin.hpp` | Energy-minimizing prolongation. Оптимальная интерполяция, самый дорогой setup. |
| **Ruge–Stüben** | `ruge_stuben.hpp` | Классический RS-AMG. Стандарт для скалярных эллиптических задач. Для блочных — через `as_scalar`. |
| **as_scalar** | `as_scalar.hpp` | Обёртка: скалярный coarsening для блочной матрицы (извлекает trace/norm блока). |

### Рекомендации для GDM

- **Smoothed Aggregation** — должен дать лучшую сходимость, чем текущий `aggregation`, ценой ~50% более дорогого setup. Если reuse setup (фаза 4.1), стоимость setup амортизируется.
- **Ruge–Stüben** — может не работать напрямую с блочными типами. Нужно через `as_scalar` обёртку.
- **E-min** — для исследования: даёт ли оно заметный выигрыш над SA.

## 4. Специализированные предобуславливатели

Пространство имён: `amgcl::preconditioner`

| Предобуславливатель | Файл | Примечание |
|---|---|---|
| **CPR** | `cpr.hpp` | Constrained Pressure Residual. Стандарт для FIM-симуляторов. AMG для давления, ILU для полной. |
| **CPR-DRS** | `cpr_drs.hpp` | CPR с Dynamic Row Summing. Улучшает сходимость на FIM-задачах. |
| **Schur Pressure Correction** | `schur_pressure_correction.hpp` | Предобуславливание через дополнение Шура. AMG для давления, ILU для Шурова дополнения. |
| **Dummy** | `dummy.hpp` | Пустой предобуславливатель (identity). Для тестирования. |

### CPR — подробнее

CPR декомпозирует блочную систему на:
1. **Давление** (эллиптическая часть) → решается AMG
2. **Полная система** → решается ILU(0)

Алгоритм:
1. Извлечь давление: $A_{pp}$ — давленческий блок
2. Решить для давления: $M_{AMG} r_p = r_p$
3. Обновить решение
4. Один шаг ILU для полной системы

В amgcl:
```cpp
using Precond = amgcl::preconditioner::cpr<
    amgcl::amg<Backend, Coarsening, Relaxation>,  // для давления
    amgcl::relaxation::ilu0<Backend>               // для полной системы
>;
using Solver = amgcl::make_solver<Precond, amgcl::solver::bicgstab<Backend>>;
```

Для нашей системы:
- `pressure_variable = 1` (индекс давления в блоке 2×2)
- Или задаётся через `pmask` (pressure mask)

### CPR-DRS vs CPR

CPR-DRS добавляет преобразование "true-IMPES" перед извлечением давления. Это делает давленческий блок лучше обусловленным за счёт dynamic row summing. Рекомендуется для FIM-формулировки.

### Schur Pressure Correction

Более теоретически обоснованный подход, чем CPR. Решает дополнение Шура $S = A_{pp} - A_{ps} A_{ss}^{-1} A_{sp}$ через AMG. Дороже CPR, но может дать лучшую сходимость для сильно связанных систем.

## 5. Параметры AMG (amg::params)

| Параметр | Default | Описание |
|---|---|---|
| `coarse_enough` | 300 | Не огрублять уровень, если < coarse_enough ячеек |
| `max_levels` | 25 | Максимум уровней иерархии |
| `ncycle` | 1 | 1 = V-cycle, 2 = W-cycle |
| `npre` | 1 | Число pre-smoothing шагов |
| `npost` | 1 | Число post-smoothing шагов |
| `pre_cycles` | 1 | Число V-cycles в pre-smoothing |
| `direct_coarse` | false | Прямой солвер на грубейшем уровне |
| `coarsening.aggr.eps_strong` | 0.08 | Порог "сильной связи" (агрегация) |
| `coarsening.aggr.block_size` | 0 | Блочный размер для скалярного coarsening |
| `coarsening.over_interp` | 1.5 | Overinterpolation для smoothed aggregation |
| `coarsening.estimate_spectral_radius` | false | Оценка спектрального радиуса (для Chebyshev) |
| `relax.damping` | 2/3 | Damping для damped_jacobi |

## 6. Backends (вычислительные бэкенды)

| Backend | Описание |
|---|---|
| `builtin` | CPU, последовательный (наш текущий) |
| `omp` | OpenMP (CPU-параллелизм) |
| `vexcl` | OpenCL/CUDA через VexCL |
| `cuda` | CUDA напрямую |
| `eigen` | Интерфейс к Eigen |

Для GPU-ускорения нужно переходить на `vexcl` или `cuda` backend. Это отдельная фаза, не в текущем плане.

OpenMP-backend (`amgcl::backend::builtin` уже поддерживает параллельные SpMV через OpenMP, если скомпилировано с -fopenmp). Стоит проверить.

## 7. API для reuse и диагностики

### make_solver: решение с другой матрицей

`make_solver` позволяет решать с матрицей, отличной от той, на которой строился preconditioner:

```cpp
// Конструктор: строит preconditioner P по матрице A0
make_solver<Precond, Solver> solve(A0, prm);
// Решение с той же матрицей:
auto [iters, err] = solve(rhs, x);
// Решение с ДРУГОЙ матрицей A1 (preconditioner от A0):
auto [iters, err] = solve(A1, rhs, x);
```

Это штатный механизм для «frozen preconditioner» — документация amgcl прямо рекомендует его для нестационарных задач: «a preconditioner built for a time step will act as a reasonably good preconditioner for several subsequent time steps».

### CPR/CPR-DRS: partial_update()

```cpp
void partial_update(const Matrix &K, bool update_transfer_ops = true, ...);
```

- `update_transfer_ops = false` — обновляет только ILU (SPrecond), оставляя AMG-иерархию и трансферный оператор Fpp нетронутыми
- `update_transfer_ops = true` — обновляет ILU + Fpp (row extraction), но AMG-иерархия по-прежнему фиксирована
- Полная пересборка (включая AMG) — только через конструктор

### operator<< для amg

Выводит: number of levels, operator complexity, grid complexity, memory footprint. Формат:

```
Number of levels:    4
Operator complexity: 1.23
Grid complexity:     1.15
Memory footprint:    1.23 M
```

`operator<<` для `make_solver` выводит и Solver, и Preconditioner.

### Доступ к preconditioner/solver

```cpp
make_solver<P, S> solve(A, prm);
const P& precond = solve.precond();   // доступ к preconditioner
const S& solver  = solve.solver();    // доступ к iterative solver
size_t n = solve.size();              // размер системы
```

---

## Источники

- amgcl документация: https://amgcl.readthedocs.io/
- Демидов Д.Е. — AMGCL: An Efficient, Flexible, and Extensible Algebraic Multigrid Implementation // Lobachevskii J. Math., 2019
- Демидов Д.Е. — Block-preconditioner for reservoir simulation (amgcl examples)

---

См. также:
- [[линейный солвер — AMG через amgcl]]
- [[план профилирования и оптимизации AMGCL]]
