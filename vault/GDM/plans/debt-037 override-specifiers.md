---
tags:
  - план
  - рефакторинг
date: 2026-07-17
issue: DEBT-037
github: 37
branch: refactor/debt-037/override-specifiers
status: в процессе
audit:
  date: 2026-07-17
  runs: 2
  findings: 0 критических / 0 существенных / 0 мелких
  auto-fixed: 0
  manual-required: 0
  previous:
    date: 2026-07-17
    findings: 1 критических / 2 существенных / 1 мелких
    auto-fixed: 4
    manual-required: 0
---

# DEBT-037: добавить `override` ко всем переопределённым виртуальным методам

## Контекст

168 объявлений `virtual` в проектных заголовочных файлах, из них только 2 в продакшен-коде используют `override` (`Wells.h:24`, `WellDataHandler.h:277`). Тестовые кейсы (`SimulationCase` → наследники) уже корректно используют `override` (~48 мест).

Без `override` изменение сигнатуры виртуального метода в базовом классе приведёт к тому, что наследник молча создаст новый метод вместо переопределения — компилятор не выдаст ошибку.

**Подтип:** рефакторинг — чисто синтаксическое изменение, не затрагивающее поведение.

**C++ Core Guidelines C.128:** при переопределении виртуальной функции — `override` без `virtual`.

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-037/override-specifiers`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Baseline: **313 тестов**, все зелёные
6. Начни с шага 1. После каждого шага: сборка + тесты

## Обнаруженные проблемы

### ПРОБЛЕМА: Config_JSON hiding (14 методов)

При анализе обнаружено: `Config_JSON.h` содержит 14 методов с `virtual`, которые **НЕ переопределяют** виртуальный метод базового класса, а **скрывают** (hiding) не-виртуальный метод из `SchemeParamaters`, `AnomalyDetectionProperties` или `Config`:

**Hiding из `SchemeParamaters`** (методы в `SchemeParamaters` — не виртуальные, обычные):
- `RequiredNewtonTol()` (Config_JSON.h:114)
- `NewtonMaxIterCount()` (Config_JSON.h:130) — тип тоже отличается: base `size_t`, derived `int`
- `AMG_RelTol()` (Config_JSON.h:131)
- `AMG_AbsTol()` (Config_JSON.h:132)
- `MinCellThickness()` (Config_JSON.h:139)
- `MinPorosity()` (Config_JSON.h:140)
- `MinPermeability()` (Config_JSON.h:141)

**Hiding из `AnomalyDetectionProperties`** (методы — не виртуальные, обычные):
- `VelocityMultiplier()` (Config_JSON.h:119)
- `InitialTrajectoryDistance()` (Config_JSON.h:120)
- `StartSignalRollbackTime()` (Config_JSON.h:127)
- `EndSignalRollbackTime()` (Config_JSON.h:128)

**Hiding из `Config`** (методы в Config не виртуальные):
- `g()` (Config_JSON.h:115) — в Config.h:66 стал `constexpr`, не виртуальный
- `WaterPhaseProperties()` (Config_JSON.h:98) — в Config.h:68 не виртуальный
- `OilPhaseProperties()` (Config_JSON.h:106) — в Config.h:69 не виртуальный

Эти методы **нельзя** пометить `override` — компилятор выдаст ошибку.

**Действие в рамках DEBT-037:** к hiding-методам — убрать ложный `virtual` (они не переопределяют ничего). Полноценное исправление hiding (сделать базовые методы виртуальными или перестроить иерархию) — отдельная задача, зарегистрировать как DEBT-059.

---

## Инвентаризация изменений

### Файлы с настоящими override (virtual → override)

| Файл | Класс | Базовый класс | Методы | Кол-во |
|---|---|---|---|---|
| `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.h` | `TwoPhaseFlowCell` | `PhysPropCell`, `TimeDependentCell` | `ApplyPhysicalConstraints`, `UpdateDependentFieldProperties`, `SetPreviousStateDependentFieldProperties` | 3 |
| `HydroSolver/Helpers/Config_JSON.h` | `Config_JSON` | `Config` | `GetWideProjectPath`, `LayerAggregation`, `ExtBoundaryPressure`, `StartTimeStep`, `WaterSaturation_fileName`, `OilSaturation_fileName`, `Pressure_fileName`, `OverallBalance_fileName` | 8 (+ ещё `GetProjectPath`, `GetWideProject_FileName_Full`, `GetProject_FileName_Full` унаследованы без переопределения) |
| `HydroSolver/Utils/WellDataHandler.h` | `PerfData` и др. (10 классов) | `IData` | `Push(wstring, wstring)` | 10 |
| `HydroSolver/Utils/WellDataHandler.h` | `MerData` | `IData` | `GetValue` (уже имеет override, но также `virtual`) | 1 (убрать `virtual`, оставить `override`) |
| `HydroSolver/Utils/UniversalSVWriter.h` | `UTF8Writer` | `ISVWriter` | `Write` | 1 |
| `HydroSolver/Utils/UniversalSVParser.h` | `CP1251FileParser` | `IFileParser` | `Read`, `ReadArray` | 2 |
| `HydroSolver/Utils/UniversalSVParser.h` | `UTF8FileParser` | `IFileParser` | `Read`, `ReadArray` | 2 |
| `HydroSolver/Utils/JSON/JSONCreate.h` | `JValue`, `JArray`, `JObject` | `IJObject` | `Value` | 3 |
| `HydroSolver/Utils/IRCGEngine.h` | `IrapClassicGrid` | `IGrid` | `Add`, `Write` | 2 |
| `HydroSolver/Utils/IRCGEngine.h` | `XYZGrid` | `IGrid` | `Add`, `Write` | 2 |
| `GridEngine/GridEngine/DBUtils.h` | `DBSaver` | `IGrdeclSaver` | `SetObject`, `ResetGRDECL`, `ResetGRID`, `GetContour` | 4 |
| `GridEngine/GridEngine/DBUtils.h` | `TempGRIDSaver` | `IGrdeclSaver` | `SetObject`, `ResetGRDECL`, `ResetGRID`, `GetContour` | 4 |
| `GridEngine/GridEngine/DBUtils.h` | `DBLoader` | `IGrdeclLoader` | `GetModelParams`, `LoadPillars`, `LoadZCORN`, `LoadValues`, `GetAvailableGRIDS`, `GetIdxLayerTable`, `GetLayerCoefs` | 7 |
| `GridEngine/GridEngine/DBUtils.h` | `TempGRDLoader` | `IGrdeclLoader` | те же 7 | 7 |
| `GridEngine/GridEngine/UniversalSVParser.h` | `CP1251FileParser`, `UTF8FileParser` | `IFileParser` | `Read`, `ReadArray` | 4 |
| `GridEngine/GridEngine/JSONCreate.h` | `JValue`, `JArray`, `JObject` | `IJObject` | `Value` | 3 |

**Итого настоящих override:** ~63 метода

### Файлы с hiding (virtual → убрать virtual)

| Файл | Класс | Методы | Кол-во |
|---|---|---|---|
| `HydroSolver/Helpers/Config_JSON.h` | `Config_JSON` | см. раздел «Обнаруженные проблемы» выше | 14 |

### Файлы без изменений

- `HydroSolver/Reservoir/Well/SomeWell.h` — `AddWellToMatrix` — pure virtual в самом `SomeWell`, это базовый класс, не наследник. Override уже есть в `Wells.h:24`
- `HydroSolver/Reservoir/AnomalySimulator.h` — пустой наследник `ReservoirSimulator`, методов нет
- `HydroSolver/Utils/JSON/geojson.h` — `virtual ~GeoJsonEngine()` — деструктор базового класса, не наследник
- `HydroSolver/Reservoir/ReservoirSimulator.h` — `virtual ~ReservoirSimulator()` — деструктор, не переопределение
- `HydroSolver/Solver/Math/LinearProblem.h` — `virtual ~LinearProblem()` — деструктор, не переопределение
- `HydroSolver/Solver/Math/MatrixCSR.h` — `virtual ~MatrixCSR()` — деструктор, не переопределение
- Тестовые кейсы (`SimulationCase.h`, `MultiLayerCase.h`, `SingleInjectorCase.h`, `TwoWellCase.h`, `VariableDebitCase.h`) — уже корректно используют `override`
- Шаблонные методы `IEncodingParser<_chartype>` — базовый класс, не переопределения

### Метод с override без virtual → оставить

- `Wells.h:24` — уже корректно: `CellNumericalData AddWellToMatrix(double nextTimeMoment) override;`

### Особые случаи

1. **`WellDataHandler.h:277`** — `MerData::GetValue` — уже `virtual ... override`. Нужно убрать `virtual`, оставить только `override`.
2. **`WellDataHandler.h`** — вторая перегрузка `Push(wstring)` в каждом наследнике — это **НЕ** переопределение (сигнатура отличается от базовой `Push(wstring, wstring) = 0`). Оставить без изменений. Некоторые из них помечены `virtual`, некоторые нет — это новые методы, `virtual` для них не нужен, но его удаление выходит за scope DEBT-037 (методы не переопределяют ничего).
3. **`RaschData::Push(wstring, wstring)` (строка 356)** — `virtual`, это override `IData::Push`. Нужен `override`. А `Push(wstring path_to_rasch)` (строка 357) — **не** virtual и не override.
4. **`WellCoordData::Push(wstring, wstring)` (строка 362)** — аналогично, override нужен.

---

## Чеклист подводных камней

- [x] **Все call sites найдены:** grep по `virtual` в наследниках — полный список выше
- [x] **Потокобезопасность:** не затрагивается (только сигнатуры, не поведение)
- [x] **Зависимости сборки:** не меняются (не трогаем CMake в шагах 1–8)
- [x] **Обратная совместимость API:** `override` — чисто декларативное, ABI не меняется
- [x] **Тесты:** все существующие тесты (313 штук) должны оставаться зелёными
- [x] **Кодировки:** не затрагиваются
- [x] **Платформозависимость:** не затрагивается
- [x] **Мёртвый код:** не удаляем код, только добавляем/убираем спецификаторы
- [x] **Связь с другими задачами:** DEBT-038 (виртуальный деструктор SomeGrid) — независима; обнаруженный hiding → отдельная задача DEBT-059
- [x] **Производительность:** не затрагивается (ABI идентичен)

---

## Шаги

### Шаг 1: TwoPhaseFlowCell.h — 3 метода

**Цель:** пометить переопределённые методы в `TwoPhaseFlowCell` спецификатором `override`.

**Файлы:** `HydroSolver/Solver/Grids/Cells/TwoPhaseFlowCell.h`

**Контекст:**
`TwoPhaseFlowCell : public SomeProcessCell_TimeDependent<Dim3Cell>` переопределяет три метода из базовых классов `PhysPropCell` и `TimeDependentCell` (определены в `AbstractCells.h`):
- `ApplyPhysicalConstraints()` — pure virtual в `PhysPropCell` (строка 154)
- `UpdateDependentFieldProperties()` — pure virtual в `PhysPropCell` (строка 153)
- `SetPreviousStateDependentFieldProperties()` — pure virtual в `TimeDependentCell` (строка 122)

**Что сделать:**

Строка 21 — заменить:
```cpp
// до:
virtual void ApplyPhysicalConstraints();
// после:
void ApplyPhysicalConstraints() override;
```

Строки 23–56 — заменить:
```cpp
// до:
virtual void UpdateDependentFieldProperties()
{
    // ... тело ...
}
// после:
void UpdateDependentFieldProperties() override
{
    // ... тело без изменений ...
}
```

Строка 58 — заменить:
```cpp
// до:
virtual void SetPreviousStateDependentFieldProperties();
// после:
void SetPreviousStateDependentFieldProperties() override;
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 313 тестов должны остаться зелёными

