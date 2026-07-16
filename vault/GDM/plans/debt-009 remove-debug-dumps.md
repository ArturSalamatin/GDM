---
tags:
  - план
  - инфраструктура
date: 2026-07-16
issue: DEBT-009
github: 30
branch: refactor/debt-009/remove-debug-dumps
status: реализован
audit:
  date: 2026-07-16
  round: 2
  findings: 0 критических / 0 существенных / 1 мелких
  auto-fixed: 0
  manual-required: 0
  note: повторный аудит после автофикса 4 находок в раунде 1
---

# DEBT-009: управляемый отладочный дамп с изоляцией по target

## Суть задачи

Код ядра безусловно пишет отладочные файлы `test_*.txt` и `well_MER_debit.txt` при **каждом** запуске — в конструкторах `ProcessGrid_3D`, `MER_Data`, `SomeWell`. Файлы пишутся в cwd, перетирая друг друга при запуске разных исполняемых файлов.

**Цель:** сохранить весь отладочный функционал, но:
1. Включать только при сборке с `-DGDM_DUMP_DEBUG=ON` (compile-time, нулевой overhead в production)
2. Писать в изолированную папку `results/debug/<exe_name>/` (каждый target — своя папка, не перетирают друг друга)

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай запись DEBT-009 в `vault/GDM/roadmap/известные баги и технический долг.md`
3. Прочитай GitHub issue: `gh issue view 30 --repo ArturSalamatin/GDM`
4. Создай ветку: `git checkout -b refactor/debt-009/remove-debug-dumps experimental`
5. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
6. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
7. Запомни количество тестов и время — это baseline (ожидается: 310 pass, ~100–123 сек)
8. Начни с шага 1. После каждого шага: сборка + тесты

## Архитектура решения

### Compile-time: `#ifdef GDM_DUMP_DEBUG`

CMake option `GDM_DUMP_DEBUG` (по умолчанию OFF). При включении:
- Определяет макрос `GDM_DUMP_DEBUG` для всех target-ов, линкующих `gdm_core`
- Определяет макрос `GDM_EXE_NAME` с именем конкретного target-а для каждого исполняемого файла

### Runtime: класс `DebugDump`

Заголовок `HydroSolver/Helpers/DebugDump.h`. Под `#ifdef GDM_DUMP_DEBUG`:
- `DebugDump::path("test_connections.txt")` → `"results/debug/gdm_tests/test_connections.txt"`
- Автоматически создаёт каталог при первом вызове
- Имя exe берётся из макроса `GDM_EXE_NAME` (задаётся CMake для каждого target)

Под `#ifndef GDM_DUMP_DEBUG`:
- Класс пустой — все методы no-op, `enabled()` возвращает `false`
- Весь код дампов внутри `if constexpr (DebugDump::enabled())` или `#ifdef GDM_DUMP_DEBUG` — компилятор полностью вырезает

### Использование в коде

Вместо:
```cpp
std::ofstream myfile;
myfile.open("test_connections.txt");
// ...
```

Будет:
```cpp
#ifdef GDM_DUMP_DEBUG
{
    std::ofstream myfile(DebugDump::path("test_connections.txt"));
    // ...
}
#endif
```

Безусловные вызовы в конструкторах (`PrintMER()`, `printConnectivity()`, `PrintWell()`) оборачиваются в `#ifdef GDM_DUMP_DEBUG`.

## Затронутые файлы

