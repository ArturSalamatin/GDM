---
tags:
  - план
  - фича
  - модель
date: 2026-07-09
issue: FEAT-011
github: 22
branch: feat/feat-011/true-impes-weights
phase: 3
status: реализован
audit:
  date: 2026-07-09
  revision: 4
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# FEAT-011: True-IMPES weights для CPR decoupling

## Описание

Заменить quasi-IMPES decoupling (первый столбец A⁻¹ через LU-факторизацию) на True-IMPES weights в CPR-прекондиционере. True-IMPES вычисляет weights из nullspace столбца ∂F/∂Sw — обращение B×B блока не требуется, singular/near-singular блоки обрабатываются корректно.

Уровень 3 (принципиальное решение) в трёхуровневой стратегии:

| Уровень | Задача | Суть | Статус |
|---|---|---|---|
| 1 | BUG-002 | Fallback d=1 при exact zero pivot | ✅ |
| 2 | FEAT-010 | Threshold-based: \|d\| < τ·max(\|diag\|) → d=1, continue LU | ✅ |
| 3 | **FEAT-011** | **True-IMPES weights из nullspace ∂F/∂Sw** | ⬜ |

## Математическое обоснование

### Quasi-IMPES (текущий amgcl `invert()`)

CPR строит скалярную pressure-pressure подматрицу App. Для каждого блока Якобиана K_diag[i] (B×B):

1. Транспонировать: A = Kᵀ
2. LU-факторизация A in-place
3. Обратный ход: y = A⁻¹ · e₁ (первый столбец обратной матрицы)
4. App entry: app = yᵀ · K_offdiag[i,j] · e_P (dot product weights · pressure column)

При singular блоке (Sw → 0): LU pivot → 0 → overflow → weights ≈ (0,0) → строка App ≈ 0 → AMG получает near-singular pressure matrix.

### True-IMPES (Wallis 1983, Cao 2005)

Weights w из условия wᵀ · ∂F/∂Sw = 0:

Для B=2 (двухфазная нефть–вода): столбец ∂F/∂Sw = (∂F_oil/∂Sw, ∂F_water/∂Sw). Любой вектор, ортогональный этому столбцу, удовлетворяет условию. Аналитически:

```
w₁ =  ∂F_water/∂Sw
w₂ = −∂F_oil/∂Sw
```

Нормировка: w /= max(|w₁|, |w₂|).

### Специальные случаи

- **Sw → 0:** ∂Fw/∂Sw → 0, ∂Fo/∂Sw ≠ 0 → w = (0, −1) нормализованное. App берёт только water equation для pressure (корректно: oil equation вырождена)
- **Оба = 0:** w = (0,0) → fallback w = (1,0). Давление decoupled тривиально. Это единственный случай, требующий fallback, и он неопасен: обе производные нулевые → блок Якобиана пустой → ячейка не влияет на давление
- **Невырожденный блок (B=2):** True-IMPES weights **пропорциональны** quasi-IMPES: w_true = (d, −c), w_quasi = (d, −c) / det(A). Отличие — row-dependent скалярный множитель на каждую строку App. AMG coarsening чувствителен к relative row scaling → число GMRES-итераций может отличаться на ±1–3. Побитовое совпадение weights **не гарантировано** — это нормально, не ошибка формулы

### Layout блока в amgcl

GDM использует InterleavedPSw: [P0, Sw0, P1, Sw1, ...]. Переменная 0 (col%B==0) = P, переменная 1 (col%B==1) = Sw.

Якобиан K (row-major, B×B):
```
K = [[∂Fo/∂P,  ∂Fo/∂Sw],
     [∂Fw/∂P,  ∂Fw/∂Sw]]
```

Все три call sites `invert()` передают **транспонированный** блок A = Kᵀ (row-major):
```
A = [[∂Fo/∂P,  ∂Fw/∂P ],
     [∂Fo/∂Sw, ∂Fw/∂Sw]]
```

Индексация (row-major, `A[i*B+j]`):
- `A[1*B+0] = ∂Fo/∂Sw`
- `A[1*B+1] = ∂Fw/∂Sw`

True-IMPES weights из транспонированного блока:
```
w[0] =  A[1*B+1]   // ∂Fw/∂Sw
w[1] = −A[1*B+0]   // −∂Fo/∂Sw
```

### Литература

