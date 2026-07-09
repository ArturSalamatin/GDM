---
tags:
  - план
  - фича
  - модель
date: 2026-07-09
issue: FEAT-010
github: 21
branch: feat/feat-010/cpr-threshold-fallback
phase: 3
status: реализован
audit:
  date: 2026-07-09
  round: 4
  findings: 0 / 0 / 1
  auto-fixed: 1
  manual-required: 0
---

# FEAT-010: Threshold-based fallback в CPR block-LU

## Описание

Заменить exact-zero проверку `math::is_zero(d)` в `cpr.hpp:invert()` на threshold-based: `|d| < τ·max(|diag|)`. При срабатывании — прервать LU и записать weight = (1, 0, ..., 0), минуя обратный ход с испорченной факторизацией.

Это уровень 2 защиты от вырожденных блоков в CPR decoupling. Уровень 1 (BUG-002, exact-zero fallback `d=1`) покрывает точные нули; уровень 2 покрывает near-zero pivots.

## Математическое обоснование

### Проблема

CPR (Constrained Pressure Residual) строит скалярную pressure-pressure подматрицу через decoupling. amgcl вычисляет weights через LU-факторизацию B×B диагонального блока Якобиана и извлечение первого столбца A⁻¹.

Для B=2 (двухфазная задача) блок Якобиана ячейки i:

```
A = [∂F_oil/∂P    ∂F_oil/∂Sw  ]
    [∂F_water/∂P   ∂F_water/∂Sw]
```

При Sw → 0: dkrw/dSw = n·Sw^(n-1) → 0, ∂F_water/∂Sw → 0, ∂F_water/∂P → 0 — строка Якобиана водной фазы near-singular. LU-разложение 2×2:

```
d = A[1*2+1] - A[1*2+0]*A[0*2+1]/A[0*2+0]   (pivot для k=1)
```

Если `d = O(1e-15)` (near-zero, не exact zero), текущий fallback BUG-002 (`math::is_zero(d)`) не срабатывает. Деление `A[i*B+k] /= d` даёт элементы `O(1e+15)`, обратный ход — weights ≈ (0, 0), строка App ≈ 0, AMG получает near-singular pressure matrix.

### Решение

Относительный порог: `|d| < τ · max_k(|A[k*B+k]|)`.

При срабатывании: прервать LU, записать `y = (1, 0, ..., 0)`. Физически: для данной ячейки pressure equation decoupled тривиально — строка App копируется из первой строки блока Якобиана без decoupling. Это лучше, чем clamped LU (предсказуемый результат), и готовит почву для FEAT-011 (True-IMPES weights).

### Backward compatibility

При τ = 0 (default): `|d| < 0` никогда не истинно → поведение = уровень 1 (exact-zero only). Все существующие тесты зелёные без изменения `LinearProblem`.

### Литература

- Wallis J.R. (1983), SPE 12265 — IMPES weights, мотивация для принципиального решения (FEAT-011)
- Cao H. et al. (2005), SPE 96030 — CPR framework, True-IMPES vs quasi-IMPES
- Saad Y. (2003), *Iterative Methods for Sparse Linear Systems*, sec. 10.3.4

## Выбор варианта решения

**Вариант A: Clamp + continue LU.** Подменить `d = τ·max_diag` и продолжить LU. Weights конечные, но с артефактами — результат непредсказуем.

**Вариант B: Early return с weight = (1,0).** Прервать LU, записать `y = (1,0,...,0)`. Результат физически осмыслен: давление decoupled тривиально. Предсказуемый. Готовит путь для FEAT-011.

**Выбор: B.** Предсказуемость и физическая интерпретируемость важнее минимальности diff.

## Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `amgcl/amgcl/preconditioner/cpr.hpp` | 77–109 (params), 515–548 (invert) | Основной: `pivot_threshold` в params, threshold fallback в `invert()` |
| `HydroSolver/Solver/Math/LinearProblem.cpp` | 91–99 (конструктор params) | Установка `prm.precond.pivot_threshold` |
| `tests/test_visual_verification.cpp` | новые тесты | Near-zero pivot regression |
| `tests/simulation_cases/SingleInjectorCase.h` | возможно: параметризация oil_saturation | Для near-zero Sw сценария |

