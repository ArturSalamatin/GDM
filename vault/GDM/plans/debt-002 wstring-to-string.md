---
tags:
  - план
  - кодировка
date: 2026-07-15
issue: DEBT-002
github: 29
branch: refactor/debt-002/wstring-to-string
status: реализован
audit:
  date: 2026-07-15
  round: 4
  findings: 0 / 0 / 2
  auto-fixed: 2
  manual-required: 0
  cumulative-fixes: 14
---

# DEBT-002: wstring → string (UTF-8)

## Проблема

Ядро GDM использует `std::wstring` (наследие Windows GUI) для имён скважин, слоёв, MER-ключей, путей файлов и диагностических сообщений. Это блокирует кроссплатформенную сборку и усложняет интерфейсы.

**Корень:** три alias'а в `HydroSolver/defines.h`:
```cpp
using WellName = std::wstring;       // строка 12
using LayerID = std::wstring;        // строка 18
using SingleMERrecord = std::map<std::wstring, float>;  // строка 26
```

Каскад: `WellName` используется в `SomeWell`, `WellJobs`, `MER_Data`, `LogFile`, `ExceptionFactory`, `ReservoirSimulator`, тестах.

## Целевое состояние

- `WellName = std::string`, `LayerID = std::string`, `SingleMERrecord = std::map<std::string, float>`
- Все `L"..."` → `"..."`
- Все `std::to_wstring()` → `std::to_string()`
- Все `wchar_t` буферы + `swprintf` → `char` + `snprintf`
- `std::wcout` → `std::cout`, `std::wostream` → `std::ostream`
- `LogFile` API → `std::string`

## Стратегия: инкрементальная миграция по слоям

Замена идёт послойно от корня (`defines.h`) к периферии. Каждый шаг компилируется и все тесты зелёные.

**Зона неприкосновенности:**
- Физические вычисления (Jacobian, Newton, CPR, balance) — не затрагиваются
- amgcl, Eigen, Catch2 — не затрагиваются
- CMakeLists.txt — не затрагивается (файлы не добавляются/удаляются)

**Связано с:** DEBT-005 (UniversalSVParser — та же проблема кодировки, cp1251). JSONCreate.h включает `UniversalSVParser.h` (строка 7). Этот `#include` нужно удалить или заменить — JSONCreate не использует UniversalSVParser API.

## Подводные камни

- ✅ **Все call sites найдены:** grep по `wstring`, `WellName`, `LayerID`, `SingleMERrecord`, `L"`, `wchar_t`, `wcout`, `wostream` выполнен
- ✅ **Потокобезопасность:** OpenMP не используется в затронутых модулях
- ✅ **Зависимости сборки:** CMakeLists.txt не меняется, файлы те же
- ✅ **Обратная совместимость:** внешний API не ломается (имена функций те же, меняются только типы параметров)
- ⚠️ **Кодировки:** `SomeWell::Name()` сейчас конвертирует wstring→string через `wcstombs_s`. После миграции конвертация не нужна — метод вернёт поле напрямую. `NameWide()` станет избыточен
- ⚠️ **Платформозависимость:** `BinaryFileHandler.cpp` использует `GetPrivateProfileStringW` (Win32 API) — вне сборки, мигрируем заголовок, .cpp трогать не обязательно
- ⚠️ **JSONCreate.h → UniversalSVParser.h:** `#include "../UniversalSVParser.h"` — JSONCreate не использует SVParser API. Этот include нужно удалить (иначе при смене wstring в JSONCreate тянутся типы из SVParser). Строго говоря это DEBT-005 территория, но include мешает DEBT-002
- ✅ **Тесты:** test_MER_Data.cpp и test_WellJobs.cpp используют `L"..."` литералы — мигрируются на последнем шаге
- ✅ **Производительность:** `std::string` не медленнее `std::wstring` (вдвое меньше памяти на символ)
- ✅ **Связь с другими задачами:** не конфликтует. Текущая ветка `research/res-007/bicgstab-vs-lgmres` — уже merged
- ⚠️ **Кириллица в runtime-данных:** MER-ключи и литералы в коде — ASCII, но `WellName` и `LayerID` содержат кириллические значения в runtime (имена скважин «Скв-1», имена пластов «Д1», «ЮС2»). `std::string` как байтовый контейнер хранит UTF-8 кириллицу корректно — сравнение, map-ключи, передача по ссылке работают побайтово. **Консольный вывод:** ExceptionFactory.h выводит `well_name` через `std::cout` (после миграции). На Windows консоль по умолчанию использует OEM-кодовую страницу (cp866), не UTF-8. Чтобы кириллические имена отображались корректно, в `main.cpp` нужно вызвать `SetConsoleOutputCP(CP_UTF8)`. Добавлено в шаг 3 как подшаг. Источник кириллических данных (cp1251 файлы) — DEBT-005, DEBT-002 обеспечивает корректную передачу и вывод. `snprintf` кроссплатформенен (POSIX), в отличие от `swprintf` (разная сигнатура MSVC vs GCC)

## Обнаруженные проблемы

1. **JSONCreate.h:7 `#include "../UniversalSVParser.h"`** — JSONCreate не использует SVParser, но include тянет `<Windows.h>`. Удалить в шаге 8. Не блокирует: включение работает сейчас (Windows), после удаления будет только лучше
2. **`SomeWell::NameWide()`** — после миграции дублирует `Name()`. Можно удалить и заменить все call sites на `Name()`. Но это расширение скоупа — отмечаем как DEBT, не делаем

