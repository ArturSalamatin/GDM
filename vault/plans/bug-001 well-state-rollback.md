---
tags:
  - план
  - баг
date: 2026-06-28
issue: BUG-001
github: 1
branch: fix/bug-001/well-state-rollback
status: реализован
---

# BUG-001: SIGSEGV при закачке воды — out-of-bounds в AddFlowFieldSnapShot при ny=1

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[Newton divergence при закачке воды через скважину]]
   - [[стратегия тестирования GDM]]
3. Создай ветку: `git checkout -b fix/bug-001/well-state-rollback`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни количество тестов и время — это baseline (274 теста, ~277 сек)
7. Воспроизведи баг: `ctest --test-dir build -C Release -R "Well injection"`
8. Убедись, что тест падает с SIGSEGV (access violation), а не exception
9. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание бага

При добавлении нагнетательной скважины (закачка воды, `water_mass_rate < 0`) в одномерную сетку (Nx×1×1) симулятор падает с SIGSEGV после первого успешного временного шага.

**Важно:** ранее баг описывался как «Newton divergence / NaN в SetRefWellPressure». Это **неверный диагноз**. Ньютон сходится нормально (4 итерации), временной шаг принимается. Crash происходит в post-processing (`AddFlowFieldSnapShot`), а не в солвере.

**Критичность:** блокирует верификацию Buckley–Leverett с реальными скважинами и любые сценарии с `ny=1` или `nx=1`.

---

## Цепочка причинно-следственных связей

### Поток управления

```
Solve() [ReservoirSimulator.cpp:398-425]
  └─ while (currentMoment < target)
       ├─ PerformNewtonLoop(tau, nextMoment)  [строка 407]
       │    └─ Newton сходится за 4 итерации ← ОК
       ├─ if (success)
       │    ├─ MassBalance(tau)              [строка 412]
       │    ├─ Grid.AcceptState()            [строка 413]
       │    ├─ update_currentMoment()        [строка 414]
       │    └─ AddFlowFieldSnapShot()        [строка 415] ← CRASH
       ...
```

### Корневая причина

`AddFlowFieldSnapShot()` [ReservoirSimulator.cpp:616-628] вызывает `OverallFluxes()`, который возвращает `tuple<jOil_X, jOil_Y, j_X, j_Y>`. Затем итерирует по слоям:

```cpp
for (int k = 0; k < Grid.Nz(); k++)
{
    flowFields[k].add_snapshot(
        numPrm.CurrentTimeMoment(),
        std::move(j_X[k]),    // потоки через YZ-грани
        std::move(j_Y[k]));   // потоки через XZ-грани ← OUT-OF-BOUNDS при ny=1
}
```

`OverallFluxes()` [ReservoirSimulator.cpp:796-1050]:
- **Строка 804:** `if (ny > 1)` — блок заполнения `j_Y` и `jOil_Y` пропускается при `ny=1`
- **Строка 931:** `if (nx > 1)` — блок заполнения `j_X` и `jOil_X` пропускается при `nx=1`

При `ny=1`: `j_Y` остаётся пустым вектором (size=0). Обращение `j_Y[0]` → **out-of-bounds → SIGSEGV**.

Тест создаёт сетку `10×1×1` (`Nx=10, Ny=1, Nz=1`). Поэтому `j_X` заполнен (nx=10 > 1), но `j_Y` пуст.

### Почему ранее считали, что проблема в denom=0

Гипотеза «denom=0 в SetRefWellPressure» **математически опровергнута**:

Для модели Кори: `k_ro = (1-S)^3`, `k_rw = S^3`.
При `S ∈ [0, 1]`: `(1-S)^3 + S^3 = 1 - 3S(1-S) ≥ 1/4 > 0`.
Следовательно, `MobilityOverall > 0` всегда, и `denom = Σ factor[l] * MobilityOverall(l) > 0`.

Пороги `1e-15` и fallback'и в SetRefWellPressure **не нужны**.

### Связь с BUG-008

BUG-008 описывает «деление на ноль в SetRefWellPressure». По результатам анализа, при `Sw ∈ [0,1]` это невозможно. BUG-008 следует закрыть как invalid, либо переклассифицировать — проблема может проявиться только при `Sw < 0` (что означает баг в ApplyPhysicalConstraints, не в SetRefWellPressure).