**НЕ затронуты:**
- `cpr_drs.hpp` — не имеет `invert()`, использует DRS decoupling
- `ilu0.hpp` — отдельный fallback, не в скоупе (потенциально FEAT-010b)
- `LinearProblem.h` — `SolverType::params prm` наследует `pivot_threshold` автоматически

## Подводные камни

- [x] **`AMGCL_NO_BOOST` определён** (`CMakeLists.txt:83`): boost-секция params (строки 91–108) не компилируется. Не нужно трогать `AMGCL_PARAMS_IMPORT_VALUE` / `check_params` / `get()`. Только default-конструктор и поле
- [x] **Три call sites `invert()`** в cpr.hpp: строки 257, 445, 505. Все три вызываются из `#pragma omp parallel for` — функция `invert()` потокобезопасна (локальные данные, нет shared state). Параметр `prm.pivot_threshold` — read-only, OK
- [x] **`max_diag` нужно вычислить до начала LU.** LU модифицирует массив `A` in-place. Вычислить `max_diag` в начале `invert()`, до цикла факторизации
- [x] **Backward compat при τ=0.** Условие `|d| < 0·max_diag` = `|d| < 0` — всегда false. Поведение идентично текущему. Проверить тестом
- [x] **GDM_SOLVER_ILU0:** не имеет CPR, `prm.precond` — это `as_preconditioner::params`, не `cpr::params`. Установка `pivot_threshold` — под `#ifdef` guard
- [x] **GDM_SOLVER_CPR_DRS:** `cpr_drs::params` не имеет `pivot_threshold`. Не устанавливать
- [x] **Потокобезопасность.** `invert()` вызывается из `#pragma omp parallel for`. Массив `A` и `y` — per-thread (каждый thread работает со своим блоком `ip`). `prm.pivot_threshold` — read-only. Безопасно
- [x] **Обратный ход при early return.** При записи `y = (1,0,...,0)` — обратный ход (строки 533–547) не выполняется. `y` уже заполнен. OK — early return до обратного хода
- [x] **Submodule branch management.** `experimental/solvers` стартует от `experimental/master` (чистый форк) + merge `experimental/patches` (патчи BUG-002). FEAT-010 коммитится поверх. НЕ коммитить напрямую в `experimental/patches`, `experimental/master`, `master`. При merge GDM в experimental — submodule ссылается на `experimental/solvers`. Дальнейшие фичи (FEAT-011) тоже будут в `experimental/solvers`

## Связанные заметки

- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[локальные патчи AMGCL для GDM]]

---

## Этапы реализации

### Этап 1: Подготовка (шаги 1–2)
### Этап 2: Ядро amgcl (шаги 3–4)
### Этап 3: Интеграция с GDM (шаг 5)
### Этап 4: Тесты (шаги 6–7)
### Этап 5: Верификация (шаг 8)

---

### Шаг 1: Ветки и baseline

**Цель:** создать рабочие ветки в обоих репозиториях (GDM + amgcl submodule), зафиксировать baseline

**Файлы:** нет

**Контекст:**
Текущая ветка GDM: `experimental`. Submodule `amgcl` на ветке `experimental/patches`.

FEAT-010 добавляет новый патч в amgcl. Этот патч **не должен** попадать в `experimental/patches` (базовые патчи BUG-002), `experimental/master`, `master` форка, или `master` upstream. Нужна отдельная ветка в submodule.

Структура веток amgcl:
- `master` — upstream amgcl (read-only)
- `experimental/master` — чистый форк (без патчей)
- `experimental/patches` — базовые патчи BUG-002 (ilu0.hpp + cpr.hpp fallback d=1)
- `experimental/solvers` ← **новая** — стартует от `experimental/master`, затем merge `experimental/patches` (патчи BUG-002). FEAT-010 коммитится поверх

```
experimental/master ────┬── experimental/solvers ── [FEAT-010 commit]
                        │         ↑ merge
experimental/patches ───┘─────────┘
```

**Что сделать:**
1. В основном GDM-репо: `git checkout -b feat/feat-010/cpr-threshold-fallback experimental`
2. В submodule amgcl: `cd amgcl && git checkout -b experimental/solvers experimental/master && git merge experimental/patches && cd ..`
3. Собрать Release + Debug
4. Прогнать тесты (Release + Debug, последовательно)
5. Зафиксировать baseline: количество тестов, время