---

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай `vault/GDM/roadmap/технический долг.md` — запись DEBT-002
3. Прочитай GitHub issue: `gh issue view 29 --repo ArturSalamatin/GDM`
4. Создай ветку: `git checkout -b refactor/debt-002/wstring-to-string experimental`
5. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"`; `cmake --build build --config Release`
6. Прогони тесты: `ctest --test-dir build -C Release --output-on-failure`
7. Запомни количество тестов и время — это baseline
8. Начни с шага 1. После каждого шага: сборка + тесты

**Baseline:** 310 тестов, Release ~99 сек, Debug ~400 сек. 1 pre-existing warning: C4267 в test_JacobianAssembly.cpp:34.

---

## Шаг 1: Корневые alias'ы в defines.h

**Цель:** сменить `WellName`, `LayerID`, `SingleMERrecord` с `wstring` на `string` — корень каскада.

**Файлы:** `HydroSolver/defines.h`

**Контекст:**
`defines.h` определяет три alias'а, от которых зависят ВСЕ остальные файлы. Смена alias'ов вызовет каскад ошибок компиляции во всех зависимых файлах. Это ожидаемо — каждый последующий шаг исправляет конкретную группу зависимостей.

**Важно:** после этого шага проект **НЕ компилируется**. Компилируемость восстанавливается к концу шага 7. Это единственное исключение из правила «каждый шаг компилируется» — корневое определение невозможно менять инкрементально. Вместо этого шаги 1–7 выполняются как единый блок с одним коммитом в конце.

**Что сделать:**

1. В `HydroSolver/defines.h`:

До:
```cpp
using WellName = std::wstring;
```
После:
```cpp
using WellName = std::string;
```

До:
```cpp
using LayerID = std::wstring;
```
После:
```cpp
using LayerID = std::string;
```

До:
```cpp
using SingleMERrecord = std::map<std::wstring, float>;
```
После:
```cpp
using SingleMERrecord = std::map<std::string, float>;
```

2. Также в `defines.h`, строка 78, исправить комментарий:

До:
```cpp
/// Operations(WellJobs), performed on each well(std::wstring = well_name)
```
После:
```cpp
/// Operations(WellJobs), performed on each well(std::string = well_name)
```

**Оценка:** 4 строки

---

## Шаг 2: LogFile — API

**Цель:** сменить все `std::wstring` параметры `LogFile` на `std::string`.

**Файлы:** `HydroSolver/Helpers/LogFile.h`, `HydroSolver/Helpers/LogFile.cpp`

**Контекст:**
Все методы LogFile — пустые заглушки (реализация = `{}`). API на wstring — единственное, что нужно менять. LogFile используется в ExceptionFactory (шаг 3) и в Wells (шаг 5).

**Что сделать:**

1. В `LogFile.h` заменить все `std::wstring` на `std::string`:

До:
```cpp
static void Well_InstantiationStarted(const std::wstring& name);
static void Well_InstantiationEnded(const std::wstring& name);
static void Well_RequestTimeIsBeforeFirstMER(const std::wstring& name, int curTime, int firstDate);
static void Well_RequestTimeIsAfterLastMER(const std::wstring& name, int curTime, int lastDate);
static void Well_NoMER_Data(const std::wstring& name);
static void Well_out_of_domain(const std::wstring& name);
static void Well_AllMER_DataRemoved(const std::wstring& name);
static void Well_InitialMER_RecordsRemoved(const std::wstring& name, int i);
static void Well_LastMER_RecordsRemoved(const std::wstring& name, int i);
static void Well_OverallTimeFrame(const std::wstring& name, const std::pair<double, double>& interval);
```
После:
```cpp
static void Well_InstantiationStarted(const std::string& name);
static void Well_InstantiationEnded(const std::string& name);
static void Well_RequestTimeIsBeforeFirstMER(const std::string& name, int curTime, int firstDate);
static void Well_RequestTimeIsAfterLastMER(const std::string& name, int curTime, int lastDate);
static void Well_NoMER_Data(const std::string& name);
static void Well_out_of_domain(const std::string& name);
static void Well_AllMER_DataRemoved(const std::string& name);
static void Well_InitialMER_RecordsRemoved(const std::string& name, int i);
static void Well_LastMER_RecordsRemoved(const std::string& name, int i);
static void Well_OverallTimeFrame(const std::string& name, const std::pair<double, double>& interval);
```

2. Аналогично Perforation_FirstMoved, Perforation_LastMoved:

До:
```cpp
static void Perforation_FirstMoved(const std::wstring& name, double oldDate, double newDate);
static void Perforation_LastMoved(const std::wstring& name, int oldDate, int newDate);
```
После:
```cpp
static void Perforation_FirstMoved(const std::string& name, double oldDate, double newDate);
static void Perforation_LastMoved(const std::string& name, int oldDate, int newDate);
```

3. WriteLog:

До:
```cpp
static void WriteLog(std::wstring logLine, bool isNewLine = false);
static void WriteLog(
    std::wstring&& Sender,
    std::wstring&& ChildSender,
    std::wstring&& Status,
    std::wstring&& Options = L"",
    const char* msg0 = "");
```
После:
```cpp
static void WriteLog(std::string logLine, bool isNewLine = false);
static void WriteLog(
    std::string&& Sender,
    std::string&& ChildSender,
    std::string&& Status,
    std::string&& Options = "",
    const char* msg0 = "");
```

4. В `LogFile.cpp` — аналогичная замена всех `std::wstring` → `std::string` в параметрах реализаций.

**Оценка:** ~25 строк

---

## Шаг 3: ExceptionFactory.h — диагностика

**Цель:** заменить все `L"..."`, `std::wcout`, `std::wostream` в ExceptionFactory.h на narrow аналоги.

**Файлы:** `HydroSolver/Data/ExceptionFactory.h`