| Файл | Роль |
|---|---|
| `CMakeLists.txt` | CMake option + `target_compile_definitions` |
| `HydroSolver/Helpers/DebugDump.h` | **новый** — класс управления дампом |
| `HydroSolver/Solver/Grids/AbstractGrid.h` | обернуть `printConnectivity()` в `#ifdef` + путь через DebugDump |
| `HydroSolver/Descriptors/MER_Descriptor.cpp` | обернуть вызов `PrintMER()` в `#ifdef` |
| `HydroSolver/Descriptors/MER_Descriptor.h` | `#ifdef` вокруг объявлений `PrintMER()` |
| `HydroSolver/Descriptors/MER_Descriptor.cpp` | `#ifdef` вокруг реализаций `PrintMER()` + путь через DebugDump |
| `HydroSolver/Reservoir/Well/SomeWell.cpp` | обернуть `PrintWell()`, `PrintMER("inside_well")` в `#ifdef` + путь |
| `HydroSolver/Reservoir/Well/SomeWell.h` | `#ifdef` вокруг `PrintWell()`, `PrintWellMERDebit()`, `OutputPath()` |
| `HydroSolver/Solver/Math/LinearProblem.cpp` | `#ifdef` вокруг `PrintRHS()`, `PrintCorrections()`, `Print()` |
| `HydroSolver/Solver/Math/LinearProblem.h` | `#ifdef` вокруг объявлений |
| `HydroSolver/Solver/Math/MatrixCSR.cpp` | `#ifdef` вокруг `PrintCRS()`, `PrintDiagBlocks()` |
| `HydroSolver/Solver/Math/MatrixCSR.h` | `#ifdef` вокруг объявлений |
| `HydroSolver/Solver/Math/SparsityPattern.cpp` | `#ifdef` вокруг `printPattern()`, `printDiagonalBlocks()`, `printOffDiagBlocks()` |
| `HydroSolver/Solver/Math/SparsityPattern.h` | `#ifdef` вокруг объявлений |
| `tests/test_buckley_leverett.cpp` | BL CSV → `results/validation/` (не зависит от `GDM_DUMP_DEBUG`) |
| `.gitignore` | очистить правила для файлов, которые больше не пишутся в cwd |

## Зона неприкосновенности

- `ReservoirSimulator::OutputPath()` — `"ReservoirTestData//"`, production-вывод, **не трогать**
- `ReservoirSimulator::PrintWellCoords()`, `SaveAllData()` — production-вывод, **не трогать**
- Шаблонные helper-ы (`sendMER2Stream`, `sendCRS2Stream`, `sendDiagVals2Stream`, `printPattern2Stream`, `printDiagBlocks2Stream`, `printOffDiagBlocks2Stream`, `PrintWellDebitLength`) — **оставить без изменений**, они не зависят от `<fstream>` и не пишут файлы

## Поиск подводных камней

- ✅ **Все call sites найдены:** `printConnectivity()` — 1 (AbstractGrid.h:204); `PrintMER()` — 1 (MER_Descriptor.cpp:177); `PrintMER("inside_well")` — 1 (SomeWell.cpp:165); `PrintWell()` — 1 (SomeWell.cpp:153); мёртвые Print-методы (LinearProblem, MatrixCSR, SparsityPattern) — 0 вызовов
- ✅ **Потокобезопасность:** дамп-методы не вызываются из OpenMP-секций; `DebugDump` — потокобезопасен (только чтение после инициализации)
- ✅ **Зависимости сборки:** CMakeLists.txt затрагивается (добавление option), но target_sources не меняется
- ✅ **Обратная совместимость API:** при `GDM_DUMP_DEBUG=OFF` (default) поведение идентично текущему минус побочный эффект записи файлов
- ✅ **Тесты:** тесты не вызывают ни один из Print-методов (проверено grep по tests/)
- ✅ **Мёртвый код:** `LinearProblem::Print()`, `MatrixCSR::PrintCRS/PrintDiagBlocks`, `SparsityPattern::print*()` не вызываются — но сохраняются за `#ifdef` для ручной отладки
- ✅ **Производительность:** при `OFF` — нулевой overhead (compile-time exclusion); при `ON` — I/O как сейчас

---

## Шаги реализации

### Шаг 1: Создать DebugDump.h и CMake option

**Цель:** инфраструктура для управления дампами — без изменения существующего кода

**Файлы:**
- `HydroSolver/Helpers/DebugDump.h` (новый)
- `CMakeLists.txt`

**Контекст:**
Нужен простой заголовок, который:
- Под `GDM_DUMP_DEBUG` предоставляет `DebugDump::path(filename)` → `"results/debug/<exe_name>/<filename>"`
- Без `GDM_DUMP_DEBUG` — пустой (ничего не делает, `enabled()` = false)
- Использует макрос `GDM_EXE_NAME`, который CMake задаёт для каждого target-а
- Создаёт каталог автоматически при первом вызове `path()`