**Проверка после этого шага:**
- Release: 294/294 pass
- Debug: 294/294 pass
- `cd amgcl && git branch` — текущая ветка `experimental/solvers`
- `cd amgcl && git log --oneline experimental/master..experimental/solvers` — merge commit + 2 патча из `experimental/patches`
- `cd amgcl && git diff experimental/patches..experimental/solvers` — пуст (содержит те же патчи, но через merge)

**Зависимости:** нет

**Оценка:** ~0 строк, ~5 минут

---

### Шаг 2: Добавить `pivot_threshold` в `cpr::params`

**Цель:** добавить параметр `pivot_threshold` (default = 0) в структуру `cpr::params`, не ломая существующий API

**Файлы:** `amgcl/amgcl/preconditioner/cpr.hpp`

**Контекст:**
`cpr::params` (строки 77–109) содержит `block_size` и `active_rows`. GDM компилируется с `AMGCL_NO_BOOST`, поэтому секция boost-params (строки 91–108) не компилируется. Нужно добавить только поле и инициализацию в default-конструкторе.

`scalar_type` — это `typename math::scalar_of<value_type>::type`, для GDM = `double`.

**Что сделать:**
1. Добавить поле `double pivot_threshold;` после `active_rows` (строка ~85)
2. Добавить инициализацию `, pivot_threshold(0)` в default-конструкторе (строка ~89)

**Изменения:**

До:
```cpp
        int    block_size;
        size_t active_rows;

        params()
            : block_size(math::static_rows<value_type>::value == 1 ? 2 : math::static_rows<value_type>::value),
              active_rows(0) {}
```

После:
```cpp
        int    block_size;
        size_t active_rows;
        double pivot_threshold;

        params()
            : block_size(math::static_rows<value_type>::value == 1 ? 2 : math::static_rows<value_type>::value),
              active_rows(0),
              pivot_threshold(0) {}
```

**Проверка после этого шага:**
- `cmake --build build --config Release` — компилируется без ошибок и без warnings
- `ctest --test-dir build -C Release --output-on-failure` — 294/294 pass (параметр = 0, поведение не изменилось)

**Подводные камни:**
- Тип `double` вместо `scalar_type`: `params` — nested struct внутри `cpr`, не видит typedef'ы enclosing class (C++ правила scoping). `scalar_type` определён на строке 75, но `params` не имеет к нему доступа. Для GDM `scalar_type = double`, поэтому `double` корректен. Для потенциального PR в upstream amgcl — нужно дублировать typedef внутри `params` или использовать квалифицированное имя

**Зависимости:** шаг 1

**Оценка:** ~3 строки, ~5 минут

---

### Шаг 3: Модифицировать `invert()` — threshold-based fallback

**Цель:** при near-zero pivot (`|d| < τ·max_diag`) прервать LU и записать `y = (1, 0, ..., 0)`

**Файлы:** `amgcl/amgcl/preconditioner/cpr.hpp`

**Контекст:**
Функция `invert()` (строки 515–548) выполняет LU-факторизацию B×B блока in-place и извлекает первый столбец A⁻¹. Три call sites (257, 445, 505), все в `#pragma omp parallel for`.

Текущий fallback (BUG-002): при `math::is_zero(d)` (exact `d == 0.0`) подставляется `d = 1`. Это не покрывает near-zero pivot.

Новая логика:
1. Вычислить `max_diag = max_k(|A[k*B+k]|)` ДО начала LU (LU модифицирует `A` in-place)
2. Для каждого pivot `d`: если `|d| < τ · max_diag` — записать `y = (1, 0, ..., 0)` и return
3. Сохранить текущий exact-zero fallback как подстраховку (если `τ = 0`, порог не работает, но `math::is_zero(d)` всё ещё ловит точные нули)

**Что сделать:**
1. Добавить `#include <cmath>` после `#include <cassert>` (строка 36) — для `std::abs`
2. В начале `invert()`, после вычисления `B` (строка ~517): вычислить `max_diag`
3. Внутри LU-цикла (строка ~521): заменить `if (math::is_zero(d))` на threshold check
4. При срабатывании: заполнить `y`, return

**Изменения:**