**Оценка:** 3 строки, ~2 минуты

---

### Шаг 2: UniversalSVWriter.h — 1 метод

**Цель:** пометить переопределённый метод `Write` в `UTF8Writer` спецификатором `override`.

**Файлы:** `HydroSolver/Utils/UniversalSVWriter.h`

**Контекст:**
`UTF8Writer : public ISVWriter` переопределяет `Write(std::wstring content)` — pure virtual в `ISVWriter` (строка 12). Метод `Close()` в `ISVWriter` — virtual с телом (строка 13), но `UTF8Writer` его не переопределяет, так что Close не трогаем.

**Что сделать:**

Строка 29 — заменить:
```cpp
// до:
virtual void Write(std::wstring content);
// после:
void Write(std::wstring content) override;
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`

**Оценка:** 1 строка, ~1 минута

---

### Шаг 3: UniversalSVParser.h (HydroSolver) — 4 метода

**Цель:** пометить переопределённые методы в `CP1251FileParser` и `UTF8FileParser`.

**Файлы:** `HydroSolver/Utils/UniversalSVParser.h`

**Контекст:**
`IFileParser` объявляет pure virtual `Read()` и `ReadArray()` (строки 136–137).
`CP1251FileParser : public IFileParser` и `UTF8FileParser : public IFileParser` переопределяют оба.

