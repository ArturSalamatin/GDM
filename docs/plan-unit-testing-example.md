# План: юнит-тестирование gdm_core

## Context

Библиотека `gdm_core` содержит ~30 классов, покрытых только интеграционными тестами (45 тестов в `gdm_tests`/`gdm_benchmark`). Интеграционные тесты запускают полный симулятор и проверяют физику, но не тестируют отдельные классы изолированно. Юнит-тесты нужны, чтобы:
- ловить регрессии в отдельных классах до запуска тяжёлого симулятора
- документировать контракты методов (пред/постусловия)
- упростить рефакторинг (фаза 3 в roadmap)

Граф зависимостей классов построен и сохранён: `vault/GDM/atlas/граф зависимостей классов gdm_core.md`.

---

## 1. Структура файлов

```
tests/
├── unit/
│   ├── geometry/
│   │   ├── test_Point.cpp           → Point, trPoint, Contour
│   │   ├── test_GridDescriptors.cpp → GridBounds, GridSize, BlockSize
│   │   └── test_MathRoutines.cpp    → LinearInterp, BilinearInterp, LowerPoint*
│   ├── physics/
│   │   ├── test_PropertyDescriptor.cpp → PhaseProperties, CustomUnitConverter, OtherProperties
│   │   ├── test_AbstractCells.cpp      → Dim3Cell, SomeDimCell (Volume, X/Y/Z)
│   │   └── test_TwoPhaseFlowCell.cpp   → relperm, mobility, fractional flow, density, Accept/Reverse
│   ├── math/
│   │   ├── test_CRSStructure.cpp    → Layout, GlobalIndex, UnpackCorrections
│   │   ├── test_SparsityPattern.cpp → Row/Col/DiagBlocks на маленьких графах
│   │   └── test_MatrixCSR.cpp       → AddDiagBlock, AddOffDiagBlock, ResetMatrix
│   ├── wells/
│   │   ├── test_GeosPoint.cpp         → SimplePoint, GeosPoint (shared_ptr)
│   │   ├── test_Segment.cpp           → Segment, WellJob, WellJobTime
│   │   ├── test_SetOfPoints.cpp       → sweep-line: AddSegment, Sweep, totalLength
│   │   ├── test_SetOfPerforations.cpp → AddNewJob, MoveDate, TotalLength
│   │   ├── test_AccumulatedPerforations.cpp → история перфораций, getPerforations(t)
│   │   └── test_WellJobs.cpp          → AccumulatePerforations по слоям
│   ├── reservoir/
│   │   ├── test_PIController.cpp      → ComputeMultiplier, Reset, clamping
│   │   ├── test_NumericalParameters.cpp → increase/decrease_schemeTau, PI toggle
│   │   └── test_RawHorizon.cpp        → GridSize(), initial_water_saturation()
│   └── descriptors/
│       ├── test_Descriptors.cpp       → SchemeParameters, AnomalyDetectionProperties
│       └── test_MER_Data.cpp          → identify_cur_MER_record, debitPartial, CleanMER_record
├── test_smoke.cpp                     (существующие интеграционные — не трогаем)
├── test_components.cpp
├── ...
└── test_helpers.h
```

**Итого:** ~20 тестовых файлов в 6 модулях.

Каждый файл тестирует один класс (или тесно связанную группу: Segment + WellJob + WellJobTime).

---

## 2. CMake: один executable на уровень зависимостей

В `CMakeLists.txt` добавляются 4 новых executable:

```cmake
# Уровень 0 — нулевые зависимости (STL only)
add_executable(gdm_unit_level0
    tests/unit/geometry/test_Point.cpp
    tests/unit/geometry/test_GridDescriptors.cpp
    tests/unit/physics/test_PropertyDescriptor.cpp
    tests/unit/wells/test_GeosPoint.cpp
    tests/unit/wells/test_Segment.cpp
    tests/unit/reservoir/test_PIController.cpp
)
target_link_libraries(gdm_unit_level0 PRIVATE gdm_core Catch2::Catch2WithMain)

# Уровень 1 — зависят от уровня 0
add_executable(gdm_unit_level1
    tests/unit/wells/test_SetOfPoints.cpp
    tests/unit/wells/test_SetOfPerforations.cpp
    tests/unit/reservoir/test_RawHorizon.cpp
    tests/unit/descriptors/test_Descriptors.cpp
    tests/unit/geometry/test_MathRoutines.cpp
)
target_link_libraries(gdm_unit_level1 PRIVATE gdm_core Catch2::Catch2WithMain)

# Уровень 2 — средняя глубина
add_executable(gdm_unit_level2
    tests/unit/wells/test_AccumulatedPerforations.cpp
    tests/unit/descriptors/test_MER_Data.cpp
    tests/unit/reservoir/test_NumericalParameters.cpp
    tests/unit/math/test_CRSStructure.cpp
    tests/unit/math/test_SparsityPattern.cpp
    tests/unit/physics/test_AbstractCells.cpp
)
target_link_libraries(gdm_unit_level2 PRIVATE gdm_core Catch2::Catch2WithMain)

# Уровень 3 — составные классы
add_executable(gdm_unit_level3
    tests/unit/math/test_MatrixCSR.cpp
    tests/unit/physics/test_TwoPhaseFlowCell.cpp
    tests/unit/wells/test_WellJobs.cpp
)
target_link_libraries(gdm_unit_level3 PRIVATE gdm_core Catch2::Catch2WithMain)

# CTest discovery для каждого уровня
catch_discover_tests(gdm_unit_level0 PROPERTIES TIMEOUT 30)
catch_discover_tests(gdm_unit_level1 PROPERTIES TIMEOUT 30)
catch_discover_tests(gdm_unit_level2 PROPERTIES TIMEOUT 60)
catch_discover_tests(gdm_unit_level3 PROPERTIES TIMEOUT 60)
```