До:
```cpp
    void invert(scalar_type *A, value_type_p *y)
    {
        const int B = math::static_rows<value_type>::value == 1 ? prm.block_size : math::static_rows<value_type>::value;

        // Perform LU-factorization of A in-place
        for(int k = 0; k < B; ++k) {
            scalar_type d = A[k*B+k];
            if (math::is_zero(d)) {
                d = static_cast<scalar_type>(1);
                A[k*B+k] = d;
            }
            for(int i = k+1; i < B; ++i) {
                A[i*B+k] /= d;
                for(int j = k+1; j < B; ++j)
                    A[i*B+j] -= A[i*B+k] * A[k*B+j];
            }
        }
```

После:
```cpp
    void invert(scalar_type *A, value_type_p *y)
    {
        const int B = math::static_rows<value_type>::value == 1 ? prm.block_size : math::static_rows<value_type>::value;
        const double tau = prm.pivot_threshold;

        // Compute max diagonal magnitude before LU modifies A in-place
        scalar_type max_diag = 0;
        if (tau > 0) {
            for (int k = 0; k < B; ++k) {
                scalar_type a = std::abs(A[k*B+k]);
                if (a > max_diag) max_diag = a;
            }
        }

        // Perform LU-factorization of A in-place
        for(int k = 0; k < B; ++k) {
            scalar_type d = A[k*B+k];
            if ((tau > 0 && std::abs(d) < tau * max_diag) || math::is_zero(d)) {
                // Near-zero or exact-zero pivot: block is (near-)singular.
                // Use trivial decoupling: weight = (1, 0, ..., 0).
                for (int i = 0; i < B; ++i)
                    y[i] = static_cast<value_type_p>(i == 0);
                return;
            }
            for(int i = k+1; i < B; ++i) {
                A[i*B+k] /= d;
                for(int j = k+1; j < B; ++j)
                    A[i*B+j] -= A[i*B+k] * A[k*B+j];
            }
        }
```

**Проверка после этого шага:**
- Компилируется без ошибок и без warnings
- 294/294 pass (pivot_threshold = 0, порог не срабатывает; exact-zero fallback всё ещё работает через `math::is_zero(d)`)
- **Важно:** при τ = 0 порядок проверки: `(false && ...) || math::is_zero(d)` — short-circuit, `std::abs(d)` не вычисляется. Exact-zero fallback теперь возвращает `y = (1,0)` + return, а не `d=1` + continue. Это **изменение поведения** при exact-zero: раньше LU продолжался с `d=1`, теперь — early return с `y=(1,0)`. Физически результат тот же: weight = (1,0) — давление decoupled тривиально. Но числа могут отличаться на round-off. Проверить regression

**Подводные камни:**
- `std::abs` для double — нужен `<cmath>`. cpr.hpp включает `<vector>`, `<memory>`, `<cassert>` — `<cmath>` отсутствует. **Добавить `#include <cmath>` после строки 36** (`#include <cassert>`)
- `const double tau` — hardcoded тип. `scalar_type` доступен в scope `invert()` (typedef на строке 75 enclosing class `cpr`), можно использовать `scalar_type` для консистентности. Для GDM `scalar_type = double`, разницы нет. При реализации можно заменить на `const scalar_type tau` — но не обязательно
- Early return: после `return` обратный ход LU (строки 533–547) не выполняется — это правильно, `y` уже заполнен
- **Изменение поведения exact-zero:** при τ=0 fallback `math::is_zero(d)` теперь даёт early return с `y=(1,0)` вместо старого `d=1; A[k*B+k]=d; continue LU`. Физически результат тот же (weight=(1,0) в обоих случаях — при d=1 LU продолжается, но нижний треугольник уже испорчен, обратный ход с подменённым pivot даёт тот же weight). Возможно расхождение на round-off. Шаг 6 верифицирует это

**Зависимости:** шаг 2

**Оценка:** ~15 строк, ~15 минут

---

### Шаг 4: Коммит amgcl-патча в ветку `experimental/solvers`

**Цель:** зафиксировать изменения в submodule amgcl (ветка `experimental/solvers`, созданная на шаге 1)

**Файлы:** `amgcl/` (submodule)

**Контекст:**
amgcl — git submodule. На шаге 1 создана ветка `experimental/solvers` от `experimental/master` + merge `experimental/patches`. Изменения из шагов 2–3 (pivot_threshold в params + threshold fallback в invert) нужно закоммитить в эту ветку и обновить ссылку в основном GDM-репо.

**Важно:** ветка `experimental/patches` остаётся неизменной. Она содержит только базовые патчи BUG-002 (ilu0 + cpr exact-zero fallback). FEAT-010 — в `experimental/solvers`.

