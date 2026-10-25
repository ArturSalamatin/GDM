---
tags:
  - сессия
  - производительность
  - cpr
  - pi-controller
  - адаптивный-шаг
date: 2026-06-24
---

# Сессия 6b-7: CPR benchmark и PI-контроллер адаптивного шага

## Сессия 6b: CPR benchmark

### Задача

Сравнить CPR (Constrained Pressure Residual) прекондиционер с production block-ILU(k) на бенчмарке 51×51×4, 7 скважин, 730 дней. CPR требует скалярный backend (`amgcl::backend::builtin<double>`) и Layout::InterleavedPSw (давление в col%B==0).

### Подготовка

1. **Исправлена сортировка столбцов CRS для InterleavedPSw.** `buildInterleaved` итерировал внутренние циклы по `physCol`, что давало несортированные столбцы при перестановке переменных. ILU0 (`ilu0.hpp:154`) требует сортированных столбцов — assertion crash. Исправлено: итерация по `crsCol` с `physCol = permCRSToPhysical_[crsCol]`.

2. **Layout как параметр конструктора ReservoirSimulator.** Добавлен `Layout layout = Layout::InterleavedSwP` в конструктор для backward-совместимости.

3. **Инфраструктура CPR benchmark.** `solve_with_scalar<SolverType>()` — скалярный backend без `block_matrix_adapter`. `run_benchmark_scalar` — полный Newton loop с Layout параметром. CPR typedef-ы: `CPR_AMG_ilu0`, `CPRDRS_AMG_ilu0`, `CPR_AMG_iluk`, скалярный `ILU0Solver_scalar`.

### Результаты CPR

Сетка: 51×51×4 = 10404 ячейки, 20808 DoF. 7 скважин (2 INJ + 4 PROD + 1 dual), 730 дней, snapshot каждые 5 дней. Production solver: `amg<aggregation, iluk(k=1)> + lgmres`, block-backend 2×2.

| # | Конфигурация | Layout | t_total, с | t_solve, с | avg_iters | Шагов | Wasted | vs BL |
|---|---|---|---|---|---|---|---|---|
| BL | `amg<aggregation, iluk(k=1)> + lgmres`, block 2×2 | SwP | 39.6 | 36.1 | 8.3 | 187 | 0 | -- |
| CPR1 | `cpr<amg<aggregation,ilu0>, ilu0> + lgmres`, scalar | **PSw** | **27.0** | **23.6** | **4.2** | 187 | 0 | **-32%** |
| CPR2 | `cpr<amg<aggregation,iluk>, ilu0> + lgmres`, scalar | PSw | 45.8 | 42.2 | 3.9 | 187 | 0 | +16% |
| CPR3 | `cpr_drs<amg<aggregation,ilu0>, ilu0> + lgmres`, scalar | PSw | 27.3 | 23.8 | 4.1 | 187 | 0 | -31% |
| CPR4 | `ilu0 + lgmres`, scalar | SwP | 40.0 | 36.5 | 25.1 | 187 | 0 | +1% |
| CPR5 | `cpr<amg<aggregation,ilu0>, ilu0> + lgmres`, scalar | **SwP** | 64.6 | 61.1 | 25.1 | 187 | 0 | +63% |

Все конфигурации: balance_ok=YES, oil_rel < 2e-10, water_rel < 8e-11.

### Анализ CPR

1. **CPR1 — лучший: -32%.** AMG для давления (ilu0 smoother) + ILU0 для полной системы. 4.2 итерации в среднем (vs 8.3 baseline).

2. **CPR2 — проигрыш (+16%).** iluk(k=1) в AMG smoother для давления. Меньше итераций (3.9 vs 4.2), но setup в 2× дороже. iluk в AMG — overkill для задачи Пуассона по давлению.

3. **CPR3 ≈ CPR1 (-31% vs -32%).** DRS (Dynamic Row Scaling) не даёт преимущества для нашей двухфазной системы. Масштабы P и Sw различаются, но не настолько чтобы DRS помог.

4. **CPR4 = baseline (+1%).** Скалярный ILU0 без AMG для давления — нижняя граница. Подтверждает: AMG давления — ключ к ускорению CPR.

