---
tags:
  - план
  - рефакторинг
date: 2026-07-18
issue: DEBT-059
github: 38
branch: refactor/debt-059/config-json-hiding
status: реализован
audit:
  date: 2026-07-18
  round: 4
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-059: устранение name hiding в Config_JSON

## Контекст

`Config_JSON` объявляет 14 методов, скрывающих (name hiding) одноимённые не-виртуальные методы базовых классов `SchemeParamaters` (7), `AnomalyDetectionProperties` (4) и `Config` (3). Вызов через указатель на базу вызовет не ту версию.

**Решение (вариант A):** убрать hiding-методы, перенести логику инициализации в конструктор `Config_JSON`. Базовые геттеры (const, корректные типы) используются напрямую.

**Принятые решения пользователя:**
1. `g()` = constexpr 9.81, конфигурируемость не нужна (моделируем только земные условия)
2. `NewtonMaxIterCount` → `size_t` (не `int`)
3. Придерживаемся const-correctness

**Важно:** `Config_JSON.cpp` не включён в CMakeLists.txt (`gdm_core`). Файлы `Config.h`, `Config_JSON.h`, `Config_JSON.cpp` — legacy, не компилируются в текущей сборке. Рефакторинг не влияет на работающий код, но приводит legacy в порядок для потенциального возвращения в строй.

## Связанные заметки