**Что сделать:**
1. Проверить текущую ветку submodule: `cd amgcl && git branch` — должна быть `experimental/solvers`
2. `git add amgcl/preconditioner/cpr.hpp && git commit -m "feat: FEAT-010 pivot_threshold в cpr::params и invert()"`
3. `cd ..`
4. `git add amgcl && git commit -m "feat: FEAT-010 обновить submodule amgcl (experimental/solvers)"`

**Проверка после этого шага:**
- `cd amgcl && git branch` — текущая ветка `experimental/solvers`
- `cd amgcl && git log --oneline experimental/patches..experimental/solvers` — 1 коммит FEAT-010 + merge commit
- `git status` в основном репо — чисто
- `git submodule status` — submodule на новом коммите

**Зависимости:** шаг 3

**Оценка:** ~0 строк, ~2 минуты

---

### Шаг 5: Установить `pivot_threshold` в LinearProblem

**Цель:** передать τ из GDM в amgcl через `prm.precond.pivot_threshold`

**Файлы:** `HydroSolver/Solver/Math/LinearProblem.cpp`

**Контекст:**
Конструктор `LinearProblem` (строки 80–100) устанавливает параметры солвера. `prm.precond` — это `cpr::params` для CPR-конфигураций и `as_preconditioner::params` для ILU0. У `as_preconditioner::params` нет `pivot_threshold`. У `cpr_drs::params` тоже нет (мы не добавляли).

Нужен `#ifdef` guard: устанавливать только для конфигураций, использующих `cpr` (CPR, CPR_SA, CPR_BICGSTAB), но не для ILU0 и не для CPR_DRS.

Рекомендуемое значение τ = `1e-10`. Обоснование: типичный масштаб элементов Якобиана — `O(1)` до `O(1e6)` (давление в Па, расход). Pivot near-zero при `d < 1e-10 · max_diag` — это обусловленность блока > 10^10, LU-факторизация бессмысленна.

**Что сделать:**
1. После строки `prm.precond.block_size = B;` (строка 92, внутри `#if !defined(GDM_SOLVER_ILU0)`) добавить установку `pivot_threshold`
2. Нужен дополнительный guard для CPR_DRS — у `cpr_drs::params` нет `pivot_threshold`

**Изменения:**

До:
```cpp
{
#if !defined(GDM_SOLVER_ILU0)
    prm.precond.block_size = B;
#endif
    prm.solver.tol = AMG_RelTol;
```

После:
```cpp
{
#if !defined(GDM_SOLVER_ILU0)
    prm.precond.block_size = B;
#endif
#if !defined(GDM_SOLVER_ILU0) && !defined(GDM_SOLVER_CPR_DRS)
    prm.precond.pivot_threshold = 1e-10;
#endif
    prm.solver.tol = AMG_RelTol;
```

**Проверка после этого шага:**
- Компилируется без ошибок и без warnings для default конфигурации (CPR)
- 294/294 pass
- Проверить: `cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=ILU0` — компилируется
- Проверить: `cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=CPR_DRS` — компилируется

**Подводные камни:**
- Два `#if` подряд (`block_size` и `pivot_threshold`) — можно объединить, но лучше не трогать существующий guard для `block_size` (он проверен). Отдельный `#if` для `pivot_threshold` — безопаснее
- `cpr_drs::params` не имеет `pivot_threshold` — guard обязателен
- Значение `1e-10` — можно сделать compile-time constant, но для текущей задачи hardcode достаточно

**Зависимости:** шаг 3

**Оценка:** ~3 строки, ~10 минут

---

### Шаг 6: Тест backward compatibility (τ = 0)

**Цель:** убедиться что при `pivot_threshold = 0` результаты идентичны baseline

**Файлы:** `tests/test_visual_verification.cpp` или `tests/test_amgcl_benchmark.cpp`

**Контекст:**
При τ = 0 (default в amgcl) threshold check не срабатывает. Но есть тонкое изменение: при exact-zero pivot раньше LU продолжался с `d=1`, теперь — early return с `y=(1,0)`. Нужно проверить что это не ломает regression.