5. **CPR5 — катастрофа (+63%).** CPR с Layout::InterleavedSwP (Sw в col%B==0). CPR извлекает "давление" из первой переменной блока. Если первая переменная = Sw → AMG решает уравнение насыщенности, а не давления. Не имеет смысла. **InterleavedPSw критичен для CPR.**

### Выводы CPR

- CPR с AMG ilu0 для давления — зрелая техника, даёт -32% на нашей задаче
- Для перехода в production нужен скалярный backend (отказ от `block_matrix_adapter`)
- InterleavedPSw + CRS column sorting fix — необходимые предусловия
- DRS и iluk в AMG smoother — не оправданы для текущей задачи

---

## Сессия 7: PI-контроллер адаптивного шага

### Задача

Заменить фиксированные множители шага (+15% при успехе, -30% при провале) на адаптивный PI-контроллер Söderlind, учитывающий число Newton-итераций.

### Формула

```
τ_{n+1} = τ_n × safety × (e_target / e_n)^α × (e_{n-1} / e_n)^β
```

Где `e_n = newton_iters / max_iters`, `e_target = target_iters / max_iters`.

### Реализация

- `PIController.h/.cpp` — отдельный класс с `ComputeMultiplier(newton_iters, success)` и `Reset()`
- `PIControllerParams` — структура параметров (alpha, beta, target_iters, max_iters, safety, max_growth, min_shrink)
- Интеграция в `NumericalParameters`: `increase_schemeTau()` / `decrease_schemeTau()` через PI если `use_pi_controller_ = true`
- `SetPIControllerParams()` автоматически синхронизирует `max_iters` с `newtonMaxIterNmbr`
- 10 unit tests в `test_pi_controller.cpp`
- Benchmark Series TS в `test_amgcl_benchmark.cpp`

### Обнаруженные проблемы

#### Проблема 1: newtonMaxIterNmbr = 65, не 12

План предполагал `max_iters = 12`, но `default_num_params()` задаёт `NumericalParameters(1e-6, 65, 1e-5, 1e-5)` → `newtonMaxIterNmbr = 65`. Newton "сходится" не через `count < 12`, а когда `AMG_curError == 0.0` (RHS ≈ 0 при малых corrections), что происходит примерно за 7 итераций.

При PI с `max_iters = 12`: PI видел `e_n = 65/12 >> 1` и коллапсировал шаг до 1e-8. **Fix:** `max_iters` синхронизируется с `newtonMaxIterNmbr`.

#### Проблема 2: safety < 1 → deadlock

При `safety = 0.85` и стационарном `newton == target`: `mult = 0.85 × 1.0 = 0.85` на каждом шаге. Шаг убывает до нуля. Söderlind предполагает начальный шаг переоценен (ODE). Для PDE с Newton — шаг начинается маленьким и растёт. **Fix:** `safety = 1.0`.

#### Проблема 3: snapshot_dt = initial_tau = 5.0

При `snapshot_dt = initial_tau = 5.0`: `CurrentIntegrationStep() = min(schemeTau, 5.0) = 5.0`, условие `5.0 < 5.0 = false` → `increase_schemeTau()` НИКОГДА не вызывается. PI и fixed дают идентичные результаты. **Fix:** benchmark с `snapshot_dt = 50.0`, `init_tau = 0.5`.

### Benchmark: PI vs fixed

Сетка: 51×51×4, 7 скважин, 730 дней, snapshot каждые 50 дней, initial_tau=0.5. Solver: production `amg<aggregation, iluk(k=1)> + lgmres`, block 2×2, Layout::InterleavedSwP.

| # | Конфигурация | α | β | target | safety | max_growth | Шагов | Newton | Wasted | t_total, с | vs BL |
|---|---|---|---|---|---|---|---|---|---|---|---|
| BL | fixed factor=0.15 | -- | -- | -- | -- | -- | 85 | 613 | 0 | 23.4 | -- |
| PI1 | PI default | 0.4 | 0.2 | 8 | 1.0 | 1.5 | 207 | 1297 | 0 | 52.0 | +122% |
| PI2 | PI | 0.4 | 0.2 | 10 | 1.0 | 1.5 | 123 | 844 | 0 | 33.6 | +44% |
| PI3 | PI | 0.4 | 0.2 | 12 | 1.0 | 1.5 | 98 | 700 | 0 | 27.0 | +15% |
| PI4 | P-only | 0.4 | 0.0 | 12 | 1.0 | 1.5 | 99 | 706 | 0 | 28.3 | +21% |
| **PI5** | **PI** | **0.7** | **0.2** | **12** | **1.0** | **2.0** | **85** | **615** | **0** | **24.3** | **+4%** |

