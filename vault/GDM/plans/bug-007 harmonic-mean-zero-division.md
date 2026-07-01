---
tags:
  - план
  - баг
date: 2026-07-01
issue: BUG-007
github: 6
branch: fix/bug-007/harmonic-mean-zero-division
status: готов к реализации
---

# BUG-007: Деление на ноль в гармоническом среднем подвижности

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[code-review-2026-06-28-числовая-устойчивость]] — CR-NUM-001
   - [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — BUG-002 (контекст CPR)
3. Создай ветку: `git checkout -b fix/bug-007/harmonic-mean-zero-division`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни baseline: **281 тест, ~51 сек, 280 pass** (1 flaky — `3D completions: partial perforation`, числовой шум 1e-10)
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание бага

Деление на ноль при вычислении гармонического среднего подвижности в нескольких местах кода. Гармоническое среднее: `H(a, b) = 2ab / (a+b)`. При `a = b = 0` знаменатель = 0 → NaN/Inf.

Физика: подвижность фазы `λ = k·kr(Sw)/μ`. Если проницаемость `k = 0` (непроницаемый барьер) или обе relperm = 0 (невозможно при стандартной Corey, но возможно при расширенных моделях или потере физичности Sw), суммарная подвижность `λ_total = λ_oil + λ_water = 0`.

Дополнительно: `F_Oil = λ_oil / λ_total` и `Derivative_F_Oil = (...)/ λ_total` в `TwoPhaseFlowCell::UpdateDependentFieldProperties()` делят на `MobilityOverall()` — это проявляется уже при инициализации ячейки, а не только при сборке Якобиана.

**Текущее поведение при стандартной модели Corey:**
- `RelativePermeabilityOil() = max(0, So_scaled)^3`, `RelativePermeabilityWater() = max(0, Sw_scaled)^3`
- `max(0, ...)` гарантирует relperm ≥ 0, и при любом Sw хотя бы одна relperm > 0
- Поэтому `MobilityOverall > 0` ВСЕГДА при `Permeability > 0`
- Баг проявляется при `Permeability = 0` (непроницаемый барьер) или при будущих расширениях модели relperm

**Критичность:** средняя. При текущей модели relperm баг не проявляется (Corey + max(0,...) защищает). Но это латентный дефект — при расширении модели (табличные relperm, другие корреляции, барьеры проницаемости) NaN появится без предупреждения.

---

## Затронутые файлы

| Файл | Строки | Роль | Количество делений на (a+b) |
|---|---|---|---|
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 579-584 | Якобиан: `fillMatrixBlockRow()` — `MeanOverallMobility`, `c1`, `c3` | 3 деления |
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 870-871 | `OverallFluxes()` Y-direction | 1 деление |
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | 999-1000 | `OverallFluxes()` X-direction | 1 деление |
| `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.h` | 34 | `UpdateDependentFieldProperties()`: `F_Oil = oil_mobility / MobilityOverall()` | 1 деление |
| `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.h` | 46 | `UpdateDependentFieldProperties()`: `Derivative_F_Oil = (...) / MobilityOverall()` | 1 деление |

---

## Цепочка причинно-следственных связей

```
Permeability = 0  ИЛИ  будущая модель relperm без max(0,...)
        │
        ▼
MobilityOverall() = 0  (обе λ_oil и λ_water = 0)
        │
        ├──► UpdateDependentFieldProperties(): F_Oil = 0/0 = NaN
        │    Derivative_F_Oil = 0/0 = NaN
        │    (при инициализации или UpdateState каждой ячейки)
        │
        └──► fillMatrixBlockRow():
             denom = 0 + 0 = 0
             MeanOverallMobility = 0/0 = NaN
             c1 = (0/0)^2 * ... = NaN
             c3 = (0/0)^2 * ... = NaN
             ↓
             blOffDiag[*] = NaN, blDiag[*] = NaN, rhsBlock[*] = NaN
             ↓
             Матрица Якобиана содержит NaN → AMG diverges
             ↓
             (После BUG-006) converged = isfinite(NaN) = false
             → ReverseState() + decrease_schemeTau()
             → wasted trials пока schemeTau не станет достаточно малым
```

---

## Целевое состояние

1. При `MobilityOverall = 0` (или `denom = 0` для пары ячеек):
   - `F_Oil = 0`, `F_Water = 0`, `Derivative_F_Oil = 0` (нет потока → нет фракции)
   - `MeanOverallMobility = 0` (нет потока между ячейками)
   - Никаких NaN в Якобиане, RHS или dependent properties
2. Все 281 существующий тест зелёные
3. Поведение на стандартных задачах (Sw ∈ (0, 1), Permeability > 0) **не меняется** — guard clause не срабатывает

---

## Варианты решения

### Вариант A: Guard clause в fillMatrixBlockRow + UpdateDependentFieldProperties

**Суть:** проверка `denom > 0` перед делением, skip или return 0

**Изменения:**
1. `fillMatrixBlockRow` — `if (denom == 0.0) continue;` после строки 579
2. `OverallFluxes` — аналогичный guard на строках 870, 999
3. `UpdateDependentFieldProperties` — `if (MobilityOverall() > 0.0) { ... } else { F_Oil = 0; Derivative_F_Oil = 0; }`

**Плюсы:** минимальное изменение, локальный фикс, физически корректен (нулевая подвижность = нулевой поток = нулевой вклад в Якобиан)
**Минусы:** повторяющийся паттерн (copy-paste guards). Не решает проблему на архитектурном уровне.
**Риски:** при `continue` в fillMatrixBlockRow пропускается вклад соседа в `blDiag` (строки 600-611: `OilMobilitySum, WaterMobilitySum`). Если `denom = 0`, то `MeanOverallMobility = 0` → все `c_oil, c_water = 0` → сумма += 0. Значит skip = добавить 0, что корректно.
**Совместимость:** не ломает существующее поведение — guard не срабатывает при `denom > 0`.
**Трудоёмкость:** 3 файла, ~15 строк.

### Вариант B: Inline utility `safe_harmonic_mean`

**Суть:** вынести гармоническое среднее в функцию с guard

```cpp
inline double safe_harmonic_mean(double a, double b) {
    double denom = a + b;
    return (denom > 0.0) ? 2.0 * a * b / denom : 0.0;
}
```

**Плюсы:** DRY, покрывает все 3 места в ReservoirSimulator одной функцией.
**Минусы:** не покрывает `UpdateDependentFieldProperties` (там деление на `MobilityOverall`, не гармоническое среднее). Производные `c1, c3` используют `denom` отдельно — нужна дополнительная обработка.
**Риски:** рефакторинг `c1, c3` — они используют `OverallMobilityNeighbour / denom` и `OverallMobilityCell / denom`, это не harmonic mean. Нужен отдельный guard или обёртка.
**Совместимость:** поведение при `denom > 0` идентично.
**Трудоёмкость:** 3 файла, ~20 строк + новая функция.

### Выбор: Вариант A (guard clauses)

Причины:
1. Минимальный фикс с максимальной читаемостью
2. Вариант B не покрывает `UpdateDependentFieldProperties` и `c1/c3` — всё равно нужны guards
3. Utility function имеет смысл после рефакторинга ReservoirSimulator (DEBT, приоритет 3)
4. Guard `continue` физически корректен: при нулевой подвижности обеих ячеек поток = 0

---

## Чеклист подводных камней

- ✅ **Побочные эффекты:** `fillMatrixBlockRow` вызывается из `AssembleMyProblem` → `#pragma omp parallel for`. Каждый `l` независим. `continue` для внутреннего цикла по соседям — безопасно.
- ✅ **Потокобезопасность:** `continue` внутри вложенного `for (neibCount)` — не затрагивает OMP.
- ✅ **Граничные случаи:**
  - `denom = 0`: guard → skip, корректно
  - `denom > 0, один из Mobility = 0`: `MeanOverallMobility = 0` (harmonic mean с нулём = 0), нет деления на ноль
  - `denom > 0, оба > 0`: нормальный путь, guard не срабатывает
- ✅ **Производительность:** одна проверка `if (denom == 0.0)` — O(1), горячий цикл не замедляется.
- ✅ **Обратная совместимость:** при `denom > 0` (все текущие тесты) guard не срабатывает → поведение идентично.
- ✅ **Порядок вызовов:** `UpdateDependentFieldProperties` вызывается из `UpdateState`/конструктора → guard в `UpdateDependentFieldProperties` выполнится до `fillMatrixBlockRow`.
- ✅ **Численная устойчивость:** `denom == 0.0` точное сравнение корректно — `MobilityOverall ≥ 0` (max(0,...) в relperm), сумма двух неотрицательных чисел = 0 iff оба = 0.
- ✅ **Связь с другими задачами:** BUG-002 (CPR zero pivot) — NaN в Якобиане вызывает zero pivot. Фикс BUG-007 уменьшит частоту NaN → меньше срабатываний CPR fallback.
- ✅ **Зависимости сборки:** нет новых include, нет CMake-изменений.

---

## Обнаруженные проблемы

Нет новых проблем. Отсутствие clipping Sw при `UpdateState` — известный DEBT (не блокирует BUG-007, т.к. при текущей Corey модели `MobilityOverall > 0` всегда при `Permeability > 0`).

---

## Шаги реализации

Минимальный фикс = архитектурное решение (guard clauses). Этапы 2-3 и 4-5 объединены.

### Шаг 1: Guard в UpdateDependentFieldProperties (TwoPhaseFlowCell)

**Цель:** предотвратить NaN в `F_Oil` и `Derivative_F_Oil` при `MobilityOverall() = 0`

**Файлы:** `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.h`

**Контекст:**
`UpdateDependentFieldProperties()` вычисляет зависимые свойства ячейки при каждом `UpdateState()`. Строка 34: `F_Oil = oil_mobility / MobilityOverall()` — fractional flow нефти. Строка 46: `Derivative_F_Oil = (...) / MobilityOverall()` — производная fractional flow по Sw. При `MobilityOverall = 0` (нулевая проницаемость) обе дают NaN. Физически: при нулевой подвижности потока нет → fractional flow не определена, но для расчётов безопасно установить 0.

**Что сделать:**
1. В файле `TwoPhaseFlowCell.h`, функция `UpdateDependentFieldProperties()`, строки ~32-46:
   Обернуть вычисления F_Oil и Derivative_F_Oil в `if (MobilityOverall() > 0.0)`, иначе установить 0.

**Изменения (старый → новый код):**

До (строки 32-46):
```cpp
DependentFieldProperties[2] = oil_mobility + MobilityWater(); // overall mobility

DependentFieldProperties[3] = oil_mobility / MobilityOverall(); // oil fraction of flowing stream, f_Oil
DependentFieldProperties[4] = 1 - F_Oil(); // water fraction of flowing stream, f_Water

double oil_volume = OilVolume(); // oil volume
double water_volume = WaterVolume(); // water volume

DependentFieldProperties[5] = oil_volume * DensityOil(); // oil mass
DependentFieldProperties[6] = water_volume * DensityWater(); // water mass

DependentFieldProperties[7] = Permeability() * DerivativeRelativePermeabilityOil() / ViscousityOil();
DependentFieldProperties[8] = Permeability() * DerivativeRelativePermeabilityWater() / ViscousityWater();

DependentFieldProperties[9] = (F_Water() * DerivativeMobilityOil() - F_Oil() * DerivativeMobilityWater()) / MobilityOverall();
```

После:
```cpp
DependentFieldProperties[2] = oil_mobility + MobilityWater(); // overall mobility

if (MobilityOverall() > 0.0) {
    DependentFieldProperties[3] = oil_mobility / MobilityOverall(); // f_Oil
    DependentFieldProperties[4] = 1 - F_Oil(); // f_Water
} else {
    DependentFieldProperties[3] = 0.0;
    DependentFieldProperties[4] = 0.0;
}

double oil_volume = OilVolume();
double water_volume = WaterVolume();

DependentFieldProperties[5] = oil_volume * DensityOil(); // oil mass
DependentFieldProperties[6] = water_volume * DensityWater(); // water mass

DependentFieldProperties[7] = Permeability() * DerivativeRelativePermeabilityOil() / ViscousityOil();
DependentFieldProperties[8] = Permeability() * DerivativeRelativePermeabilityWater() / ViscousityWater();

if (MobilityOverall() > 0.0) {
    DependentFieldProperties[9] = (F_Water() * DerivativeMobilityOil() - F_Oil() * DerivativeMobilityWater()) / MobilityOverall();
} else {
    DependentFieldProperties[9] = 0.0;
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release` (все тесты зелёные — guard не срабатывает при стандартных условиях)
- Ожидаемый результат: 280/281 pass (тот же flaky)

**Подводные камни:**
- `F_Oil = 0, F_Water = 0` при нулевой подвижности: нарушает `F_Oil + F_Water = 1`. Физически: доля потока не определена при отсутствии потока. Для Якобиана это безопасно — `MeanOverallMobility = 0` → все члены с F_Oil/F_Water умножаются на 0.

**Зависимости:** нет
**Блокирует:** шаг 2 (Якобиан зависит от F_Oil, Derivative_F_Oil)

**Оценка:** ~10 строк, ~5 минут

---

### Шаг 2: Guard в fillMatrixBlockRow (Якобиан)

**Цель:** предотвратить NaN при `denom = OverallMobilityCell + OverallMobilityNeighbour = 0`

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
`fillMatrixBlockRow(l, loc_tau)` собирает строку Якобиана для ячейки `l`. Для каждого соседа `neibCount` вычисляется `denom = OverallMobilityCell + OverallMobilityNeighbour` (строка 579), затем деление на `denom` в трёх местах: `MeanOverallMobility` (строка 581), `c1` (строка 583), `c3` (строка 584). При `denom = 0` все три дают NaN.

Физический смысл guard: если суммарная подвижность пары ячеек = 0, потока между ними нет → вклад этого соседа в Якобиан = 0. `continue` пропускает:
- `blOffDiag[*]` — не накапливается (остаётся 0 — инициализирован в строке 560)
- `blDiag[*]` через `bwBlock` — не накапливается (c1 = 0 → вклад = 0)
- `OilMobilitySum, WaterMobilitySum` — не увеличиваются (c_oil = c_water = 0 при MeanOverallMobility = 0)
- `rhsBlock[*]` — не увеличивается
- `AddOffDiagBlock(l, neibCount, blOffDiag)` (строка 607) — не вызывается. Это безопасно: off-diag блок останется нулевым (ResetProblem обнуляет матрицу).

ВАЖНО: `AddOffDiagBlock` вызывается для КАЖДОГО соседа в обычном пути (строка 607, после цикла). Если `continue` пропускает этого соседа, off-diag блок для него не записывается. Это корректно — матрица обнулена в `ResetProblem`, и нулевой блок = нулевой вклад.

**Что сделать:**
1. В файле `ReservoirSimulator.cpp`, функция `fillMatrixBlockRow()`, после строки 579 (`double denom = ...`):
   Добавить `if (denom == 0.0) continue;`

**Изменения (старый → новый код):**

До (строки 579-581):
```cpp
double denom = OverallMobilityCell + OverallMobilityNeighbour;

double MeanOverallMobility = 2 * OverallMobilityNeighbour * OverallMobilityCell / denom;
```

После:
```cpp
double denom = OverallMobilityCell + OverallMobilityNeighbour;
if (denom == 0.0) continue;

double MeanOverallMobility = 2 * OverallMobilityNeighbour * OverallMobilityCell / denom;
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release` (все тесты зелёные)
- Ожидаемый результат: 280/281 pass

**Подводные камни:**
- `continue` пропускает `AddOffDiagBlock(l, neibCount, blOffDiag)` — корректно, т.к. `blOffDiag` инициализирован нулями (строка 560) и не был модифицирован до guard.
- Учёт пропущенного `neibCount` в нумерации: `neibCount` — позиция в `neighbourCells`, не cell index. `continue` корректно пропускает одного соседа.

**Зависимости:** Требует: шаг 1 (F_Oil должен быть 0, а не NaN)
**Блокирует:** шаг 3

**Оценка:** ~1 строка, ~2 минуты

---

### Шаг 3: Guard в OverallFluxes (поле скоростей)

**Цель:** предотвратить NaN в вычислении поля скоростей для линий тока

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
`OverallFluxes()` вычисляет поле скоростей для визуализации / линий тока. Содержит копии гармонического среднего:
- Строки 870-871: Y-direction flux
- Строки 999-1000: X-direction flux

Тот же паттерн: `2 * a * b / (a + b)`. При `a + b = 0` → NaN. `OverallFluxes` не используется в Newton loop, только для post-processing.

**Что сделать:**
1. Строки 870-871: заменить деление на guard — если `(a + b) == 0`, то `mobility = 0`
2. Строки 999-1000: аналогично

**Изменения (старый → новый код):**

До (строки 870-871):
```cpp
double mobility = 2 * cellNeighbour.MobilityOverall() * cell.MobilityOverall() /
    (cellNeighbour.MobilityOverall() + cell.MobilityOverall());
```

После:
```cpp
double mob_sum = cellNeighbour.MobilityOverall() + cell.MobilityOverall();
double mobility = (mob_sum > 0.0) ? 2.0 * cellNeighbour.MobilityOverall() * cell.MobilityOverall() / mob_sum : 0.0;
```

Повторить для строк 999-1000.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release` (все тесты зелёные)

**Зависимости:** Требует: шаг 1 (без guard в UpdateDependentFieldProperties `F_Oil = NaN` → `f_oil * mobility = NaN * 0 = NaN` по IEEE 754, даже при `mobility = 0`)
**Блокирует:** шаг 4

**Оценка:** ~4 строки, ~3 минуты

---

### Шаг 4: Unit-тест для нулевой подвижности

**Цель:** верифицировать, что NaN не генерируются при `MobilityOverall = 0`

**Файлы:** `tests/unit/physics/test_TwoPhaseFlowCell.cpp`

**Контекст:**
Существующие тесты проверяют `TwoPhaseFlowCell` только при `Sw ∈ {0.2, 0.5, 0.9}` — всегда `MobilityOverall > 0`. Нужен тест с `Permeability = 0` → `MobilityOverall = 0`.

**Что сделать:**
1. Добавить тест `TwoPhaseFlowCell: zero permeability gives finite dependent properties`
2. Создать ячейку с `Permeability = 0` через `make_cell(0.5, 1e7, 0.0)`
3. Проверить: `isfinite(F_Oil)`, `isfinite(F_Water)`, `isfinite(Derivative_F_Oil)`, `MobilityOverall() == 0.0`, `F_Oil() == 0.0`

**Изменения:**

После последнего теста в файле (после строки ~110) добавить:

```cpp
TEST_CASE("TwoPhaseFlowCell: zero permeability gives finite properties",
          "[unit][level3][physics][TwoPhaseFlowCell]") {
    auto cell = make_cell(0.5, 1e7, 0.0);
    CHECK(cell.MobilityOverall() == 0.0);
    CHECK(cell.F_Oil() == 0.0);
    CHECK(cell.F_Water() == 0.0);
    CHECK(cell.Derivative_F_Oil() == 0.0);
    CHECK(std::isfinite(cell.OilMass()));
    CHECK(std::isfinite(cell.WaterMass()));
}
```

Добавить `#include <cmath>` в начало файла, если отсутствует.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "zero permeability"` — зелёный
- Регрессия: `ctest --test-dir build -C Release` — 281/282 pass

**Зависимости:** Требует: шаг 1 (без guard тест упадёт — F_Oil = NaN)
**Блокирует:** шаг 5

**Оценка:** ~10 строк, ~5 минут

---

### Шаг 5: Верификация и vault

**Цель:** убедиться, что все тесты зелёные, обновить vault

**Файлы:** vault-файлы

**Что сделать:**
1. Полная сборка Release + Debug
2. `ctest --test-dir build -C Release` — все зелёные
3. `ctest --test-dir build -C Debug` — все зелёные
4. Обновить vault:
   - `vault/GDM/roadmap/известные баги и технический долг.md`: BUG-007 → ✅ ИСПРАВЛЕН
   - `vault/GDM/00-home/текущие приоритеты.md`: обновить snapshot
   - Создать `vault/GDM/knowledge/debugging/BUG-007 harmonic mean zero division.md`
   - Обновить `vault/GDM/00-home/index.md`
5. Прокомментировать GitHub issue #6
6. Закрыть issue #6

**Проверка:** все тесты зелёные (Release + Debug)

**Зависимости:** Требует: шаги 1-4

**Оценка:** ~5 минут

---

## Тестовая стратегия

**Тест-воспроизводитель:**
- `TwoPhaseFlowCell: zero permeability gives finite properties` (шаг 4)
- Без фикса: `F_Oil() = NaN` → CHECK falls
- С фиксом: `F_Oil() = 0.0` → CHECK pass

**Граничные тесты:**
- `Permeability = 0`: обе Mobility = 0, guard срабатывает
- `Sw = Sw_res` (0.2): MobilityWater = 0, MobilityOil > 0, guard НЕ срабатывает
- `Sw = 1 - So_res` (0.9): MobilityOil = 0, MobilityWater > 0, guard НЕ срабатывает

**Инвариантные тесты:**
- `F_Oil + F_Water = 1` при `MobilityOverall > 0`
- `F_Oil = F_Water = 0` при `MobilityOverall = 0`
- `isfinite(все dependent properties)` при любых входных данных

**Regression:**
- Все 281 существующий тест (guard не срабатывает → поведение идентично)

---

## Критерии завершения

- [ ] Тест `zero permeability gives finite properties` зелёный
- [ ] Все существующие тесты зелёные (Release + Debug)
- [ ] Нет NaN в `F_Oil`, `F_Water`, `Derivative_F_Oil` при `MobilityOverall = 0`
- [ ] Нет NaN в Якобиане при `denom = 0` в `fillMatrixBlockRow`
- [ ] Vault обновлён: roadmap, debugging, priorities, index
- [ ] GitHub issue #6 закрыт
