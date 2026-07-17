---
tags:
  - план
  - рефакторинг
date: 2026-07-17
issue: DEBT-039
github: 35
branch: refactor/debt-039/amg-iter-count-type
status: в процессе
audit:
  date: 2026-07-17
  round: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  note: раунд 3 — полный повторный аудит. Номера строк верифицированы по коду на диске. Мысленный прогон теста шаг-за-шагом (все 6 вызовов update_currentAMGState) — CHECK-и корректны. Нет конфликтующих веток. Нет workaround-ов
---

# DEBT-039: `double AMG_maxSolverIterCount` → `int` + дробный аккумулятор

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-039/amg-iter-count-type`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Baseline: 312 тестов, все зелёные
6. Начни с шага 1. После каждого шага: сборка + тесты

---

## Текущее состояние

`AMG_maxSolverIterCount` — лимит итераций AMG-солвера. Семантически целое число, но хранится как `double` для «плавной» адаптации: при каждой успешной итерации Ньютона (ошибка уменьшилась) лимит растёт на 0.4 (каждые ~3 итерации — на 1). Геттер `CurrentAMG_maxSolverIterationCount()` округляет через `static_cast<size_t>(round(...))`.

```cpp
// NumericalParameters.h:21
static constexpr size_t AMG_MAXSOLVERITERCOUNT = 45;

// NumericalParameters.h:30
double AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT;

// NumericalParameters.h:67
size_t CurrentAMG_maxSolverIterationCount() const {
    return static_cast<size_t>(round(AMG_maxSolverIterCount));
}

// NumericalParameters.h:96
void set_currentAMG_maxSolverIterationCount() {
    AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT;
}

// NumericalParameters.cpp:22
AMG_maxSolverIterCount += 0.4;

// NumericalParameters.cpp:29
AMG_maxSolverIterCount = std::max(15.0, AMG_maxSolverIterCount);

// NumericalParameters.cpp:34
AMG_maxSolverIterCount = std::min(AMG_maxSolverIterCount, 45.0);
```

Проблемы:
1. Если `AMG_maxSolverIterCount` станет отрицательным (закомментированный `AMG_maxSolverIterCount -= 3` на строке 26), `round()` вернёт отрицательное → `static_cast<size_t>()` → огромное положительное число → AMG-солвер итерирует «бесконечно»
2. `double` для целочисленного лимита — несоответствие типа и семантики
3. `AMG_MAXSOLVERITERCOUNT` (`size_t`) присваивается в `double` поле — неявное сужение

### Вызывающие `CurrentAMG_maxSolverIterationCount()`

| Файл | Строка | Контекст |
|---|---|---|
| `HydroSolver/Reservoir/NewtonSolver.cpp` | 53 | `problem.Solve(numPrm.CurrentAMG_maxSolverIterationCount())` — production |
| `examples/ex_benchmark_series_cpr.cpp` | 216, 358 | benchmark solve loops |
| `examples/ex_benchmark_series_ts.cpp` | 167 | benchmark solve loop |
| `tests/test_amgcl_benchmark.cpp` | 227, 369 | benchmark solve loops |

### Вызывающие `set_currentAMG_maxSolverIterationCount()` (reset)

| Файл | Строка | Контекст |
|---|---|---|
| `HydroSolver/Reservoir/NewtonSolver.cpp` | 20 | reset перед Newton loop |
| `examples/ex_benchmark_series_cpr.cpp` | 206, 348 | reset перед Newton loop |
| `examples/ex_benchmark_series_ts.cpp` | 157 | reset перед Newton loop |
| `tests/test_amgcl_benchmark.cpp` | 217, 359 | reset перед Newton loop |

### Вызывающие `update_currentAMGState()` (адаптация)

| Файл | Строка | Контекст |
|---|---|---|
| `HydroSolver/Reservoir/NewtonSolver.cpp` | 54 | после `problem.Solve()` |
| `examples/ex_benchmark_series_cpr.cpp` | 218, 360 | после solve |
| `examples/ex_benchmark_series_ts.cpp` | 169 | после solve |
| `tests/test_amgcl_benchmark.cpp` | 229, 371 | после solve |

---

## Целевое состояние

```cpp
// NumericalParameters.h
static constexpr int AMG_MAXSOLVERITERCOUNT = 45;

int AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT;
double AMG_maxSolverIterAccum = 0.0;

