---
tags:
  - code-review
  - архитектура
  - рефакторинг
date: 2026-06-28
---

# Code review 2026-06-28 — архитектурные ограничения

## CR-ARCH-001: ReservoirSimulator — God Object (1361 строк)

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp` + `ReservoirSImulator.h`

Класс отвечает за:
- Конструирование сетки и скважин
- Newton solver loop
- Сборку якобиана (`fillMatrixBlockRow`, `AccountForBoundaryConditions`)
- Граничные условия
- Баланс масс
- Подсчёт потоков (`OverallFluxes`, `OilContourFlux`, `WaterContourFlux`)
- I/O (JSON, binary, text)
- Flow field управление

Совпадает с Приоритетом 3 в roadmap. `AccountForBoundaryConditions` определён прямо в заголовочном файле (~100 строк inline-кода в `.h`).

---

## CR-ARCH-002: Все члены класса `ReservoirSimulator` — public

**Файл:** `HydroSolver/Reservoir/ReservoirSImulator.h:39-72`

```cpp
class ReservoirSimulator
{
public:
    NumericalParameters numPrm;
public:
    double RefPressure = 100.0;
    OilField Grid;
    size_t ActiveCellsNmbr;
    LinearProblem MyProblem;
    std::map<WellName, wells::SomeWell*> Wells;
    // ...
    double prevOil, curOil, accumOil, accumOilOutFlux, accumDebet;
```

Инкапсуляция отсутствует. Любой клиент может изменить состояние солвера напрямую (Grid, MyProblem, Wells, accumulators). Тесты вынуждены работать через public members, что делает API хрупким.

---

## CR-ARCH-003: `B = 2` захардкожен как `constexpr`

**Файл:** Множественные — `LinearProblem.h`, `ReservoirSimulator`, `SomeWell.h:69`

```cpp
const int B = 2;  // SomeWell
static constexpr unsigned char B = 2;  // LinearProblem
```

Число уравнений на ячейку (`B = 2`: давление + насыщенность) — глобальная константа, размазанная по классам. Расширение до трёхфазной модели или добавление уравнения энергии потребует поиска и замены во всём коде.

---

## CR-ARCH-004: `OverallFluxes()` и `OilContourFlux()`/`WaterContourFlux()` — дублирование логики

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:794-1247`

Три метода (~450 строк суммарно) содержат по сути одинаковую структуру: тройной цикл по (i,j,k), вычисление `p_grad`, upstream-выбор `f_oil`/`f_water`, умножение на подвижность. Одинаковая логика повторяется для каждого направления (X, Y) и для граничных/внутренних ячеек.

Можно свести к одному параметризованному методу обхода граней.

---

## CR-ARCH-005: `const_cast` и `mutable` для обхода const-корректности

- `MER_Data::CleanMER_record()` объявлен `const`, но мутирует `MER_records` (объявлен `mutable`)
- `MER_Data::identify_cur_MER_record()` объявлен `const`, мутирует `monthID`, `curTime`, `curFluidDebit` и т.д. (все `mutable`)
- `WellJobs::RawWellPerforationData` — `mutable`
- `SomeWell` конструктор: `const_cast` на `perfs`

Паттерн: методы `const` de facto, но мутируют через `mutable`. Это означает, что const-корректность — фасад. Кэшированное состояние MER (curTime, monthID) в const-методах делает класс thread-unsafe.

---

## CR-ARCH-006: `SomeWell::Name()` конвертирует wstring→string через `wcstombs_s`

**Файл:** `HydroSolver/Reservoir/Well/SomeWell.cpp:60-66`

```cpp
std::string SomeWell::Name() const 
{
    std::string str;
    size_t size;
    str.resize(NameWide().length());
    wcstombs_s(&size, &str[0], str.size() + 1, NameWide().c_str(), NameWide().size());
    return str;
}
```

Этот `wcstombs_s` вызывается при каждом обращении к имени (включая debug-вывод). Потенциально locale-зависимый. Ещё один аргумент за миграцию `WellName` с `wstring` на `string`.

---

## CR-ARCH-007: Глубокая иерархия наследования для ячеек

```
SomeDimCell → Dim1Cell → Dim2Cell → Dim3Cell
                                        ↓
PhysPropCell + TimeDependentCell → SomeProcessCell_TimeDependent<Dim3Cell>
                                        ↓
                                  TwoPhaseFlowCell
```

6 уровней наследования (включая множественное). `Dim1Cell`/`Dim2Cell` никогда не инстанцируются отдельно. Наследование используется для доступа к `Center[0]`/`Center[1]`/`Center[2]` — это можно заменить одной структурой.

---

## CR-ARCH-008: Индексирование массивов через magic numbers

**Файл:** `TwoPhaseFlowCell.cpp`, `AbstractCells.h`

Свойства ячеек хранятся в `std::vector<double>` с доступом по числовым индексам:
- `ConstantFieldProperties[0]` = Permeability
- `ConstantFieldProperties[7]` = PoreVolume
- `DependentFieldProperties[3]` = F_Oil
- `VariableFieldProperties[0]` = SWater_Scaled

Accessor-методы скрывают это, но при любом изменении порядка свойств — молчаливая ошибка. Enum или именованная структура безопаснее.
