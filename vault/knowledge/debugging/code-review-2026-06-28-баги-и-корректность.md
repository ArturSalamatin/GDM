---
tags:
  - code-review
  - баги
  - корректность
date: 2026-06-28
---

# Code review 2026-06-28 — баги и корректность

## CR-BUG-001: `WellJobs::IsEmpty()` проверяет не тот контейнер

**Файл:** `HydroSolver/Reservoir/Well/WellJobs.cpp:93-98`

```cpp
bool WellJobs::IsEmpty() const
{
    for (size_t i = 0; i < RawWellPerforationData.size(); ++i)
        if (!RawWellPerforationData.empty())  // ← BUG: проверяется весь вектор, а не элемент [i]
            return false;
    return true;
}
```

Должно быть `!RawWellPerforationData[i].empty()`. Сейчас метод **всегда возвращает `false`**, если в `RawWellPerforationData` хотя бы один элемент (включая пустые слои). Скважина без реальных jobs не отсеивается.

**Последствия:** скважина с пустыми данными может попасть в симуляцию → undefined behavior при попытке построить перфорации.

---

## CR-BUG-002: `ActiveCells` всегда `true`

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:142`

```cpp
ActiveCells.push_back(cellIdx+1 > 0);
```

`cellIdx` имеет тип `size_t` (unsigned), поэтому `cellIdx + 1 > 0` **всегда true**. Конверсия из `ConvertTriple2Local` возвращает `size_t`, не `int`. Фильтр неактивных ячеек не работает.

---

## CR-BUG-003: `f_oil = f_oil = cell.F_Oil()`  — дублирование присваивания

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:908, 1029`

```cpp
f_oil = f_oil = cell.F_Oil();
```

Двойное присваивание — опечатка. Компилятор не генерирует ошибку, но это cosmetic bug, который сигнализирует о copy-paste OverallFluxes.

---

## CR-BUG-004: `f_water` объявляется, но не используется в `OverallFluxes`

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp` — множественные вхождения в `OverallFluxes()`

Переменная `f_water` присваивается значение, но нигде не участвует в вычислении потоков нефти/воды. В `OverallFluxes` фактически считается только `jOil` и `j` (overall), а `jWater` не вычисляется. При этом upstream-выбор `f_water` не влияет ни на какой результат — потенциальный источник ошибок при расширении.

---

## CR-BUG-005: `LoadFlowFieldFromFile_bin` хардкодит `nz = 4`

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:747`

```cpp
int nx = grid_dim[0], ny = grid_dim[1], nz = 4;// grid_dim[2];
```

Читается размерность сетки из бинарного файла, но `nz` игнорируется и заменяется на `4`. Код будет некорректно работать при любом другом числе слоёв.

---

## CR-BUG-006: `const_cast` на `AccumulatedPerforations` в конструкторе `SomeWell`

**Файл:** `HydroSolver/Reservoir/Well/SomeWell.cpp:143`

```cpp
const_cast<set_of_points::AccumulatedPerforations&>(perfs).AveragePerforationsOut(jobIntervals);
```

`const_cast` — обход const-контракта. `perfs` получен по `const auto&` из range-based for. Мутация через `const_cast` — undefined behavior если оригинальный объект const.

---

## CR-BUG-007: `throw` строкового литерала вместо исключения

**Файл:** `HydroSolver/Reservoir/Well/Wells.cpp:41`

```cpp
throw("well production is not determined. Date:" + std::to_string(nextTimeMoment));
```

Здесь `throw` без `std::exception()` — выбрасывается `std::string`, который не ловится `catch(std::exception&)`. Исключение утечёт мимо всех обработчиков.

---

## CR-BUG-008: `SomeWell::CurOilDebit_Num` — двойная точка с запятой

**Файл:** `HydroSolver/Reservoir/Well/SomeWell.cpp:171, 179`

```cpp
return overallDebit * f_oil_in;;
return overallDebit * (1 - f_oil_in);;
```

Безвредно (пустой statement), но показатель copy-paste.