- [[debt-037 override-specifiers]] — обнаружено при этой работе
- GitHub: [#38](https://github.com/ArturSalamatin/GDM/issues/38), Related: [#37](https://github.com/ArturSalamatin/GDM/issues/37)

## Иерархия наследования

```
SchemeParamaters (struct, Descriptors.h:12)
    └── 7 const-геттеров, 7 protected-полей

AnomalyDetectionProperties (struct, Descriptors.h:44)
    └── 8 const-геттеров, 9 protected-полей

Config (class, Config.h:41)  : public AnomalyDetectionProperties, public SchemeParamaters
    └── g() constexpr 9.81
    └── WaterPhaseProperties() → waterPhaseProperties (protected)
    └── OilPhaseProperties()   → oilPhaseProperties (protected)
    └── чисто виртуальные: ExtBoundaryPressure, StartTimeStep, etc.

Config_JSON (class, Config_JSON.h:9)  : public Config
    └── 14 hiding-методов (каждый парсит JSON при вызове)
    └── конструктор уже заполняет 5 полей AnomalyDetectionProperties
```

## Текущее состояние

### Конструктор Config_JSON (строки 19–67)

Уже заполняет из JSON:
- `anomaly_date_start` ← `ConvertDateToExcelDate("temporal_parameters", "anomaly_date_start")`
- `anomaly_date_end` ← аналогично
- `saturation_field_date` ← аналогично
- `number_of_snapshots` ← `(int)GetValue("temporal_parameters", "number_of_snapshots")`
- `anomaly_detection_interval` ← `GetValue("anomaly", "anomaly_detection_interval")`

**НЕ** заполняет (делегировано hiding-методам, которые парсят JSON при каждом вызове):
- `SchemeParamaters`: `requiredNewtonTol`, `newtonMaxIterCount`, `amg_RelTol`, `amg_AbsTol`, `minCellThickness`, `minPorosity`, `minPermeability`
- `AnomalyDetectionProperties`: `veclocity_multiplier`, `initTrajectoryDistance`, `startSignalRollbackTime`, `endSignalRollbackTime`
- `Config`: `waterPhaseProperties`, `oilPhaseProperties`

### Hiding-методы (будут удалены)

| Строка | Метод | Базовый тип | Различия |
|--------|-------|-------------|----------|
| 98–105 | `WaterPhaseProperties()` | `Config` | возвращает PhaseProperties из JSON с UnitsConversionFactors |
| 106–113 | `OilPhaseProperties()` | `Config` | аналогично |
| 114 | `RequiredNewtonTol()` | `SchemeParamaters` | не-const, double |
| 115 | `g()` | `Config` | читает из JSON (базовый — constexpr 9.81) |
| 119 | `VelocityMultiplier()` | `AnomalyDetectionProperties` | не-const |
| 120–125 | `InitialTrajectoryDistance()` | `AnomalyDetectionProperties` | fallback val<0→0.5, не-const |
| 127 | `StartSignalRollbackTime()` | `AnomalyDetectionProperties` | не-const |
| 128 | `EndSignalRollbackTime()` | `AnomalyDetectionProperties` | не-const |
| 130 | `NewtonMaxIterCount()` | `SchemeParamaters` | возвращает int (база — size_t), не-const |
| 131 | `AMG_RelTol()` | `SchemeParamaters` | не-const |
| 132 | `AMG_AbsTol()` | `SchemeParamaters` | не-const |
| 139 | `MinCellThickness()` | `SchemeParamaters` | не-const |
| 140 | `MinPorosity()` | `SchemeParamaters` | не-const |
| 141 | `MinPermeability()` | `SchemeParamaters` | UnitsConversionFactors, не-const |

## Целевое состояние

- Конструктор `Config_JSON` заполняет **все** protected-поля базовых классов из JSON (однократно)
- 14 hiding-методов удалены
- Геттеры вызываются из базовых классов (const, корректные типы)
- `g()` из `Config` — constexpr 9.81, hiding-метод удалён

## Подводные камни

- [x] **Все call sites найдены:** ✅ call sites = 0 (класс не используется)
- [x] **Потокобезопасность:** ✅ не применимо (код не компилируется)
- [x] **Зависимости сборки:** ✅ файлы не в CMakeLists, сборка не затронута
- [x] **Обратная совместимость API:** ✅ класс не используется
- [x] **Тесты:** ✅ нет тестов на Config_JSON (и не нужны — класс legacy)
- [x] **Кодировки:** ✅ не применимо
- [x] **Платформозависимость:** ✅ не применимо
- [x] **Мёртвый код:** ✅ подтверждено grep-ом
- [x] **Связь с другими задачами:** ✅ DEBT-037 завершён, конфликтов нет
- [ ] **⚠️ Двойная конвертация единиц:** `Config_JSON` применяет `UnitsConversionFactors::viscosityConversion` к вязкости перед передачей в `PhaseProperties(...)`, а конструктор `PhaseProperties` (PropertyDescriptor.h:38) дополнительно умножает на `unitConverter::viscosityConverter()`. Если `UnitsConversionFactors::viscosityConversion` == `unitConverter::viscosityConverter()` — это двойная конвертация. Нужно проверить при возвращении класса в строй. **В рамках текущей задачи:** переносим as-is (поведение legacy сохраняется), фиксируем как отдельную проблему.

## Обнаруженные проблемы

### Подтверждённая двойная конвертация вязкости (новый DEBT)

`Config_JSON::WaterPhaseProperties()` умножает viscosity на `UnitsConversionFactors::viscosityConversion` = `(1E-3) / (24*3600)` ≈ 1.157e-8, а конструктор `PhaseProperties<CustomUnitConverter>` (PropertyDescriptor.h:38) тоже умножает на `CustomUnitConverter::viscosityConverter()` = `1.0 / 86400.0 / 1000` — **то же самое значение**. Двойная конвертация подтверждена — результат: viscosity * (1.157e-8)² — физически бессмысленно. Это legacy-баг, переносим as-is.

### `PhaseProperties` — шаблонный тип без аргумента (новый DEBT)

`Config.h:68` и `Config_JSON.h:98` используют `PhaseProperties` без template-аргумента. `PhaseProperties` определён как `template<typename unitConverter> struct PhaseProperties` (PropertyDescriptor.h:30). Алиасы: `WaterPhaseProperty = PhaseProperties<CustomUnitConverter>`, `OilPhaseProperty = PhaseProperties<CustomUnitConverter>`. Без `using PhaseProperties = ...` — код некомпилируем. Это подтверждает мёртвость файла. При возвращении в строй: заменить на `WaterPhaseProperty`/`OilPhaseProperty` или добавить using.

### Существующий C-style cast `number_of_snapshots` (мелкий)

Строка 30: `number_of_snapshots = (int)GetValue(...)` — поле `size_t`, cast в `int`. Не блокирует, но при возвращении в строй — заменить на `static_cast<size_t>`.

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-059/config-json-hiding`
3. Файлы не в CMakeLists — сборка не затронута. Но для верификации: `cmake --build build --config Release`
4. Тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Начни с шага 1. После финального шага: сборка + тесты (должны остаться зелёными — изменения в некомпилируемых файлах)

---

## Шаг 1: Расширить конструктор Config_JSON — заполнить поля SchemeParamaters

**Цель:** конструктор заполняет 7 protected-полей `SchemeParamaters` из JSON. После этого шага базовые const-геттеры возвращают корректные значения.

**Файлы:** `HydroSolver/Helpers/Config_JSON.h`

**Контекст:**
Сейчас конструктор (строки 19–67) парсит JSON и заполняет только поля `AnomalyDetectionProperties`. Поля `SchemeParamaters` остаются с default-значениями из Descriptors.h:30–38. Hiding-методы читают JSON при каждом вызове — мы переносим эту логику в конструктор (однократное чтение).

**Что сделать:**

После строки 31 (`anomaly_detection_interval = ...`) и перед блоком `if (number_of_snapshots < 2)` (строка 33), добавить:

```cpp
// SchemeParamaters fields
requiredNewtonTol = GetValue("scheme_parameters", "required_Newton_Tolerance");
newtonMaxIterCount = static_cast<size_t>(GetValue("scheme_parameters", "newton_max_iteration_count"));
amg_RelTol = GetValue("scheme_parameters", "AMG_RelTol");
amg_AbsTol = GetValue("scheme_parameters", "AMG_AbsTol");
minCellThickness = GetValue("common_properties", "min_cell_thickness");
minPorosity = GetValue("common_properties", "min_cell_porosity");
minPermeability = UnitsConversionFactors::permeabilityConversion * GetValue("common_properties", "min_cell_permeability");
```

**Проверка после этого шага:**
- Файл синтаксически корректен (визуальная проверка)
- Сборка `cmake --build build --config Release` — должна пройти (файл не компилируется)

**Подводные камни:**
- `GetValue` возвращает `double` — cast в `size_t` через `static_cast` (не C-style cast)
- `minPermeability` — единственное поле с unit conversion

**Зависимости:** нет

**Оценка:** ~7 строк, ~5 минут

---

## Шаг 2: Расширить конструктор Config_JSON — заполнить поля AnomalyDetectionProperties

**Цель:** конструктор заполняет оставшиеся 4 protected-поля `AnomalyDetectionProperties`.

**Файлы:** `HydroSolver/Helpers/Config_JSON.h`

**Контекст:**
Конструктор уже заполняет 5 из 9 полей. Остаются: `veclocity_multiplier`, `initTrajectoryDistance`, `startSignalRollbackTime`, `endSignalRollbackTime`. Hiding-метод `InitialTrajectoryDistance()` содержит fallback-логику (`val < 0 → 0.5`), которую нужно перенести.

**Что сделать:**

После строки 31 (`anomaly_detection_interval = ...`) — вместе с блоком SchemeParamaters из шага 1 (порядок между шагами 1–3 не важен), добавить:

```cpp
// AnomalyDetectionProperties fields (remaining)
veclocity_multiplier = GetValue("trajectories", "velocity_multiplier");
{
    auto val = GetValue("trajectories", "start_distance");
    initTrajectoryDistance = (val < 0.0) ? 0.5 : val;
}
startSignalRollbackTime = GetValue("trajectories", "start_signal_rollback_time");
endSignalRollbackTime = GetValue("trajectories", "end_signal_rollback_time");
```

**Проверка после этого шага:**
- Синтаксически корректно
- Сборка проходит

**Подводные камни:**
- Fallback `val < 0 → 0.5` перенесён as-is из hiding-метода (строки 120–124)
- Скобки `{}` ограничивают scope переменной `val`

**Зависимости:** нет (шаги 1–3 независимы — все добавляют код в конструктор, порядок вставки не важен)

**Оценка:** ~6 строк, ~3 минуты

---

## Шаг 3: Расширить конструктор Config_JSON — заполнить waterPhaseProperties / oilPhaseProperties

**Цель:** конструктор заполняет protected-поля `Config::waterPhaseProperties` и `Config::oilPhaseProperties`.

**Файлы:** `HydroSolver/Helpers/Config_JSON.h`

**Контекст:**
Hiding-методы `WaterPhaseProperties()` (строки 98–105) и `OilPhaseProperties()` (строки 106–113) создают `PhaseProperties` при каждом вызове. Переносим в конструктор. Конвертацию единиц переносим as-is (потенциальная двойная конвертация — отдельная задача).

**Что сделать:**

После строки 31 — вместе с блоками из шагов 1–2 (порядок не важен), добавить:

```cpp
// Config fields
waterPhaseProperties = PhaseProperties(
    UnitsConversionFactors::viscosityConversion * GetValue("phase_properties", "viscosity", "0"),
    UnitsConversionFactors::densityConversion * GetValue("phase_properties", "density", "0"),
    UnitsConversionFactors::compressibilityConversion * GetValue("phase_properties", "compressibility", "0"),
    UnitsConversionFactors::saturationConversion * GetValue("phase_properties", "residual", "0"),
    UnitsConversionFactors::pressureConversion * GetValue("phase_properties", "reference_pressure_for_compressibility", "0"));
oilPhaseProperties = PhaseProperties(
    UnitsConversionFactors::viscosityConversion * GetValue("phase_properties", "viscosity", "1"),
    UnitsConversionFactors::densityConversion * GetValue("phase_properties", "density", "1"),
    UnitsConversionFactors::compressibilityConversion * GetValue("phase_properties", "compressibility", "1"),
    UnitsConversionFactors::saturationConversion * GetValue("phase_properties", "residual", "1"),
    UnitsConversionFactors::pressureConversion * GetValue("phase_properties", "reference_pressure_for_compressibility", "1"));
```

**Проверка после этого шага:**
- Синтаксически корректно
- Сборка проходит

**Подводные камни:**
- ⚠️ Потенциальная двойная конвертация вязкости (см. раздел «Обнаруженные проблемы»). Переносим as-is — поведение legacy сохраняется
- `GetValue` с 3 аргументами (section, name, id) — перегрузка, не путать с 2-аргументной

**Зависимости:** нет (шаги 1–3 независимы)

**Оценка:** ~12 строк, ~5 минут

---

## Шаг 4: Удалить 14 hiding-методов из Config_JSON

**Цель:** убрать все методы, скрывающие базовые. Класс теперь полагается на геттеры `SchemeParamaters`, `AnomalyDetectionProperties` и `Config`.

**Файлы:** `HydroSolver/Helpers/Config_JSON.h`

**Контекст:**
После шагов 1–3 конструктор заполняет все protected-поля. Hiding-методы больше не нужны — базовые геттеры вернут корректные значения.

**Что сделать:**

Удалить следующие блоки:

1. **Строки 98–105** — `WaterPhaseProperties()` (8 строк включая `}`)
2. **Строки 106–113** — `OilPhaseProperties()` (8 строк включая `}`)
3. **Строка 114** — `RequiredNewtonTol()`
4. **Строка 115** — `g()` (решено: используем constexpr 9.81 из Config)
5. **Строка 119** — `VelocityMultiplier()`
6. **Строки 120–125** — `InitialTrajectoryDistance()` (с fallback-логикой, перенесённой в конструктор)
7. **Строка 127** — `StartSignalRollbackTime()`
8. **Строка 128** — `EndSignalRollbackTime()`
9. **Строка 130** — `NewtonMaxIterCount()`
10. **Строка 131** — `AMG_RelTol()`
11. **Строка 132** — `AMG_AbsTol()`
12. **Строка 139** — `MinCellThickness()`
13. **Строка 140** — `MinPorosity()`
14. **Строка 141** — `MinPermeability()`

**Изменения (до → после):**

До (пример, строка 114):
```cpp
double RequiredNewtonTol() { return GetValue("scheme_parameters", "required_Newton_Tolerance"); }
```

После: удалено. Базовый `SchemeParamaters::RequiredNewtonTol() const` возвращает `requiredNewtonTol` (заполнен в конструкторе).

**Проверка после этого шага:**
- Синтаксически корректно — нет ошибок скобок
- Сборка `cmake --build build --config Release` — проходит
- `ctest --test-dir build -C Release --output-on-failure` — все тесты зелёные

**Подводные камни — методы, которые НЕЛЬЗЯ удалять (override, не hiding):**
- Строка 97: `ExtBoundaryPressure() override` — расположена ПЕРЕД первым hiding-блоком
- Строка 116: `StartTimeStep() override` — расположена МЕЖДУ `g()` (115, удалить) и `VelocityMultiplier()` (119, удалить)
- Строки 133–136: `WaterSaturation_fileName()`, `OilSaturation_fileName()`, `Pressure_fileName()`, `OverallBalance_fileName()` — все `override`, расположены МЕЖДУ `AMG_AbsTol()` (132, удалить) и `MinCellThickness()` (139, удалить)
- Строка 126: закомментированный `TrajectoryCount()` — оставить как есть (не hiding)

**Безопасный порядок удаления:** удалять снизу вверх (141→139→132→131→130→128→127→120–125→119→115→114→106–113→98–105), чтобы не сдвигать номера строк для оставшихся удалений.

**Зависимости:** шаги 1–3

**Оценка:** удалить ~30 строк, ~10 минут

---

## Шаг 5: Верификация

**Цель:** убедиться, что изменения корректны и не ломают сборку.

**Файлы:** все изменённые

**Что сделать:**

1. `cmake --build build --config Release` — сборка проходит
2. `ctest --test-dir build -C Release --output-on-failure` — все тесты зелёные
3. Визуальная проверка: `Config_JSON.h` не содержит методов с именами из базовых классов (grep)
4. Grep по проекту: ни одного вызова удалённых методов через `Config_JSON` объект

**Ожидание:** сборка и тесты не изменятся, потому что `Config_JSON.cpp` не включён в CMakeLists.

**Зависимости:** шаг 4

**Оценка:** ~5 минут

---

## Критерии завершения

- [x] Конструктор `Config_JSON` заполняет все protected-поля базовых классов
- [x] 14 hiding-методов удалены
- [x] `g()` — constexpr 9.81 из `Config`, hiding-версия удалена
- [x] `NewtonMaxIterCount` хранится как `size_t`
- [x] Сборка проходит без ошибок
- [x] Все тесты зелёные
- [x] Двойная конвертация вязкости зафиксирована как отдельная проблема
- [x] GitHub issue #38 прокомментирован с результатом
- [x] Vault обновлён