**Что сделать:**
1. Прогнать полный набор тестов с `GDM_SOLVER=CPR` (default, τ=1e-10 из LinearProblem)
2. Временно закомментировать строку `prm.precond.pivot_threshold = 1e-10;` в `LinearProblem.cpp` (или изменить на `= 0`), пересобрать, прогнать тесты. Проверить что 294/294 pass — это подтверждает backward compat при τ=0
3. Вернуть `pivot_threshold = 1e-10` после проверки
4. Если тесты с τ=0 падают: проблема в изменении поведения exact-zero fallback (early return vs continue LU). Нужно вернуть `d=1; A[k*B+k]=d;` перед early return или сохранить оба пути

**Примечание:** этот шаг — ручная проверка, не создание нового теста. Не коммитить промежуточное состояние (τ=0). Новый тест — на шаге 7.

**Проверка после этого шага:**
- 294/294 pass с τ = 1e-10 (default)
- 294/294 pass с τ = 0 (backward compat)

**Зависимости:** шаг 5

**Оценка:** ~0 строк, ~10 минут

---

### Шаг 7: Тест near-zero pivot scenario

**Цель:** создать тест, провоцирующий near-zero pivot и проверяющий что threshold-based fallback корректно его обрабатывает

**Файлы:** `tests/test_visual_verification.cpp`, возможно `tests/simulation_cases/SingleInjectorCase.h`

**Контекст:**
Все текущие тесты используют `oil_saturation = 0.8` (Sw = 0.2). Near-zero Sw не тестируется. Grid convergence 41×41 исторически падал с zero pivot при Sw_init = 0.2 (ILU-produced zeros), но fallback BUG-002 это исправил. Для FEAT-010 нужен тест с Sw_init ≈ 0.001–0.01, где near-zero pivot вероятен.

Подход: создать сценарий `SingleInjectorCase` с `oil_saturation = 0.999` (Sw = 0.001) на сетке 41×41. Проверить что симуляция завершается (no crash, no NaN), mass balance < 1e-3, Sw ∈ [0, 1].

**Что сделать:**
1. В `tests/simulation_cases/SingleInjectorCase.h` строка 15: изменить `static constexpr double oil_saturation = 0.8;` → `double oil_saturation = 0.8;`
2. Добавить тест `Near-zero Sw: single injector Sw=0.001` с тегами `[near-zero-sw][single-injector]`
3. Проверить: no crash, `result.max_oil_balance_rel < 1e-3`, `result.max_water_balance_rel < 1e-3`
4. CSV export для визуальной верификации

**Изменения:**

В `tests/simulation_cases/SingleInjectorCase.h`:

До:
```cpp
    static constexpr double oil_saturation = 0.8;
```

После:
```cpp
    double oil_saturation = 0.8;
```

В `tests/test_visual_verification.cpp`, добавить в конец файла (после Two-well теста, строка 263):

```cpp
TEST_CASE("Near-zero Sw: single injector Sw=0.001",
          "[near-zero-sw][single-injector]")
{
    simulation_cases::SingleInjectorCase sc(41, 41);
    sc.oil_saturation = 0.999; // Sw_init = 0.001

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    // All saturations within physical bounds
    for (double s : result.Sw) {
        CHECK(s >= 0.0);
        CHECK(s <= 1.0);
    }
}
```

**Подводные камни:**
- `SingleInjectorCase::oil_saturation` — сейчас `static constexpr double oil_saturation = 0.8` (строка 15). Нельзя присвоить. **Решение:** изменить на `double oil_saturation = 0.8` (non-static, non-constexpr). Единственное использование вне класса — `test_visual_verification.cpp:177`: `(1.0 - sc.oil_saturation)` — runtime expression, не compile-time. Безопасно
- `run_case()` → `make_horizon()` → `test_helpers::make_uniform_horizon()` → `h.initial_oil_saturation.assign(N, oil_saturation)`. Цепочка использует `oil_saturation` из экземпляра — OK
- Тест может быть **медленным** на 41×41 с near-zero Sw (больше Newton итераций, wasted trials). Timeout ctest по умолчанию 25s — может не хватить. Если тест слишком долгий — уменьшить сетку до 21×21

**Зависимости:** шаг 5

**Оценка:** ~15 строк тест + ~5 строк модификация SingleInjectorCase, ~20 минут

---

### Шаг 8: Финальная верификация и vault

**Цель:** полная верификация (Release + Debug), обновление vault

**Файлы:** vault