size_t CurrentAMG_maxSolverIterationCount() const {
    return static_cast<size_t>(AMG_maxSolverIterCount);
}

void set_currentAMG_maxSolverIterationCount() {
    AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT;
    AMG_maxSolverIterAccum = 0.0;
}

// NumericalParameters.cpp (update_currentAMGState)
if (newAMG_error < CurrentAMG_Error()) {
    AMG_maxSolverIterAccum += 0.4;
    if (AMG_maxSolverIterAccum >= 1.0) {
        AMG_maxSolverIterCount += 1;
        AMG_maxSolverIterAccum -= 1.0;
    }
}

if (newAMG_error > 0.7)
    AMG_maxSolverIterCount = std::max(15, AMG_maxSolverIterCount);

AMG_maxSolverIterCount = std::min(AMG_maxSolverIterCount, AMG_MAXSOLVERITERCOUNT);
```

---

## Варианты решения

### Вариант A (рекомендуемый): `double` → `int`, адаптация через дробный аккумулятор

- **Суть:** `int AMG_maxSolverIterCount` + `double AMG_maxSolverIterAccum`, аккумулятор копит дробные приращения, при достижении 1.0 инкрементирует целочисленный лимит
- **Изменения:** `NumericalParameters.h` (3 правки: поле, геттер, сеттер + новое поле), `NumericalParameters.cpp` (3 правки в `update_currentAMGState`)
- **Плюсы:** тип соответствует семантике, `static_cast<size_t>` из неотрицательного `int` безопасен (clamp 15..45 гарантирует), адаптация эквивалентна (те же пороги, тот же темп роста ~1 за 3 итерации)
- **Минусы:** добавляется одно поле (`double accum`), чуть больше кода
- **Риски:** нет — поведение математически эквивалентно
- **Трудоёмкость:** 2 файла, ~15 строк

### Вариант B: оставить `double`, защитить геттер

- **Суть:** clamp в геттере: `std::max(1, static_cast<int>(round(...)))`
- **Изменения:** только `NumericalParameters.h:67`
- **Плюсы:** минимальный diff
- **Минусы:** не решает корневую проблему — тип остаётся `double` для целочисленной величины, `std::max/min` с `double` литералами остаются
- **Трудоёмкость:** 1 файл, ~2 строки

### Вариант C: `int`, адаптацию `+= 1` вместо `+= 0.4`

- **Суть:** простой `int`, каждое успешное приращение +1
- **Плюсы:** простейший код
- **Минусы:** меняет поведение — лимит растёт в 2.5× быстрее, что может привести к избыточному числу AMG-итераций на ранних Newton-шагах (лимит быстро упирается в 45)
- **Трудоёмкость:** 2 файла, ~8 строк

### Выбор: Вариант A

Вариант B — полумера, не решает корневую проблему типизации. Вариант C меняет поведение адаптации. Вариант A — минимальный diff, полностью корректный тип, эквивалентное поведение.

---

## Поиск подводных камней

- ✅ **Все call sites найдены:** геттер — 5 call sites (1 production, 4 benchmarks/tests). Сеттер (reset) — 5 call sites. `update_currentAMGState` — 5 call sites. Все используют публичный API → интерфейс не меняется
- ✅ **Потокобезопасность:** `update_currentAMGState` вызывается последовательно (внутри Newton loop, без OpenMP). `set_currentAMG_maxSolverIterationCount` — аналогично. Grep по `#pragma omp` в NumericalParameters.cpp — нет
- ✅ **Зависимости сборки:** только `NumericalParameters.h/.cpp`, CMake не затрагивается
- ✅ **Обратная совместимость API:** сигнатуры `CurrentAMG_maxSolverIterationCount()`, `set_currentAMG_maxSolverIterationCount()`, `update_currentAMGState()` не меняются
- ✅ **Тесты:** `test_NumericalParameters.cpp` покрывает конструкторы, tau, PI-контроллер, Newton-счётчик — но НЕ покрывает AMG-адаптацию. Тест `update_currentAMGState` будет добавлен (шаг 3)
- ✅ **Связь с другими задачами:** DEBT-058 (общая типизация int/size_t) — DEBT-039 закрывает один из пунктов. Не конфликтует с DEBT-026 (другой модуль) или DEBT-037 (virtual/override). Нет активных веток, затрагивающих NumericalParameters
- ✅ **Производительность:** `int` + одно сравнение вместо `round()` — если что-то и изменится, станет быстрее. Код в Newton loop (не в hot assembly), влияние пренебрежимо
- ✅ **Константа `AMG_MAXSOLVERITERCOUNT`:** меняется с `size_t` на `int`. Используется только: инициализатор поля, `set_currentAMG_maxSolverIterationCount()`, верхний clamp в `update_currentAMGState`. Все три — внутри класса
- ✅ **Закомментированный `AMG_maxSolverIterCount -= 3`:** если когда-нибудь раскомментируют, `int -= 3` корректнее `double -= 3`. Условие `CurrentAMG_maxSolverIterationCount() > 5.0` — тип геттера (`size_t`) не меняется → безопасно. Закомментированный код не трогаем
- ✅ **Каскадные эффекты:** NumericalParameters не сериализуется, не копируется вручную, не сравнивается. Добавление `double AMG_maxSolverIterAccum` — новое protected поле, implicit copy/move остаются корректными (default memberwise)
- ✅ **Нет workaround-ов:** grep по TODO/FIXME/HACK/workaround в NumericalParameters.h и .cpp — нет