**Что сделать:**

CP1251FileParser (строки 146–147):
```cpp
// до:
virtual std::wstring Read();
virtual std::vector<wchar_t> ReadArray();
// после:
std::wstring Read() override;
std::vector<wchar_t> ReadArray() override;
```

UTF8FileParser (строки 160–161):
```cpp
// до:
virtual std::wstring Read();
virtual std::vector<wchar_t> ReadArray();
// после:
std::wstring Read() override;
std::vector<wchar_t> ReadArray() override;
```

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 4 строки, ~2 минуты

---

### Шаг 4: UniversalSVParser.h (GridEngine) — 4 метода

**Цель:** аналогичные изменения в дубликате файла.

**Файлы:** `GridEngine/GridEngine/UniversalSVParser.h`

**⚠️ GridEngine не входит в CMake-сборку GDM** — этот модуль не компилируется, не тестируется, изменения нельзя проверить сборкой. Применять по аналогии с шагом 3; верификация — только визуальная.

**Контекст:**
Дублирующий файл с идентичной структурой. Базовый класс `IFileParser` — строки 139–140.

**Что сделать:**

CP1251FileParser (строки 149–150):
```cpp
// до:
virtual std::wstring Read();
virtual std::vector<wchar_t> ReadArray();
// после:
std::wstring Read() override;
std::vector<wchar_t> ReadArray() override;
```