### Связь с BUG-002

BUG-002 (CPR diverges при `Sw=0`) — отдельная проблема с zero pivot в ILU0. Не связана с текущим багом.

---

## Воспроизведение

**Тест:** `"Well injection: basic well works without crash"`
**Тег:** `[.buckley-leverett][.wells]` (скрытый — запускается только явно)
**Файл:** `tests/test_buckley_leverett.cpp:79-125`

```
ctest --test-dir build -C Release -R "Well injection"
```

**Ожидаемое поведение:** Solve завершается, `Sw[0] > Sw_initial` (вода закачана).
**Фактическое поведение:** SIGSEGV (access violation) в `AddFlowFieldSnapShot()` после первого успешного временного шага. Лог показывает нормальную сходимость Ньютона:
```
    1    1    4.09e-16    1
    2    1    4.83e-16    2
    3    1    2.39e-16    3
    4    1    3.31e-16    4
    5    0              1    4
>>> Just used cur_tau            = 1 days
>>> Current time: 1 days
<CRASH>
```

---

## Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 616-628 | `AddFlowFieldSnapShot`: безусловный доступ `j_Y[k]` |
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 796-1050 | `OverallFluxes`: `j_Y` не заполняется при `ny=1`, `j_X` — при `nx=1` |
| `HydroSolver/Reservoir/Well/Wells.cpp` | 42 | `throw(string)` вместо `throw std::runtime_error` (BUG-004) |
| `tests/test_buckley_leverett.cpp` | 79-125 | Тест-воспроизводитель |

---

## Варианты решения

### Вариант A: Заполнять пустые слои в OverallFluxes при nx=1 / ny=1

**Суть:** при `ny <= 1` (или `nx <= 1`) после основного блока `if (ny > 1)` добавить создание пустых слоёв `j_Y` / `jOil_Y` нужного размера (nz штук, каждый с пустыми sub-векторами). Это гарантирует, что `j_Y[k]` валиден для любого `k < nz`.

**Изменения:** `ReservoirSimulator.cpp` — 2 блока по ~5 строк (для Y и X).

**Плюсы:** фикс в одном месте, не требует изменения вызывающего кода. `add_snapshot` получает пустые данные (нет Y-границ → нет потоков — физически корректно).

**Минусы:** создаётся пустая структура, которая никогда не используется. Но overhead минимален.

**Риски:** нет. Пустые вектора передаются в `add_snapshot` → `FlowFieldSnapshot` хранит пустое vyField — это валидное состояние.

### Вариант B: Проверка в AddFlowFieldSnapShot

**Суть:** в `AddFlowFieldSnapShot` проверять `j_Y.size()` перед доступом. Если пусто — передавать пустой вектор.

**Изменения:** `ReservoirSimulator.cpp` — 3 строки.

**Плюсы:** минимальный diff.

**Минусы:** маскирует проблему в `OverallFluxes`. Если кто-то другой вызовет `OverallFluxes` напрямую — тот же crash.

### Выбор: Вариант A

Фикс в корне (`OverallFluxes`) надёжнее, чем проверка в каждом вызывающем месте.

---

## Чеклист подводных камней

- ✅ **Побочные эффекты:** `OverallFluxes` вызывается из `AddFlowFieldSnapShot` (строка 622) и `ReadFlowFieldFromFile` (строки 740-778). В `ReadFlowFieldFromFile` j_Y читается из файла, а не из `OverallFluxes` → не затрагивается. Других вызовов нет.
- ✅ **Потокобезопасность:** `OverallFluxes` — const метод, не в `#pragma omp parallel`. Безопасно.
- ✅ **Граничные случаи:** `nx=1, ny=1, nz=1` — все три пусты (j_X, j_Y). Проверим в тесте. `nx=1, ny=1, nz>1` — нужно nz пустых слоёв в j_X и j_Y.
- ✅ **Производительность:** создание пустых векторов — пренебрежимо. `OverallFluxes` вызывается 1 раз на временной шаг.
- ✅ **Обратная совместимость:** при `nx>1, ny>1` поведение не меняется (блок `if` выполняется).
- ✅ **Порядок вызовов:** нет зависимости от порядка инициализации.
- ✅ **Состояние при ошибке:** нет throw/early return.
- ✅ **Численная устойчивость:** нет вычислений, только структуры данных.
- ✅ **Связь с другими задачами:** не конфликтует ни с чем.
- ✅ **Зависимости сборки:** изменения только в .cpp, CMakeLists.txt не затрагивается.