**Timeout:** юнит-тесты — 30–60 с (vs 600 с для интеграционных).

---

## 3. Catch2 теги и конвенции

### Теги
Каждый TEST_CASE получает теги:
- `[unit]` — все юнит-тесты (отличить от интеграционных при запуске)
- `[level0]` / `[level1]` / `[level2]` / `[level3]` — уровень зависимости
- `[module]` — модуль: `[geometry]`, `[physics]`, `[math]`, `[wells]`, `[reservoir]`, `[descriptors]`
- `[class]` — имя класса: `[PIController]`, `[CRSStructure]`, etc.

Пример:
```cpp
TEST_CASE("PIController: fast convergence gives growth > 1",
          "[unit][level0][reservoir][PIController]") {
```

### Запуск по фильтрам
```powershell
# все юнит-тесты уровня 0
ctest --test-dir build -C Release -R "gdm_unit_level0"

# все юнит-тесты всех уровней
ctest --test-dir build -C Release -R "gdm_unit"

# конкретный класс через Catch2 напрямую
build\Release\gdm_unit_level0.exe "[PIController]"

# все юнит-тесты (все уровни) через Catch2 tag
build\Release\gdm_unit_level0.exe "[unit]"
```

### Именование TEST_CASE
Формат: `"ClassName: описание поведения"`. Примеры:
- `"GridBounds: constructor computes lengths from min/max"`
- `"SetOfPoints: Sweep merges overlapping open segments"`
- `"TwoPhaseFlowCell: ReverseState restores previous Sw and P"`

### SECTION для вариаций
Для тестирования разных входов одного метода — SECTION вместо отдельных TEST_CASE:
```cpp
TEST_CASE("PIController: ComputeMultiplier clamping", "[unit][level0][reservoir][PIController]") {
    PIController pi;
    SECTION("growth clamped to max_growth") {
        double m = pi.ComputeMultiplier(1, true);
        CHECK(m <= 2.0);
    }
    SECTION("shrink clamped to min_shrink") {
        double m = pi.ComputeMultiplier(65, false);
        CHECK(m >= 0.3);
    }
}
```

### GENERATE для параметризации
Для sweep по значениям:
```cpp
TEST_CASE("PhaseProperties: viscosity conversion", "[unit][level0][physics][PhaseProperties]") {
    double visc = GENERATE(0.5, 1.0, 3.15, 10.0);
    WaterPhaseProperty w(visc, 1000.0, 0.0, 0.2, 1e5);
    CHECK(w.Viscosity() == Approx(visc / 86400.0 / 1000.0));
}
```

---

## 4. Паттерн тестирования

Каждый тестовый файл следует шаблону:

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
// include тестируемого заголовка — минимально
#include "path/to/TestedClass.h"

using namespace reservoir_simulator; // или set_of_points, etc.
using Catch::Approx;

// 1. Конструкторы
TEST_CASE("ClassName: default constructor", "[unit][levelN][module][ClassName]") {
    ClassName obj;
    CHECK(obj.SomeGetter() == expected);
}

TEST_CASE("ClassName: parameterized constructor", "[unit][levelN][module][ClassName]") {
    ClassName obj(args...);
    CHECK(obj.SomeGetter() == expected);
}

// 2. Методы — нормальный путь
TEST_CASE("ClassName: MethodName normal behavior", "[unit][levelN][module][ClassName]") {
    ClassName obj(setup...);
    auto result = obj.MethodName(input);
    CHECK(result == expected);
    // проверка состояния объекта после вызова
    CHECK(obj.StateGetter() == expected_state);
}

// 3. Граничные случаи
TEST_CASE("ClassName: MethodName edge case", "[unit][levelN][module][ClassName]") {
    // ...
}

