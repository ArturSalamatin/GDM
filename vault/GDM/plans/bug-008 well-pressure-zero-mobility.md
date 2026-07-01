---
tags:
  - план
  - баг
date: 2026-07-01
issue: BUG-008
github: 7
branch: fix/bug-008/well-pressure-zero-mobility
status: в процессе
audit:
  date: 2026-07-01
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# BUG-008: Деление на ноль в SetRefWellPressure при нулевой подвижности

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[code-review-2026-06-28-числовая-устойчивость]] — CR-NUM-001
   - [[BUG-007 harmonic mean zero division]] — аналогичный паттерн (уже исправлен)
3. Создай ветку: `git checkout -b fix/bug-008/well-pressure-zero-mobility`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни baseline: **282 теста, ~53 сек, 281 pass** (1 flaky — `3D completions: partial perforation`)
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание бага

Деление на ноль в `WellFixedProduction::SetRefWellPressure()` (`Wells.cpp:89`). Давление скважины вычисляется как взвешенное среднее давлений пласта: `P_well = numer / denom`, где `denom = Σ factor[l] * OverallMobility(l)`. Если суммарная подвижность всех перфораций = 0, `denom = 0` → `P_well = ±Inf`.

Два сценария `denom = 0`:
1. Все перфорации имеют `OverallMobility(l) = 0` (непроницаемый барьер, `Permeability = 0`)
2. `NmbrOfOpenedCells() = 0` (пустая скважина — цикл не выполняется, `denom = 0`)

Физика: давление скважины `P_well` определяется из уравнения Пикмана. Формула:

```
Q_total = Σ_l PI_l * λ_total_l * (P_reservoir_l - P_well)
```

где `PI_l = factor[l]` — индекс продуктивности перфорации l, `λ_total_l = OverallMobility(l)` — суммарная подвижность. Отсюда:

```
P_well = (Σ_l PI_l * λ_l * P_res_l - Q_total) / (Σ_l PI_l * λ_l)
```

При `λ_l = 0` для всех l — знаменатель = 0, и P_well не определено (нет потока, уравнение Пикмана вырождается). Физически: если подвижность = 0, скважина не может производить — давление скважины уравновешивается с пластовым.

**Текущее поведение при стандартной модели Corey:**
Аналогично BUG-007: при `Permeability > 0` и Corey relperm `MobilityOverall > 0` всегда. Баг латентный.

**Критичность:** средняя. Латентный дефект, тот же класс что BUG-007.

---

## Затронутые файлы

| Файл | Строки | Роль |
|---|---|---|
| `HydroSolver/Reservoir/Well/Wells.cpp` | 80-92 | `SetRefWellPressure()` — деление `numer / denom` |

---

## Цепочка причинно-следственных связей

```
OverallMobility(l) = 0 для всех перфораций  ИЛИ  NmbrOfOpenedCells() = 0
        │
        ▼
denom = Σ factor[l] * OverallMobility(l) = 0
        │
        ▼
P_well = numer / 0 = ±Inf
        │
        ▼
SetWellPressure(±Inf)  →  P_Well[l] = ±Inf
        │
        ▼
SetProductions():
  productions[l] = factor * OverallMobility * (P_res - ±Inf)
  При OverallMobility > 0: productions = ±Inf → throw на строке 28-29
  При OverallMobility = 0 (после BUG-007): productions = 0 * Inf = NaN → throw
        │
        ▼
throw std::exception("well production is not determined")
```

Примечание: после фикса BUG-007, `F_Oil = 0` при `MobilityOverall = 0`. Но `P_Well = Inf` всё равно → `0 * Inf = NaN` → isfinite check → throw. Так что BUG-007 не спасает от BUG-008.

---

## Целевое состояние

1. При `denom = 0`:
   - `P_well = среднее давление пласта` (или 0, если нет перфораций)
   - `productions[l] = 0` (нет потока при нулевой подвижности)
   - Никаких throw, NaN, Inf
2. Все 282 существующих теста зелёные
3. Поведение при `denom > 0` **не меняется**

---

## Варианты решения

### Вариант A: Guard + return (без SetWellPressure)

```cpp
if (denom == 0.0) return;
```