---

## Этап 1: Минимальный фикс — out-of-bounds в OverallFluxes

### Шаг 1: Заполнить пустые j_Y / j_X при ny=1 / nx=1

**Цель:** гарантировать, что `OverallFluxes()` возвращает `j_X` и `j_Y` размера `nz` при любых `nx`, `ny`.

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
`OverallFluxes()` вычисляет потоки через границы ячеек. Потоки через XZ-грани (Y-направление) хранятся в `j_Y`, через YZ-грани (X-направление) — в `j_X`. Каждый имеет структуру `vector<vector<vector<double>>>` — слои × строки × грани.

При `ny <= 1` нет внутренних Y-границ → блок `if (ny > 1)` [строка 804] пропускается → `j_Y` остаётся пустым. Аналогично `j_X` при `nx <= 1`. Но `AddFlowFieldSnapShot()` ожидает `j_Y.size() >= nz` для доступа `j_Y[k]`.

**Что сделать:**
1. После блока `if (ny > 1) { ... }` (заканчивается ~строка 928) добавить `else`:
   - Заполнить `j_Y` и `jOil_Y` пустыми слоями: `nz` элементов, каждый — пустой `vector<vector<double>>`

2. После блока `if (nx > 1) { ... }` (заканчивается ~строка 1048) добавить `else`:
   - Заполнить `j_X` и `jOil_X` пустыми слоями

**Изменения (старый → новый код):**

**Место 1 — после блока `if (ny > 1) { ... }` (~строка 928):**

Найти закрывающую скобку блока `if (ny > 1)`. После неё добавить:

```cpp
else
{
    for (int k = 0; k < nz; k++)
    {
        jOil_Y.push_back(std::vector<std::vector<double>>());
        j_Y.push_back(std::vector<std::vector<double>>());
    }
}
```

**Место 2 — после блока `if (nx > 1) { ... }` (~строка 1048):**

Аналогично:

```cpp
else
{
    for (int k = 0; k < nz; k++)
    {
        jOil_X.push_back(std::vector<std::vector<double>>());
        j_X.push_back(std::vector<std::vector<double>>());
    }
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "Well injection"`
- Ожидаемый результат: тест зелёный (SIGSEGV устранён, Ньютон сходится нормально)
- Регрессия: `ctest --test-dir build -C Release` — все 274 теста зелёные

**Подводные камни:**
- Пустые `vector<vector<double>>` передаются в `add_snapshot` → `FlowFieldSnapshot` создаётся с пустым `vyField` (или `vxField`). Это валидно: при `ny=1` нет Y-границ, нет Y-потоков. `FlowFieldSnapshot::V()` использует интерполяцию на этих данных — при пустом поле вернёт 0. Для 1D задачи (Buckley–Leverett) это корректно.
- Для `ReadFlowFieldFromFile` [строки 740-778]: данные читаются из бинарного файла, этот путь не затрагивается.

**Зависимости:** нет
**Блокирует:** шаг 3
**Оценка:** ~16 строк, ~10 минут

---

### Шаг 2: Убрать throw(string) в AddWellToMatrix (попутный фикс BUG-004)

**Цель:** строка 42 в `Wells.cpp` выбрасывает `throw(string)` вместо `throw std::exception`. Этот exception не ловится `catch(std::exception&)` в тестах и в `ReservoirSimulator`. Заменить на `std::runtime_error`.

**Файлы:** `HydroSolver/Reservoir/Well/Wells.cpp`

**Контекст:**
В `AddWellToMatrix` есть две проверки на NaN. Первая (строка 28-29) выбрасывает `std::exception`. Вторая (строка 42) выбрасывает `std::string` — это BUG-004 из vault. При NaN во втором блоке exception утекает мимо catch → `std::terminate`.

**Что сделать:**
1. В `Wells.cpp`, строка 42: заменить `throw(...)` на `throw std::runtime_error(...)`

**Изменения (старый → новый код):**

До:
```cpp
throw("well production is not determined. Date:" + std::to_string(nextTimeMoment));
```

