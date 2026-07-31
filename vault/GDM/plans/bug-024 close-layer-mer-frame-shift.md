---
tags:
  - план
  - баг
date: 2026-07-31
issue: BUG-024
github: 56
branch: fix/bug-024/close-layer-mer-shift
status: в процессе
audit:
  date: 2026-07-31
  pass: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# BUG-024: `AveragePerforationsOut` сдвигает `close_layer` к концу MER-frame

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[AveragePerforationsOut сдвигает close_layer]]
3. Создай ветку: `git checkout -b fix/bug-024/close-layer-mer-shift`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Воспроизведи баг: тест-воспроизводитель создаётся на шаге 1

## Связанные vault-заметки

- [[AveragePerforationsOut сдвигает close_layer]] — подробное описание с данными Sw
- [[val-020 shutin-restart-close-layer]] — валидационный кейс, при реализации которого обнаружен баг

## Воспроизведение бага

Тест VAL-020 (`tests/test_3d_completions.cpp:639`) **обходит** баг: в нём `close_layer(0, 180.74)` — дата закрытия подобрана равной `endOfCurPeriod`, поэтому сдвиг не меняет результата.

**Воспроизводящий сценарий:**

Скважина INJ с двумя слоями. Оба открыты с t=0. Закрытие слоя 0 в t=150.0.
MER-записи формируются расписанием теста VAL-020: `for_days(100).shut_in().for_days(50).inject_water(30).for_days(150)`. Даты записей: 30.74, 61.48, 92.22, 100.0, 130.74, **150.0**, 180.74, ... Дата 150.0 — MER record date (конец shut-in фазы). MER-frame для t=150: `[150.0, 180.74)` (т.к. `identify_cur_MER_record(150.0)` находит следующую запись 180.74, и `beginOfCurPeriod = 150.0`).

Ожидание: перфорация k=0 закрыта с t=150.0, `getPerforations(155.0).TotalLength() == 0` в этом слое.

Факт: `AveragePerforationsOut` сдвигает закрытие к t=180.74. Перфорация остаётся открытой ещё ~30 дней.

**Важно:** баг проявляется, когда дата `close_layer` совпадает с `beginOfCurPeriod`. Если дата строго внутри MER-frame (например, 155.0 в frame [150.0, 180.74)), сдвиг к `endOfCurPeriod` — ожидаемое поведение MER-усреднения.

## Цепочка причинно-следственных связей

```
close_layer(0, 150.0)
→ WellJobTime(segment, false, 150.0) добавлен в AccumulatedPerforations
→ AllJobMoments() содержит 150.0
→ SomeWell конструктор (SomeWell.cpp:127):
    beginOfCurPeriod(150.0) = 150.0
    endOfCurPeriod(150.0) = 180.74
    jobIntervals содержит {perf_id, {150.0, 180.74}}
→ AveragePerforationsOut (SetOfPoints.cpp:445):
    Шаг 1 (стр. 454): MoveDate(150.0) — корректно, дата уже на границе
    Шаг 2 (стр. 456–461): TotalLength() == 0.0 → MoveDate(180.74)  ← БАГ
→ getPerforations(155.0) возвращает снимок с TotalLength > 0
→ скважина работает лишние ~30 дней в слое k=0
```

## Целевое состояние

После фикса:
- Если исходная дата `close_layer` совпадает с `beginOfCurPeriod`, закрытие остаётся на начале MER-frame (не сдвигается к концу)
- Если исходная дата `close_layer` строго внутри MER-frame, закрытие сдвигается к `endOfCurPeriod` (текущее поведение, корректное для этого случая)
- Все существующие тесты остаются зелёными
- Тест-воспроизводитель проходит

## Варианты решения

### Вариант A: Проверка по исходной дате из `RawPerorationsInTime`

**Суть:** перед сдвигом закрытия к `endOfCurPeriod` проверяем, совпадала ли исходная дата операции (до averaging) с `beginOfCurPeriod`. Если да — не сдвигаем.