CMake:
- `option(GDM_DUMP_DEBUG ...)` + `if(GDM_DUMP_DEBUG) target_compile_definitions(gdm_core PUBLIC GDM_DUMP_DEBUG)`
- Для каждого исполняемого target: `target_compile_definitions(<target> PRIVATE GDM_EXE_NAME="<target>")`

**Что сделать:**

1. Создать `HydroSolver/Helpers/DebugDump.h`:
```cpp
#pragma once

#include <string>

#ifdef GDM_DUMP_DEBUG
#include <filesystem>

#ifndef GDM_EXE_NAME
#define GDM_EXE_NAME "unknown"
#endif
#endif

namespace debug_dump {

class DebugDump {
public:
    static constexpr bool enabled() {
#ifdef GDM_DUMP_DEBUG
        return true;
#else
        return false;
#endif
    }

#ifdef GDM_DUMP_DEBUG
    static std::string path(const std::string& filename) {
        static const std::string base = init_base();
        return base + filename;
    }

    static std::string dir(const std::string& subdir) {
        static const std::string base = init_base();
        std::string full = base + subdir;
        std::filesystem::create_directories(full);
        return full + "/";
    }

private:
    static std::string init_base() {
        std::string base = "results/debug/" + std::string(GDM_EXE_NAME) + "/";
        std::filesystem::create_directories(base);
        return base;
    }
#endif
};

} // namespace debug_dump
```

2. В `CMakeLists.txt` — после определения `gdm_core`:
   - Добавить option и compile definition:
```cmake
option(GDM_DUMP_DEBUG "Enable debug file dumps to results/debug/<exe>/" OFF)
if(GDM_DUMP_DEBUG)
    target_compile_definitions(gdm_core PUBLIC GDM_DUMP_DEBUG)
endif()
```
   - Для каждого executable target добавить `GDM_EXE_NAME`. Найти все `add_executable` и после каждого (или после `target_link_libraries`) добавить:
```cmake
target_compile_definitions(gdm PRIVATE GDM_EXE_NAME="gdm")
target_compile_definitions(gdm_tests PRIVATE GDM_EXE_NAME="gdm_tests")
target_compile_definitions(gdm_benchmark PRIVATE GDM_EXE_NAME="gdm_benchmark")
target_compile_definitions(gdm_unit_level0 PRIVATE GDM_EXE_NAME="gdm_unit_level0")
target_compile_definitions(gdm_unit_level1 PRIVATE GDM_EXE_NAME="gdm_unit_level1")
target_compile_definitions(gdm_unit_level2 PRIVATE GDM_EXE_NAME="gdm_unit_level2")
target_compile_definitions(gdm_unit_level3 PRIVATE GDM_EXE_NAME="gdm_unit_level3")
target_compile_definitions(gdm_unit_level4 PRIVATE GDM_EXE_NAME="gdm_unit_level4")
```
   - Для examples (в цикле `foreach`):
```cmake
target_compile_definitions(${ex} PRIVATE GDM_EXE_NAME="${ex}")
```

**Проверка после этого шага:**
- Сборка: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 310 тестов зелёные (DebugDump.h создан, но нигде не используется — no-op)
- Также: сборка с `GDM_DUMP_DEBUG=ON`: `cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_DUMP_DEBUG=ON; if ($?) { cmake --build build --config Release }` — должна скомпилироваться без ошибок

**Подводные камни:**
- `GDM_EXE_NAME` — строковый макрос, в CMake нужно экранировать кавычки: `GDM_EXE_NAME="gdm"` (CMake передаст как `-DGDM_EXE_NAME=\"gdm\"`)
- `std::filesystem` — уже используется в проекте (SomeWell.cpp, ReservoirSimulator.cpp), C++23 поддерживает

**Зависимости:**
- Требует: —
- Блокирует: все последующие шаги

**Оценка:** ~50 строк нового кода + ~15 строк CMake, ~15 минут

---

### Шаг 2: Обернуть printConnectivity() в #ifdef

**Цель:** дамп connectivity — только при GDM_DUMP_DEBUG

**Файлы:**
- `HydroSolver/Solver/Grids/AbstractGrid.h`