UTF8FileParser (строки 163–164):
```cpp
// до:
virtual std::wstring Read();
virtual std::vector<wchar_t> ReadArray();
// после:
std::wstring Read() override;
std::vector<wchar_t> ReadArray() override;
```

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 4 строки, ~2 минуты

---

### Шаг 5: JSONCreate.h (HydroSolver) — 3 метода

**Цель:** пометить `Value()` в наследниках `IJObject`.

**Файлы:** `HydroSolver/Utils/JSON/JSONCreate.h`

**Контекст:**
`IJObject` объявляет `virtual std::map<std::string, std::unique_ptr<IJObject>> Value() = 0` (строка 137).
`JValue`, `JArray`, `JObject` наследуют и переопределяют.

**Что сделать:**

JValue (строка 151):
```cpp
// до:
virtual std::map<std::string, std::unique_ptr<IJObject>> Value();
// после:
std::map<std::string, std::unique_ptr<IJObject>> Value() override;
```

JArray (строка 158):
```cpp
// аналогично
```

JObject (строка 164):
```cpp
// аналогично
```

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 3 строки, ~1 минута

---

### Шаг 6: JSONCreate.h (GridEngine) — 3 метода

**Цель:** аналогичные изменения в дубликате.

**Файлы:** `GridEngine/GridEngine/JSONCreate.h`

**⚠️ GridEngine не входит в CMake-сборку GDM** — изменения нельзя проверить сборкой, только визуально.

**Контекст:**
Дубликат. Базовый `IJObject::Value() = 0` на строке 138. Наследники: `JValue` (152), `JArray` (159), `JObject` (165).