**Контекст:**
ExceptionFactory.h содержит ~40 точек с `L"..."` литералами, `std::wostream& out = std::wcout` (строка 27), и `std::to_wstring()`. Все warning/message классы выводят через `out <<`. WellName (уже `string` после шага 1) передаётся в конструкторы.

**Что сделать:**

1. Строка 27 — глобальный поток вывода:

До:
```cpp
static std::wostream& out = std::wcout;
```
После:
```cpp
static std::ostream& out = std::cout;
```

2. Все `L"..."` литералы → `"..."` (remove prefix `L`). Примеры:

До: `<< L">>>>>There is no permeability data in Y direction. "`
После: `<< ">>>>>There is no permeability data in Y direction. "`

До: `L"Permeability value = " + std::to_wstring(val) + L" is used instead."`
После: `"Permeability value = " + std::to_string(val) + " is used instead."`

3. Все `std::to_wstring(...)` → `std::to_string(...)`.

4. Все `std::wcout <<` → `std::cout <<` (строки 255, 262, 269, 276, 285, 295, 304).

5. В WarningFactory::MERrecordsAtSameDate (строка 400):

До:
```cpp
static void MERrecordsAtSameDate(const WellName& well_name, std::map<std::wstring, float>& r1, std::map<std::wstring, float>& r2)
```
После:
```cpp
static void MERrecordsAtSameDate(const WellName& well_name, std::map<std::string, float>& r1, std::map<std::string, float>& r2)
```

6. Аналогично в wMERrecordsAtSameDate (строка ~177):

До:
```cpp
wMERrecordsAtSameDate(const WellName& well_name, std::map<std::wstring, float>& r1, std::map<std::wstring, float>& r2)
```
После:
```cpp
wMERrecordsAtSameDate(const WellName& well_name, std::map<std::string, float>& r1, std::map<std::string, float>& r2)
```

7. Все `L"time"`, `L"oil_v"` и прочие MER-ключи → `"time"`, `"oil_v"` (в методах PrintMER-подобных, строки ~179-220).

8. В `src/main.cpp` — добавить `SetConsoleOutputCP(CP_UTF8)` в начало `main()` для корректного вывода кириллических имён скважин через `std::cout`:

До (начало `main()`):
```cpp
// (первая строка функции main)
```
После:
```cpp
#include <windows.h>
// ...
int main() {
    SetConsoleOutputCP(CP_UTF8);
    // ...
```

Это нужно потому что ExceptionFactory выводит `well_name` через `std::cout`, а имена скважин могут быть кириллическими. Без `SetConsoleOutputCP` Windows-консоль интерпретирует байты в OEM-кодовой странице (cp866), а не UTF-8.

**Примечание:** `<windows.h>` уже доступен через цепочку include (stdafx.h → ...), но явный include в main.cpp безопаснее. Для кроссплатформенности обернуть в `#ifdef _WIN32`.

**Оценка:** ~85 строк замены

---

## Шаг 4: MER_Descriptor — MER ключи

**Цель:** заменить все `L"..."` MER-ключи в MER_Descriptor.h/.cpp на narrow строки.

**Файлы:** `HydroSolver/Descriptors/MER_Descriptor.h`, `HydroSolver/Descriptors/MER_Descriptor.cpp`

**Контекст:**
MER-записи — это `map<string, float>` (после шага 1). Все ключи: `"time"`, `"oil_v"`, `"water_v"`, `"oil_m"`, `"water_m"`, `"pump_water"`, `"idle_time"`, `"type"`, `"is_work"`. В коде они были `L"time"`, `L"oil_v"` и т.д.

**Что сделать:**

1. В `MER_Descriptor.h`, строка 31:

До:
```cpp
return MERdata()[id].at(L"time"); }
```
После:
```cpp
return MERdata()[id].at("time"); }
```

2. В `MER_Descriptor.h`, строки 129-145 — PrintMER_Data: все `L"time"`, `L"oil_v"` и т.д. → `"time"`, `"oil_v"`.

3. В `MER_Descriptor.cpp` — все ~30 точек с `L"..."` ключами: `L"oil_v"` → `"oil_v"`, `L"time"` → `"time"`, `L"water_v"` → `"water_v"`, `L"oil_m"` → `"oil_m"`, `L"water_m"` → `"water_m"`, `L"pump_water"` → `"pump_water"`, `L"idle_time"` → `"idle_time"`, `L"type"` → `"type"`, `L"is_work"` → `"is_work"`.

4. В `MER_Descriptor.cpp`, строки 198-204 — метод `MER_Data::Name()`:

До:
```cpp
std::string MER_Data::Name() const
{
    std::string str;
    size_t size;
    str.resize(itsName.length());
    wcstombs_s(&size, &str[0], str.size() + 1, itsName.c_str(), itsName.size());
    return str;
}
```
После:
```cpp
std::string MER_Data::Name() const
{
    return itsName;
}
```

(`itsName` теперь `std::string` через alias `WellName`, конвертация через `wcstombs_s` не нужна и не скомпилируется.)

**Оценка:** ~40 строк замены

---

## Шаг 5: Wells — SomeWell, WellJobs, WellTrajectory

**Цель:** мигрировать wstring в модулях скважин.

**Файлы:**
- `HydroSolver/Reservoir/Well/SomeWell.h`
- `HydroSolver/Reservoir/Well/SomeWell.cpp`
- `HydroSolver/Reservoir/Well/WellJobs.h`
- `HydroSolver/Reservoir/Well/WellJobs.cpp`
- `HydroSolver/Reservoir/Well/WellTrajectory.h`