**Контекст:**
`printConnectivity()` — inline-метод в шаблонном классе `SomeStructuredGrid3Dim`. Вызывается из конструктора (строка 204). Пишет `test_connections.txt` в cwd.

**Что сделать:**
1. Добавить `#include "../../Helpers/DebugDump.h"` в начало файла (или после существующих include)
2. Обернуть вызов в конструкторе (строка 204) в `#ifdef GDM_DUMP_DEBUG`
3. Обернуть метод `printConnectivity()` (строки 275–289) в `#ifdef GDM_DUMP_DEBUG`
4. Заменить путь файла на `DebugDump::path("test_connections.txt")`

**Изменения:**

Конструктор:
До:
```cpp
				SetConnectivityGraph_3D(active_cells);
				printConnectivity();
			}
```
После:
```cpp
				SetConnectivityGraph_3D(active_cells);
#ifdef GDM_DUMP_DEBUG
				printConnectivity();
#endif
			}
```

Метод:
До:
```cpp
			// print connectivityGraph to file
			void printConnectivity()
			{
				std::ofstream myfile;
				myfile.open("test_connections.txt");

				for (int l = 0; l < activeCellsNmbr; l++)
				{
					for (auto neighbourId : connectivityGraph[l])
					{
						myfile << neighbourId << " ";
					}
					myfile << std::endl;
				}
			}
```
После:
```cpp
#ifdef GDM_DUMP_DEBUG
			void printConnectivity()
			{
				std::ofstream myfile;
				myfile.open(debug_dump::DebugDump::path("test_connections.txt"));

				for (int l = 0; l < activeCellsNmbr; l++)
				{
					for (auto neighbourId : connectivityGraph[l])
					{
						myfile << neighbourId << " ";
					}
					myfile << std::endl;
				}
			}
#endif
```

**Проверка после этого шага:**
- Сборка (без GDM_DUMP_DEBUG): `cmake --build build --config Release` — 310 pass, `test_connections.txt` не появляется
- Сборка (с GDM_DUMP_DEBUG): пересобрать с `-DGDM_DUMP_DEBUG=ON`, запустить один тест — файл должен появиться в `results/debug/<exe>/test_connections.txt`

**Зависимости:**
- Требует: шаг 1
- Блокирует: —

**Оценка:** ~5 строк изменено, ~5 минут

---

### Шаг 3: Обернуть PrintMER() в #ifdef

**Цель:** дамп MER — только при GDM_DUMP_DEBUG

**Файлы:**
- `HydroSolver/Descriptors/MER_Descriptor.cpp`
- `HydroSolver/Descriptors/MER_Descriptor.h`
- `HydroSolver/Reservoir/Well/SomeWell.cpp` (только строка 165 — вызов `PrintMER("inside_well")`)

**Контекст:**
`MER_Data::PrintMER()` (2 перегрузки) вызывается из:
- Конструктора MER_Data (строка 177) — `PrintMER()`
- `SomeWell::initialize_MER_data()` (SomeWell.cpp:165) — `PrintMER("inside_well")`

Пишет `test_MER_<name>.txt` и `test_MER_<name>inside_well.txt` в cwd.

**Что сделать:**
1. `MER_Descriptor.h` — обернуть объявления `PrintMER()` в `#ifdef GDM_DUMP_DEBUG`
2. `MER_Descriptor.cpp` — добавить `#include "../Helpers/DebugDump.h"`, обернуть вызов в конструкторе + обе реализации в `#ifdef`, заменить пути
3. `SomeWell.cpp` — обернуть вызов `mer_Data->PrintMER("inside_well")` (строка 165) в `#ifdef GDM_DUMP_DEBUG`. **Обязательно в этом шаге**, иначе при `GDM_DUMP_DEBUG=OFF` объявление `PrintMER` исчезнет, а вызов останется → ошибка компиляции

**Изменения:**

MER_Descriptor.h:
До:
```cpp
			void PrintMER() const;
			void PrintMER(const std::string& str) const;
```
После:
```cpp
#ifdef GDM_DUMP_DEBUG
			void PrintMER() const;
			void PrintMER(const std::string& str) const;
#endif
```