**Изменения:** `SetOfPoints.cpp`, функция `AveragePerforationsOut`, строки 455–461.

Сейчас (шаг 2 — сдвиг закрытий):
```cpp
// bring operations that close the entire cell to the next time frame
for (size_t i = 1; i < jobIntervals.size(); ++i)
    if (PerforationsInTime[i-1].TotalLength() == 0.0) // the well becomes closed
        if (PerforationsInTime[i-1].curTime() < PerforationsInTime[i].curTime())
            PerforationsInTime[i-1].MoveDate(jobIntervals[i-1].second.second);
if (PerforationsInTime.back().TotalLength() == 0.0)
    PerforationsInTime.back().MoveDate(jobIntervals.back().second.second);
```

После:
```cpp
// bring operations that close the entire cell to the next time frame,
// BUT only if the original date was strictly inside the MER frame
// (not already on its left boundary)
for (size_t i = 1; i < jobIntervals.size(); ++i)
{
    const auto& [perf_id_prev, interval_prev] = jobIntervals[i-1];
    if (PerforationsInTime[perf_id_prev].TotalLength() == 0.0)
        if (PerforationsInTime[perf_id_prev].curTime() < PerforationsInTime[jobIntervals[i].first].curTime())
            if (RawPerorationsInTime[perf_id_prev].curTime() != interval_prev.first)
                PerforationsInTime[perf_id_prev].MoveDate(interval_prev.second);
}
{
    const auto& [perf_id_last, interval_last] = jobIntervals.back();
    if (PerforationsInTime[perf_id_last].TotalLength() == 0.0)
        if (RawPerorationsInTime[perf_id_last].curTime() != interval_last.first)
            PerforationsInTime[perf_id_last].MoveDate(interval_last.second);
}
```

**Плюсы:**
- Минимальное изменение, точечный фикс
- Использует уже существующий бэкап `RawPerorationsInTime`
- Не меняет поведение для операций строго внутри MER-frame

**Минусы:**
- Сравнение `double == double` для исходной даты и `beginOfCurPeriod`. Но это корректно: обе величины приходят из одного и того же пути — `beginOfCurPeriod(timeMoment)` в `SomeWell.cpp:131`, где `timeMoment` — та же самая дата, которая записана в `RawPerorationsInTime`. Если дата совпадает с MER-записью, `identify_cur_MER_record` вернёт `itsBeginOfCurPeriod` = та же точная дата (без арифметики), поэтому `==` корректно.

**Риски:**
- Нужно проверить, что `RawPerorationsInTime` уже заполнен к моменту проверки. Да — заполняется на строке 449–450, до шага 2.

**Трудоёмкость:** 1 файл, ~15 строк изменений.

### Вариант B: Передать исходные даты через `jobIntervals`

**Суть:** расширить структуру `jobIntervals` тройкой `{beginOfCurPeriod, endOfCurPeriod, originalDate}`, чтобы `AveragePerforationsOut` мог принять решение без обращения к `RawPerorationsInTime`.

**Изменения:** `SomeWell.cpp` (формирование `jobIntervals`) + `SetOfPoints.cpp` (`AveragePerforationsOut`) + `SetOfPoints.h` (сигнатура).

**Плюсы:**
- Явная передача данных, не зависим от побочного состояния `RawPerorationsInTime`
- Чище с точки зрения data flow

**Минусы:**
- Три файла, изменение сигнатуры публичного метода
- `RawPerorationsInTime` всё равно существует и заполняется — дублирование информации
- Бо́льший объём изменений ради той же логики

**Трудоёмкость:** 3 файла, ~30 строк.

### Выбор: Вариант A

Вариант A минимален и использует данные, которые уже есть. `RawPerorationsInTime` создан именно для хранения исходных дат — использовать его для принятия решения о сдвиге — это ровно его назначение.

## Поиск подводных камней