---

## Шаги

### Шаг 1: типы и логика в `.h` + `.cpp` (атомарный блок)

**Цель:** привести тип поля к семантике (целочисленный лимит итераций), добавить дробный аккумулятор, обновить адаптивную логику.

**Файлы:** `HydroSolver/Reservoir/NumericalParameters.h`, `HydroSolver/Reservoir/NumericalParameters.cpp`

**Контекст:**

`AMG_maxSolverIterCount` — лимит итераций AMG-солвера. Семантически — целое число в диапазоне [15, 45]. Хранится как `double` только ради `+= 0.4` в адаптивной логике. Геттер `CurrentAMG_maxSolverIterationCount()` возвращает `size_t` через `static_cast<size_t>(round(...))`, что опасно при отрицательных значениях.

Решение: `int AMG_maxSolverIterCount` (целочисленный лимит) + `double AMG_maxSolverIterAccum` (аккумулятор дробных приращений). Геттер упрощается до `static_cast<size_t>(AMG_maxSolverIterCount)` — безопасно, т.к. clamp [15, 45] гарантирует положительность. Сеттер (reset) обнуляет аккумулятор.

Правки в `.h` и `.cpp` должны применяться **вместе** — после правки `.h` сборка сломается (`std::max(15.0, int)` в `.cpp`), и починится только после правки `.cpp`.

**Что сделать (NumericalParameters.h):**

1. `NumericalParameters.h:21`: `static constexpr size_t AMG_MAXSOLVERITERCOUNT = 45` → `static constexpr int AMG_MAXSOLVERITERCOUNT = 45`

2. `NumericalParameters.h:30`: `double AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT` → `int AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT`

3. Добавить поле `double AMG_maxSolverIterAccum = 0.0;` сразу после строки 30 (после `int AMG_maxSolverIterCount = ...;`), перед `double AMG_curError = 0.0;` (строка 31)

4. `NumericalParameters.h:67`: упростить геттер
   ```
   До:  size_t CurrentAMG_maxSolverIterationCount() const { return static_cast<size_t>(round(AMG_maxSolverIterCount)); }
   После: size_t CurrentAMG_maxSolverIterationCount() const { return static_cast<size_t>(AMG_maxSolverIterCount); }
   ```

5. `NumericalParameters.h:96`: добавить сброс аккумулятора
   ```
   До:  void set_currentAMG_maxSolverIterationCount() { AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT; }
   После: void set_currentAMG_maxSolverIterationCount() { AMG_maxSolverIterCount = AMG_MAXSOLVERITERCOUNT; AMG_maxSolverIterAccum = 0.0; }
   ```

**Что сделать (NumericalParameters.cpp):**

6. `NumericalParameters.cpp:21-22`: заменить `AMG_maxSolverIterCount += 0.4` на аккумуляторную логику
   ```
   До:
   		if (newAMG_error < CurrentAMG_Error())
   			AMG_maxSolverIterCount += 0.4;
   После:
   		if (newAMG_error < CurrentAMG_Error()) {
   			AMG_maxSolverIterAccum += 0.4;
   			if (AMG_maxSolverIterAccum >= 1.0) {
   				AMG_maxSolverIterCount += 1;
   				AMG_maxSolverIterAccum -= 1.0;
   			}
   		}
   ```