MER_Descriptor.cpp — вызов в конструкторе:
До:
```cpp
			}
		}
		//	initialize_MER(itsData);
		PrintMER();
	}
```
После:
```cpp
			}
		}
#ifdef GDM_DUMP_DEBUG
		PrintMER();
#endif
	}
```

MER_Descriptor.cpp — реализации:
До:
```cpp
		void MER_Data::PrintMER() const
		{
			std::ofstream myfile;
			std::string str{ "test_MER_" + Name() + ".txt" };
			myfile.open(str);
			sendMER2Stream(myfile);
			myfile.close();
		}

		void MER_Data::PrintMER(const std::string& str0) const
		{
			std::ofstream myfile;
			std::string str{ "test_MER_" + Name() + str0 + ".txt" };
			myfile.open(str);
			sendMER2Stream(myfile);
			myfile.close();
		}
```
После:
```cpp
#ifdef GDM_DUMP_DEBUG
		void MER_Data::PrintMER() const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_MER_" + Name() + ".txt"));
			sendMER2Stream(myfile);
			myfile.close();
		}

		void MER_Data::PrintMER(const std::string& str0) const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_MER_" + Name() + str0 + ".txt"));
			sendMER2Stream(myfile);
			myfile.close();
		}
#endif
```

SomeWell.cpp — вызов в `initialize_MER_data()` (строка 165):
До:
```cpp
			reservoir_simulator::MessageFactory::WellOverallTimeFrame(NameWide(), KnownExploitationPeriod());
			mer_Data->PrintMER("inside_well");
			reservoir_simulator::MessageFactory::MERInitializationDone(NameWide());
```
После:
```cpp
			reservoir_simulator::MessageFactory::WellOverallTimeFrame(NameWide(), KnownExploitationPeriod());
#ifdef GDM_DUMP_DEBUG
			mer_Data->PrintMER("inside_well");
#endif
			reservoir_simulator::MessageFactory::MERInitializationDone(NameWide());
```

**Проверка после этого шага:**
- Сборка (без GDM_DUMP_DEBUG): 310 pass, файлы `test_MER_*.txt` не появляются
- Grep: `PrintMER` — вхождения только внутри `#ifdef GDM_DUMP_DEBUG` блоков (включая SomeWell.cpp:165)

**Зависимости:**
- Требует: шаг 1
- Блокирует: —

**Оценка:** ~15 строк изменено в 3 файлах, ~8 минут

---

### Шаг 4: Обернуть PrintWell() и связанные методы в #ifdef

**Цель:** дамп скважинных данных — только при GDM_DUMP_DEBUG

**Файлы:**
- `HydroSolver/Reservoir/Well/SomeWell.cpp`
- `HydroSolver/Reservoir/Well/SomeWell.h`

**Контекст:**
`PrintWell()` (SomeWell.cpp:88–93) создаёт `WellTestData/<name>/` и вызывает `PrintWellMERDebit()`.
`PrintWellMERDebit()` (SomeWell.cpp:80–85) пишет `well_MER_debit.txt`.
`OutputPath()` (SomeWell.cpp:95–97) возвращает `"WellTestData//" + Name()`.
Вызовы: `PrintWell()` из конструктора (строка 153), `PrintMER("inside_well")` из `initialize_MER_data()` (строка 165).

После изменения: `OutputPath()` будет возвращать `DebugDump::dir("WellTestData") + Name()` — файлы попадут в `results/debug/<exe>/WellTestData/<name>/`.

**Что сделать:**
1. `SomeWell.h` — обернуть объявления `PrintWell()`, `OutputPath()`, `PrintWellMERDebit()` в `#ifdef GDM_DUMP_DEBUG`
2. `SomeWell.cpp` — добавить `#include "../../Helpers/DebugDump.h"`, обернуть вызов `PrintWell()` в конструкторе (строка 153) и реализации (строки 80–97) в `#ifdef`, заменить путь в `OutputPath()`
   (вызов `PrintMER("inside_well")` уже обёрнут на шаге 3)

**Изменения:**