- [x] **Побочные эффекты:** `AveragePerforationsOut` вызывается из одного места — конструктор `SomeWell` (SomeWell.cpp:149). `RawPerorationsInTime` читается только в `ReverseAveraging` (нигде не вызывается).
- [x] **Потокобезопасность:** код вызывается при инициализации скважины, вне `#pragma omp parallel`.
- [x] **Граничные случаи:**
  - Пустой `jobIntervals` — цикл не выполняется, ничего не ломается.
  - Одна операция в `jobIntervals` — внутренний цикл `for (i = 1; ...)` не выполняется, обрабатывается только `back()`.
  - Закрытие строго внутри MER-frame (не на границе) — `RawPerorationsInTime[perf_id].curTime() != interval.first` → сдвиг к концу выполняется (как раньше).
  - Закрытие точно на `beginOfCurPeriod` — сдвиг не выполняется (фикс).
  - Закрытие точно на `endOfCurPeriod` (как в VAL-020) — шаг 1 сдвигает к `beginOfCurPeriod`, `RawPerorationsInTime.curTime() != beginOfCurPeriod` → сдвиг к `endOfCurPeriod` выполняется. Результат тот же, что и раньше.
- [x] **Производительность:** код инициализации, не горячий цикл.
- [x] **Обратная совместимость:** для операций строго внутри MER-frame поведение не меняется. Для операций на границе — меняется (это и есть фикс). VAL-020 использует `close_layer(0, 180.74)` — попадает в случай «внутри MER-frame» → сдвигается к концу → поведение не меняется.
- [x] **Порядок вызовов:** `RawPerorationsInTime` заполняется (строка 449–450) до шага 2 (строка 455+).
- [x] **Состояние при ошибке:** нет throw/early return в модифицируемом коде.
- [x] **Численная устойчивость:** сравнение `double == double` корректно (см. анализ в варианте A).
- [x] **Связь с другими задачами:** BUG-010 (const_cast) уже исправлен. `ReverseAveraging` определён, но не используется — конфликтов нет.
- [x] **Зависимости сборки:** изменений в CMakeLists нет.
- [x] **Расширение скоупа:** фикс одновременно исправляет побочную ошибку индексации (raw-индекс `i-1` вместо `perf_id` из jobIntervals). Это корректно и не вносит нового риска, т.к. `perf_id` — семантически правильный индекс.
- [x] **MER record dates в тестах:** 150.0 является MER record date в VAL-020 (конец shut_in phase). 153.70 = 5×30.74 — MER record date в интеграционном тесте шага 3 (равномерное расписание). Оба значения проверены расчётом из `for_days` логики `WellScheduleBuilder`.

## Обнаруженные проблемы

Нет новых проблем.

---

## Этапы реализации

### Шаг 1: Тест-воспроизводитель

**Цель:** создать unit-тест, который падает без фикса и проходит после

**Файлы:** `tests/unit/wells/test_AccumulatedPerforations.cpp`

**Контекст:**

`AccumulatedPerforations::AveragePerforationsOut` принимает вектор `jobIntervals`, где каждый элемент — `{perf_id, {beginOfCurPeriod, endOfCurPeriod}}`. Метод сдвигает даты операций к началу MER-frame, а операции закрытия (TotalLength == 0) — к концу MER-frame. Баг: закрытие на дате, совпадающей с `beginOfCurPeriod`, тоже сдвигается к `endOfCurPeriod`.

Тест создаёт `AccumulatedPerforations` с двумя операциями:
1. `open(0, 100, true, 0.0)` — открытие полного интервала в t=0
2. `close(0, 100, false, 150.0)` — закрытие того же интервала в t=150

Затем вызывает `AveragePerforationsOut` с `jobIntervals`, где MER-frame для закрытия = `{150.0, 180.74}` (дата закрытия = начало frame).

Ожидание: после averaging дата закрытия = 150.0 (не 180.74).