**Плюсы:** 1 строка.
**Минусы:** `P_Well` остаётся нулевым (от `UpdateWellState` строка 379: `P_Well = vector<double>(N)`). При `OverallMobility = 0` → `productions = factor * 0 * (P_res - 0) = 0`. Функционально корректно, но `P_Well = 0` физически абсурдно (давление скважины ≠ 0).
**Риски:** если в будущем кто-то использует `P_Well` для визуализации или диагностики — увидит 0 вместо реального давления.

### Вариант B: Guard + P_well = среднее давление пласта (рекомендуемый)

```cpp
if (denom == 0.0) {
    double avg_P = 0.0;
    for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
        avg_P += P_Reservoir(l);
    if (NmbrOfOpenedCells() > 0) avg_P /= NmbrOfOpenedCells();
    SetWellPressure(avg_P);
    return;
}
```

**Плюсы:** физически корректно — при нулевом потоке P_well → P_reservoir. `productions = factor * 0 * (P_res - P_res) = 0`. Значение `P_Well` осмысленно для диагностики.
**Минусы:** дополнительный цикл O(N), где N = число перфораций (обычно 1-20). Пренебрежимо.
**Риски:** при `NmbrOfOpenedCells() = 0` → `avg_P = 0.0` → `SetWellPressure(0.0)`. Это нормально — пустая скважина без перфораций, `productions` пуст.

### Выбор: Вариант B

Причины:
1. Физически корректен
2. `P_Well` имеет осмысленное значение для диагностики
3. Минимальный риск побочных эффектов
4. Вариант A функционально эквивалентен (productions = 0 в обоих случаях), но B чище

---

## Чеклист подводных камней

- ✅ **Побочные эффекты:** `SetRefWellPressure` вызывается только из `AddWellToMatrix` (строка 22). Одно место.
- ✅ **Потокобезопасность:** `AddWellToMatrix` вызывается из последовательного цикла `for (auto& [name, well] : Wells)`, не под OMP. Безопасно.
- ✅ **Граничные случаи:**
  - `denom = 0, NmbrOfOpenedCells > 0`: guard → avg_P = среднее давление → `SetWellPressure(avg_P)` → `productions = factor * 0 * 0 = 0`
  - `denom = 0, NmbrOfOpenedCells = 0`: guard → avg_P = 0 → `SetWellPressure(0)` → цикл `SetProductions` пуст → безопасно
  - `denom > 0`: guard не срабатывает → поведение идентично
- ✅ **Производительность:** один дополнительный цикл по перфорациям (обычно 1-20) — пренебрежимо. Не в горячем пути.
- ✅ **Обратная совместимость:** guard не срабатывает при `denom > 0`. Все текущие тесты: `denom > 0`.
- ✅ **Порядок вызовов:** `UpdateWellState` → `SetRefWellPressure` → `SetProductions` → `BalanceOil`. Guard в `SetRefWellPressure` → `SetWellPressure(avg_P)` → `P_Well` установлен перед `SetProductions`. Порядок соблюдён.
- ✅ **Состояние при ошибке:** `return` после `SetWellPressure` — объект валиден. `P_Well` установлен, `productions` будет 0 (из `SetProductions`).
- ✅ **Численная устойчивость:** `denom == 0.0` — точное сравнение корректно: `factor[l] ≥ 0` (геометрический параметр), `OverallMobility ≥ 0` (после BUG-007). Сумма неотрицательных = 0 iff все = 0.
- ✅ **Связь с другими задачами:** BUG-007 (исправлен) — `OverallMobility = 0` корректно (не NaN). BUG-008 использует `OverallMobility(l)` через `WellEnvironment::OverallMobility()` → `cells[...]->MobilityOverall()` → `DependentFieldProperties[2]`. С fix BUG-007 это 0 (не NaN). Совместимо.
- ✅ **Зависимости сборки:** нет новых include, нет CMake-изменений.

---

## Обнаруженные проблемы

Нет новых проблем.

---

## Шаги реализации

Минимальный фикс = архитектурное решение. Этапы 2-3 и 4-5 объединены.

### Шаг 1: Guard в SetRefWellPressure

**Цель:** предотвратить деление на ноль при `denom = 0`

**Файлы:** `HydroSolver/Reservoir/Well/Wells.cpp`

