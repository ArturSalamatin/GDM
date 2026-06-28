---
tags:
  - code-review
  - память
  - ресурсы
date: 2026-06-28
---

# Code review 2026-06-28 — утечки ресурсов и управление памятью

## CR-MEM-001: Ручной `new`/`delete` для скважин в `ReservoirSimulator`

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:145-156`

```cpp
Wells.insert({ name,
    new wells::WellFixedProduction(...) });
// ...
ReservoirSimulator::~ReservoirSimulator()
{
    for (auto [name, p] : Wells)
        delete p;
}
```

`std::map<WellName, wells::SomeWell*>` с `new`/`delete` — классический source of leaks при исключениях между вставкой и деструктором. Если `AddWell_FixedProduction` бросает исключение после `new` но до `insert`, объект утекает. Деструктор итерирует по копии (`auto [name, p]`, не `auto& [name, p]`), что безвредно, но стилистически неаккуратно.

**Рекомендация:** `std::map<WellName, std::unique_ptr<wells::SomeWell>>` — удаляет деструктор полностью.

---

## CR-MEM-002: `PhysPropCell::ConstantPointProperties` — глобальный `static` для PVT-свойств

**Файл:** `HydroSolver/Solver/Grids/Cells/AbstractCells.h:148`, `AbstractCells.cpp:7`

```cpp
static std::array<double, 8> ConstantPointProperties;
```

Все ячейки разделяют один набор PVT-констант через `static`. Это:
- Делает невозможным запуск нескольких симуляций с разными PVT одновременно
- Thread-unsafe: race condition при параллельных расчётах
- Неявная глобальная зависимость, скрытая за `static` членом класса

---

## CR-MEM-003: `std::vector<double>` аллоцируется в каждом вызове `AccountForBoundaryConditions`

**Файл:** `HydroSolver/Reservoir/ReservoirSImulator.h:145-146`

```cpp
std::vector<double> blDiag = std::vector<double>(blockSize, 0);
std::vector<double> rhsBlock = std::vector<double>(B, 0);
```

Внутри тройного цикла по `(i, j, k)` на каждой итерации аллоцируется два `std::vector`. Для сетки 51×51×4 это тысячи heap-аллокаций за один Newton step. Следует использовать `std::array<double, 4>` (B=2, blockSize=4) или вынести аллокацию за цикл.

**То же самое** в `fillMatrixBlockRow` — но там уже правильно используется `double blDiag[blockSize]` на стеке.

---

## CR-MEM-004: `OverallFluxes()` создаёт O(nx·ny·nz) временных `std::vector`

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:794-1050`

Метод `OverallFluxes()` строит 4 трёхмерных `vector<vector<vector<double>>>` через `push_back`. Каждый `push_back(std::vector<double>())` — отдельная аллокация. Для сетки 51×51×4 это ~40 000 аллокаций, плюс возврат по значению четвёрки 3D-массивов.

Метод вызывается каждый шаг по времени из `AddFlowFieldSnapShot()`. На 100 шагов — 4M аллокаций.

---

## CR-MEM-005: `GetFlowFieldsPtr` копирует flowFields

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:167-176`

```cpp
vec.push_back(std::make_shared<phasePortrait::SomeFlowField>(flowFields[k]));
```

Каждый вызов создаёт `shared_ptr` с **копией** `SomeFlowField`. Если flowField содержит снимки скоростей за все шаги, это значительный объём данных.