- Wallis J.R. (1983), SPE 12265 — IMPES weights, мотивация для CPR decoupling
- Cao H., Tchelepi H.A. et al. (2005), SPE 96030 — CPR framework, сравнение quasi-IMPES / True-IMPES / ABF
- Cao H. (2002), PhD thesis Stanford — три decoupling strategies: quasi-IMPES, True-IMPES, ABF
- Saad Y. (2003), *Iterative Methods for Sparse Linear Systems*, sec. 10.3.4

## Дизайн: template parameter + tag dispatch

Выбран после обсуждения: не enum, не наследование, не runtime полиморфизм. Причина: amgcl — pure compile-time polymorphism, без virtual methods.

### Tags

```cpp
namespace amgcl {
namespace preconditioner {

struct quasi_impes_weights {};
struct true_impes_weights {};

} // preconditioner
} // amgcl
```

### Template parameter

```cpp
template <class PPrecond, class SPrecond, class WeightsPolicy = quasi_impes_weights>
class cpr { ... };
```

Default `quasi_impes_weights` → backward compatibility. Все существующие `cpr<PPrecond, SPrecond>` компилируются без изменений.

### Tag dispatch в `invert()`

```cpp
void invert(scalar_type *A, value_type_p *y) {
    invert_impl(A, y, WeightsPolicy{});
}

void invert_impl(scalar_type *A, value_type_p *y, quasi_impes_weights) {
    // текущая LU-факторизация (оригинальный код с master)
}

void invert_impl(scalar_type *A, value_type_p *y, true_impes_weights) {
    const int B = math::static_rows<value_type>::value == 1
        ? prm.block_size
        : math::static_rows<value_type>::value;
    // A — транспонированный Якобиан (row-major)
    scalar_type w0 =  A[1*B+1];  // ∂Fw/∂Sw
    scalar_type w1 = -A[1*B+0];  // −∂Fo/∂Sw
    scalar_type m = std::max(std::abs(w0), std::abs(w1));
    if (m > 0) { w0 /= m; w1 /= m; }
    else { w0 = 1; w1 = 0; }
    y[0] = static_cast<value_type_p>(w0);
    y[1] = static_cast<value_type_p>(w1);
}
```

### SolverConfig.h

Три CPR-конфигурации (CPR, CPR_BICGSTAB, CPR_SA) получают третий шаблонный параметр:

```cpp
using PrecondType = amgcl::preconditioner::cpr<
    PPrecond, SPrecond,
    amgcl::preconditioner::true_impes_weights
>;
```

ILU0 и CPR_DRS не затронуты.

## Backward compatibility

- Default `WeightsPolicy = quasi_impes_weights` → поведение идентично текущему
- Ветка `experimental/true-impes` от `master` — без `pivot_threshold` в params (это FEAT-010, отдельная ветка `experimental/solvers`). Для `true_impes_weights` LU не используется — патчи не нужны
- Все 295 существующих тестов зелёные при quasi_impes_weights
- При переключении на `true_impes_weights` — weights пропорциональны quasi-IMPES для невырожденных блоков, App отличается на row-dependent scalar, число GMRES-итераций может незначительно измениться (±1–3). Sw=0 работает корректно

## Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `amgcl/amgcl/preconditioner/cpr.hpp` | 44 (template), 78–112 (params), 515–548 (invert) | Tags, template param, invert dispatch |
| `HydroSolver/Solver/Math/SolverConfig.h` | 41, 49, 63 (using PrecondType) | Третий шаблонный параметр |
| `tests/test_visual_verification.cpp` | 265+ (near-zero Sw test) | Новый тест: Sw=0 |

| `HydroSolver/Solver/Math/LinearProblem.cpp` | 94–96 (`pivot_threshold`) | Удалить установку `pivot_threshold` — поле не существует на master |

**НЕ затронуты:**
- `cpr_drs.hpp` — свой механизм weights (DRS), не в скоупе
- `ilu0.hpp` — fallback d=1, не в скоупе

## Поиск подводных камней

