---
tags:
  - план
  - баг
date: 2026-06-29
issue: BUG-002
github: 4
branch: fix/bug-002/cpr-zero-pivot
status: реализован
---

# BUG-002: CPR zero pivot — abort() в cpr.hpp:522 при LU-факторизации блока

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
3. Создай ветку: `git checkout -b fix/bug-002/cpr-zero-pivot experimental`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни количество тестов и время — это baseline (279 тестов, 278 passed, 1 failed)
7. Воспроизведи баг: `ctest --test-dir build -C Release -R "Grid convergence: single injector"`
8. Убедись, что тест падает с `Assertion failed: !math::is_zero(d), file .../cpr.hpp, line 522`
9. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание бага

При запуске теста `Grid convergence: single injector` на сетке 41×41 CPR-прекондиционер (Constrained Pressure Residual) падает с `abort()` из-за zero pivot при LU-факторизации 2×2 блока Якобиана.

Тест проходит на сетках 11×11 и 21×21, но на 41×41 после ~200 дней модельного времени один из диагональных 2×2 блоков становится (near-)singular → LU-факторизация получает нулевой pivot → `assert(!math::is_zero(d))` → `abort()`.

**Критичность:** блокирует зелёный CI (278/279 тестов). Блокирует merge ветки `fix/bug-001/well-state-rollback`.

---

## Цепочка причинно-следственных связей

### Поток управления

```
ReservoirSimulator::Solve()
  └─ PerformNewtonLoop()
       └─ SingleIteration()
            └─ LinearProblem::Solve()
                 └─ CPRSolver::operator()       ← создание CPR
                      └─ cpr::cpr()             ← конструктор
                           └─ setup_preconditioner()
                                └─ invert(v.data(), ...)   [cpr.hpp:257]
                                     └─ LU-факторизация B×B блока
                                          └─ assert(!math::is_zero(d))  [cpr.hpp:522]
                                               └─ abort()  ← CRASH
```

### Корневая причина

Функция `invert()` в `cpr.hpp:515-545` выполняет in-place LU-разложение B×B блока (B=2 для двухфазной задачи). Входная матрица — диагональный блок Якобиана (транспонированный):

```
A = | dF_oil/dSw    dF_water/dSw  |^T
    | dF_oil/dP     dF_water/dP   |
```

При определённых распределениях насыщенности на тонкой сетке (41×41) один из pivot'ов обнуляется при LU elimination. Это не обязательно означает Sw=0 — ILU0 producing zeros через elimination может обнулить pivot даже при Sw=0.2 на тонких сетках (больше fill-in paths).

### Три проявления BUG-002

| # | Место | Файл:строка | Покрытие |
|---|---|---|---|
| 1 | ILU0 factorization | `ilu0.hpp:156` | ✅ Патч (fallback `D[i]=1`) |
| 2 | CPR block inversion при Sw=0 | `cpr.hpp:257` (через `invert`) | ✅ Обход (Sw_init ≥ 0.001) |
| 3 | CPR block-LU при тонкой сетке | `cpr.hpp:522` (внутри `invert`) | ❌ Не покрыто |

Текущий план фиксит **проявление 3**.

### Связь с BUG-007

BUG-007 описывает деление на ноль в гармоническом среднем подвижности (`ReservoirSimulator.cpp:579`). Та же корневая причина (нулевая подвижность), но другой путь propagation. Текущий фикс не адресует BUG-007.

---

## Воспроизведение

**Тест:** `"Grid convergence: single injector"`
**Тег:** `[convergence][single-injector]`
**Файл:** `tests/test_visual_verification.cpp:181-240`

```
ctest --test-dir build -C Release -R "Grid convergence: single injector"
```

**Ожидаемое поведение:** тест проходит для всех трёх сеток (11×11, 21×21, 41×41), grid convergence подтверждён.

**Фактическое поведение:** на сетке 41×41 после ~200 дней модельного времени — `Assertion failed: !math::is_zero(d), file .../cpr.hpp, line 522` → `abort()`. Ньютон сходится нормально на предыдущих шагах, проблема возникает при создании CPR-прекондиционера для очередного solve.

---

## Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `HydroSolver/AMGSolver/amgcl/amgcl/preconditioner/cpr.hpp` | 515-545 | `invert()` — LU-факторизация B×B блока, assert на строке 522 |
| `HydroSolver/AMGSolver/amgcl/amgcl/preconditioner/cpr.hpp` | 246-257 | Вызов `invert()` для диагонального блока при setup |
| `HydroSolver/AMGSolver/amgcl/amgcl/relaxation/ilu0.hpp` | 156-157 | Существующий патч (проявление 1) — образец для фикса |
| `tests/test_visual_verification.cpp` | 181-240 | Тест-воспроизводитель |