**Контекст:**
`WellFixedProduction::SetRefWellPressure()` вычисляет давление скважины как взвешенное среднее давлений пласта в перфорациях с весами `PI * λ_total`. При `λ_total = 0` для всех перфораций знаменатель = 0 → `P_well = Inf`. Физически: при нулевой подвижности поток отсутствует, давление скважины уравновешивается с пластовым.

**Что сделать:**
1. В файле `Wells.cpp`, функция `SetRefWellPressure()`, после строки 88 (конец цикла), перед строкой 89 (`double P = numer / denom`):
   Добавить guard — если `denom == 0.0`, вычислить среднее давление пласта и вернуться.

**Изменения (старый → новый код):**

До (строки 80-92):
```cpp
void WellFixedProduction::SetRefWellPressure()
{
    double denom = 0, numer = -CurOverallDebit();
    for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
    {
        double temp = factor[l] * OverallMobility(l);
        denom += temp;
        numer += temp * P_Reservoir(l);
    }
    double P = numer / denom;

    SetWellPressure(P);
}
```

После:
```cpp
void WellFixedProduction::SetRefWellPressure()
{
    double denom = 0, numer = -CurOverallDebit();
    for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
    {
        double temp = factor[l] * OverallMobility(l);
        denom += temp;
        numer += temp * P_Reservoir(l);
    }
    if (denom == 0.0) {
        double avg_P = 0.0;
        for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
            avg_P += P_Reservoir(l);
        if (NmbrOfOpenedCells() > 0) avg_P /= NmbrOfOpenedCells();
        SetWellPressure(avg_P);
        return;
    }
    double P = numer / denom;

    SetWellPressure(P);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release` (все тесты зелёные — guard не срабатывает при стандартных условиях)
- Ожидаемый результат: 281/282 pass (тот же flaky)

**Подводные камни:**
- `NmbrOfOpenedCells()` может быть 0 → `avg_P = 0.0`, `SetWellPressure(0.0)`. Безопасно — `SetProductions` не выполнится (цикл пуст).

**Зависимости:** нет
**Блокирует:** шаг 2

**Оценка:** ~7 строк, ~3 минуты

---

### Шаг 2: Верификация и vault

**Цель:** убедиться, что все тесты зелёные, обновить vault

**Файлы:** vault-файлы

**Что сделать:**
1. Полная сборка Release + Debug
2. `ctest --test-dir build -C Release` — все зелёные
3. `ctest --test-dir build -C Debug` — все зелёные
4. Обновить vault:
   - `vault/GDM/roadmap/известные баги и технический долг.md`: BUG-008 → ✅ ИСПРАВЛЕН
   - `vault/GDM/00-home/текущие приоритеты.md`: обновить snapshot если нужно
   - Создать `vault/GDM/knowledge/debugging/BUG-008 well pressure zero mobility.md`
   - Обновить `vault/GDM/00-home/index.md`
5. Прокомментировать GitHub issue #7

**Проверка:** все тесты зелёные (Release + Debug)

**Зависимости:** Требует: шаг 1

**Оценка:** ~5 минут

---

## Тестовая стратегия

**Тест-воспроизводитель:**
Прямой unit-тест для `SetRefWellPressure` затруднителен — требуется полная конструкция `WellFixedProduction` с сеткой, перфорациями, ячейками. Это уровень интеграционного теста. На данном этапе — regression через существующие 282 теста. Guard не меняет поведение при `denom > 0`.

Косвенная верификация: тест `TwoPhaseFlowCell: zero permeability gives finite properties` (из BUG-007) подтверждает, что `MobilityOverall() = 0` при `Permeability = 0`, что является триггером BUG-008.

**Regression:**
- Все 282 существующих теста (guard не срабатывает → поведение идентично)

**Примечание:** полноценный unit-тест для скважинного кода — задача рефакторинга (DEBT, приоритет 3). Скважины сейчас тесно связаны с `ReservoirSimulator` и не тестируются изолированно.

---

## Критерии завершения

- [ ] Guard в `SetRefWellPressure` — `denom == 0.0` → `P_well = avg(P_reservoir)`
- [ ] Все существующие тесты зелёные (Release + Debug)
- [ ] Vault обновлён: roadmap, debugging, priorities, index
- [ ] GitHub issue #7 прокомментирован