SomeWell.h:
До:
```cpp
			void PrintWell() const;
			std::string OutputPath() const;

			void PrintWellMERDebit() const;
```
После:
```cpp
#ifdef GDM_DUMP_DEBUG
			void PrintWell() const;
			std::string OutputPath() const;
			void PrintWellMERDebit() const;
#endif
```

SomeWell.cpp — вызов в конструкторе:
До:
```cpp
			reservoir_simulator::MessageFactory::WellInitializationDone(NameWide());

			PrintWell();
		}
```
После:
```cpp
			reservoir_simulator::MessageFactory::WellInitializationDone(NameWide());

#ifdef GDM_DUMP_DEBUG
			PrintWell();
#endif
		}
```

SomeWell.cpp — реализации:
До:
```cpp
		void SomeWell::PrintWellMERDebit() const
		{
			std::ofstream myFile{ OutputPath() + "//well_MER_debit.txt"};
			PrintWellDebitLength(myFile);
			myFile.close();
		}


		void SomeWell::PrintWell() const
		{
			namespace fs = std::filesystem;
			fs::create_directories(OutputPath());
			PrintWellMERDebit();
		}

		std::string SomeWell::OutputPath() const
		{
			return "WellTestData//" + Name();
		}
```
После:
```cpp
#ifdef GDM_DUMP_DEBUG
		void SomeWell::PrintWellMERDebit() const
		{
			std::ofstream myFile{ OutputPath() + "/well_MER_debit.txt"};
			PrintWellDebitLength(myFile);
			myFile.close();
		}

		void SomeWell::PrintWell() const
		{
			namespace fs = std::filesystem;
			fs::create_directories(OutputPath());
			PrintWellMERDebit();
		}

		std::string SomeWell::OutputPath() const
		{
			return debug_dump::DebugDump::dir("WellTestData") + Name();
		}
#endif
```

**Проверка после этого шага:**
- Сборка (без GDM_DUMP_DEBUG): 310 pass, `WellTestData/` не создаётся
- Сборка (с GDM_DUMP_DEBUG): `WellTestData/` внутри `results/debug/<exe>/`

**Подводные камни:**
- Путь `"//"` в `OutputPath()` → `"/"` — исправить заодно (Windows принимает оба, но `//` нестандартно)
- `SomeWell::OutputPath()` и `ReservoirSimulator::OutputPath()` — разные функции, не путать. ReservoirSimulator — свободная функция в namespace, не трогаем

**Зависимости:**
- Требует: шаг 1
- Блокирует: —

**Оценка:** ~15 строк изменено, ~10 минут

---

### Шаг 5: Обернуть мёртвые Print-методы в #ifdef

**Цель:** сохранить Print-методы LinearProblem, MatrixCSR, SparsityPattern за `#ifdef` для ручной отладки

**Файлы:**
- `HydroSolver/Solver/Math/LinearProblem.cpp`
- `HydroSolver/Solver/Math/LinearProblem.h`
- `HydroSolver/Solver/Math/MatrixCSR.cpp`
- `HydroSolver/Solver/Math/MatrixCSR.h`
- `HydroSolver/Solver/Math/SparsityPattern.cpp`
- `HydroSolver/Solver/Math/SparsityPattern.h`

**Контекст:**
Эти Print-методы не вызываются нигде — ни из production, ни из тестов. Но пользователь хочет сохранить их для ручной отладки (можно вызвать из debugger watch или добавить временный вызов). Оборачиваем в `#ifdef GDM_DUMP_DEBUG`, заменяем пути на `DebugDump::path()`.

**Что сделать:**

1. `LinearProblem.cpp` — добавить `#include "../../Helpers/DebugDump.h"`, обернуть `PrintRHS()`, `Print()`, `PrintCorrections()` в `#ifdef`, заменить пути
2. `LinearProblem.h` — обернуть объявления в `#ifdef`
3. `MatrixCSR.cpp` — обернуть `PrintCRS()`, `PrintDiagBlocks()` в `#ifdef`, заменить пути. `#include <fstream>` — переместить внутрь `#ifdef` (нужен только для Print-методов)
4. `MatrixCSR.h` — обернуть объявления в `#ifdef`
5. `SparsityPattern.cpp` — обернуть `printPattern()`, `printDiagonalBlocks()`, `printOffDiagBlocks()` в `#ifdef`, заменить пути. `#include <fstream>` — переместить внутрь `#ifdef`
6. `SparsityPattern.h` — обернуть объявления в `#ifdef`