**Контекст:**
`SomeWell` хранит `itsGUID` как `wstring` и имеет два метода для имени: `NameWide()` → `const WellName&` и `Name()` → `std::string` (конвертация через `wcstombs_s`). После миграции `WellName = string`, `Name()` должен возвращать `itsName` напрямую, а `NameWide()` становится идентичным `Name()`.

`WellJobs.h` определяет `using LayerName = std::wstring` (строка 25) — это локальный alias, не связанный с `LayerID` из defines.h. Нужно мигрировать тоже.

**Что сделать:**

### SomeWell.h:

1. Строка 69:

До:
```cpp
std::wstring itsGUID;
```
После:
```cpp
std::string itsGUID;
```

2. Строка 85:

До:
```cpp
std::wstring Guid() const;
```
После:
```cpp
std::string Guid() const;
```

3. Строка 163:

До:
```cpp
SomeWell(const WellName& name, const std::wstring& guid,
```
После:
```cpp
SomeWell(const WellName& name, const std::string& guid,
```

4. Строки 223-267 — MER ключи `L"time"`, `L"oil_v"` и т.д. → `"time"`, `"oil_v"`.

5. Строка 267 — `std::wcout` → `std::cout`:

До:
```cpp
std::wcout << L"<<<<<  " << NameWide() << L" at date " << time << std::endl;
```
После:
```cpp
std::cout << "<<<<<  " << NameWide() << " at date " << time << std::endl;
```

### SomeWell.cpp:

1. Строки 70-75 — метод `Name()`:

До:
```cpp
		std::string str;
			size_t size;
			str.resize(NameWide().length());
			wcstombs_s(&size, &str[0], str.size() + 1, NameWide().c_str(), NameWide().size());
			return str;; }
```
После:
```cpp
		return itsName; }
```

(`itsName` теперь `std::string` через alias `WellName`, конвертация не нужна. Возвращаем напрямую, без лишней копии.)

2. Строка 107:

До:
```cpp
SomeWell::SomeWell(const WellName& name, const std::wstring& guid,
```
После:
```cpp
SomeWell::SomeWell(const WellName& name, const std::string& guid,
```

### WellJobs.h:

1. Строка 25:

До:
```cpp
using LayerName = std::wstring;
```
После:
```cpp
using LayerName = std::string;
```

### WellJobs.cpp:

1. Строка 51:

До:
```cpp
const std::wstring& WellName, 
```
После:
```cpp
const std::string& WellName, 
```

### WellTrajectory.h:

1. Строка 24 — `IntersectionCoords_json()`:

До:
```cpp
std::wstring IntersectionCoords_json() const { std::wstring result = L"[" + std::to_wstring(PosX()) + L"," + std::to_wstring(PosY()) + L"]";    return result; }
```
После:
```cpp
std::string IntersectionCoords_json() const { std::string result = "[" + std::to_string(PosX()) + "," + std::to_string(PosY()) + "]";    return result; }
```

**Оценка:** ~30 строк замены

---

## Шаг 6: ReservoirSimulator — вывод и пути

**Цель:** заменить wstring в ReservoirSimulator для путей файлов, print-методов, JSON-вывода.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSImulator.h`
- `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
ReservoirSimulator использует wstring для путей файлов вывода (`OutputPath()`, имена файлов в `PrintReservoirState()`), для параметров методов `printPointVariable()`, `printFieldVariable()`, и для бинарного ввода/вывода (`SaveFlowField2File_bin`, `LoadFlowFieldFromFile_bin`). JSON-вывод в `pressure_json()` использует `JSONObject`/`JSONArray` (wstring-based из JSONCreate.h) — это будет исправлено на шаге 8 вместе с JSONCreate.

**Что сделать:**

### ReservoirSImulator.h:

1. Строка 62:

До:
```cpp
std::wstring saveWaterSaturation_fName;
```
После:
```cpp
std::string saveWaterSaturation_fName;
```

2. Строка 65:

До:
```cpp
void PrintPlanarMesh(const std::wstring fName) const;
```
После:
```cpp
void PrintPlanarMesh(const std::string fName) const;
```

3. Строка 86:

До:
```cpp
void SaveFlowField2File(const std::wstring& configPath, const std::wstring& fileName);
```
После:
```cpp
void SaveFlowField2File(const std::string& configPath, const std::string& fileName);
```

4. Строка 89 — SaveFlowField2File_bin:

До:
```cpp
void SaveFlowField2File_bin(const std::wstring& configPath, const std::wstring& fileName,
```
После:
```cpp
void SaveFlowField2File_bin(const std::string& configPath, const std::string& fileName,
```

5. Аналогично SaveSaturationPressure_bin и LoadFlowFieldFromFile, LoadFlowFieldFromFile_bin.

### ReservoirSimulator.cpp:

1. Строки 9-11 — OutputPath():

До:
```cpp
std::wstring OutputPath()
{
    return L"ReservoirTestData//";
}
```
После:
```cpp
std::string OutputPath()
{
    return "ReservoirTestData//";
}
```

2. Строка 16:

До:
```cpp
std::wstring fName{ OutputPath() + L"well_coordinates.txt" };
```
После:
```cpp
std::string fName{ OutputPath() + "well_coordinates.txt" };
```

3. Строки 217-220:

До:
```cpp
std::wstring 
    saveOilSaturation_fName = OutputPath() + L"oil_saturation.txt",
    savePressure_fName = OutputPath() + L"pressure.txt",
    saveOverallBalance_fName = OutputPath() + L"overall_balance.txt";
```
После:
```cpp
std::string 
    saveOilSaturation_fName = OutputPath() + "oil_saturation.txt",
    savePressure_fName = OutputPath() + "pressure.txt",
    saveOverallBalance_fName = OutputPath() + "overall_balance.txt";
```

4. Строки 249, 267 — параметры:

До:
```cpp
void ReservoirSimulator::printPointVariable(const std::wstring fName,
```
После:
```cpp
void ReservoirSimulator::printPointVariable(const std::string fName,
```

До:
```cpp
void ReservoirSimulator::printFieldVariable(const std::wstring fName,
```
После:
```cpp
void ReservoirSimulator::printFieldVariable(const std::string fName,
```

5. Строки 455, 460, 494, 529, 535 — Save/Load методы: `const std::wstring&` → `const std::string&`, `L".bin"` → `".bin"`.

6. Строки 325-336 — JSON-вывод в pressure_json(): `L"x"` → `"x"`, `L"y"` → `"y"`, `L"p"` → `"p"`, `std::to_wstring()` → `std::to_string()`. Но `JSONObject` и `JSONArray` — typedef на `vector<pair<wstring,wstring>>` и `vector<wstring>` из JSONCreate.h. Эти typedef'ы изменятся на шаге 8. Если шаг 6 выполняется до шага 8, здесь будет конфликт типов. **Решение:** заменить `L"..."` на `"..."` и `to_wstring` на `to_string` только если JSONCreate уже мигрирован (шаг 8). В рамках единого блока 1–9 порядок не критичен — компиляция проверяется в конце.

7. Строки 290-291 — аналогично: `L"name"` → `"name"`, `L"p"` → `"p"`, `L"dimension"` → `"dimension"`, `L"names"` → `"names"`, `L"d0"` и т.д.

**Оценка:** ~50 строк замены

---

## Шаг 7: Anomaly/FlowField — wchar_t буферы

**Цель:** заменить `wchar_t` + `swprintf` + `std::wstring` на `char` + `snprintf` + `std::string` в FlowField, SomeFlowField, Trajectory.

**Файлы:**
- `HydroSolver/Anomaly/FlowField/Point.h`
- `HydroSolver/Anomaly/FlowField/SomeFlowField.h`
- `HydroSolver/Anomaly/FlowField/SomeFlowField.cpp`
- `HydroSolver/Anomaly/FlowField/FlowField.h`
- `HydroSolver/Anomaly/Trajectory.h`
- `HydroSolver/Solver/Grids/RawHorizon.h`
- `HydroSolver/Solver/Math/MathRoutines.h`

**Контекст:**
Эти файлы используют `wchar_t buffer[N]; swprintf(buffer, N, L"...", ...)` для форматирования чисел в строку. Метод `print()` возвращает `std::wstring` — но не вызывается из тестов или main (только внутренне в SomeFlowField). `FlowField.h` также содержит `using Features = struct { std::wstring layer_name; }` и `std::map<std::wstring, double>` параметры.

**Что сделать:**

### SomeFlowField.h:

1. Все `const std::wstring print() const;` → `const std::string print() const;` (строки 63, 123, 177-178).

### SomeFlowField.cpp:

1. Строки 26-34 — SomeField2D::print():

До:
```cpp
std::wstring result;
for (int j = 0; j < field.size(); j++)
    for (int i = 0; i < field[j].size(); i++)
    {
        wchar_t buffer[40];
        swprintf(buffer, 40, L"%+19.11E;", field[j][i]);
        result += buffer;
    }
return result;
```
После:
```cpp
std::string result;
for (int j = 0; j < field.size(); j++)
    for (int i = 0; i < field[j].size(); i++)
    {
        char buffer[40];
        snprintf(buffer, 40, "%+19.11E;", field[j][i]);
        result += buffer;
    }
return result;
```

2. Строки 49-56 — FlowFieldSnapshot::print():

До:
```cpp
const std::wstring FlowFieldSnapshot::print() const
{
    wchar_t buffer[40];
    swprintf(buffer, 40, L"%+19.11E;", time);
    std::wstring result = buffer;
    result += vxField.print() + vyField.print();
    return result;
}
```
После:
```cpp
const std::string FlowFieldSnapshot::print() const
{
    char buffer[40];
    snprintf(buffer, 40, "%+19.11E;", time);
    std::string result = buffer;
    result += vxField.print() + vyField.print();
    return result;
}
```

3. Строки 86-95 — SomeFlowField::print():

До:
```cpp
const std::wstring
    SomeFlowField::print() const
{
    wchar_t buffer[40];
    swprintf(buffer, 40, L"%u;", field.size());
    std::wstring result = buffer;
```
После:
```cpp
const std::string
    SomeFlowField::print() const
{
    char buffer[40];
    snprintf(buffer, 40, "%u;", (unsigned int)field.size());
    std::string result = buffer;
```

### Trajectory.h:

1. Строки 59-71 — PrintTrajectory():

До:
```cpp
std::wstring PrintTrajectory() const
{
    wchar_t buffer[200];
    swprintf(buffer, 200, L"%u;", (unsigned int)points.size());
    std::wstring result{ buffer };
    for (int l = 0; l < points.size(); l++)
    {
        wchar_t buffer[700];
        swprintf(buffer, 700, L"%+19.11E;%+19.11E;", points[l].x(), points[l].y());
        result += buffer;
    }
    return result;
}
```
После:
```cpp
std::string PrintTrajectory() const
{
    char buffer[200];
    snprintf(buffer, 200, "%u;", (unsigned int)points.size());
    std::string result{ buffer };
    for (int l = 0; l < points.size(); l++)
    {
        char buffer[700];
        snprintf(buffer, 700, "%+19.11E;%+19.11E;", points[l].x(), points[l].y());
        result += buffer;
    }
    return result;
}
```

### Point.h (`HydroSolver/Anomaly/FlowField/Point.h`):