**Что сделать:**

1. В файле `tests/unit/wells/test_AccumulatedPerforations.cpp` добавить тест после последнего `TEST_CASE`:

```cpp
TEST_CASE("AccumulatedPerforations: AveragePerforationsOut does not shift close-on-boundary to end",
          "[unit][level2][wells][AccumulatedPerforations][bug-024]") {
    AccumulatedPerforations ap;
    // open full interval [0, 100] at t=0 (implicitly via constructor default + AddNewJob)
    ap.AddNewJob(WellJobTime(0.0, 100.0, true, 0.0));
    // close the same interval at t=150 — TotalLength becomes 0
    ap.AddNewJob(WellJobTime(0.0, 100.0, false, 150.0));

    CHECK(ap.getPerforationsSet().size() == 3); // default(t=-max) + open(t=0) + close(t=150)
    CHECK(ap.getPerforations(150.0).TotalLength() == Approx(0.0));

    // MER frame for the close operation: [150.0, 180.74)
    // perf_id=2 corresponds to the close operation (index in PerforationsInTime)
    // We skip perf_id=0 (default) and perf_id=1 (open at t=0 — skip because beginOfCurPeriod=0)
    // So jobIntervals has one entry for perf_id=2
    std::vector<std::pair<size_t, std::pair<double, double>>> jobIntervals = {
        {2, {150.0, 180.74}}
    };

    ap.AveragePerforationsOut(jobIntervals);

    // The close operation date must remain at 150.0 (boundary of MER frame)
    // BUG-024: without fix, it shifts to 180.74
    auto& perfs = ap.getPerforationsSet();
    // Find the close operation (TotalLength == 0)
    double closeDate = -1.0;
    for (const auto& p : perfs)
        if (p.TotalLength() == Approx(0.0) && p.curTime() > 0.0)
            closeDate = p.curTime();

    CHECK(closeDate == Approx(150.0));
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "bug-024"`
- Ожидаемый результат: **ПАДАЕТ** (closeDate == 180.74, не 150.0). Это подтверждает баг.

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~25 строк, ~5 минут

---

### Шаг 2: Фикс `AveragePerforationsOut`

**Цель:** не сдвигать закрытие к `endOfCurPeriod`, если исходная дата совпадает с `beginOfCurPeriod`

**Файлы:** `HydroSolver/Reservoir/Well/SetOfPoints.cpp`

**Контекст:**

`AveragePerforationsOut` (строки 445–483) выполняет три этапа:
1. Бэкап в `RawPerorationsInTime` (строка 449–450)
2. Сдвиг всех дат к `beginOfCurPeriod` (строка 452–454)
3. Сдвиг закрытий к `endOfCurPeriod` (строки 456–461)

Этап 3 не проверяет, была ли исходная дата уже на границе MER-frame. Если дата закрытия = `beginOfCurPeriod`, то шаг 2 ничего не меняет, а шаг 3 сдвигает к концу — ошибочно.

Фикс: перед сдвигом проверять `RawPerorationsInTime[perf_id].curTime() != interval.first`. Бэкап `RawPerorationsInTime` хранит исходные (до averaging) даты.

Текущий код (строки 455–461):

```cpp
// bring operations that close the entire cell to the next time frame
for (size_t i = 1; i < jobIntervals.size(); ++i)
    if (PerforationsInTime[i-1].TotalLength() == 0.0) // the well becomes closed
        if (PerforationsInTime[i-1].curTime() < PerforationsInTime[i].curTime())
            PerforationsInTime[i-1].MoveDate(jobIntervals[i-1].second.second);
if (PerforationsInTime.back().TotalLength() == 0.0)
    PerforationsInTime.back().MoveDate(jobIntervals.back().second.second);
```