**ВНИМАНИЕ:** в GridEngine версии `IJObject` использует `std::wstring` ключи (`std::map<std::wstring, ...>`), а не `std::string`. Проверить при сборке.

**Что сделать:**

JValue (строка 152):
```cpp
// до:
virtual std::map<std::wstring, std::unique_ptr<IJObject>> Value();
// после:
std::map<std::wstring, std::unique_ptr<IJObject>> Value() override;
```

JArray (строка 159), JObject (строка 165) — аналогично.

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 3 строки, ~1 минута

---

### Шаг 7: IRCGEngine.h — 4 метода

**Цель:** пометить переопределённые методы в `IrapClassicGrid` и `XYZGrid`.

**Файлы:** `HydroSolver/Utils/IRCGEngine.h`

**Контекст:**
`IGrid` объявляет pure virtual `Add(std::pair<float,float>, float) = 0` и `Write() = 0` (строки 14–15).
`IrapClassicGrid : public IGrid` и `XYZGrid : public IGrid` переопределяют оба.

**Что сделать:**

IrapClassicGrid (строки 35–36):
```cpp
// до:
virtual void Add(std::pair<float, float> p, float value);
virtual void Write();
// после:
void Add(std::pair<float, float> p, float value) override;
void Write() override;
```

XYZGrid (строки 54–55):
```cpp
// аналогично
```

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 4 строки, ~2 минуты

---

### Шаг 8: WellDataHandler.h — 11 методов

**Цель:** пометить переопределения `Push(wstring, wstring)` в наследниках `IData`, убрать лишний `virtual` у `MerData::GetValue`.

**Файлы:** `HydroSolver/Utils/WellDataHandler.h`

**Контекст:**
`IData::Push(std::wstring path, std::wstring name) = 0` — pure virtual (строка 56).
10 наследников переопределяют его. Все перегрузки `Push(wstring)` — **не** переопределения (другая сигнатура), оставить без изменений.

`MerData::GetValue` (строка 277) уже имеет `override`, но также `virtual` — убрать `virtual`.

**Что сделать:**

PerfData (строка 241):
```cpp
// до:
virtual void Push(std::wstring path, std::wstring name);
// после:
void Push(std::wstring path, std::wstring name) override;
```

GISData (строка 246) — аналогично.
MerData (строка 251) — аналогично.
GDISData (строка 292) — аналогично.
WCData (строка 297) — аналогично.
GeoChemData (строка 302) — аналогично.
FECData (строка 307) — аналогично.
AnomData (строка 313) — аналогично.
RaschData (строка 356) — аналогично.
WellCoordData (строка 362) — аналогично.

MerData::GetValue (строка 277):
```cpp
// до:
virtual float GetValue(int id, std::wstring name) override;
// после:
float GetValue(int id, std::wstring name) override;
```

**Подводные камни:**
- Вторая перегрузка `Push(wstring)` — НЕ трогать. Она помечена `virtual` в некоторых классах (PerfData, GISData, MerData, GDISData, WCData, GeoChemData, FECData, AnomData), но не является переопределением. Убирать `virtual` у них можно, но это отдельное решение — сейчас не входит в scope, т.к. ни один из этих классов сам по себе не наследуется дальше.
- `RaschData::Push(wstring path_to_rasch)` (строка 357) — уже без `virtual`, корректно.

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 11 строк, ~5 минут

---

### Шаг 9: Config_JSON.h — 8 override + 14 убрать virtual

**Цель:** пометить настоящие переопределения спецификатором `override`, убрать ложный `virtual` у hiding-методов.

**Файлы:** `HydroSolver/Helpers/Config_JSON.h`

**Контекст:**
`Config_JSON : public Config`. `Config : public AnomalyDetectionProperties, public SchemeParamaters`.

8 методов — настоящие override (виртуальные в `Config`):