- ✅ **Backward compat:** `cpr<PP, SP>` = `cpr<PP, SP, quasi_impes_weights>` — три existing using-а компилируются без изменений
- ✅ **Все call sites:** три call sites `invert()` (строки 257, 445, 505 на master) — все через `this->invert()`, dispatch работает автоматически
- ✅ **Транспозиция:** все три call sites транспонируют блок перед `invert()`. Формула True-IMPES учитывает транспонированный layout
- ✅ **B > 2 защита:** `static_assert` для static block path + `assert` для scalar path. Нет молчаливого fallback — ошибка компиляции (static) или assert (Debug). Для B>2 — использовать `quasi_impes_weights` или реализовать ABF (Cao 2002)
- ✅ **OpenMP:** `invert()` вызывается внутри `#pragma omp for` (строки 218, 488 на master). Новая реализация не добавляет shared state — потокобезопасна
- ✅ **AMGCL_NO_BOOST:** boost-секция params не компилируется (AMGCL_NO_BOOST определён в CMakeLists.txt:83). Новый template param не затрагивает boost-секцию
- ✅ **Производительность:** True-IMPES — арифметические операции O(1) vs LU O(B³). Быстрее quasi-IMPES
- ✅ **Чистая ветка, без merge:** `experimental/true-impes` от `master` — только True-IMPES код, без BUG-002/FEAT-010. Интеграция всех патчей — отдельная ветка (за пределами скоупа). ILU0 zero pivot (BUG-002) — риск, но на тестовых сценариях с `true_impes_weights` не проявляется (CPR weights не используют LU). Если ILU0 abort возникнет при тестировании — это сигнал, не блокер FEAT-011
- ✅ **update_transfer (call site 3):** обновляет только weights (fpp), не App. При True-IMPES weights вычисляются из свежего блока — корректно

## Ветка amgcl для FEAT-011

### Стратегия

Ветка `experimental/true-impes` в форке amgcl, от `master` (upstream HEAD). Не от `experimental/master`, не от `experimental/patches`, не от `experimental/solvers`.

```
master (upstream HEAD, 28296c2)
├── experimental/patches   ← BUG-002 + FEAT-010 (ilu0 d=1, cpr pivot_threshold)
│   └── experimental/solvers  ← FEAT-010 (submodule HEAD сейчас)
└── experimental/true-impes  ← FEAT-011 (новая ветка)
```

### Принцип: одна ветка — один патч

Каждая ветка amgcl содержит ровно один логический патч поверх чистого upstream `master`. Никаких merge между ветками — `experimental/true-impes` не включает BUG-002, FEAT-010, и наоборот.

| Ветка | Патч | База |
|---|---|---|
| `experimental/patches` | BUG-002 (ilu0 d=1) + cpr d=1 | `master` |
| `experimental/solvers` | FEAT-010 (pivot_threshold) | `experimental/patches` |
| `experimental/true-impes` | **FEAT-011 (True-IMPES weights)** | `master` |

Интеграция всех патчей — **отдельная ветка** (за пределами скоупа FEAT-011). Там будет merge или cherry-pick BUG-002 + FEAT-010 + FEAT-011 в одну точку. До этого момента каждая ветка остаётся чистой.

### Почему от master

True-IMPES path **полностью заменяет** LU-факторизацию в `invert()`. Ему не нужны:
- BUG-002 (fallback `d=1` при `is_zero(d)`) — нет LU, нет pivot
- FEAT-010 (`pivot_threshold`) — нет LU, нет threshold

Чистая ветка от `master` упрощает потенциальный upstream PR и делает ревью тривиальным: diff показывает только True-IMPES код.

### Что отсутствует на experimental/true-impes

На `master` нет патчей BUG-002 и FEAT-010. Это означает:
- `ilu0.hpp` — оригинальный код, без fallback `d=1`. Zero pivot в ILU0 (SPrecond) → `assert` → abort в Debug
- `cpr.hpp` — `quasi_impes_weights` path содержит оригинальный `assert(!is_zero(d))`, без `pivot_threshold`

Для FEAT-011 это приемлемо: GDM переключается на `true_impes_weights` (нет LU в CPR weights). ILU0 zero pivot — существующий риск, адресованный BUG-002 в другой ветке. На этапе FEAT-011 тестируем с `true_impes_weights` — если ILU0 abort не возникает на тестовых сценариях, ветка валидна. Полная защита — после интеграции всех патчей.

### Submodule в GDM

При реализации FEAT-011 submodule переключается на ветку `experimental/true-impes`:

```powershell
cd amgcl
git checkout -b experimental/true-impes master
# ... внести изменения, commit ...
git push origin experimental/true-impes
cd ..
git add amgcl
# submodule pointer обновится
```