Обрати внимание: текущий код содержит **побочную ошибку индексации** — он использует `PerforationsInTime[i-1]` и `PerforationsInTime[i]` по индексу `i` в `jobIntervals`, но `jobIntervals` может иметь пропуски (не все `perf_id` попадают — из-за `continue` в SomeWell.cpp:145). Аналогично, `PerforationsInTime.back()` в строке 460 может не соответствовать `jobIntervals.back().first`. Эта ошибка не проявляется на текущих тестах, но семантически некорректна. Фикс исправляет оба бага: BUG-024 (неуместный сдвиг к endOfCurPeriod) и ошибку индексации (используем `perf_id` явно вместо raw-индекса).

**Что сделать:**

1. В `SetOfPoints.cpp`, заменить строки 455–461 (от комментария `// bring operations that close the entire cell` до строки `PerforationsInTime.back().MoveDate(...)`) на:

```cpp
// bring operations that close the entire cell to the next time frame,
// but only if the original date was strictly inside the MER frame
for (size_t i = 1; i < jobIntervals.size(); ++i)
{
    const auto& [perf_id, interval] = jobIntervals[i - 1];
    if (PerforationsInTime[perf_id].TotalLength() == 0.0)
        if (PerforationsInTime[perf_id].curTime() < PerforationsInTime[jobIntervals[i].first].curTime())
            if (RawPerorationsInTime[perf_id].curTime() != interval.first)
                PerforationsInTime[perf_id].MoveDate(interval.second);
}
{
    const auto& [perf_id, interval] = jobIntervals.back();
    if (PerforationsInTime[perf_id].TotalLength() == 0.0)
        if (RawPerorationsInTime[perf_id].curTime() != interval.first)
            PerforationsInTime[perf_id].MoveDate(interval.second);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест BUG-024: `ctest --test-dir build -C Release -R "bug-024"` → **зелёный**
- Регрессия: `ctest --test-dir build -C Release` → все тесты зелёные
- Особое внимание: `ctest --test-dir build -C Release -R "val-020"` → зелёный (обратная совместимость)

**Подводные камни:**
- `RawPerorationsInTime` заполнен до этого кода — проверено (строка 449–450)
- Сравнение `double == double` корректно — обе величины из одного пути, без арифметики
- `jobIntervals` может быть пустым — цикл и блок `back()` не выполняются (но `back()` на пустом — UB). Нужен guard: `if (!jobIntervals.empty())`. Однако в текущем коде guard отсутствует и не нужен: если `jobIntervals` пуст, `jobIntervals.size()` = 0, цикл `for (i = 1; i < 0)` не выполняется, а `back()` — UB. Текущий код тоже имеет этот `back()` без guard — значит, гарантируется непустой `jobIntervals`. Сохраняем ту же гарантию.

**Зависимости:**
- Требует: шаг 1 (тест-воспроизводитель)
- Блокирует: шаг 3

**Оценка:** ~15 строк, ~10 минут

---

### Шаг 3: Тест с close_layer на дату точно равную beginOfCurPeriod в интеграционном тесте

**Цель:** убедиться, что фикс работает на уровне полной симуляции, а не только unit-теста

**Файлы:** `tests/test_3d_completions.cpp`

**Контекст:**

Существующий тест VAL-020 (`test_3d_completions.cpp:639`) обходит баг, используя `close_layer(0, 180.74)` — дату, равную `endOfCurPeriod`. Нужен тест, где `close_layer` указан на дату, совпадающую с `beginOfCurPeriod`.

Тест использует `inject_water(30.0).for_days(300.0)` — непрерывную инъекцию. MER-записи формируются с шагом `month_ = 30.74`: t=30.74, 61.48, 92.22, 122.96, **153.70**, 184.44, ... (последняя: 300.0, `ceil(300/30.74)=10` записей).

Дата `t=153.70 = 5 × 30.74` — MER record date. `identify_cur_MER_record(153.70)` находит `record_time > 153.70` → 184.44, и `beginOfCurPeriod = record_time(monthID-1) = 153.70`. MER-frame: [153.70, 184.44).

Используем `close_layer(0, 153.70)`. До фикса: перфорация закроется в t≈184.44 (сдвиг к endOfCurPeriod). После фикса: `RawPerorationsInTime.curTime() = 153.70 == interval.first = 153.70` → сдвиг не выполняется → закрытие в t=153.70.

**Что сделать:**

1. В файле `tests/test_3d_completions.cpp` добавить тест после теста VAL-020 (после строки ~709):

```cpp
TEST_CASE("3D completions: close_layer on MER boundary is not shifted",
          "[3d][completions][close-on-boundary][bug-024]")
{
    constexpr size_t Nz = 2;

    simulation_cases::MultiLayerCase sc(
        "3d_close_on_mer_boundary", Nx, Ny, Nz, Lx, Ly, hz,
        300.0, 10.0,
        [&](double, double) {
            std::vector<test_helpers::WellScheduleBuilder> builders;

            // close_layer at t=153.70 — exactly a MER record date (beginOfCurPeriod)
            auto c_inj = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(0, 153.70);
            builders.emplace_back("INJ", 125.0, 250.0);
            builders.back()
                .set_completions(c_inj)
                .inject_water(30.0).for_days(300.0);

            auto c_prod = test_helpers::WellCompletionBuilder(Nz, hz)
                .open_layer(0, 0.0).open_layer(1, 0.0)
                .close_layer(0, 153.70);
            builders.emplace_back("PROD", 375.0, 250.0);
            builders.back()
                .set_completions(c_prod)
                .produce_oil(20.0).for_days(300.0);

            return builders;
        },
        {{"INJ",  "injector", 125.0, 250.0},
         {"PROD", "producer", 375.0, 250.0}}
    );

    auto result = run_case_3d(sc, true);
    const auto& h = result.Sw_history;

    CHECK(result.max_oil_balance_rel < 1e-3);
    CHECK(result.max_water_balance_rel < 1e-3);

    size_t inj_i = static_cast<size_t>(125.0 / (Lx / Nx));
    size_t inj_j = static_cast<size_t>(250.0 / (Ly / Ny));
    size_t prod_i = static_cast<size_t>(375.0 / (Lx / Nx));
    size_t prod_j = static_cast<size_t>(250.0 / (Ly / Ny));

    size_t inj_k0  = Nx * inj_j + inj_i;
    size_t prod_k0 = Nx * prod_j + prod_i;

    // snapshot index for t≈153.70: ceil(153.70/10) ≈ 16 (dt=10)
    // snapshot index for t≈184.44: ceil(184.44/10) ≈ 19
    // snapshot index for t=300: 30

    // After close_layer at t=153.70, k=0 must freeze:
    // Sw at snapshot 30 (t=300) == Sw at snapshot ~16 (t≈153.70)
    // The exact snapshot index depends on dt, so find it by looking for the freeze point
    // Conservatively: Sw(t=300) in k=0 should equal Sw at some point near t≈154
    // If BUG-024 is present, Sw would keep changing until t≈184

    // k=0 at INJ frozen after close: last snapshot == snapshot at ~t=160
    // Use snapshot 20 (t=200, well after 153.70 and well after 184.44):
    // if frozen at 153.70: h[30][inj_k0] == h[20][inj_k0]
    // if frozen at 184.44: h[30][inj_k0] == h[20][inj_k0] (also true since 200 > 184.44)
    // Need to check BETWEEN 153.70 and 184.44 — snapshot 17 (t=170)
    // If buggy: h[17][inj_k0] != h[16][inj_k0] (still changing)
    // If fixed: h[17][inj_k0] == h[16][inj_k0] (already frozen)
    CHECK(h[17][inj_k0]  == Catch::Approx(h[16][inj_k0]).margin(1e-10));
    CHECK(h[17][prod_k0] == Catch::Approx(h[16][prod_k0]).margin(1e-10));
    CHECK(h[30][inj_k0]  == Catch::Approx(h[16][inj_k0]).margin(1e-10));
    CHECK(h[30][prod_k0] == Catch::Approx(h[16][prod_k0]).margin(1e-10));
}
```

**Snapshot mapping:** `MultiLayerCase` вызывается с `total_time=300.0, dt=10.0` → snapshot[k] = t=k×10. Закрытие в t=153.70: между snapshot 15 (t=150, ещё открыто) и snapshot 16 (t=160, уже закрыто). Проверяем `h[17] == h[16]` (оба заморожены) и `h[30] == h[16]`.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "bug-024"` → оба теста зелёные
- Регрессия: `ctest --test-dir build -C Release` → все зелёные