---

## Варианты решения

### Вариант A: Fallback в `cpr.hpp:522` (аналогично патчу ilu0)

**Суть:** заменить `assert(!math::is_zero(d))` на fallback `if (math::is_zero(d)) d = 1.0`.

**Изменения:** `cpr.hpp:522` — 1 строка.

```cpp
// было:
assert(!math::is_zero(d));
// стало:
if (math::is_zero(d)) d = static_cast<scalar_type>(1);
```

**Плюсы:**
- Минимальный diff (1 строка), аналогичен существующему патчу ilu0
- `d = 1` → identity для вырожденной строки → прекондиционер «не помогает» для этого блока, но не ломает GMRES
- GMRES всё равно скорректирует — прекондиционер влияет на скорость сходимости, не на решение
- Проверенный подход: патч ilu0 с тем же паттерном работает стабильно уже с момента перехода на CPR

**Минусы:**
- Второй патч в amgcl (уже есть DEBT-007 на вынос патчей)
- Маскирует проблему — прекондиционер менее эффективен на вырожденных блоках (больше GMRES-итераций)

**Риски:** практически нет. `d = 1` делает строку LU-разложения тривиальной. Результат invert → первый столбец «обратной» матрицы используется как weights для pressure restrictor. При `d = 1` weight будет неоптимальным, но не NaN и не infinity.

### Вариант B: Регуляризация блоков Якобиана перед CPR

**Суть:** в `LinearProblem::Solve()` перед созданием CPRSolver — regularization pass: для Sw-строк с `|diag| < ε` ставить `diag = ε`.

**Изменения:** `LinearProblem.cpp` или `ReservoirSimulator.cpp` — ~10-15 строк.

**Плюсы:**
- Решает проблему на уровне матрицы, не в библиотеке
- Не требует патчей amgcl

**Минусы:**
- Нужно выбрать ε — зависит от масштаба задачи
- Изменяет матрицу Якобиана → может повлиять на точность Newton-итераций
- Не защищает от future edge cases в других частях CPR
- Сложнее верифицировать корректность

### Выбор: Вариант A

Минимальный, проверенный подход. Аналог существующего патча ilu0. Не изменяет Якобиан. Риск нулевой. DEBT-007 уже отслеживает необходимость выноса патчей amgcl.

---

## Чеклист подводных камней

- ✅ **Побочные эффекты:** `invert()` вызывается из 3 мест (строки 257, 445, 505). Fallback `d = 1` безопасен во всех трёх: результат — неоптимальные weights/diagonal, но не NaN/infinity.
- ✅ **Потокобезопасность:** CPR конструируется внутри `LinearProblem::Solve()`, однопоточно. Нет `#pragma omp parallel`.
- ✅ **Граничные случаи:** `d = 0` → fallback `d = 1`. `d = 1e-300` (not zero but tiny) → `math::is_zero` проверяет exact zero, не near-zero → пройдёт. Для near-zero значений деление `A[i*B+k] /= d` может дать большие числа, но это не abort — GMRES обработает.
- ✅ **Производительность:** один `if` в цикле из 2 итераций (B=2). Пренебрежимо.
- ✅ **Обратная совместимость:** при non-zero pivot поведение не меняется (`if` не выполняется).
- ✅ **Порядок вызовов:** нет зависимости.
- ✅ **Состояние при ошибке:** нет throw/early return.
- ✅ **Численная устойчивость:** `d = 1` → строка LU получает identity → weight ≈ 0 → pressure restrictor не учитывает этот блок. Физически: ячейка временно «отключена» от pressure solve. GMRES компенсирует через global relaxation (ILU0 smoother).
- ✅ **Связь с другими задачами:** DEBT-007 (вынос патчей) — добавляется второй патч, не конфликтует. BUG-001 — разблокируется merge.
- ✅ **Зависимости сборки:** header-only, CMakeLists.txt не затрагивается.

---

## Этап 1: Минимальный фикс — fallback в cpr.hpp:522

### Шаг 1: Заменить assert на fallback в invert()

**Цель:** устранить `abort()` при zero pivot в LU-факторизации B×B блока внутри CPR.