// 4. Инварианты
TEST_CASE("ClassName: invariant holds after operations", "[unit][levelN][module][ClassName]") {
    // ...
}
```

---

## 5. Best practices

### Arrange–Act–Assert (AAA)
Каждый тест: setup → вызов → проверка. Одна логическая проверка на TEST_CASE (или SECTION).

### Изоляция
- Никаких файлов, сети, БД.
- Статическое состояние (`PhysPropCell::ConstantPointProperties`) инициализировать в начале теста или через Catch2 listener, не полагаться на порядок тестов.
- `WellJobs::ItsName` — const ref, тестовая строка должна жить дольше объекта.

### Детерминизм
- Фиксированные входы, фиксированные ожидания.
- Для float: `Approx` с заданным `margin` или `epsilon`, не `==`.
- Не использовать `rand()`, `time()`.

### Быстрота
- Юнит-тест < 1 секунды. Цель: весь уровень 0 < 1 с, весь уровень 3 < 5 с.
- Маленькие графы (2–4 ячейки) для CRSStructure, MatrixCSR. Не создавать полную сетку.

### Имена говорят о поведении
- Не `"test1"`, а `"GridBounds: negative coords produce positive lengths"`.
- Тег `[ClassName]` позволяет запустить все тесты класса одной командой.

### F.I.R.S.T.
- **Fast** — ms, не секунды
- **Independent** — порядок запуска не важен
- **Repeatable** — одинаковый результат всегда
- **Self-validating** — CHECK/REQUIRE, не визуальный вывод
- **Timely** — пишутся вместе с кодом (или при рефакторинге)

### Что НЕ тестировать юнитами
- Тривиальные геттеры POD-структур без логики (GridSize.Nx).
- Приватные методы — только через публичный API.
- Интеграцию с amgcl (LinearProblem::Solve) — это интеграционный тест.

---

## 6. Интеграция с VS Code

### CTest + CMake Tools extension
VS Code с расширением CMake Tools автоматически обнаруживает CTest-тесты. После `cmake --build build`:
- Панель "Testing" (Ctrl+Shift+P → "Test: Focus on Test Explorer View") покажет все тесты
- Юнит-тесты группируются по executable (gdm_unit_level0, level1, ...)
- Клик на тест → запуск одного теста
- Зелёный/красный статус

### Catch2 Test Explorer (опционально)
Расширение `hbenl.vscode-test-explorer` + `matepek.vscode-catch2-test-adapter` — даёт дерево по тегам Catch2 внутри VS Code.

### tasks.json (опционально)
Быстрый запуск через Ctrl+Shift+B:
```json
{
    "label": "Run unit tests",
    "type": "shell",
    "command": "ctest --test-dir build -C Release -R gdm_unit --output-on-failure",
    "group": "test"
}
```

---

## 7. Порядок реализации

Реализация поуровневая. Каждый уровень — отдельный коммит:

### Шаг 0: Инфраструктура
- Создать `tests/unit/` и подпапки (geometry, physics, math, wells, reservoir, descriptors)
- Добавить 4 executable в CMakeLists.txt
- Один smoke-тест (test_PIController.cpp) для проверки сборки

### Шаг 1: Уровень 0 (6 файлов)
1. `test_PIController.cpp` — перенести/расширить существующий `tests/test_pi_controller.cpp`
2. `test_GridDescriptors.cpp`
3. `test_PropertyDescriptor.cpp`
4. `test_Point.cpp`
5. `test_Segment.cpp`
6. `test_GeosPoint.cpp`

### Шаг 2: Уровень 1 (5 файлов)
1. `test_SetOfPoints.cpp`
2. `test_SetOfPerforations.cpp`
3. `test_RawHorizon.cpp`
4. `test_Descriptors.cpp`
5. `test_MathRoutines.cpp`

### Шаг 3: Уровень 2 (6 файлов)
1. `test_CRSStructure.cpp`
2. `test_SparsityPattern.cpp`
3. `test_AbstractCells.cpp`
4. `test_AccumulatedPerforations.cpp`
5. `test_MER_Data.cpp`
6. `test_NumericalParameters.cpp`

### Шаг 4: Уровень 3 (3 файла)
1. `test_MatrixCSR.cpp`
2. `test_TwoPhaseFlowCell.cpp`
3. `test_WellJobs.cpp`

---

## 8. Верификация

После каждого шага:
```powershell
cmake --build build --config Release
ctest --test-dir build -C Release -R "gdm_unit_levelN" --output-on-failure
```

Финальная проверка — все тесты (юнит + интеграционные):
```powershell
ctest --test-dir build -C Release --output-on-failure
```

---

## 9. Замечание: существующий test_pi_controller.cpp

Файл `tests/test_pi_controller.cpp` (10 тестов) уже тестирует PIController. Варианты:
- **Перенести** в `tests/unit/reservoir/test_PIController.cpp` и убрать из `gdm_tests`
- **Оставить** дублирование (PIController в обоих executable)

Рекомендация: перенести, добавить теги `[unit][level0][reservoir][PIController]`.