**Зависимости:**
- Требует: шаг 2 (фикс)
- Блокирует: шаг 4

**Оценка:** ~50 строк, ~15 минут

---

### Шаг 4: Обновить тест VAL-020 — вернуть «честную» дату закрытия

**Цель:** теперь, когда баг исправлен, тест VAL-020 может использовать дату 150.0 вместо обходного 180.74

**Файлы:** `tests/test_3d_completions.cpp`

**Контекст:**

VAL-020 использовал `close_layer(0, 180.74)` как workaround для BUG-024. Теперь можно использовать дату 150.0 — момент restart (t=150 = 100 inject + 50 shut-in). Это корректно, потому что 150.0 является MER record date в расписании VAL-020: `for_days(100)` → records до cursor=100, `shut_in().for_days(50)` → records до cursor=150.0. Таким образом, `beginOfCurPeriod(150.0) = 150.0`, и фикс BUG-024 гарантирует, что закрытие не сдвинется к 180.74.

**Что сделать:**

1. В `tests/test_3d_completions.cpp`, строки 652 и 662: заменить `close_layer(0, 180.74)` на `close_layer(0, 150.0)`
2. Обновить комментарий в строке 700: заменить `t≈180.74 (snapshot 19+)` на `t=150 (snapshot 15+)`
3. Обновить проверки в строках 701–702: snapshot 19 → 15 (или тот, что соответствует t=150)
4. В визуальном тесте VAL-020 (`[.visual]`, строки 729 и 739): заменить `close_layer(0, 180.74)` на `close_layer(0, 150.0)` — аналогично основному тесту. Визуальный тест не запускается автоматически (скрытый тег `[.]`), но должен быть синхронизирован с основным тестом