Ветки `experimental/patches` и `experimental/solvers` не трогаем — они остаются в форке для будущей интеграции.

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - `vault/GDM/knowledge/debugging/zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане.md`
   - `vault/GDM/knowledge/literature/Wallis 1983 Incomplete Gaussian Elimination as Preconditioning for CPR.md`
   - `vault/GDM/knowledge/literature/Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver.md`
   - `vault/GDM/knowledge/decisions/переход с блочного AMG на скалярный CPR в production.md`
3. Прочитай текущий `amgcl/amgcl/preconditioner/cpr.hpp` — строки 44–45 (template), 78–112 (params), 515–548 (invert), 257 + 445 + 505 (call sites)
4. Прочитай `HydroSolver/Solver/Math/SolverConfig.h` — три CPR using-а (строки 41, 49, 63)
5. Создай ветку GDM: `git checkout -b feat/feat-011/true-impes-weights experimental`
6. Начни с **шага 0** (подготовка submodule). Baseline фиксируется после шага 0
7. После каждого шага: сборка + тесты

---

## Этап 0: Подготовка — переключение submodule на master

### Шаг 0: Переключить amgcl submodule на master и удалить pivot_threshold

**Цель:** переключить amgcl submodule с `experimental/solvers` на ветку `experimental/true-impes` (от `master`). Удалить из `LinearProblem.cpp` установку `pivot_threshold` — поле не существует на `master`.

**Файлы:** `amgcl` (submodule), `HydroSolver/Solver/Math/LinearProblem.cpp`

**Контекст:**
Submodule сейчас указывает на `experimental/solvers`, где `cpr::params` содержит поле `pivot_threshold` (FEAT-010). На `master` этого поля нет. `LinearProblem.cpp:94-96` устанавливает `prm.precond.pivot_threshold = 1e-14` — без удаления этих строк проект не скомпилируется с submodule на master.

**Что сделать:**
1. Создать ветку amgcl от master: `cd amgcl && git checkout -b experimental/true-impes master && cd ..`
2. Удалить из `LinearProblem.cpp` строки 94–96 (установка `pivot_threshold`)

**Изменения:**

В `LinearProblem.cpp` — удалить:

До (строки 94–96):
```cpp
#if !defined(GDM_SOLVER_ILU0) && !defined(GDM_SOLVER_CPR_DRS)
			prm.precond.pivot_threshold = 1e-14;
#endif
```

После: (удалено целиком — поле не существует на master)

**Проверка после этого шага:**
- Сборка: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Это **baseline**: запомнить количество тестов (295) и время (~104 сек)
- Поведение: quasi-IMPES на master без `pivot_threshold` → `assert(!is_zero(d))` вместо threshold fallback. Если все 295 тестов зелёные — baseline валиден

**Подводные камни:**
- На `master` `invert()` содержит `assert(!math::is_zero(d))` вместо threshold fallback. Если тесты проходят — значит zero pivot не возникает на тестовых сценариях при quasi-IMPES. Если assert сработает — это не блокер FEAT-011 (после шага 3 quasi-IMPES path не вызывается), но baseline не зелёный. В этом случае: пропустить baseline тестирование quasi-IMPES, перейти к шагам 1–3, тестировать после переключения на True-IMPES

**Зависимости:** нет
**Оценка:** ~3 строки удалить, ~5 минут

---

## Этап 1: Ядро amgcl — tags + template parameter + dispatch

### Шаг 1: Добавить tags и третий template parameter

**Цель:** определить tags `quasi_impes_weights` / `true_impes_weights` и добавить третий template parameter с default = `quasi_impes_weights`.

**Файлы:** `amgcl/amgcl/preconditioner/cpr.hpp`

**Контекст:**
amgcl — header-only библиотека с pure compile-time polymorphism. Класс `cpr` (строка 45–46) принимает два template-параметра: `PPrecond` (AMG для давления) и `SPrecond` (ILU для полной системы). Добавление третьего параметра с default value — standard C++ pattern, backward compatible.

**Что сделать:**
1. Перед объявлением класса `cpr` (строка 45) добавить определения tag-структур
2. Добавить третий шаблонный параметр `class WeightsPolicy = quasi_impes_weights`

**Изменения:**

До (строки 44–46):
```cpp
template <class PPrecond, class SPrecond>
class cpr {
```