**Изменения (ключевые, остальные аналогичны):**

LinearProblem.h:
До:
```cpp
			void Print() const;
			void PrintRHS() const;

			void PrintCorrections() const;
```
После:
```cpp
#ifdef GDM_DUMP_DEBUG
			void Print() const;
			void PrintRHS() const;
			void PrintCorrections() const;
#endif
```

LinearProblem.cpp (все 3 метода одним блоком):
```cpp
#ifdef GDM_DUMP_DEBUG
		void LinearProblem::PrintRHS() const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_RHS.txt"));
			// ... без изменений ...
		}

		void LinearProblem::Print() const
		{
			Matrix().PrintCRS();
			Matrix().PrintDiagBlocks();
			PrintRHS();
			PrintCorrections();
		}

		void LinearProblem::PrintCorrections() const
		{
			std::ofstream myfile;
			myfile.open(debug_dump::DebugDump::path("test_Corrections.txt"));
			// ... без изменений ...
		}
#endif
```

MatrixCSR.cpp:
До:
```cpp
#include <fstream>
#include "MatrixCSR.h"
```
После:
```cpp
#include "MatrixCSR.h"
#ifdef GDM_DUMP_DEBUG
#include <fstream>
#include "../../Helpers/DebugDump.h"
#endif
```

SparsityPattern.cpp — аналогично: `#include <fstream>` внутрь `#ifdef`.

Пути во всех Print-методах: `"test_Matrix.txt"` → `debug_dump::DebugDump::path("test_Matrix.txt")` и т.д.

**Проверка после этого шага:**
- Сборка (без GDM_DUMP_DEBUG): 310 pass
- Grep: `PrintRHS|PrintCRS|printPattern\b` — все внутри `#ifdef GDM_DUMP_DEBUG`

**Подводные камни:**
- `LinearProblem::Print()` вызывает `Matrix().PrintCRS()` — оба за одним `#ifdef`, корректно
- `#include <fstream>` перемещается внутрь `#ifdef` в MatrixCSR.cpp и SparsityPattern.cpp — эти файлы не подключают `stdafx.h`, `<fstream>` нужен только для Print-методов

**Зависимости:**
- Требует: шаг 1
- Блокирует: —

**Оценка:** ~30 строк обёрнуто в 6 файлах, ~10 минут

---

### Шаг 6: Направить BL CSV в results/validation/

**Цель:** `bl_validation_profile.csv` — артефакт валидации, не отладочный дамп. Не зависит от `GDM_DUMP_DEBUG`, но не должен засорять cwd

**Файлы:**
- `tests/test_buckley_leverett.cpp`

**Контекст:**
Тест Buckley–Leverett (VAL-001) безусловно пишет `bl_validation_profile.csv` с профилем насыщенности. Файл используется для визуальной верификации. `results/` уже в `.gitignore`.

**Что сделать:**
1. Добавить `#include <filesystem>`
2. Перед записью: `std::filesystem::create_directories("results/validation")`
3. Путь: `"results/validation/bl_validation_profile.csv"`
4. Обновить INFO-сообщение

**Изменения:**

test_buckley_leverett.cpp:
До:
```cpp
#include <fstream>
```
После:
```cpp
#include <fstream>
#include <filesystem>
```

До:
```cpp
    std::ofstream csv("bl_validation_profile.csv");
    ...
    INFO("CSV written to bl_validation_profile.csv, L2 = " << L2);
```
После:
```cpp
    std::filesystem::create_directories("results/validation");
    std::ofstream csv("results/validation/bl_validation_profile.csv");
    ...
    INFO("CSV written to results/validation/bl_validation_profile.csv, L2 = " << L2);
```

**Проверка после этого шага:**
- Сборка + тесты: 310 pass
- `bl_validation_profile.csv` не в cwd, а в `results/validation/`

**Зависимости:**
- Требует: —
- Блокирует: шаг 7