**Изменения:**

До:
```cpp
.close_layer(0, 180.74);
```

После:
```cpp
.close_layer(0, 150.0);
```

До (строки 700–702):
```cpp
// close_layer: k=0 замораживается после t≈180.74 (snapshot 19+)
CHECK(h[30][inj_k0]  == Catch::Approx(h[19][inj_k0]).margin(1e-10));
CHECK(h[30][prod_k0] == Catch::Approx(h[19][prod_k0]).margin(1e-10));
```

После:
```cpp
// close_layer: k=0 замораживается после t=150 (snapshot 15+)
CHECK(h[30][inj_k0]  == Catch::Approx(h[15][inj_k0]).margin(1e-10));
CHECK(h[30][prod_k0] == Catch::Approx(h[15][prod_k0]).margin(1e-10));
```

**Snapshot mapping:** `MultiLayerCase` вызывается с `total_time=300.0, dt=10.0` → snapshots: 0=t0, 1=t10, ..., 15=t150, ..., 30=t300. Snapshot 15 = t=150, snapshot 19 = t=190. При `close_layer(0, 150.0)` перфорация закрыта с t=150 (snapshot 15), Sw в k=0 заморожена начиная с snapshot 15.

Строки 705–706 (k=1 продолжает меняться, `h[30] > h[19]`) **не нуждаются в обновлении** — скважина работает через k=1 после restart, и snapshot 19 (t=190) > 150 (момент restart).

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "val-020"` → зелёный
- Регрессия: `ctest --test-dir build -C Release` → все зелёные

**Зависимости:**
- Требует: шаг 2 (фикс)
- Блокирует: ничего

**Оценка:** ~5 строк, ~5 минут

---

### Шаг 5: Обновить vault

**Цель:** зафиксировать результат в vault

**Файлы:**
- `vault/GDM/roadmap/известные баги.md`
- `vault/GDM/knowledge/debugging/BUG-024 close-layer-mer-frame-shift.md` (новый)

**Что сделать:**

1. В `vault/GDM/roadmap/известные баги.md`, запись BUG-024: обновить статус на `✅ исправлено <дата>`, добавить `- **План:** [[bug-024 close-layer-mer-frame-shift]]`

2. Создать `vault/GDM/knowledge/debugging/BUG-024 close-layer-mer-frame-shift.md`:
```yaml
---
tags: [debugging, баг-исправлен]
date: <дата>
---
```
Содержание: причина (сдвиг закрытия к endOfCurPeriod без проверки границы), фикс (проверка RawPerorationsInTime), связи с VAL-020.

3. Перенести `vault/GDM/inbox/AveragePerforationsOut сдвигает close_layer.md` в debugging (или удалить, т.к. создаётся полноценная заметка).

**Зависимости:**
- Требует: шаги 2–4 (фикс и тесты прошли)
- Блокирует: ничего

**Оценка:** ~5 минут

---

## Тестовая стратегия

**Тест 1 (воспроизводитель):**
- **Тест:** `AccumulatedPerforations: AveragePerforationsOut does not shift close-on-boundary to end`
- **Тег:** `[unit][level2][wells][AccumulatedPerforations][bug-024]`
- **Файл:** `tests/unit/wells/test_AccumulatedPerforations.cpp` (существующий)
- **Сценарий:** open + close, затем AveragePerforationsOut с MER-frame, где close_date == beginOfCurPeriod
- **Ожидание:** дата закрытия после averaging == 150.0, не 180.74
- **Предотвращает:** сдвиг close_layer на границе MER-frame

**Тест 2 (интеграционный):**
- **Тест:** `3D completions: close_layer on MER boundary is not shifted`
- **Тег:** `[3d][completions][close-on-boundary][bug-024]`
- **Файл:** `tests/test_3d_completions.cpp` (существующий)
- **Сценарий:** 2-слойная модель, close_layer(0, 153.70) — дата = MER record date
- **Ожидание:** Sw в k=0 замораживается после t≈153.70, не после t≈184.44
- **Предотвращает:** регрессия в полной симуляции

**Тест 3 (регрессия, обновлённый):**
- **Тест:** `3D completions: shut-in + restart with closed layer` (VAL-020)
- **Тег:** `[3d][completions][shutin-restart][val-020]`
- **Файл:** `tests/test_3d_completions.cpp` (существующий)
- **Сценарий:** обновлён: close_layer(0, 150.0) вместо обходного 180.74
- **Ожидание:** Sw в k=0 замораживается после t=150, все существующие проверки проходят

**Regression:** все существующие тесты (`ctest --test-dir build -C Release`) должны остаться зелёными.

## Критерии завершения

- [ ] Тест-воспроизводитель зелёный (`bug-024` тег)
- [ ] Интеграционный тест зелёный (`close-on-boundary` тег)
- [ ] VAL-020 зелёный с close_layer(0, 150.0)
- [ ] Все существующие тесты зелёные
- [ ] Vault обновлён: запись BUG-024 → `✅ исправлено`, debugging заметка создана
- [ ] Inbox-заметка перенесена/удалена

## Суммарная оценка

- **Файлов:** 3 (SetOfPoints.cpp, test_AccumulatedPerforations.cpp, test_3d_completions.cpp) + 2 vault
- **Строк кода:** ~95
- **Шагов:** 5
- **Время:** ~40 минут
- **Ветка:** `fix/bug-024/close-layer-mer-shift`