Point.h определяет `print_json()` для `Point`, `Contour`, `geos_polygon` — все возвращают `std::wstring` с `std::to_wstring`. Файл в сборке (Point.cpp в target_sources, строка 72 CMakeLists.txt). Методы inline — не вызываются из живого кода, но определения содержат wstring.

1. Строка 30 — `Point::print_json()`:

До:
```cpp
const std::wstring print_json() const
{
    return L"[" + std::to_wstring((int)x()) + L"," + std::to_wstring((int)y()) + L"]";
}
```
После:
```cpp
const std::string print_json() const
{
    return "[" + std::to_string((int)x()) + "," + std::to_string((int)y()) + "]";
}
```

2. Строки 110-121 — `Contour::print_json()`:

До:
```cpp
std::wstring print_json() const
{
    if (size() == 0)
        return L"[]";
    std::wstring result = L"[";
    for (size_t i{0ll}; i < size(); ++i)
    {
        if (i > 0) result += L",";
        result += Point(X[i], Y[i]).print_json();
    }
    return result + L"]";
}
```
После:
```cpp
std::string print_json() const
{
    if (size() == 0)
        return "[]";
    std::string result = "[";
    for (size_t i{0ll}; i < size(); ++i)
    {
        if (i > 0) result += ",";
        result += Point(X[i], Y[i]).print_json();
    }
    return result + "]";
}
```

3. Строка 142 — `geos_polygon::print_json()`:

До:
```cpp
std::wstring print_json() const { return L"[]"; }
```
После:
```cpp
std::string print_json() const { return "[]"; }
```

### RawHorizon.h (`HydroSolver/Solver/Grids/RawHorizon.h`):

В сборке (через DevelopedHorizon.h → OilField.h). Строка 39 — прямой `std::wstring`, не через alias.

1. Строка 39:

До:
```cpp
std::wstring fName{ L"ReservoirTestData//grid_descriptor.txt" };
```
После:
```cpp
std::string fName{ "ReservoirTestData//grid_descriptor.txt" };
```

### FlowField.h:

1. Строка 243:

До:
```cpp
using Features = struct { std::wstring layer_name; };
```
После:
```cpp
using Features = struct { std::string layer_name; };
```

2. Строка 254:

До:
```cpp
const std::wstring& layer_name() { return features.layer_name; }
```
После:
```cpp
const std::string& layer_name() { return features.layer_name; }
```

3. Строка 256:

До:
```cpp
FlowField(const SomeFlowField::Ptr& f, const std::wstring& layer_name)
```
После:
```cpp
FlowField(const SomeFlowField::Ptr& f, const std::string& layer_name)
```

4. Все `std::map<std::wstring, double>` → `std::map<std::string, double>` (строки 29, 63, 104, 270, 284, 290, 303).

5. Закомментированные блоки с `wchar_t` (строки 197-212, 314-317) — заменить `wchar_t` → `char`, `swprintf` → `snprintf`, `L"..."` → `"..."`, `std::wstring` → `std::string`. Или оставить как есть (комментарии).

### MathRoutines.h:

1. Строка 240:

До:
```cpp
std::wcout << result.size() << std::endl;
```
После:
```cpp
std::cout << result.size() << std::endl;
```

**Оценка:** ~70 строк замены (Point.h +15, RawHorizon.h +1)

---

## Шаг 8: JSONCreate.h и заголовки вне сборки