**Оценка:** ~5 строк, ~3 минуты

---

### Шаг 7: Очистить .gitignore

**Цель:** убрать правила для файлов, которые больше не пишутся в cwd

**Файлы:**
- `.gitignore`

**Контекст:**
`.gitignore` содержит две группы правил для отладочных дампов (строки 12–21 и 100–135). После шагов 2–6 эти файлы при `GDM_DUMP_DEBUG=OFF` не генерируются, а при `ON` пишутся в `results/debug/` (уже покрыто правилом `results/` на строке 45). `bl_validation_profile.csv` теперь в `results/validation/`.

**Что сделать:**
1. Удалить существующие дамп-файлы с диска (они больше не нужны — при `OFF` не генерируются, при `ON` пишутся в `results/debug/`):
   ```powershell
   Remove-Item -Force test_connections.txt, test_MER_*.txt, bl_validation_profile.csv -ErrorAction SilentlyContinue
   Remove-Item -Recurse -Force WellTestData -ErrorAction SilentlyContinue
   ```
2. Удалить строки 12–21 (первая группа): `test_connections.txt`, `test_diagValues.txt`, `test_Matrix.txt`, `test_MER_*.txt`, `test_SparsityPattern*.txt`, `WellTestData/INJ/well_MER_debit.txt`, `bl_validation_profile.csv`
3. Удалить строки 100–135 (вторая группа): повторные `test_*.txt`, `WellTestData/*/well_MER_debit.txt`, `HydroSolver/test_*.txt`
4. Вместо конкретных правил добавить wildcard-ы (на случай запуска старых бинарников или сборки с `ON` вне `results/`):
   ```
   # Debug dump artifacts (legacy, normally in results/)
   test_connections.txt
   test_*.txt
   WellTestData/
   ```

**Проверка после этого шага:**
- `git diff .gitignore` — удалены конкретные правила, добавлены wildcard-ы
- `git status` — нет новых untracked файлов (дампы удалены с диска)
- Сборка + тесты: 310 pass

**Зависимости:**
- Требует: шаги 2–6
- Блокирует: —

**Оценка:** ~40 строк удалено, ~3 минуты

---

## Тестовая стратегия

### Основной инвариант: все 310 тестов зелёные при GDM_DUMP_DEBUG=OFF (default)

Тесты, покрывающие затронутые модули:
- `test_MER_Data.cpp` — тесты MER_Data
- `test_WellJobs.cpp` — тесты скважин
- `test_SparsityPattern.cpp` — тесты SparsityPattern
- `test_MatrixCSR.cpp` — тесты MatrixCSR
- `test_LinearProblemAssembly.cpp` — тесты LinearProblem
- `test_buckley_leverett.cpp` — BL валидация (CSV-путь меняется)
- Все интеграционные тесты (используют конструкторы сетки и скважин)

### Дополнительная проверка: сборка и тесты при GDM_DUMP_DEBUG=ON

На финальной верификации:
1. `cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_DUMP_DEBUG=ON`
2. `cmake --build build --config Release`
3. `ctest --test-dir build -C Release --output-on-failure`
4. Проверить: файлы появляются в `results/debug/<exe_name>/`
5. Разные target-ы пишут в разные подпапки

Новые unit-тесты не нужны — задача инфраструктурная, поведение проверяется наличием/отсутствием файлов.

## Критерии завершения

- [ ] Все шаги выполнены (7/7)
- [ ] Все 310 тестов зелёные (Release + Debug, GDM_DUMP_DEBUG=OFF)
- [ ] Все 310 тестов зелёные (Release, GDM_DUMP_DEBUG=ON)
- [ ] При OFF: никакие `test_*.txt`, `WellTestData/` не появляются в cwd
- [ ] При ON: файлы в `results/debug/<exe_name>/`, разные target-ы изолированы
- [ ] `bl_validation_profile.csv` пишется в `results/validation/` (всегда, не зависит от флага)
- [ ] `.gitignore` очищен от правил для удалённых файлов
- [ ] Ноль warnings при сборке (Release + Debug)
- [ ] Vault обновлён: статус DEBT-009, приоритеты
- [ ] GitHub issue #30 прокомментирован