| Строка | Метод | Базовый |
|---|---|---|
| 68 | `GetWideProjectPath()` | `Config::GetWideProjectPath() = 0` |
| 75 | `LayerAggregation()` | `Config::LayerAggregation() = 0` |
| 97 | `ExtBoundaryPressure()` | `Config::ExtBoundaryPressure() = 0` |
| 116 | `StartTimeStep()` | `Config::StartTimeStep() = 0` |
| 133 | `WaterSaturation_fileName()` | `Config::WaterSaturation_fileName() = 0` |
| 134 | `OilSaturation_fileName()` | `Config::OilSaturation_fileName() = 0` |
| 135 | `Pressure_fileName()` | `Config::Pressure_fileName() = 0` |
| 136 | `OverallBalance_fileName()` | `Config::OverallBalance_fileName() = 0` |

14 методов — hiding (базовые не виртуальные). Убрать `virtual`:

| Строка | Метод | Скрывает |
|---|---|---|
| 98 | `WaterPhaseProperties()` | `Config::WaterPhaseProperties()` (не virtual) |
| 106 | `OilPhaseProperties()` | `Config::OilPhaseProperties()` (не virtual) |
| 114 | `RequiredNewtonTol()` | `SchemeParamaters::RequiredNewtonTol()` |
| 115 | `g()` | `Config::g()` (constexpr) |
| 119 | `VelocityMultiplier()` | `AnomalyDetectionProperties::VelocityMultiplier()` |
| 120 | `InitialTrajectoryDistance()` | `AnomalyDetectionProperties::InitialTrajectoryDistance()` |
| 127 | `StartSignalRollbackTime()` | `AnomalyDetectionProperties::StartSignalRollbackTime()` |
| 128 | `EndSignalRollbackTime()` | `AnomalyDetectionProperties::EndSignalRollbackTime()` |
| 130 | `NewtonMaxIterCount()` | `SchemeParamaters::NewtonMaxIterCount()` (тип отличается: base `size_t`, derived `int`) |
| 131 | `AMG_RelTol()` | `SchemeParamaters::AMG_RelTol()` |
| 132 | `AMG_AbsTol()` | `SchemeParamaters::AMG_AbsTol()` |
| 139 | `MinCellThickness()` | `SchemeParamaters::MinCellThickness()` |
| 140 | `MinPorosity()` | `SchemeParamaters::MinPorosity()` |
| 141 | `MinPermeability()` | `SchemeParamaters::MinPermeability()` |

**Что сделать:**

Для 9 override-методов — заменить `virtual <тип> <метод>(...)` на `<тип> <метод>(...) override`:

Строка 68:
```cpp
// до:
virtual  std::string GetWideProjectPath() {
// после:
std::string GetWideProjectPath() override {
```

Строка 75:
```cpp
// до:
virtual std::vector<std::vector<std::string>> LayerAggregation()
// после:
std::vector<std::vector<std::string>> LayerAggregation() override
```

Строка 97:
```cpp
// до:
virtual double ExtBoundaryPressure() { ...
// после:
double ExtBoundaryPressure() override { ...
```

Строка 116:
```cpp
// до:
virtual double StartTimeStep() { ...
// после:
double StartTimeStep() override { ...
```

Строки 133–136:
```cpp
// до:
virtual std::string WaterSaturation_fileName() { ...
virtual std::string OilSaturation_fileName() { ...
virtual std::string Pressure_fileName() { ...
virtual std::string OverallBalance_fileName() { ...
// после:
std::string WaterSaturation_fileName() override { ...
std::string OilSaturation_fileName() override { ...
std::string Pressure_fileName() override { ...
std::string OverallBalance_fileName() override { ...
```

Для 14 hiding-методов — убрать `virtual`:

Строка 98:
```cpp
// до:
virtual PhaseProperties WaterPhaseProperties()
// после:
PhaseProperties WaterPhaseProperties()
```

Строка 106:
```cpp
// до:
virtual PhaseProperties OilPhaseProperties()
// после:
PhaseProperties OilPhaseProperties()
```

И аналогично для строк 114, 115, 119, 120, 127, 128, 130, 131, 132, 139, 140, 141.

**Подводные камни:**
- ⚠️ После удаления `virtual` у hiding-методов поведение не изменится (эти методы и раньше не участвовали в виртуальном dispatch — базовые методы не виртуальные).
- ⚠️ `NewtonMaxIterCount()` — в базе возвращает `size_t`, в наследнике `int`. Это несовпадение типов, но для hiding это допустимо (разные функции). Оставить как есть, зарегистрировать в DEBT-059.

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 23 строки, ~10 минут

