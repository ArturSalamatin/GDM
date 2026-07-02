---
tags:
  - план
  - баг
date: 2026-07-02
issue: BUG-003
github: 12
branch: fix/bug-003/welljobs-isempty
status: в процессе
audit:
  date: 2026-07-02
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# План: BUG-003 — WellJobs::IsEmpty() всегда возвращает false

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[известные баги]] (BUG-003), [[code-review-2026-06-28-баги-и-корректность]]
3. Создай ветку: `git checkout -b fix/bug-003/welljobs-isempty experimental`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

**Baseline (experimental, 2026-07-02):** 287 тестов, ~60 сек Release.

---

## Описание проблемы

### Что происходит

Метод `WellJobs::IsEmpty()` проверяет, содержит ли скважина хотя бы одну операцию перфорации. Метод используется как фильтр: если скважина «пуста», она пропускается при загрузке в симуляцию. Из-за ошибки в индексации фильтр не работает — все скважины считаются непустыми.

### Цепочка причинно-следственных связей

1. **Триггер:** конструктор `WellJobs` (WellJobs.cpp:50-57) инициализирует `RawWellPerforationData` как `vector<vector<WellJobTime>>(NmbrOfLayers, пустой_вектор)`. Вектор верхнего уровня непуст (содержит `NmbrOfLayers` пустых слоёв)
2. **Место ошибки:** `IsEmpty()` (WellJobs.cpp:97) проверяет `!RawWellPerforationData.empty()` — это всегда `true` при `NmbrOfLayers > 0`. Должно быть `!RawWellPerforationData[i].empty()` — проверка конкретного слоя
3. **Распространение:**
   - `ReservoirSimulator.cpp:91` — `if (horizon.well_jobs.at(name).IsEmpty()) { continue; }` — скважина с пустыми перфорациями не пропускается → передаётся в `AddWell_FixedProduction()`
   - `RawWellFactory.cpp:120` — `if (!jobs.IsEmpty()) { jobs_container.emplace(name, jobs); }` — пустая скважина попадает в контейнер
4. **Проявление:** UB при построении `AccumulatePerforations()` из пустых данных. На текущих тестовых данных не проявляется (все скважины имеют перфорации)

### Затронутый код

```cpp
// WellJobs.cpp:94-99
bool WellJobs::IsEmpty() const
{
    for (size_t i = 0; i < RawWellPerforationData.size(); ++i)
        if (!RawWellPerforationData.empty())  // BUG: проверяет ВЕСЬ вектор
            return false;
    return true;
}
```

Типы:
- `RawWellPerforationData` — `WellJobsPerLayer` = `std::vector<JobsInLayer>` = `std::vector<std::vector<set_of_points::WellJobTime>>`
- `JobsInLayer` = `std::vector<set_of_points::WellJobTime>`
- Определения: `WellJobs.h:31,35`

### Целевое состояние

`IsEmpty()` возвращает `true` тогда и только тогда, когда ни один слой не содержит job records. Возвращает `false`, если хотя бы один слой непуст.

---

## Варианты решения

### Вариант A: Исправить индексацию (рекомендуемый)

**Суть:** `RawWellPerforationData.empty()` → `RawWellPerforationData[i].empty()`

```cpp
bool WellJobs::IsEmpty() const
{
    for (size_t i = 0; i < RawWellPerforationData.size(); ++i)
        if (!RawWellPerforationData[i].empty())
            return false;
    return true;
}
```

**Плюсы:** 1 символ добавления (`[i]`), минимальный diff, семантика корректна
**Минусы:** нет
**Риски:** нулевые — меняется только индексация в условии
**Совместимость:** не ломает ничего. `IsEmpty()` раньше всегда возвращал `false` — теперь может вернуть `true` для действительно пустых скважин. Это штатное поведение: вызывающий код (ReservoirSimulator, RawWellFactory) корректно обрабатывает `true` (пропускает скважину с предупреждением)
**Трудоёмкость:** 1 файл, 1 строка

### Вариант B: `std::all_of`

```cpp
bool WellJobs::IsEmpty() const
{
    return std::all_of(RawWellPerforationData.begin(), RawWellPerforationData.end(),
        [](const auto& layer) { return layer.empty(); });
}
```

**Плюсы:** идиоматичнее
**Минусы:** больший diff, требует `#include <algorithm>` (может уже быть через другие заголовки)
**Трудоёмкость:** 1 файл, ~5 строк