7. `NumericalParameters.cpp:29`: `std::max(15.0, AMG_maxSolverIterCount)` → `std::max(15, AMG_maxSolverIterCount)`
   ```
   До:  AMG_maxSolverIterCount = std::max(15.0, AMG_maxSolverIterCount);
   После: AMG_maxSolverIterCount = std::max(15, AMG_maxSolverIterCount);
   ```

8. `NumericalParameters.cpp:34`: `std::min(AMG_maxSolverIterCount, 45.0)` → `std::min(AMG_maxSolverIterCount, AMG_MAXSOLVERITERCOUNT)`
   ```
   До:  AMG_maxSolverIterCount = std::min(AMG_maxSolverIterCount, 45.0);
   После: AMG_maxSolverIterCount = std::min(AMG_maxSolverIterCount, AMG_MAXSOLVERITERCOUNT);
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 312 тестов должны остаться зелёными

**Подводные камни:**
- `std::max(15, AMG_maxSolverIterCount)` — оба `int`, OK
- `std::min(AMG_maxSolverIterCount, AMG_MAXSOLVERITERCOUNT)` — оба `int` (после пункта 1), OK
- Закомментированный блок (строки 25-26): `AMG_maxSolverIterCount -= 3` — не трогаем, `int -= 3` корректнее `double -= 3` при раскомментировании

**Оценка:** 8 изменений в 2 файлах (~15 строк), ~10 минут

---

### Шаг 2: проверка чистоты warnings

**Цель:** убедиться, что изменения не породили новых warnings.

**Что сделать:**

1. Полная пересборка (Rebuild):
   ```powershell
   cmake --build build --config Release -- /t:Rebuild 2>&1 | Select-String "warning C" | Where-Object { $_.Line -notmatch "amgcl|Catch2|Eigen|xutility|xmemory|xstring|numeric" }
   ```

2. Ожидание: 0 warnings из проектного кода

3. Особое внимание на:
   - C4244 (conversion from 'size_t' to 'int') — не ожидается, т.к. `AMG_MAXSOLVERITERCOUNT` теперь `int`
   - C4267 (conversion from 'size_t' to 'int') — аналогично

**Проверка после этого шага:**
- 0 warnings из проектного кода
- Если warnings есть — исправить по существу и повторить

**Зависимости:**
- Требует: шаг 1

**Оценка:** ~3 минуты (ожидание rebuild)

---

### Шаг 3: добавить юнит-тест для AMG-адаптации

**Цель:** покрыть тестами логику `update_currentAMGState` — плавный рост лимита через аккумулятор, clamp, reset.

**Файлы:** `tests/unit/reservoir/test_NumericalParameters.cpp`

**Контекст:**

Существующие тесты (`test_NumericalParameters.cpp`, 9 тестов) покрывают конструкторы, tau, PI-контроллер, Newton-счётчик — но НЕ покрывают `update_currentAMGState` и адаптацию AMG-лимита.

Сложность: `AMG_maxSolverIterCount` и `AMG_maxSolverIterAccum` — protected поля. Через публичный API лимит начинается с 45 (максимум) и не может быть снижен (закомментирован `AMG_maxSolverIterCount -= 3`). Значит через публичный API нельзя проверить рост аккумулятора — лимит всегда clamped на 45.

Решение: тестовый subclass `TestableNumericalParameters`, наследующий `NumericalParameters`, с методами для установки protected полей. Это стандартный паттерн для тестирования protected состояния.

**Что сделать:**

Добавить в `test_NumericalParameters.cpp`:

```cpp
struct TestableNumericalParameters : NumericalParameters {
    void set_AMG_maxSolverIterCount(int val) { AMG_maxSolverIterCount = val; }
    int get_AMG_maxSolverIterCount() const { return AMG_maxSolverIterCount; }
    double get_AMG_maxSolverIterAccum() const { return AMG_maxSolverIterAccum; }
};