---

### Шаг 10: DBUtils.h — 22 метода

**Цель:** пометить переопределённые методы в `DBSaver`, `TempGRIDSaver`, `DBLoader`, `TempGRDLoader`.

**Файлы:** `GridEngine/GridEngine/DBUtils.h`

**⚠️ GridEngine не входит в CMake-сборку GDM** — изменения нельзя проверить сборкой. Классы `DBSaver`, `DBLoader` зависят от `pqxx` (PostgreSQL), который не подключён в GDM. Применять по аналогии; верификация — визуальная. Также: ветка `refactor/debt-004/remove-db-factories` планирует удаление этих классов — при merge DEBT-004 шаг 10 станет неактуальным.

**Контекст:**
`IGrdeclSaver` — 4 pure virtual метода (строки 40–43): `SetObject`, `ResetGRDECL`, `ResetGRID`, `GetContour`.
`DBSaver : public IGrdeclSaver` — переопределяет все 4 (строки 65, 79, 74, 88).
`TempGRIDSaver : public IGrdeclSaver` — переопределяет все 4 (строки 208, 213, 217, 220).

`IGrdeclLoader` — 7 pure virtual методов (строки 279–285): `GetModelParams`, `LoadPillars`, `LoadZCORN`, `LoadValues`, `GetAvailableGRIDS`, `GetIdxLayerTable`, `GetLayerCoefs`.
`DBLoader : public IGrdeclLoader` — переопределяет все 7 (строки 296–302).
`TempGRDLoader : public IGrdeclLoader` — переопределяет все 7 (строки 317–337).

**Что сделать:**

DBSaver (4 метода, строки 65, 74, 79, 88):
```cpp
// до:
virtual void SetObject(std::string name)
virtual void ResetGRID(std::string name)
virtual void ResetGRDECL()
virtual std::vector<std::pair<float, float>> GetContour()
// после:
void SetObject(std::string name) override
void ResetGRID(std::string name) override
void ResetGRDECL() override
std::vector<std::pair<float, float>> GetContour() override
```

TempGRIDSaver (4 метода, строки 208, 213, 217, 220) — аналогично.

DBLoader (7 методов, строки 296–302):
```cpp
// до:
virtual ModelParametrs GetModelParams();
virtual std::vector<float> LoadPillars();
// ... etc
// после:
ModelParametrs GetModelParams() override;
std::vector<float> LoadPillars() override;
// ... etc
```

TempGRDLoader (7 методов, строки 317–337) — аналогично, с учётом inline-тел.

**Проверка после этого шага:**
- Сборка + тесты

**Оценка:** 22 строки, ~10 минут

---

### Шаг 11: Флаг компилятора `/w34263` в CMakeLists.txt

**Цель:** защитить от регрессий — новые переопределения без `override` будут предупреждением (= ошибкой по политике проекта).

**Файлы:** `CMakeLists.txt`

**Контекст:**
MSVC `/w34263` включает предупреждение C4263: member function does not override any base class virtual member function. Текущий уровень предупреждений — `/W3` (строка 14). Флаг нужно добавить как target-specific для проектных targets, **не** для Eigen, amgcl, Catch2.

**Что сделать:**

Добавить после определения всех проектных targets (после строки с `add_library(gdm_core ...)`), перед `target_link_libraries`:

```cmake
# C4263: метод наследника не переопределяет virtual метод базы
# Ловит отсутствие override и расхождение сигнатур
target_compile_options(gdm_core PRIVATE /w34263)
```

Если `gdm`, `gdm_tests` и другие executables компилируют собственные .cpp — добавить и для них. Но основной код — в `gdm_core`, остальные — тонкие обёртки.