### Выбор: Вариант A

Минимальное изменение, нулевой риск.

---

## Поиск подводных камней

- ✅ **Побочные эффекты:** `IsEmpty()` вызывается из `ReservoirSimulator.cpp:91` и `RawWellFactory.cpp:120`. Оба call site корректно обрабатывают `true` — пропускают скважину с предупреждением `WellOperationDataIsEmpty`. Безопасно
- ✅ **Потокобезопасность:** `IsEmpty()` — const, читает только `RawWellPerforationData`. Не вызывается из OpenMP-секций. Безопасно
- ✅ **Граничные случаи:**
  - `RawWellPerforationData` пуст (0 слоёв) → цикл не выполняется → `return true`. Корректно
  - Все слои пустые → цикл проходит все, ни один `!empty()` → `return true`. Корректно
  - Один слой непуст → `return false` на первой итерации. Корректно
- ✅ **Производительность:** `IsEmpty()` вызывается при загрузке скважин (один раз), не в горячем цикле. Безопасно
- ✅ **Обратная совместимость:** раньше `IsEmpty()` всегда возвращал `false`. Теперь может вернуть `true` — но только для скважин, которые действительно не имеют перфораций. Это правильное поведение, вызывающий код это обрабатывает
- ✅ **Порядок вызовов:** не зависит от порядка инициализации
- ✅ **Состояние при ошибке:** нет throw/early return с side effects
- ✅ **Численная устойчивость:** не применимо
- ✅ **Связь с другими задачами:** нет конфликтов. Ни один BUG/DEBT не затрагивает `IsEmpty()`
- ✅ **Зависимости сборки:** нет новых #include, нет CMake-изменений

---

## Обнаруженные проблемы

Нет.

---

## Затронутые файлы

| Файл | Роль |
|---|---|
| `HydroSolver/Reservoir/Well/WellJobs.cpp` | Исправление `IsEmpty()` |
| `tests/unit/wells/test_WellJobs.cpp` | Тест `IsEmpty()` для пустых перфораций |

---

## Шаги реализации

### Шаг 1: Исправить `IsEmpty()` — добавить индексацию элемента

**Цель:** устранить ошибку в `IsEmpty()` — проверять конкретный слой `[i]`, а не весь вектор.

**Файлы:** `HydroSolver/Reservoir/Well/WellJobs.cpp`

**Контекст:**
`WellJobs::IsEmpty()` (строки 94-99) должен проверять, есть ли хотя бы одна операция перфорации в каком-либо слое скважины. `RawWellPerforationData` — `vector<vector<WellJobTime>>` (тип `WellJobsPerLayer`, определён в `WellJobs.h:35`). Каждый элемент — перфорации одного слоя. Текущий код проверяет `!RawWellPerforationData.empty()` (пуст ли весь вектор слоёв) вместо `!RawWellPerforationData[i].empty()` (пуст ли конкретный слой `i`). Поскольку конструктор (строка 56) инициализирует вектор `NmbrOfLayers` пустыми слоями, `empty()` на верхнем уровне всегда `false`.

**Что сделать:**

1. В файле `HydroSolver/Reservoir/Well/WellJobs.cpp`, функция `IsEmpty()`, строка ~97:
   добавить `[i]` к `RawWellPerforationData` в условии `if`

**Изменения (старый → новый код):**

До:
```cpp
		if (!RawWellPerforationData.empty())
```

После:
```cpp
		if (!RawWellPerforationData[i].empty())
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — все 287 тестов зелёные
- Существующий тест `WellJobs: construct from single layer jobs` (test_WellJobs.cpp:8-18) — проверяет `CHECK_FALSE(wj.IsEmpty())` для непустой скважины — должен остаться зелёным

**Подводные камни:** нет (см. чеклист выше)

**Зависимости:**
- Требует: нет
- Блокирует: шаг 2

**Оценка:** ~1 строка (1 символ `[i]`), ~2 минуты

---

### Шаг 2: Добавить тесты `IsEmpty()` для пустых перфораций

**Цель:** добавить unit-тесты, которые проверяют корректность `IsEmpty()` для пустых скважин. Без фикса (шаг 1) эти тесты падали бы.

**Файлы:** `tests/unit/wells/test_WellJobs.cpp`

**Контекст:**
Существующий тест (строка 17) проверяет `IsEmpty() == false` для скважины с данными. Нет теста для `IsEmpty() == true`. Нужно покрыть три случая:
1. Все слои пустые (WellJobsPerLayer из пустых JobsInLayer)
2. Один слой пуст, один непуст → `false`
3. Нулевое количество слоёв → `true`

Тесты добавляются в конец файла `test_WellJobs.cpp` (после строки 78). Target `gdm_unit_level3` — файл уже зарегистрирован в CMakeLists.txt.

Для конструирования `WellJobs` используется конструктор `WellJobs(name, WellJobsPerLayer)` — определён в `WellJobs.h:74-76`. Этот конструктор принимает `WellJobsPerLayer` = `vector<vector<WellJobTime>>` напрямую, без агрегации слоёв.

**Что сделать:**

1. В файле `tests/unit/wells/test_WellJobs.cpp`, после последнего `TEST_CASE` (строка ~78):
   добавить 3 теста

**Изменения:**

После текущего конца файла добавить:

```cpp