После:
```cpp
struct quasi_impes_weights {};
struct true_impes_weights {};

template <class PPrecond, class SPrecond, class WeightsPolicy = quasi_impes_weights>
class cpr {
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — должна пройти без ошибок/warnings
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — 295/295
- Поведение не изменилось: default = quasi_impes_weights, invert() не тронут

**Подводные камни:**
- Tag-структуры должны быть в namespace `amgcl::preconditioner` (перед `class cpr`, внутри namespace)
- Убедиться что forward declaration не нужна — tags определены до использования

**Зависимости:** нет
**Оценка:** ~5 строк, ~3 минуты

---

### Шаг 2: Разделить `invert()` на dispatch + две реализации

**Цель:** превратить `invert()` в dispatcher, вынести текущий код в `invert_impl(..., quasi_impes_weights)`, добавить `invert_impl(..., true_impes_weights)`.

**Файлы:** `amgcl/amgcl/preconditioner/cpr.hpp`

**Контекст:**
Текущий `invert()` (строки 515–548 на master) выполняет LU-факторизацию B×B блока с `assert(!is_zero(d))`. Он вызывается из трёх мест (строки 257, 445, 505 на master) и каждый раз получает транспонированный блок A и выходной массив y[B]. Ветка `experimental/true-impes` от `master` — без BUG-002/FEAT-010 патчей (они не нужны True-IMPES path).

Tag dispatch: `invert()` создаёт экземпляр `WeightsPolicy{}` и передаёт в перегруженную `invert_impl()`. Компилятор выберет нужную перегрузку в compile-time — zero runtime overhead.

Для True-IMPES при B=2: weights из nullspace столбца ∂F/∂Sw транспонированного блока:
- `A[1*B+0] = ∂Fo/∂Sw`, `A[1*B+1] = ∂Fw/∂Sw`
- `w = (∂Fw/∂Sw, −∂Fo/∂Sw)` с нормировкой max(|w|)
- Fallback w=(1,0) при обоих = 0

Для B>2 True-IMPES требует SVD/nullspace (за пределами скоупа GDM). Вместо молчаливого fallback — `static_assert` + `assert`. Два уровня защиты нужны потому что в amgcl существуют два path-а:
- **Static block path** (`value_type = static_matrix<N,N>`): `B` известен в compile-time → `static_assert(B == 2)`
- **Scalar path** (`value_type = double`, `B = prm.block_size`): `B` — runtime → `assert(B == 2)` (сработает в Debug)

GDM использует scalar path (`ScalarBackend = builtin<double>`), поэтому реальная защита — `assert`. `static_assert` страхует static block path (upstream PR в amgcl, другие проекты).

**Что сделать:**
1. Заменить текущий `invert()` на dispatcher
2. Перенести текущий код в `invert_impl(A, y, quasi_impes_weights)`
3. Добавить `invert_impl(A, y, true_impes_weights)` с True-IMPES формулой

**Изменения:**

До (строки 515–548 на master):
```cpp
        void invert(scalar_type *A, value_type_p *y)
        {
            const int B = math::static_rows<value_type>::value == 1 ? prm.block_size : math::static_rows<value_type>::value;

            // Perform LU-factorization of A in-place
            for(int k = 0; k < B; ++k) {
                scalar_type d = A[k*B+k];
                assert(!math::is_zero(d));
                for(int i = k+1; i < B; ++i) {
                    A[i*B+k] /= d;
                    for(int j = k+1; j < B; ++j)
                        A[i*B+j] -= A[i*B+k] * A[k*B+j];
                }
            }

            // Invert unit vector in-place.
            // Lower triangular solve:
            for(int i = 0; i < B; ++i) {
                value_type_p b = static_cast<value_type_p>(i == 0);
                for(int j = 0; j < i; ++j)
                    b -= A[i*B+j] * y[j];
                y[i] = b;
            }

            // Upper triangular solve:
            for(int i = B; i --> 0; ) {
                for(int j = i+1; j < B; ++j)
                    y[i] -= A[i*B+j] * y[j];
                y[i] /= A[i*B+i];
            }
        }