**Подводные камни:**
- ⚠️ C4263 работает только с виртуальными функциями — не ловит hiding не-виртуальных. Это нормально для DEBT-037.
- ⚠️ Если Eigen включается через `target_link_libraries(gdm_core ... Eigen3::Eigen)` с INTERFACE compile options — `PRIVATE` для `/w34263` на `gdm_core` не распространится на Eigen headers. Но headers Eigen компилируются в контексте `gdm_core` target, и `/w34263` будет применяться. Нужно проверить — если Eigen headers генерируют C4263, можно подавить выборочно через `#pragma warning(push/disable/pop)` вокруг `#include <Eigen/...>`.

**Проверка после этого шага:**
- Полная сборка: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Убедиться: нет новых предупреждений

**Оценка:** 1–3 строки в CMake, ~5 минут

---

### Шаг 12: Зарегистрировать DEBT-059 — hiding в Config_JSON

**Цель:** зарегистрировать обнаруженную проблему с hiding как отдельную задачу.

**Файлы:** `vault/GDM/roadmap/технический долг.md`

**Контекст:**
В ходе анализа DEBT-037 обнаружено, что `Config_JSON.h` содержит 14 методов, которые скрывают (hiding) не-виртуальные методы из базовых классов `SchemeParamaters`, `AnomalyDetectionProperties` и `Config`. Текущая ситуация:
- `Config_JSON` инициализирует базовые поля (`SchemeParamaters`, `AnomalyDetectionProperties`) из JSON-конфига в конструкторе
- Затем скрывающие методы `Config_JSON` вызываются при создании `Config`, но через указатель на `Config` вызовутся **базовые** версии (не из JSON)
- Нужно проверить: вызывается ли Config_JSON через полиморфный указатель? Если да — hiding = баг. Если нет — hiding безвреден, но вводит в заблуждение.

**Что сделать:**

Добавить запись в `vault/GDM/roadmap/технический долг.md`:

```markdown
### DEBT-059: Config_JSON hiding — 14 методов скрывают не-виртуальные базовые
- **Описание:** `Config_JSON` объявляет 14 методов как `virtual`, которые скрывают (hiding) не-виртуальные методы из `SchemeParamaters` (7), `AnomalyDetectionProperties` (4) и `Config` (3: `g`, `WaterPhaseProperties`, `OilPhaseProperties`). Вызов через указатель на `Config`/`SchemeParamaters` вызовет базовую версию, не `Config_JSON`
- **Файл:** `Config_JSON.h:98,106,114,115,119,120,127,128,130–132,139–141`
- **Действие:** проверить call sites; если полиморфизм нужен — сделать базовые методы виртуальными; если нет — убрать hiding
- **Приоритет:** средний
- **Блокирует:** нет
```

**Проверка:** нет (vault-запись, не код)

**Оценка:** ~5 минут

---

### Шаг 13: Обновить vault

**Цель:** обновить статус DEBT-037 и связанные заметки.

**Файлы:**
- `vault/GDM/roadmap/технический долг.md`
- `vault/GDM/00-home/index.md`

**Что сделать:**

1. В записи DEBT-037 обновить статус:
```markdown
- **Статус:** ✅ исправлено <дата>
```

2. Прокомментировать GitHub issue #37:
```
gh issue comment 37 --repo ArturSalamatin/GDM --body "Все переопределённые виртуальные методы помечены override. Hiding-методы в Config_JSON — зарегистрированы как DEBT-059."
```

3. Закрыть issue:
```
gh issue close 37 --repo ArturSalamatin/GDM
```

4. Обновить `index.md` если создана новая заметка в vault.

**Оценка:** ~3 минуты

---

## Критерии завершения

- [ ] Все шаги 1–11 выполнены
- [ ] Все 313 тестов зелёные
- [ ] Новых предупреждений нет
- [ ] DEBT-059 зарегистрирован (шаг 12)
- [ ] Vault обновлён (шаг 13)
- [ ] GitHub issue #37 прокомментирован и закрыт

## Связанные заметки

- [[технический долг]] — реестр, запись DEBT-037
- [[известные баги и технический долг]] — ссылка на реестр
- DEBT-038: невиртуальный деструктор `SomeGrid` — смежная проблема
- DEBT-059 (новая): hiding в Config_JSON — обнаружена в ходе DEBT-037