TEST_CASE("NumericalParameters: AMG iter count adaptation with accumulator",
          "[unit][level2][reservoir][NumericalParameters]") {
    TestableNumericalParameters np;

    // Initial value = 45 (AMG_MAXSOLVERITERCOUNT)
    CHECK(np.CurrentAMG_maxSolverIterationCount() == 45);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.0));

    // Set starting value below max to observe growth
    np.set_AMG_maxSolverIterCount(20);
    CHECK(np.CurrentAMG_maxSolverIterationCount() == 20);

    // Simulate 3 successful iterations (error decreasing each time)
    // Each call: accum += 0.4. After 3rd: accum >= 1.0 → iter_count += 1
    np.set_currentAMG_Error(1.0);
    np.update_currentAMGState({10, 0.5, true});  // accum = 0.4
    CHECK(np.get_AMG_maxSolverIterCount() == 20);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.4));

    np.set_currentAMG_Error(0.5);
    np.update_currentAMGState({10, 0.3, true});  // accum = 0.8
    CHECK(np.get_AMG_maxSolverIterCount() == 20);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.8));

    np.set_currentAMG_Error(0.3);
    np.update_currentAMGState({10, 0.2, true});  // accum = 1.2 → count += 1, accum = 0.2
    CHECK(np.get_AMG_maxSolverIterCount() == 21);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.2));

    // Test upper clamp: set near max, grow past it
    np.set_AMG_maxSolverIterCount(45);
    np.set_currentAMG_Error(1.0);
    np.update_currentAMGState({10, 0.5, true});
    CHECK(np.get_AMG_maxSolverIterCount() == 45);  // clamped

    // Test lower clamp: error > 0.7 → max(15, val)
    np.set_AMG_maxSolverIterCount(10);
    np.set_currentAMG_Error(0.0);
    np.update_currentAMGState({10, 0.8, true});  // 0.8 > 0.7 → max(15, 10) = 15
    CHECK(np.get_AMG_maxSolverIterCount() == 15);

    // Test reset clears accumulator
    np.set_currentAMG_maxSolverIterationCount();
    CHECK(np.CurrentAMG_maxSolverIterationCount() == 45);
    CHECK(np.get_AMG_maxSolverIterAccum() == Approx(0.0));
}
```

**Тест:**
- **Название:** `NumericalParameters: AMG iter count adaptation with accumulator`
- **Тег:** `[unit][level2][reservoir][NumericalParameters]`
- **Файл:** `tests/unit/reservoir/test_NumericalParameters.cpp` (существующий)
- **Сценарий:** верификация аккумуляторной логики: рост 20→21 за 3 итерации, upper clamp 45, lower clamp 15, reset
- **Ожидание:** `get_AMG_maxSolverIterCount()` = 21 после 3 успешных итераций от 20; аккумулятор = 0.2 после переполнения; reset → 45 + accum = 0.0
- **Baseline:** 312 существующих тестов
- **Предотвращает:** регрессию при будущих изменениях адаптивной логики; верифицирует что аккумулятор действительно работает (а не просто clamped на максимуме)

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 312 + 1 новый тест зелёные

**Зависимости:**
- Требует: шаг 1

**Оценка:** ~30 строк, ~10 минут

---

## Критерии завершения

- [ ] `int AMG_maxSolverIterCount` в объявлении
- [ ] `double AMG_maxSolverIterAccum` добавлен
- [ ] `static constexpr int AMG_MAXSOLVERITERCOUNT` (был `size_t`)
- [ ] Геттер без `round()`: `static_cast<size_t>(AMG_maxSolverIterCount)`
- [ ] Сеттер обнуляет аккумулятор
- [ ] Адаптивная логика: `int` арифметика + аккумулятор
- [ ] Юнит-тест на адаптацию AMG-лимита (через test subclass, проверяет рост, clamp, reset)
- [ ] Все 312+ тестов зелёные (Release + Debug)
- [ ] 0 новых warnings из проектного кода
- [ ] Vault обновлён (статус DEBT-039)
- [ ] GitHub issue #35 прокомментирован

---

## Обнаруженные проблемы

1. **Нет тестов для `update_currentAMGState`** — существующие тесты `test_NumericalParameters.cpp` не покрывают AMG-адаптацию. Исправляется в шаге 3 данного плана через test subclass. Не блокирует

---

## Связанные заметки

- [[известные баги и технический долг]] — секция DEBT-039
- Related: #31 (DEBT-021, закрыт), #33 (DEBT-022, закрыт), #34 (DEBT-028, закрыт)
- DEBT-058 — общая задача типизации int/size_t (DEBT-039 закрывает один из пунктов)
- DEBT-026 — `AddOffDiagBlock` `vector<double>&` → `const` (следующая задача в очереди, не конфликтует)
