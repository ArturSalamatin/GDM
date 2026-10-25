---
tags:
  - code-review
  - рефакторинг
  - мёртвый-код
date: 2026-06-28
---

# Code review 2026-06-28 — мёртвый код и гигиена

## CR-DEAD-001: Закомментированный код NeuralNetwork в ReservoirSimulator.cpp

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:1253-1361`

Больше 100 строк закомментированного кода `NeuralNetworkCL::NeuralNetwork::SaveWeights/LoadWeights` в конце файла. Не имеет отношения к симулятору.

---

## CR-DEAD-002: Закомментированный код IRAP, JSON, LogFile

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:282-350`

`pressure_json()` содержит ~70 строк закомментированного кода (IRAP-грид, LogFile-вызовы, `UniversalWriter`, wstring-манипуляции). Метод `pressure_json()` сам по себе вызывает сомнения — он конструирует JSON через wstring, но не используется в текущем workflow.

---

## CR-DEAD-003: `if (false && (nz > 1))` в `OilContourFlux` и `WaterContourFlux`

**Файл:** `HydroSolver/Reservoir/ReservoirSimulator.cpp:1141`

```cpp
if (false && (nz > 1))
```

Z-потоки через границу намеренно отключены. Это согласуется с решением [[вертикальные перетоки исключены из модели]], но `if (false)` — плохой способ документировать это. Код внутри блока (40 строк) мёртв.

---

## CR-DEAD-004: `wstring` повсюду

`OutputPath()` возвращает `std::wstring`, `WellName = std::wstring`, `itsGUID = std::wstring`, JSON-код использует `std::wstring`. Конверсия через `wcstombs_s` в `SomeWell::Name()`. Отмечено в приоритетах как DEBT — `wstring → string`.

---

## CR-DEAD-005: `update_currentIntegrationTau` — пустой метод

**Файл:** `HydroSolver/Reservoir/NumericalParameters.h:105`

```cpp
void update_currentIntegrationTau(double tau) {}
```

Пустой метод, нигде не вызывается. Мёртвый код.

---

## CR-DEAD-006: Дамп-файлы записываются безусловно

- `MER_Data::PrintMER()` — записывает `test_MER_*.txt` при каждой инициализации скважины
- `LinearProblem::PrintRHS/PrintCorrections/PrintCRS` — пишут `test_RHS.txt`, `test_Corrections.txt`, `test_Matrix.txt` в текущую директорию
- `SomeWell::PrintWell()` — создаёт `WellTestData/` и записывает данные при каждой инициализации
- `ReservoirSimulator::PrintWellCoords()` — пишет `ReservoirTestData//well_coordinates.txt`

Файлы пишутся в текущую рабочую директорию. Совпадает с DEBT-009 в приоритетах.

---

## CR-DEAD-007: Двойные слеши в путях

**Файл:** Несколько мест:
- `ReservoirTestData//` (ReservoirSimulator.cpp:10)  
- `WellTestData//` (SomeWell.cpp:93)

Windows допускает двойные слеши, но это cosmetic issue.

---

## CR-DEAD-008: `#pragma region` в заголовках

**Файл:** `HydroSolver/Reservoir/Well/SomeWell.h:99, 135, 145, 150, 152`

`#pragma region` — артефакт Visual Studio. Не несёт семантики, загромождает код.

---

## CR-DEAD-009: Закомментированные assert-ы

**Файл:** `HydroSolver/Solver/Grids/Cells/AbstractCells.h:60-61, 82-83, 110-111`

```cpp
//	assert(center.size() < 1);
//	assert(size.size() < 1);
```

Условия assert-ов ещё и некорректны: `center.size() < 1` означает «размер 0», что невозможно после конструирования.