```

После:
```cpp
        void invert(scalar_type *A, value_type_p *y) {
            invert_impl(A, y, WeightsPolicy{});
        }

        void invert_impl(scalar_type *A, value_type_p *y, quasi_impes_weights)
        {
            const int B = math::static_rows<value_type>::value == 1 ? prm.block_size : math::static_rows<value_type>::value;

            for(int k = 0; k < B; ++k) {
                scalar_type d = A[k*B+k];
                assert(!math::is_zero(d));
                for(int i = k+1; i < B; ++i) {
                    A[i*B+k] /= d;
                    for(int j = k+1; j < B; ++j)
                        A[i*B+j] -= A[i*B+k] * A[k*B+j];
                }
            }

            for(int i = 0; i < B; ++i) {
                value_type_p b = static_cast<value_type_p>(i == 0);
                for(int j = 0; j < i; ++j)
                    b -= A[i*B+j] * y[j];
                y[i] = b;
            }

            for(int i = B; i --> 0; ) {
                for(int j = i+1; j < B; ++j)
                    y[i] -= A[i*B+j] * y[j];
                y[i] /= A[i*B+i];
            }
        }

        void invert_impl(scalar_type *A, value_type_p *y, true_impes_weights)
        {
            const int B = math::static_rows<value_type>::value == 1
                ? prm.block_size
                : math::static_rows<value_type>::value;

            // Compile-time check for static block path (value_type = static_matrix<N,N>)
            if constexpr (math::static_rows<value_type>::value != 1) {
                static_assert(math::static_rows<value_type>::value == 2,
                    "true_impes_weights requires block size 2 (two-phase system). "
                    "For B > 2, use quasi_impes_weights or implement ABF (Cao 2002).");
            }
            // Runtime check for scalar path (value_type = double, B = prm.block_size)
            assert(B == 2 && "true_impes_weights requires block_size == 2");

            scalar_type w0 =  A[1*B+1];
            scalar_type w1 = -A[1*B+0];
            scalar_type m = std::max(std::abs(w0), std::abs(w1));
            if (m > 0) { w0 /= m; w1 /= m; }
            else       { w0 = 1;  w1 = 0;  }
            y[0] = static_cast<value_type_p>(w0);
            y[1] = static_cast<value_type_p>(w1);
        }
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — без ошибок/warnings
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — 295/295
- Поведение не изменилось: default WeightsPolicy = quasi_impes_weights, dispatch вызывает тот же код

**Подводные камни:**
- `invert_impl(true_impes_weights)` при B≠2: `static_assert` (static block path) или `assert` (scalar path). Не компилируется / не проходит Debug
- Оба `invert_impl` — private методы класса `cpr`. Placement: после `invert()`, перед `operator<<`
- `static_cast<value_type_p>` нужен для совместимости типов (scalar_type может отличаться от value_type_p)

**Зависимости:** Требует: шаг 1
**Оценка:** ~45 строк (замена существующих ~45 строк + ~20 новых), ~10 минут

---

## Этап 2: Интеграция GDM — SolverConfig.h

### Шаг 3: Переключить CPR-конфигурации на true_impes_weights

**Цель:** добавить третий шаблонный параметр `true_impes_weights` во все три CPR using-объявления в SolverConfig.h.

**Файлы:** `HydroSolver/Solver/Math/SolverConfig.h`

**Контекст:**
В `SolverConfig.h` три CPR-конфигурации (CPR, CPR_BICGSTAB, CPR_SA) используют `cpr<PPrecond, SPrecond>`. После шага 1 третий параметр имеет default `quasi_impes_weights`, поэтому всё компилируется. Здесь переключаем на `true_impes_weights`.

CPR_DRS (cpr_drs) и ILU0 (без cpr) — не затронуты.

**Что сделать:**
1. В трёх using PrecondType для CPR конфигураций — добавить третий аргумент

**Изменения:**

До (строка 41–43):
```cpp
using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::smoothed_aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>
>;
```

После:
```cpp
using PrecondType = amgcl::preconditioner::cpr<
    amgcl::amg<ScalarBackend, amgcl::coarsening::smoothed_aggregation, amgcl::relaxation::ilu0>,
    amgcl::relaxation::as_preconditioner<ScalarBackend, amgcl::relaxation::ilu0>,
    amgcl::preconditioner::true_impes_weights
>;
```

Аналогично для CPR_BICGSTAB (строка 49) и CPR default (строка 63).

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — без ошибок/warnings
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — 295/295
- **Критично:** если тесты падают — это означает что True-IMPES weights дают другое качество CPR. Анализировать:
  - Какие тесты упали?
  - Какие GMRES-итерации (сравнить с baseline)?
  - Массовый баланс?
  - Если quality degrade — рассмотреть промежуточный шаг: оставить quasi_impes по умолчанию и добавить CMake-опцию для переключения