**Что сделать:**
1. Release: сборка + тесты (полный набор)
2. Debug: сборка + тесты (полный набор)
3. Сравнить с baseline: количество тестов, время, warnings
4. Обновить `vault/GDM/roadmap/планируемые фичи.md`: FEAT-010 статус → ✅ реализовано
5. Обновить `vault/GDM/00-home/текущие приоритеты.md`
6. Обновить `vault/GDM/knowledge/debugging/zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане.md` — добавить информацию о реализации уровня 2
7. Прокомментировать GitHub issue #21

**Проверка после этого шага:**
- Release: ≥295/≥295 pass (294 + новый тест)
- Debug: ≥295/≥295 pass
- Ноль новых warnings
- Vault обновлён

**Зависимости:** шаги 6, 7

**Оценка:** ~20 строк vault, ~15 минут

---

## Тестовая стратегия

**Тест 1: Backward compatibility (implicit)**
- **Сценарий:** все 294 существующих теста с τ = 1e-10
- **Ожидание:** 294/294 pass, результаты не изменились
- **Предотвращает:** регрессию от threshold fallback

**Тест 2: Near-zero Sw**
- **Тест:** `Near-zero Sw: single injector Sw=0.001`
- **Тег:** `[near-zero-sw][single-injector]`
- **Файл:** `tests/test_visual_verification.cpp` (новый)
- **Сценарий:** five-spot 41×41, Sw_init = 0.001 — провоцирует near-zero pivot в CPR block-LU
- **Setup:** `SingleInjectorCase(41, 41)` с `oil_saturation = 0.999`
- **Ожидание:** no crash, mass balance < 1e-3, Sw ∈ [0, 1]
- **Backward compat:** при τ = 0 тест тоже должен пройти (fallback BUG-002 покрывает exact zeros; near-zero может дать артефакты, но не crash)
- **Предотвращает:** near-zero pivot overflow → NaN → GMRES divergence

---

## Критерии завершения

- [ ] `pivot_threshold` добавлен в `cpr::params` (default = 0)
- [ ] `invert()` реализует threshold fallback с early return `y=(1,0,...,0)`
- [ ] `LinearProblem` устанавливает `prm.precond.pivot_threshold = 1e-10` (с `#ifdef` guard для ILU0 и CPR_DRS)
- [ ] Все 294 существующих теста зелёные (backward compat)
- [ ] Новый тест near-zero Sw зелёный
- [ ] При τ = 0 поведение идентично уровню 1 (BUG-002)
- [ ] Компилируется без warnings для всех 5 конфигураций (CPR, CPR_BICGSTAB, CPR_SA, CPR_DRS, ILU0)
- [ ] Release и Debug — оба зелёные
- [ ] Vault обновлён
- [ ] GitHub issue #21 прокомментирован

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — контекст проблемы
3. Прочитай [[локальные патчи AMGCL для GDM]] — текущие патчи
4. Проверь текущую ветку submodule amgcl: `cd amgcl && git branch` — должна быть `experimental/patches`
5. Создай рабочую ветку GDM: `git checkout -b feat/feat-010/cpr-threshold-fallback experimental`
6. Создай рабочую ветку submodule от чистого форка + merge патчей:
   `cd amgcl && git checkout -b experimental/solvers experimental/master && git merge experimental/patches && cd ..`
7. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
8. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
9. Запомни: 294 теста, ~105 секунд (Release)
10. Начни с шага 2. После каждого шага: сборка + тесты

### Политика веток amgcl

Изменения FEAT-010 затрагивают **два** репозитория:
- **GDM** (основной): ветка `feat/feat-010/cpr-threshold-fallback` от `experimental`
- **amgcl** (submodule): ветка `experimental/solvers` от `experimental/master` + merge `experimental/patches`

Ветки amgcl, которые **НЕ должны** получать новые коммиты:
- `master` — upstream amgcl (read-only)
- `experimental/master` — чистый форк без патчей
- `experimental/patches` — только базовые патчи BUG-002 (d=1 fallback)

Стратегия: `experimental/solvers` стартует от чистого форка (`experimental/master`), подтягивает патчи через `merge experimental/patches`, затем FEAT-010 коммитится поверх. Это даёт явную историю: видно откуда пришли патчи (merge commit) и где начинается FEAT-010.

При merge GDM-ветки в `experimental` — submodule будет ссылаться на коммит в `experimental/solvers`