TEST_CASE("WellJobs: IsEmpty returns true when all layers empty",
          "[unit][level3][wells][WellJobs]") {
    std::wstring name = L"W_empty";
    WellJobsPerLayer layers = {
        JobsInLayer{},
        JobsInLayer{},
        JobsInLayer{}
    };
    WellJobs wj(name, layers);
    CHECK(wj.IsEmpty());
}

TEST_CASE("WellJobs: IsEmpty returns false when one layer has jobs",
          "[unit][level3][wells][WellJobs]") {
    std::wstring name = L"W_partial";
    WellJobsPerLayer layers = {
        JobsInLayer{},
        JobsInLayer{ WellJobTime(0.0, 50.0, true, 100.0) },
        JobsInLayer{}
    };
    WellJobs wj(name, layers);
    CHECK_FALSE(wj.IsEmpty());
}

TEST_CASE("WellJobs: IsEmpty returns true when zero layers",
          "[unit][level3][wells][WellJobs]") {
    std::wstring name = L"W_zero";
    WellJobsPerLayer layers = {};
    WellJobs wj(name, layers);
    CHECK(wj.IsEmpty());
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release -R "WellJobs.*IsEmpty"` — 3 новых теста зелёные
- Регрессия: `ctest --test-dir build -C Release --output-on-failure` — все тесты зелёные (287 + 3 = 290)

**Подводные камни:** нет

**Зависимости:**
- Требует: шаг 1 (без фикса тест 1 и тест 3 упали бы)
- Блокирует: нет

**Оценка:** ~30 строк, ~5 минут

---

## Тестовая стратегия

### Тест-воспроизводитель

**Тест:** `WellJobs: IsEmpty returns true when all layers empty`
**Тег:** `[unit][level3][wells][WellJobs]`
**Файл:** `tests/unit/wells/test_WellJobs.cpp` (существующий)
**Сценарий:** скважина с 3 слоями, все пустые — `IsEmpty()` должен вернуть `true`
**Setup:** `WellJobsPerLayer` из 3 пустых `JobsInLayer`
**Ожидание:** `IsEmpty() == true`
**Предотвращает:** ошибку индексации в `IsEmpty()` — проверка `empty()` на неправильном уровне вектора

### Граничные тесты

**Тест:** `WellJobs: IsEmpty returns false when one layer has jobs`
**Сценарий:** 3 слоя, один непуст — `IsEmpty()` возвращает `false`

**Тест:** `WellJobs: IsEmpty returns true when zero layers`
**Сценарий:** 0 слоёв — `IsEmpty()` возвращает `true` (цикл не выполняется)

### Regression

Все 287 существующих тестов, включая:
- `WellJobs: construct from single layer jobs` — проверяет `IsEmpty() == false` для непустой скважины
- Все тесты с перфорациями скважин (test_helpers.h:add_simple_well)

### Visual

Не применимо.

---

## Критерии завершения

- [ ] `IsEmpty()` исправлен — `RawWellPerforationData[i].empty()` вместо `RawWellPerforationData.empty()`
- [ ] 3 новых теста `IsEmpty()` зелёные
- [ ] Все существующие тесты зелёные в Release (287 → 290)
- [ ] Все тесты зелёные в Debug
- [ ] Ноль новых warnings при сборке (оба конфига)
- [ ] Vault обновлён: запись в [[известные баги]] → статус ✅
- [ ] Заметка в `knowledge/debugging/` создана
- [ ] GitHub issue #12 прокомментирован с результатом
- [ ] `vault/GDM/00-home/index.md` обновлён