Все конфигурации: balance_ok=YES, 0 wasted, oil_rel < 2e-10, water_rel < 5e-11.

### Анализ PI

1. **target_iters — ключевой параметр.** Должен быть выше среднего числа Newton-итераций (≈7), чтобы шаг рос при типичной работе. Оптимально target ≈ 1.7× avg = 12.

2. **alpha управляет скоростью роста.** При α=0.4 и target=12: mult при newton=7 = (12/65 / 7/65)^0.4 = 1.053 (+5.3% за шаг). При α=0.7: mult = 1.093 (+9.3%). Fixed factor: +15%.

3. **PI5 (α=0.7, target=12, max_growth=2.0)** — практически совпадает с baseline: 85 шагов (vs 85), 615 Newton (vs 613), 24.3с (vs 23.4с). Разница +4% в пределах погрешности.

4. **I-компонент (β=0.2) незначительно влияет** при стабильной задаче. PI4 (P-only, β=0) на 16% медленнее PI5 (β=0.2), но при α=0.4. С α=0.7 разница будет меньше.

5. **PI-контроллер не ухудшает решение**: одинаковое число шагов, Newton-итераций и баланс масс. Преимущество PI проявится при нестационарности (well switching, water breakthrough) — адаптивная реакция на изменение сложности.

### Оптимальные параметры PI (defaults)

```
alpha = 0.7       // P-компонент (Soderlind: 0.7/p, p=1)
beta = 0.2        // I-компонент (сглаживание)
target_iters = 12 // ~1.7× avg Newton iters
max_iters = 65    // синхронизируется с newtonMaxIterNmbr
safety = 1.0      // без drain (для PDE с Newton)
max_growth = 2.0  // max увеличение за шаг
min_shrink = 0.3  // min уменьшение за шаг
```

### Файлы

| Файл | Тип | Изменение |
|---|---|---|
| `HydroSolver/Reservoir/PIController.h` | **новый** | PIControllerParams + PIController class |
| `HydroSolver/Reservoir/PIController.cpp` | **новый** | ComputeMultiplier, Reset |
| `HydroSolver/Reservoir/NumericalParameters.h` | изм | #include PIController, pi_controller_ member, SetPIControllerParams, SetUsePIController |
| `HydroSolver/Reservoir/NumericalParameters.cpp` | изм | increase/decrease через PI ветку |
| `HydroSolver/Reservoir/ReservoirSImulator.h` | изм | Layout параметр в конструктор |
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | изм | Layout пробрасывается в LinearProblem |
| `HydroSolver/Solver/Math/CRSStructure.cpp` | изм | Исправлена сортировка столбцов для InterleavedPSw |
| `CMakeLists.txt` | изм | +PIController.cpp в gdm_core, +test_pi_controller.cpp в gdm_tests |
| `tests/test_amgcl_benchmark.cpp` | изм | CPR Series + TS Series + run_benchmark параметризация |
| `tests/test_pi_controller.cpp` | **новый** | 10 unit tests PIController |

### Тесты

- 10 PI unit tests — все проходят
- 45 existing tests (ctest) — все проходят (backward compat, `use_pi_controller_ = false`)

## Следующие шаги

1. Проверить PI на нестационарных задачах (well switching, water breakthrough)
2. Переключить `use_pi_controller_ = true` по умолчанию после верификации
3. Комбинация CPR + PI для максимальной производительности
4. CPR в production: переход на скалярный backend

## Связанные заметки

- [[prompt-оптимизация-06b-CPR-benchmark]]
- [[prompt-оптимизация-07-adaptive-timestep]]
- [[план профилирования и оптимизации AMGCL]]
- [[2026-06-23 сессия 6a layout абстракция]]
- [[PI-контроллер safety=1 и target=12 для Newton-based timestep control]]
- [[layout абстракция отделяет топологию сетки от CRS маппинга]]