**Файлы:** `HydroSolver/AMGSolver/amgcl/amgcl/preconditioner/cpr.hpp`

**Контекст:**
Функция `invert()` (строки 515-545) выполняет in-place LU-разложение B×B блока (B=2 для двухфазного Якобиана). На строке 522 стоит `assert(!math::is_zero(d))` — если pivot нулевой, вызывается `abort()`. Это происходит на тонкой сетке (41×41) при определённых распределениях насыщенности.

В `ilu0.hpp:156` уже есть аналогичный патч: `if (math::is_zero((*D)[i])) (*D)[i] = static_cast<value_type>(1)` — fallback для zero pivot в ILU0. Применяем тот же подход к CPR block-LU.

При `d = 1` строка LU-разложения становится тривиальной (identity). Результат `invert()` используется как weights для pressure restrictor в CPR. Неоптимальный weight → больше GMRES-итераций для этого шага, но не NaN, не divergence.

**Что сделать:**
1. В файле `cpr.hpp`, строка 522: заменить `assert` на fallback

**Изменения (старый → новый код):**

До:
```cpp
            // Perform LU-factorization of A in-place
            for(int k = 0; k < B; ++k) {
                scalar_type d = A[k*B+k];
                assert(!math::is_zero(d));
                for(int i = k+1; i < B; ++i) {
```

После:
```cpp
            // Perform LU-factorization of A in-place
            for(int k = 0; k < B; ++k) {
                scalar_type d = A[k*B+k];
                if (math::is_zero(d)) d = static_cast<scalar_type>(1);
                A[k*B+k] = d;
                for(int i = k+1; i < B; ++i) {
```

**Важно:** нужно записать `d` обратно в `A[k*B+k]`, потому что `d` — локальная копия. Без `A[k*B+k] = d` LU-разложение продолжит использовать исходный нулевой элемент для обратного хода (строки 539-544: `y[i] /= A[i*B+i]`). Это отличие от ilu0-патча, где `(*D)[i]` — ссылка, а не копия.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "Grid convergence: single injector"`
- Ожидаемый результат: тест зелёный (все три сетки 11, 21, 41 проходят)
- Регрессия: `ctest --test-dir build -C Release` — 279/279 тестов зелёные

**Подводные камни:**
- `d` — локальная переменная, а не ссылка. Строка 543 (`y[i] /= A[i*B+i]`) читает из массива, не из `d`. Поэтому нужно записать `d` обратно: `A[k*B+k] = d`. Без этого деление на ноль в обратном ходе.
- `static_cast<scalar_type>(1)` — тип `scalar_type` определяется шаблоном CPR. В нашем случае это `double`. Cast явный для совместимости с другими instantiation.

**Зависимости:** нет
**Блокирует:** шаг 2
**Оценка:** 2 строки, ~5 минут

---

## Этап 2: Верификация

### Шаг 2: Полный набор тестов Release + Debug

**Цель:** убедиться, что fallback не вызвал регрессии.

**Файлы:** нет изменений

**Что сделать:**
1. Полная сборка и тесты Release:
   ```powershell
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure
   ```
2. Полная сборка и тесты Debug:
   ```powershell
   cmake --build build --config Debug
   ctest --test-dir build -C Debug --output-on-failure
   ```
3. Сравнить с baseline:
   - Release: 279/279 (baseline: 278/279, +1 из-за фикса Grid convergence)
   - Debug: 279/279 (baseline: 278/279)
   - Warnings: не должно быть новых

**Проверка:**
- Release: 279/279 passed, 0 failed
- Debug: 279/279 passed, 0 failed
- Grid convergence результаты (Sw_probe, P_probe) совпадают с ожидаемыми — убедиться, что fallback не испортил точность convergence test

**Зависимости:** шаг 1
**Блокирует:** шаг 3
**Оценка:** ~15 минут (сборка + тесты)

---

### Шаг 3: Проверка точности Grid convergence

**Цель:** убедиться, что fallback `d = 1` не испортил точность решения. Если прекондиционер «пропускает» вырожденный блок (identity weight), это может увеличить число GMRES-итераций, но не должно изменить решение.

**Файлы:** нет изменений

**Что сделать:**
1. Проверить CSV-выход теста Grid convergence (файл `results/convergence/convergence.csv`):
   - `Sw_probe` и `P_probe_atm` для каждой сетки
   - Разности между сетками должны уменьшаться (grid convergence)
2. Тест уже проверяет `dSw_fine < dSw_coarse` и `dP_fine < dP_coarse` — если тест зелёный, convergence подтверждён
3. Проверить лог Newton-итераций — нет ли аномального увеличения числа GMRES-итераций на 41×41

**Проверка:**
- CSV: `dSw_fine < dSw_coarse`, `dP_fine < dP_coarse`
- Newton: сходимость за разумное число итераций (не > 15 на шаг)
- Массовый баланс: `max_oil_balance_rel < 1e-3`, `max_water_balance_rel < 1e-3`

**Зависимости:** шаг 2
**Блокирует:** шаг 4
**Оценка:** ~5 минут

---

## Этап 3: Vault и документация

### Шаг 4: Обновить vault

**Цель:** зафиксировать результат фикса в vault.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md` — BUG-002 статус, DEBT-007 обновить
- `vault/GDM/knowledge/debugging/zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане.md` — добавить решение