**Подводные камни:**
- True-IMPES weights **пропорциональны** quasi-IMPES для B=2 (w_true = (d, −c), w_quasi = (d, −c)/det). App отличается на row-dependent scalar factor. Число GMRES-итераций может измениться на ±1–3 — это нормально, не ошибка. Если тесты падают из-за divergence или mass balance > tolerance — тогда проблема в формуле
- Если число GMRES-итераций значительно увеличилось (>30% на каком-то сценарии), рассмотреть row-equilibration App перед AMG (деление каждой строки на max element)

**Зависимости:** Требует: шаги 0, 2
**Оценка:** ~9 строк (3 конфигурации × 1 строка + 3 строки comma), ~5 минут

---

## Этап 3: Тесты

### Шаг 4: Новый тест — Sw=0 (five-spot с чистой нефтью)

**Цель:** добавить интеграционный тест, который воспроизводит условие Sw=0 (полностью вырожденный Якобиан водной фазы). Этот тест должен работать с True-IMPES без обхода Sw_init=0.001.

**Файлы:** `tests/test_visual_verification.cpp`

**Контекст:**
Сейчас near-zero Sw тест (строка 265) использует `oil_saturation = 0.999` (Sw=0.001) как обходной путь для CPR. С True-IMPES weights вырожденный блок обрабатывается корректно, и Sw=0 должен работать.

Класс `SingleInjectorCase` (в `tests/simulation_cases/SingleInjectorCase.h`) уже используется в existing тесте. Нужно добавить аналогичный тест с `oil_saturation = 1.0` (Sw=0).

**Что сделать:**
1. Добавить новый TEST_CASE после существующего near-zero Sw теста (после строки ~280)
2. Настроить `sc.oil_saturation = 1.0` (Sw_init = 0.0)
3. Проверить: mass balance < 1e-3, Sw ∈ [0,1]

**Изменения:**

После существующего теста "Near-zero Sw" (строка 280) добавить:
```cpp
TEST_CASE("True-IMPES: Sw=0 single injector",
          "[true-impes][single-injector]")
{
    simulation_cases::SingleInjectorCase sc(41, 41);
    sc.oil_saturation = 1.0; // Sw_init = 0.0

    auto result = run_case(sc, true);

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    for (double s : result.Sw) {
        CHECK(s >= 0.0);
        CHECK(s <= 1.0);
    }
}
```

Struct `RunResult` (строка 59) содержит: `max_oil_balance_rel`, `max_water_balance_rel`, `Sw`, `P`. Нет поля `converged` — если simulation diverges, `run_case()` выбросит exception (abort/NaN).

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — 296/296
- Новый тест проходит: True-IMPES корректно обрабатывает Sw=0
- Если тест падает — диагностировать: GMRES diverges? NaN? Assertion? Какой блок вырождается?

**Подводные камни:**
- При Sw=0 полностью: вся строка Якобиана водной фазы нулевая (dFlux_water = 0, dMass_water = 0). True-IMPES weights: w₁ = ∂Fw/∂Sw = 0, w₂ = −∂Fo/∂Sw ≠ 0 → w = (0, −1/|∂Fo/∂Sw|). App = water equation для pressure. Это корректно, если water equation содержит ненулевые pressure-производные
- Если ВСЕ производные = 0 (и pressure, и saturation) → w = (0,0) → fallback (1,0) → App = oil equation для pressure. Это тоже корректно
- ILU0 fallback (ilu0.hpp patch d=1) всё ещё нужен — True-IMPES решает проблему CPR weights, но ILU0 factorization может ещё давать zero pivots
- `run_case(sc, true)` — второй параметр = export CSV. `RunResult` struct: `max_oil_balance_rel`, `max_water_balance_rel`, `Sw`, `P` (нет поля `converged` — divergence → exception)

**Зависимости:** Требует: шаг 3 (True-IMPES активирован)
**Оценка:** ~15 строк, ~5 минут

---

### Шаг 5: Визуальная верификация — CSV export для Sw=0 сценария

**Цель:** убедиться что новый тест генерирует CSV для визуальной проверки (Python animation). Проверить что `run_case(sc, true)` сохраняет snapshots.

**Файлы:** `tests/test_visual_verification.cpp`