После:
```cpp
throw std::runtime_error("well production is not determined. Date:" + std::to_string(nextTimeMoment));
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Регрессия: `ctest --test-dir build -C Release` — все тесты зелёные

**Подводные камни:**
- `std::runtime_error` наследует `std::exception` → ловится во всех существующих catch-блоках.

**Зависимости:** нет (параллельно с шагом 1)
**Блокирует:** ничего
**Оценка:** 1 строка, ~5 минут

---

## Этап 2: Тесты и верификация

### Шаг 3: Снять скрытый тег с теста well injection

**Цель:** после фикса тест должен быть частью обычного набора. Убрать `.` из тегов и обновить код.

**Файлы:** `tests/test_buckley_leverett.cpp`

**Что сделать:**
1. Строка 80: заменить `"[.buckley-leverett][.wells]"` на `"[buckley-leverett][wells]"`
2. Строки 81-83: удалить комментарии про «известную проблему» (баг исправлен)
3. Строки 104-124: убрать try/catch обёртку — тест должен падать при exception, а не ловить его

**Изменения (старый → новый код):**

До:
```cpp
TEST_CASE("Well injection: basic well works without crash",
          "[.buckley-leverett][.wells]") {
    // Тег с точкой = скрытый тест, запускается только явно.
    // Известная проблема: Ньютон не сходится при закачке воды,
    // т.к. ReverseState не откатывает P_Well скважины → NaN propagation.
    constexpr size_t Nx = 10;
```

После:
```cpp
TEST_CASE("Well injection: basic well works without crash",
          "[buckley-leverett][wells]") {
    constexpr size_t Nx = 10;
```

Далее, убрать try/catch (строки 104-124):

До:
```cpp
    bool solve_ok = true;
    try {
        sim.Solve({0.0, 1.0});
    } catch (const std::exception& e) {
        INFO("Solve exception: " << e.what());
        solve_ok = false;
    }

    if (solve_ok) {
        auto Sw = sim.GetWaterSaturationField();
        CHECK(Sw[0] >= 0.2);
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-6);
            CHECK(Sw[i] <= 1.0 + 1e-6);
        }
    } else {
        WARN("Newton divergence with well injection — needs debugging");
    }
```

После:
```cpp
    sim.Solve({0.0, 1.0});

    auto Sw = sim.GetWaterSaturationField();
    CHECK(Sw[0] >= 0.2);
    for (size_t i = 0; i < Sw.size(); ++i) {
        CHECK(Sw[i] >= -1e-6);
        CHECK(Sw[i] <= 1.0 + 1e-6);
    }
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "Well injection"` — зелёный
- Регрессия: `ctest --test-dir build -C Release` — все тесты зелёные, **количество тестов увеличилось на 1** (ранее скрытый тест теперь в основном наборе)

**Зависимости:** шаг 1
**Блокирует:** шаг 4
**Оценка:** ~15 строк, ~10 минут

---

### Шаг 4: Дополнительный тест — 1D закачка с проверкой физики

**Цель:** убедиться, что после фикса симуляция даёт физически корректный результат (вода закачивается, фронт двигается).

**Файлы:** `tests/test_buckley_leverett.cpp` (новый TEST_CASE)

**Тест:**
```
**Тест:** "Well injection: Sw increases with water injection"
**Тег:** [buckley-leverett][wells]
**Файл:** tests/test_buckley_leverett.cpp (новый TEST_CASE)
**Сценарий:** 1D пласт (10×1×1), один инжектор, закачка воды 1000 кг/день, Solve 10 дней
**Setup:** Nx=10, L=500m, P_init=200atm, Sw_init=0.2, perm=100mD, poro=0.2
**Ожидание:** Sw[0] > 0.2 (ячейка с инжектором получает воду), все Sw ∈ [0,1]
**Предотвращает:** регрессию BUG-001
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "Well injection"` — оба теста зелёные
- Регрессия: `ctest --test-dir build -C Release`

**Зависимости:** шаг 3
**Блокирует:** шаг 5
**Оценка:** ~25 строк, ~15 минут

---

### Шаг 5: Регрессия на five-spot и полный набор

**Цель:** убедиться, что фикс не ломает существующие тесты с 5 скважинами и сетками nx>1, ny>1.

**Файлы:** нет изменений

**Что сделать:**
1. Запустить `ctest --test-dir build -C Release -R "Five-spot"` — все зелёные
2. Запустить полный набор: `ctest --test-dir build -C Release`
3. Сравнить количество тестов с baseline (должно увеличиться на 1-2)

**Проверка:**
- Ожидаемый результат: все тесты зелёные, Five-spot результаты идентичны baseline

**Зависимости:** шаг 4
**Блокирует:** шаг 6
**Оценка:** ~5 минут

---

## Этап 3: Очистка и vault

### Шаг 6: Обновить vault

**Цель:** зафиксировать результат в vault.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md` — BUG-001 статус, BUG-004 статус
- `vault/GDM/knowledge/debugging/Newton divergence при закачке воды через скважину.md` — обновить причину