**Цель:** мигрировать wstring в JSONCreate.h (используется живым кодом через #include) и удалить ненужный include UniversalSVParser.h.

**Файлы:**
- `HydroSolver/Utils/JSON/JSONCreate.h`
- `HydroSolver/Helpers/Config.h`
- `HydroSolver/Helpers/Config_JSON.h`
- `HydroSolver/Reservoir/FluxReader.h`
- `HydroSolver/Helpers/DataPrinter.h` (если содержит wstring)

**Контекст:**
`JSONCreate.h` определяет `JSONObject = vector<pair<wstring,wstring>>` и `JSONArray = vector<wstring>`. Этот файл включается в `ReservoirSimulator.cpp` (строка 5). Замена typedef'ов согласует JSONCreate с narrow-строками из шага 6.

`JSONCreate.h:7` содержит `#include "../UniversalSVParser.h"` — это тянет `<Windows.h>` и не используется JSONCreate. Удалить.

`Config.h`, `Config_JSON.h`, `FluxReader.h` — не компилируются (.cpp не в target_sources), но заголовки подтягиваются. Мигрируем заголовки для согласованности.

**Что сделать:**

### JSONCreate.h:

1. Строка 7 — удалить:

До:
```cpp
#include "../UniversalSVParser.h"
```
После:
(удалить строку)

2. Строки 9-10 — typedef'ы:

До:
```cpp
typedef std::vector<std::pair<std::wstring, std::wstring>> JSONObject;
typedef std::vector<std::wstring> JSONArray;
```
После:
```cpp
typedef std::vector<std::pair<std::string, std::string>> JSONObject;
typedef std::vector<std::string> JSONArray;
```

3. Все методы класса `CreateJSON` — заменить `std::wstring` → `std::string`, `L"..."` → `"..."`, `std::wregex` → `std::regex`, `std::wsmatch` → `std::smatch`, `std::wcout` → `std::cout`.

### PathUtils.h (`HydroSolver/Utils/PathUtils.h`):

Включается из Config.h (строка 6). Содержит 6 точек: `std::wstring`, `std::wregex`, `L"\\"`, `L"."`, `L","`, `std::wstring::npos`.

1. Все `std::wstring` → `std::string`, `L"\\"` → `"\\"`, `L"."` → `"."`, `L","` → `","`, `L".\\"`  → `".\\"`, `std::wstring::npos` → `std::string::npos`.
2. Строка 71: `std::wregex date_regex(L"\\d{2}\\W\\d{2}\\W\\d{4}")` → `std::regex date_regex("\\d{2}\\W\\d{2}\\W\\d{4}")`.

### Config.h:

1. Все `std::wstring` → `std::string`, `L"..."` → `"..."`.

### Config_JSON.h:

1. Все `std::wstring` → `std::string`, `L"..."` → `"..."`, `std::wcout` → `std::cout`.

### FluxReader.h:

**Не мигрировать на string.** FluxReader.h:64 передаёт `path` в `UniversalSCParser::CP1251SVParser(path)`, который ожидает `std::wstring`. Файл мёртвый (не included нигде), но замена wstring→string создаст невалидный вызов. Миграция FluxReader.h — вместе с UniversalSVParser в DEBT-005.

### DataPrinter.h:

1. Содержит ~15 точек wstring: конструктор `CubePrinter(... const std::wstring& path)`, методы `PrintActNum(std::wstring fName = L"...")` и т.д., поле `std::wstring path`, шаблонный метод `DataPrinter(... const std::wstring& fName)`, `MakeDirectories(const std::wstring& fName)`. Заменить все `std::wstring` → `std::string`, `L"..."` → `"..."`.
2. Файл не included никуда (мёртвый), но мигрируем для согласованности.

**Подводные камни:**
- Удаление `#include "../UniversalSVParser.h"` из JSONCreate.h: убедиться, что JSONCreate не использует ни одного типа/функции из UniversalSVParser. Grep по именам из SVParser в JSONCreate — `UniversalSCParser`, `SVParser`, `FileReader` — если нет вхождений, удаление безопасно.

**Оценка:** ~80 строк замены

---

## Шаг 9: Тесты

**Цель:** мигрировать `L"..."` литералы в тестовых файлах.

**Файлы:**
- `tests/test_helpers.h`
- `tests/well_schedule_builder.h`
- `tests/unit/descriptors/test_MER_Data.cpp`
- `tests/unit/wells/test_WellJobs.cpp`
- `tests/test_3d_completions.cpp`
- `tests/test_amgcl_benchmark.cpp`
- `tests/test_buckley_leverett.cpp`
- `tests/test_five_spot.cpp`
- `tests/test_inactive_cells.cpp`
- `tests/test_solver_comparison.cpp`
- `tests/test_variable_debit.cpp`
- `tests/simulation_cases/TwoWellCase.h`
- `tests/simulation_cases/SingleInjectorCase.h`
- `tests/unit/math/test_JacobianAssembler_standalone.cpp`
- `tests/unit/math/test_MassBalanceTracker_standalone.cpp`
- `tests/unit/math/test_NewtonSolver_standalone.cpp`
- `tests/unit/math/test_TimeIntegrator_standalone.cpp`

**Контекст:**
Тесты используют `WellName` (теперь `string`) и `L"..."` литералы для MER-ключей и имён скважин. После шагов 1-8 типы уже narrow, но литералы в тестах остались wide. Помимо unit-тестов MER_Data и WellJobs, все интеграционные и standalone тесты передают `L"INJ"`, `L"PROD"` и т.д. в `add_simple_well()` и `WellScheduleBuilder` — эти литералы тоже нужно мигрировать.

**Что сделать:**

### test_MER_Data.cpp:

1. Функция `make_record()` (строки 12-22):

До:
```cpp
return {
    {L"time", time},
    {L"oil_v", oil_v},
    {L"water_v", water_v},
    {L"oil_m", oil_m},
    {L"water_m", water_m},
    {L"pump_water", pump_water},
    {L"idle_time", idle_time},
    {L"type", type},
    {L"is_work", is_work}
};
```
После:
```cpp
return {
    {"time", time},
    {"oil_v", oil_v},
    {"water_v", water_v},
    {"oil_m", oil_m},
    {"water_m", water_m},
    {"pump_water", pump_water},
    {"idle_time", idle_time},
    {"type", type},
    {"is_work", is_work}
};
```

2. Все `std::wstring name = L"TestWell"` → `std::string name = "TestWell"` (строки 38, 53, 66, 82, 96).

### test_WellJobs.cpp:

1. Все `std::wstring name = L"W1"` → `std::string name = "W1"` (строки 11, 23, 42, 57, 73, 83, 95, 107).

### test_WellJobs.cpp (дополнительно):

2. Строка 17: `CHECK(wj.Name() == L"W1")` → `CHECK(wj.Name() == "W1")`. После миграции `Name()` возвращает `std::string`, сравнение с `L"W1"` не скомпилируется.

### test_helpers.h:

1. Строки 94-103 — все `L"time"`, `L"oil_m"` и т.д. → `"time"`, `"oil_m"`.

### well_schedule_builder.h:

1. Строки 59-68 — все `L"time"`, `L"oil_m"` и т.д. → `"time"`, `"oil_m"`.

### Интеграционные и standalone тесты:

Во всех перечисленных файлах заменить `L"..."` литералы имён скважин на `"..."`:

1. `test_3d_completions.cpp` — ~19 точек: `L"INJ-1"` → `"INJ-1"`, `L"PROD-1"` → `"PROD-1"` и т.д.
2. `test_amgcl_benchmark.cpp` — ~6 точек: `L"INJ-1"` → `"INJ-1"`, `L"PROD-1"` → `"PROD-1"` и т.д.
3. `test_buckley_leverett.cpp` — ~12 точек: `L"INJ"` → `"INJ"`, `L"PROD"` → `"PROD"` и т.д.
4. `test_five_spot.cpp` — ~10 точек: `L"INJ"` → `"INJ"`, `L"P1"` → `"P1"` и т.д.
5. `test_inactive_cells.cpp` — 2 точки: `L"PROD"` → `"PROD"`.
6. `test_solver_comparison.cpp` — 2 точки: `L"INJ"` → `"INJ"`, `L"PROD"` → `"PROD"`.
7. `test_variable_debit.cpp` — ~5 точек: `L"INJ"` → `"INJ"`, `L"PROD"` → `"PROD"` и т.д.
8. `simulation_cases/TwoWellCase.h` — 2 точки: `L"INJ"` → `"INJ"`, `L"PROD"` → `"PROD"`.
9. `simulation_cases/SingleInjectorCase.h` — 1 точка: `L"INJ"` → `"INJ"`.
10. `test_JacobianAssembler_standalone.cpp` — 2 точки: `L"INJ"` → `"INJ"`, `L"PROD"` → `"PROD"`.
11. `test_MassBalanceTracker_standalone.cpp` — 4 точки.
12. `test_NewtonSolver_standalone.cpp` — 4 точки.
13. `test_TimeIntegrator_standalone.cpp` — 4 точки.

Паттерн одинаковый: `L"имя"` → `"имя"` в аргументах `add_simple_well()` и `emplace_back()`.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- **Все 310 тестов зелёные**

**Оценка:** ~95 строк замены

---

## Шаг 10: Файлы вне сборки (.cpp) и остаточные wstring

**Цель:** мигрировать оставшиеся wstring в файлах, чьи .cpp не компилируются, и убедиться что grep по wstring/wchar_t/L" даёт ноль в живом коде.

**Файлы:**
- `HydroSolver/Anomaly/Anomaly.h`
- `HydroSolver/Anomaly/Anomaly.cpp`
- `HydroSolver/Utils/JSON/geojson.h`
- `HydroSolver/Utils/JSON/geojson.cpp`
- `HydroSolver/Utils/JSON/JSONCreate.cpp`
- `HydroSolver/Utils/BinaryFileHandler.h`
- `HydroSolver/Utils/ModelHandler.h`
- `HydroSolver/Utils/IRCGEngine.h`
- `HydroSolver/Utils/IRCGEngine.cpp`
- `HydroSolver/Reservoir/FluxReader.h` (см. шаг 8 — не мигрировать, зависит от UniversalSVParser)
- `HydroSolver/Utils/WellDataHandler.h`
- `HydroSolver/Utils/WellDataHandler.cpp`
- `HydroSolver/Temp/INIReader.h`
- `HydroSolver/Temp/CacheFile.h`

**Контекст:**
Эти файлы не входят в target_sources для gdm_core, но содержат wstring. Мигрируем для согласованности кодовой базы. `.back` файлы и `UniversalSVParser.*`/`UniversalSVWriter.*` НЕ трогаем — это DEBT-005 территория.

**Что сделать:**

1. Для каждого файла: `wstring` → `string`, `L"..."` → `"..."`, `wchar_t` → `char`, `swprintf` → `snprintf`, `wcout` → `cout`, `wostream` → `ostream`, `to_wstring` → `to_string`.

2. **BinaryFileHandler.h/.cpp** — особый случай: `.cpp` использует `GetPrivateProfileStringW` (Win32 API, принимает wchar_t*). Заголовок мигрируем (параметры `wstring` → `string`), а в .cpp — либо добавить конвертацию string→wstring перед Win32 вызовами, либо оставить .cpp как есть (он не компилируется). Рекомендация: оставить .cpp как есть, мигрировать только заголовок. BinaryFileHandler — кандидат на удаление или полную переработку в фазе 7 (файловый ввод).

3. Финальная проверка:
```
grep -rn "wstring\|wchar_t\|L\"" HydroSolver/ --include="*.h" --include="*.cpp" | grep -v ".back" | grep -v "UniversalSV"
```
Если остаются вхождения — исправить.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 310 тестов зелёные
- Grep по `wstring`/`wchar_t`/`L"` в живых файлах (без .back и UniversalSV) — ноль

**Оценка:** ~100 строк замены

---

## Коммиты

Шаги 1–9 — единый коммит: `refactor: wstring → string (UTF-8) в ядре`

Шаг 10 — отдельный коммит: `refactor: wstring → string в файлах вне сборки`

Причина: шаги 1–9 неразделимы (корневой alias меняется в шаге 1, компилируемость восстанавливается только после шага 9). Шаг 10 — независимая очистка.

---

## Тестовая стратегия

Новых тестов не требуется — изменение чисто механическое (замена типов строк). Вычисления, физика, численные методы не затрагиваются.

**Regression:** все 310 существующих тестов зелёные. Ключевые тесты, покрывающие затронутый код:
- `unit.level2.descriptors.*` — MER_Data (ключи, сортировка, CleanMER)
- `unit.level3.wells.*` — WellJobs (конструктор, AccumulatePerforations)
- `integration.*` — все интеграционные (используют WellName, MER)
- `unit.level4.*` — JacobianAssembler и т.д. (используют test_helpers.h → WellName)

---

## Критерии завершения

- [ ] `WellName = std::string`, `LayerID = std::string`, `SingleMERrecord = map<string, float>`
- [ ] Ноль `wstring`/`wchar_t`/`L"..."` в живых файлах (кроме .back и UniversalSV*)
- [ ] Все 310 тестов зелёные (Release + Debug)
- [ ] Ноль новых warnings
- [ ] LogFile API на `std::string`
- [ ] JSONCreate typedef'ы на `std::string`
- [ ] `#include "../UniversalSVParser.h"` удалён из JSONCreate.h
- [ ] Vault обновлён: DEBT-002 статус, приоритеты
- [ ] GitHub issue #29 прокомментирован с результатом