**Что сделать:**
1. BUG-002: статус → `✅ ИСПРАВЛЕНО <дата>` (все три проявления покрыты)
2. DEBT-007: в описании локальных изменений добавить: «два файла — `ilu0.hpp:156-157` и `cpr.hpp:522-523`»
3. Debugging-заметка: в раздел «Проявление 3» добавить решение (аналогично секции «Решение» для проявлений 1-2)

**Зависимости:** шаг 3
**Оценка:** ~10 минут

---

## Критерии завершения

- [ ] Тест `Grid convergence: single injector` зелёный в Release
- [ ] Тест `Grid convergence: single injector` зелёный в Debug
- [ ] Все 279 тестов зелёные в Release (279/279)
- [ ] Все 279 тестов зелёные в Debug (279/279)
- [ ] Grid convergence: `dSw_fine < dSw_coarse`, `dP_fine < dP_coarse`
- [ ] Массовый баланс: `< 1e-3` для всех сеток
- [ ] BUG-002 закрыт в vault
- [ ] DEBT-007 обновлён (второй патч)
- [ ] Debugging-заметка обновлена
- [ ] GitHub issue #4 прокомментирован с результатом

---

## Обнаруженные проблемы

### Проблема: assert в cpr.hpp активен в Release

Стандартный `<cassert>` включён в `cpr.hpp:36`. CMake определяет `/DNDEBUG` для Release (`CMAKE_CXX_FLAGS_RELEASE`), что должно выключать `assert()`. Однако в Release конфиге тест падает с `Assertion failed` — это может указывать на:
1. Смешанные объектные файлы Debug/Release в build directory (из-за переключения веток/конфигов)
2. Или: `NDEBUG` не доходит до translation unit, включающего `cpr.hpp`

При `/implement` первым делом — полная пересборка (`--target Rebuild`) чтобы исключить вариант 1. Если после полной пересборки Release assert не срабатывает — проблема была в грязном build dir, и fallback всё равно нужен для Debug.

Если assert срабатывает и после полной пересборки Release — нужно исследовать, почему NDEBUG не действует на cpr.hpp. Возможные причины:
- precompiled header (`stdafx.h`) определяет `#undef NDEBUG`
- translation unit компилируется без `/DNDEBUG`

В любом случае, замена assert на fallback решает проблему в обоих конфигах.

### Проблема: near-zero pivot не адресуется уровнем 1

Текущий патч проверяет `math::is_zero(d)` — exact zero comparison. Если `d = 1e-15` (near-zero, но не zero):
- assert не срабатывает, fallback не срабатывает
- `A[i*B+k] /= d` → элементы L ~ 1e+15 → overflow
- Обратный ход: `y[i] /= A[i*B+i]` → weights y ≈ (0, 0) → строка App ≈ 0
- AMG получает near-singular pressure matrix → плохая сходимость или divergence

Это **хуже**, чем exact zero с fallback `d = 1` (weights = (1,0) → берём Kpp напрямую).

Решение — FEAT-010 (threshold-based fallback, `|d| < τ·||A||`). Уровень 1 (этот план) фиксит только crash от exact zero. Уровень 2 (FEAT-010) покроет near-zero. Уровень 3 (FEAT-011) устранит проблему принципиально через True-IMPES / ABF weights.

### Зарегистрированные follow-up задачи

- **FEAT-010:** Threshold-based fallback в CPR block-LU (уровень 2) — покрывает near-zero pivots
- **FEAT-011:** True-IMPES / ABF weights для CPR decoupling (уровень 3) — корректное решение, PR в upstream amgcl