**Контекст:**
Существующий near-zero Sw тест (строка 269) использует `run_case(sc, true)` — второй аргумент включает CSV export. Аналогичный вызов в шаге 4 уже обеспечивает CSV. Шаг 5 — верификация что CSV создался и данные разумны.

**Что сделать:**
1. После прогона теста из шага 4 — проверить наличие CSV в build/test_output/ (или аналогичной директории)
2. Визуально убедиться: Sw-фронт движется от инжектора, P разумное
3. Если CSV-инфраструктура отсутствует для нового теста — добавить необходимые вызовы

**Проверка после этого шага:**
- CSV-файлы генерируются для нового теста
- Анимация (если есть Python-скрипт) показывает корректное поведение

**Зависимости:** Требует: шаг 4
**Оценка:** ~0 строк (верификация), ~5 минут

---

## Этап 4: Финальная верификация

### Шаг 6: Release + Debug полный прогон

**Цель:** полная верификация обоих конфигов.

**Файлы:** нет изменений

**Что сделать:**
1. Release: `cmake --build build --config Release` + `ctest --test-dir build -C Release --output-on-failure`
2. Debug: `cmake --build build --config Debug` + `ctest --test-dir build -C Debug --output-on-failure`
3. Сравнить с baseline:
   - Тестов: 295 → 296+ (новые)
   - Время Release: ~104 сек — не должно ухудшиться (True-IMPES быстрее quasi-IMPES)
   - Debug vs Release: одинаковый результат (нет UB, нет uninit memory)
4. Проверить warnings: ноль новых warnings

**Проверка после этого шага:**
- Release: все тесты зелёные
- Debug: все тесты зелёные
- Время: ≤ baseline
- Warnings: 0 новых

**Зависимости:** Требует: шаги 0–5
**Оценка:** ~0 строк, ~15 минут (сборка + тесты)

---

## Критерии завершения

- [ ] Submodule amgcl на ветке `experimental/true-impes` (от `master`)
- [ ] `LinearProblem.cpp` — `pivot_threshold` удалён
- [ ] Tags `quasi_impes_weights` / `true_impes_weights` определены в `cpr.hpp`
- [ ] `cpr` template имеет третий параметр `WeightsPolicy` с default = `quasi_impes_weights`
- [ ] `invert()` dispatches через `WeightsPolicy{}`
- [ ] `invert_impl(true_impes_weights)` корректно вычисляет weights для B=2
- [ ] `invert_impl(true_impes_weights)` содержит `static_assert(B==2)` + `assert(B==2)`
- [ ] SolverConfig.h: три CPR конфигурации используют `true_impes_weights`
- [ ] Все существующие 295 тестов зелёные (backward compat через default template param)
- [ ] Новый тест Sw=0 зелёный
- [ ] Release + Debug: все тесты зелёные
- [ ] Время выполнения ≤ baseline
- [ ] Vault обновлён: статус FEAT-011
- [ ] GitHub issue #22 прокомментирован

## Тестовая стратегия

**Тест 1: Backward compatibility (implicit)**
**Тег:** все существующие теги
**Файл:** все существующие тестовые файлы
**Сценарий:** при `WeightsPolicy = quasi_impes_weights` (default) поведение идентично
**Ожидание:** 295/295 зелёные
**Предотвращает:** regression от изменений в cpr.hpp

**Тест 2: True-IMPES: Sw=0 single injector**
**Тег:** [true-impes][single-injector]
**Файл:** tests/test_visual_verification.cpp (дополнение)
**Сценарий:** SingleInjectorCase 41×41, oil_saturation=1.0 (Sw=0). Полностью вырожденный Якобиан водной фазы
**Setup:** 41×41 сетка, один инжектор, Sw_init = 0.0
**Ожидание:** max_oil_balance_rel < 1e-3, max_water_balance_rel < 1e-3, Sw ∈ [0,1]. Если simulation diverges — run_case() выбросит exception
**Предотвращает:** crash/NaN/diverge при полностью вырожденных блоках

**Тест 3: Visual verification**
**Файл:** CSV output от теста 2
**Сценарий:** Python animation показывает корректный Sw-фронт от инжектора
**Ожидание:** визуально корректное распространение насыщенности

## Связанные заметки

- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[Wallis 1983 Incomplete Gaussian Elimination as Preconditioning for CPR]]
- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]]
- [[переход с блочного AMG на скалярный CPR в production]]
- [[локальные патчи AMGCL для GDM]]
- [[feat-010 cpr-threshold-fallback]]