**Что сделать:**
1. BUG-001: статус → `✅ ИСПРАВЛЕНО <дата>`
2. BUG-004: статус → `✅ ИСПРАВЛЕНО <дата>` (попутный фикс в шаге 2)
3. BUG-008: пересмотреть — `denom=0` невозможно при `Sw ∈ [0,1]`, предложить закрыть как invalid или переклассифицировать
4. Vault-заметка: исправить причину с «NaN в SetRefWellPressure» на «SIGSEGV из-за out-of-bounds j_Y[k] при ny=1 в OverallFluxes»

**Зависимости:** шаг 5
**Оценка:** ~10 минут

---

## Критерии завершения

- [ ] Тест-воспроизводитель `"Well injection: basic well works without crash"` зелёный
- [ ] Новый тест `"Well injection: Sw increases with water injection"` зелёный
- [ ] Все существующие тесты зелёные (`ctest --test-dir build -C Release`)
- [ ] Five-spot тесты зелёные и результаты идентичны baseline
- [ ] Визуальная верификация: CSV с фронтом вытеснения от инжектора (если возможно)
- [ ] Скрытые теги `[.]` сняты с well injection тестов
- [ ] BUG-001, BUG-004 закрыты в vault
- [ ] BUG-008 переклассифицирован
- [ ] Vault-заметка обновлена с корректной причиной
- [ ] GitHub issue #1 прокомментирован с результатом

---

## Обнаруженные проблемы

### ПРОБЛЕМА 1: Исходный диагноз BUG-001 был неверен

Исходная гипотеза («denom=0 в SetRefWellPressure → NaN → Newton divergence») опровергнута:
- Математически: `(1-S)^3 + S^3 > 0` при `S ∈ [0,1]` → `MobilityOverall > 0` всегда → `denom > 0`
- Экспериментально: Newton сходится нормально (4 итерации), crash — SIGSEGV после `AcceptState`

Реальная причина: out-of-bounds доступ к пустому вектору `j_Y` в `AddFlowFieldSnapShot`.

### ПРОБЛЕМА 2: BUG-008 (denom=0 в SetRefWellPressure) — invalid?

BUG-008 описывает «деление на ноль в SetRefWellPressure». Поскольку `denom > 0` математически гарантировано при `Sw ∈ [0,1]`, этот баг не может реализоваться при нормальных условиях. Возможные варианты:
- Закрыть как invalid
- Переклассифицировать: проблема может возникнуть при `Sw < 0` (что само по себе баг в ApplyPhysicalConstraints)

### ПРОБЛЕМА 3: BUG-007 — деление на `M_n + M_c` в fillMatrixBlockRow

Строка 579 в ReservoirSimulator.cpp: `denom = OverallMobilityCell + OverallMobilityNeighbour`. При `Sw=0` на одной ячейке и `Sw=1` на соседней это **не** ноль (доказано выше). Но при `Sw < 0` (теоретически невозможно при `ApplyPhysicalConstraints`) — может быть проблемой. Уже зарегистрировано как BUG-007.

---

## Архитектурное примечание: Save/Restore скважин

Ранее план предлагал добавить `SaveWellState`/`ReverseWellState`/`AcceptWellState` как часть фикса BUG-001. Эта работа **не нужна для фикса BUG-001**, но остаётся ценной как DEBT:
- `UpdateWellState()` пересоздаёт всё состояние скважины с нуля → текущий код корректен
- Но при расширении модели (BHP-режим, кэшированные производные) отсутствие отката создаст проблемы

Рекомендуется зарегистрировать как DEBT-NNN отдельно.
