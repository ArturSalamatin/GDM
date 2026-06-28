---
tags:
  - план
  - баг
date: 2026-06-28
issue: BUG-012
github: 2
branch: fix/bug-012/binary-grid-dim-type-mismatch
status: готов к реализации
---

# План: BUG-012 — Несовпадение типов при бинарной записи/чтении `grid_dim`

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - `vault/GDM/knowledge/debugging/code-review-2026-06-28-баги-и-корректность.md` (секция CR-BUG-005)
3. Создай ветку: `git checkout -b fix/bug-012/binary-grid-dim-type-mismatch`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

## Контекст

### Что делает текущий код

`ReservoirSimulator` имеет три функции бинарного I/O для полей потоков:

1. **`SaveFlowField2File_bin`** (строка 654) — записывает заголовок (time frame + grid dimensions) и данные `flowFields` в `.bin` файл
2. **`SaveSaturationPressure_bin`** (строка 688) — аналогичный формат, для saturation/pressure данных
3. **`LoadFlowFieldFromFile_bin`** (строка 729) — читает заголовок и восстанавливает `flowFields`

### Бинарный формат (текущий, с ошибкой)

Записывается:
```
[double saturation_date][double startDate][double endDate][int frameCount]
[size_t Nx][size_t Ny][size_t Nz]   ← 3 × 8 = 24 байта на x64
[SomeFlowField[0].write()][SomeFlowField[1].write()]...
```

Читается:
```
[double saturation_date][double startDate][double endDate][int frameCount]
[int Nx][int Ny][int Nz]             ← 3 × 4 = 12 байт
[manual read vx/vy fields...]
```

### Почему это баг

1. **Несовпадение типов:** запись через `vector<size_t>` (8 байт на элемент на x64), чтение через `array<int, 3>` (4 байта на элемент). При чтении 12 байт вместо 24 — `grid_dim[0]` получает младшие 4 байта Nx (правильно если Nx < 2³¹), `grid_dim[1]` получает старшие 4 байта Nx (0 для малых сеток), `grid_dim[2]` получает младшие 4 байта Ny.
2. **Смещение потока:** после чтения grid_dim позиция в файле сдвинута на 12 байт от ожидаемой (недочитано 12 байт). Все последующие read-операции читают не те данные.
3. **Хардкод `nz = 4`:** строка 747 — `nz = 4;// grid_dim[2]` — даже если grid_dim прочитался бы корректно, nz игнорируется.
4. **Дублирование кода:** `SaveFlowField2File_bin` и `SaveSaturationPressure_bin` содержат идентичную логику записи заголовка (copy-paste). Функция чтения использует manual read вместо `SomeFlowField::write`-парного read, что и создало расхождение.

### Связанные типы

- `Grid.Nx()`, `Grid.Ny()`, `Grid.Nz()` возвращают `size_t` (определено в `AbstractGrid.h:178`, `GridDescriptors.h:12`)
- `flowFields` — `vector<SomeFlowField>` (определено в `ReservoirSImulator.h:69`)
- `SomeFlowField::write(ofstream&)` записывает `int n = field.size()` + каждый `FlowFieldSnapshot::write()` (time + vxField + vyField через `MatrixField::write`)

## Цепочка причинно-следственных связей

```
Grid.Nx() возвращает size_t (8 байт)
    ↓
SaveFlowField2File_bin записывает vector<size_t>{Nx,Ny,Nz} — 24 байта
    ↓
LoadFlowFieldFromFile_bin читает array<int,3> — 12 байт
    ↓
grid_dim содержит мусор: [Nx_low32, Nx_high32, Ny_low32]
    ↓
nx = Nx (случайно верно для малых Nx), ny = 0, nz = 4 (хардкод)
    ↓
Поток в файле сдвинут на 12 байт — read(nt) читает часть Nz
    ↓
Все данные flowFields повреждены
```

## Целевое состояние

1. Round-trip: `SaveFlowField2File_bin` → `LoadFlowFieldFromFile_bin` корректно восстанавливает `flowFields`
2. Grid dimensions читаются верно для любых Nx, Ny, Nz
3. nz берётся из файла, не захардкожен
4. Формат фиксирован: `uint32_t` для grid dimensions — не зависит от платформы
5. Unit-тесты: round-trip для различных размерностей сетки

## Варианты решения

### Вариант A: Исправить только чтение — `size_t` вместо `int`

- Заменить `array<int, 3>` на `array<size_t, 3>` в Load
- Убрать хардкод nz
- **Плюсы:** 3 строки, совместим с существующими файлами
- **Минусы:** `size_t` платформо-зависим (4 vs 8 байт). Бинарные файлы нечитаемы при смене x86 ↔ x64. В Save/Load всё ещё дублирование и расхождение структуры
- **Трудоёмкость:** ~5 строк, 1 файл

### Вариант B: Фиксированный тип `uint32_t` для формата + парный read к write

- Запись и чтение через `array<uint32_t, 3>` (или `int32_t`, поскольку отрицательные размерности невозможны, но `int32_t` ближе к существующему `int frameCount`)
- Чтение использует `SomeFlowField`-совместимую структуру (добавить `SomeFlowField::read`)
- **Плюсы:** портируемый формат, симметрия write/read, нет дублирования
- **Минусы:** ломает совместимость с файлами, записанными в текущем формате (size_t). Требует добавления `FlowFieldSnapshot::read` и `SomeFlowField::read`
- **Трудоёмкость:** ~80 строк, 4 файла

### Вариант C (комбинированный): сначала A, потом B в отдельной задаче

- Этап 1: quick fix (вариант A) — работающий round-trip
- Этап 2: DEBT — переход на фиксированный формат с версионированием

### Выбранный вариант: A (quick fix)

**Обоснование:**
1. Проект компилируется и работает только на x64 Windows (MSVC). Кросс-платформенность бинарного формата сейчас не требуется.
2. Существующие `.bin` файлы записаны с `size_t` — вариант A не ломает их.
3. Вариант B — это рефакторинг бинарного I/O, который заслуживает отдельного DEBT с продуманным версионированием формата. Смешивать его с фиксом — overhead.
4. Хардкод `nz = 4` убирается в обоих вариантах.

## Подводные камни (чеклист)

- ✅ **Побочные эффекты:** `LoadFlowFieldFromFile_bin` вызывается только из `ReservoirSimulator` (grep по всем .cpp). Изменение типа grid_dim не затрагивает API.
- ✅ **Потокобезопасность:** функции Save/Load не вызываются из OMP-параллельных секций (I/O — однопоточный).
- ⚠️ **Граничные случаи:** если Nz в файле не совпадает с `flowFields.size()`, цикл `for (k < nz)` может выйти за пределы вектора. Адресовано в шаге 2 (проверка bounds).
- ✅ **Производительность:** I/O-код, не в горячем цикле.
- ✅ **Обратная совместимость:** формат записи не меняется (size_t остаётся). Чтение приводится в соответствие.
- ✅ **Порядок вызовов:** Save/Load — независимые операции, порядок инициализации не критичен.
- ✅ **Состояние при ошибке:** если чтение не удалось, flowFields.clear() уже вызван — объект в пустом, но валидном состоянии.
- ✅ **Численная устойчивость:** не применимо (целочисленные размерности).
- ✅ **Связь с другими задачами:** нет пересечений с текущими BUG/DEBT.
- ✅ **Зависимости сборки:** изменения только в .cpp, include `<cstdint>` уже доступен через chain, `<array>` уже включён.

## Обнаруженные проблемы

1. **Дублирование логики записи заголовка** в `SaveFlowField2File_bin` и `SaveSaturationPressure_bin` (строки 672-685 и 706-719 — идентичный код). Это источник расхождений: если один раз поменять формат в Save, легко забыть второй. → Регистрировать как DEBT после завершения плана.
2. **Отсутствие `SomeFlowField::read`** — запись через `write()`, чтение — ручной inline-код в `LoadFlowFieldFromFile_bin`. Асимметрия → регистрировать как DEBT.
3. **`LoadFlowFieldFromFile` (не _bin)** — строка 723-726, пустое тело с TODO. Мёртвый код → уже учтён или регистрировать как DEBT.

---

## Шаги реализации

### Шаг 1: Создать тест-воспроизводитель бага

**Цель:** иметь тест, который красный без фикса и зелёный после.

**Файлы:**
- `tests/unit/reservoir/test_BinaryIO.cpp` (новый)
- `CMakeLists.txt` (добавить в gdm_unit_level2 или создать level — по месту)

**Контекст:**
В проекте нет тестов на бинарный I/O. `ReservoirSimulator::SaveFlowField2File_bin` записывает grid dimensions как `vector<size_t>`, а `LoadFlowFieldFromFile_bin` читает как `array<int, 3>`. Тест должен создать ReservoirSimulator, заполнить flowFields, сохранить в файл, загрузить и проверить.

Однако `ReservoirSimulator` — тяжёлый объект. Для unit-теста нам нужна возможность конструировать его минимально. Проверим, как это делается в существующих интеграционных тестах.

**Что сделать:**

1. Создать файл `tests/unit/reservoir/test_BinaryIO.cpp`
2. Подключить Catch2 и нужные headers:
   ```cpp
   #include <catch2/catch_test_macros.hpp>
   #include <catch2/matchers/catch_matchers_floating_point.hpp>
   #include <filesystem>
   #include <fstream>
   #include <array>
   #include <cstdint>
   ```
3. Написать низкоуровневый тест, который:
   - Записывает заголовок в том же формате, что `SaveFlowField2File_bin` (3 double + 1 int + 3 size_t)
   - Читает его как `LoadFlowFieldFromFile_bin` (3 double + 1 int + 3 int)
   - Проверяет, что значения НЕ совпадают (воспроизведение бага)
   - Тег: `[binary-io][bug-012]`

```cpp
TEST_CASE("BUG-012: size_t write vs int read produces wrong grid_dim", "[binary-io][bug-012]") {
    namespace fs = std::filesystem;
    auto tmpFile = fs::temp_directory_path() / "bug012_test.bin";

    // Write header in the format of SaveFlowField2File_bin
    {
        std::ofstream out(tmpFile, std::ios::binary);
        double saturation_date = 0.0, startDate = 0.0, endDate = 100.0;
        int frameCount = 1;
        out.write(reinterpret_cast<const char*>(&saturation_date), sizeof(double));
        out.write(reinterpret_cast<const char*>(&startDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&endDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&frameCount), sizeof(int));

        std::vector<size_t> grid_dim{ 51, 51, 4 };
        out.write(reinterpret_cast<const char*>(grid_dim.data()), grid_dim.size() * sizeof(size_t));
    }

    // Read header in the format of LoadFlowFieldFromFile_bin (buggy)
    {
        std::ifstream in(tmpFile, std::ios::binary);
        double saturation_date, startDate, endDate;
        int frameCount;
        in.read(reinterpret_cast<char*>(&saturation_date), sizeof(double));
        in.read(reinterpret_cast<char*>(&startDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&endDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

        std::array<int, 3> grid_dim_int;
        in.read(reinterpret_cast<char*>(&grid_dim_int), 3 * sizeof(int));

        // BUG: grid_dim_int[1] will be 0 (high 32 bits of size_t 51)
        // This test documents the bug — it PASSES with the bug present
        CHECK(grid_dim_int[0] == 51);   // low 32 bits of Nx — coincidentally correct
        CHECK(grid_dim_int[1] == 0);    // high 32 bits of Nx — NOT Ny!
        CHECK(grid_dim_int[2] == 51);   // low 32 bits of Ny — NOT Nz!
    }

    fs::remove(tmpFile);
}
```

4. Написать тест «как должно быть» — round-trip с `size_t`:

```cpp
TEST_CASE("BUG-012 fix: size_t round-trip preserves grid_dim", "[binary-io][bug-012]") {
    namespace fs = std::filesystem;
    auto tmpFile = fs::temp_directory_path() / "bug012_fix_test.bin";

    const size_t expectedNx = 51, expectedNy = 51, expectedNz = 4;

    {
        std::ofstream out(tmpFile, std::ios::binary);
        double saturation_date = 0.0, startDate = 0.0, endDate = 100.0;
        int frameCount = 1;
        out.write(reinterpret_cast<const char*>(&saturation_date), sizeof(double));
        out.write(reinterpret_cast<const char*>(&startDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&endDate), sizeof(double));
        out.write(reinterpret_cast<const char*>(&frameCount), sizeof(int));

        std::vector<size_t> grid_dim{ expectedNx, expectedNy, expectedNz };
        out.write(reinterpret_cast<const char*>(grid_dim.data()), grid_dim.size() * sizeof(size_t));
    }

    {
        std::ifstream in(tmpFile, std::ios::binary);
        double saturation_date, startDate, endDate;
        int frameCount;
        in.read(reinterpret_cast<char*>(&saturation_date), sizeof(double));
        in.read(reinterpret_cast<char*>(&startDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&endDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

        // Fixed: read as size_t, matching the write format
        std::array<size_t, 3> grid_dim;
        in.read(reinterpret_cast<char*>(grid_dim.data()), 3 * sizeof(size_t));

        REQUIRE(grid_dim[0] == expectedNx);
        REQUIRE(grid_dim[1] == expectedNy);
        REQUIRE(grid_dim[2] == expectedNz);
    }

    fs::remove(tmpFile);
}
```

5. В `CMakeLists.txt`: добавить `tests/unit/reservoir/test_BinaryIO.cpp` в `gdm_unit_level2` (строка 176, перед закрывающей скобкой):

До:
```cmake
add_executable(gdm_unit_level2
    tests/unit/math/test_CRSStructure.cpp
    tests/unit/math/test_SparsityPattern.cpp
    tests/unit/physics/test_AbstractCells.cpp
    tests/unit/wells/test_AccumulatedPerforations.cpp
    tests/unit/descriptors/test_MER_Data.cpp
    tests/unit/reservoir/test_NumericalParameters.cpp
)
```

После:
```cmake
add_executable(gdm_unit_level2
    tests/unit/math/test_CRSStructure.cpp
    tests/unit/math/test_SparsityPattern.cpp
    tests/unit/physics/test_AbstractCells.cpp
    tests/unit/wells/test_AccumulatedPerforations.cpp
    tests/unit/descriptors/test_MER_Data.cpp
    tests/unit/reservoir/test_NumericalParameters.cpp
    tests/unit/reservoir/test_BinaryIO.cpp
)
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "binary-io"`
- Ожидаемый результат: тест «size_t write vs int read» — зелёный (документирует баг). Тест «size_t round-trip» — зелёный (читает корректно).
- Регрессия: `ctest --test-dir build -C Release` — все тесты зелёные

**Подводные камни:**
- Тест работает напрямую с файловым I/O, не через ReservoirSimulator — это намеренно: изолируем баг от тяжёлого объекта
- `fs::temp_directory_path()` — кросс-платформенный путь, работает на Windows

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~60 строк нового кода, ~15 минут

---

### Шаг 2: Исправить `LoadFlowFieldFromFile_bin` — тип `grid_dim` и убрать хардкод `nz`

**Цель:** привести чтение grid_dim в соответствие с записью (size_t) и использовать nz из файла вместо хардкода.

**Файлы:**
- `HydroSolver/Reservoir/ReservoirSimulator.cpp` (строки 745-749)

**Контекст:**
Функция `LoadFlowFieldFromFile_bin` читает заголовок бинарного файла. Grid dimensions записаны как `vector<size_t>` (3 × 8 байт на x64), но читаются как `array<int, 3>` (3 × 4 байта). После фикса тип чтения должен совпадать с типом записи — `size_t`.

Хардкод `nz = 4` (строка 747) заменяется на значение из файла. Однако нужна защита: если `nz` из файла больше `flowFields.size()`, цикл на строке 749 выйдет за пределы вектора.

**Что сделать:**

1. В файле `HydroSolver/Reservoir/ReservoirSimulator.cpp`, функция `LoadFlowFieldFromFile_bin`, строки 744-749:

До:
```cpp
		// grid dimensions
		std::array<int, 3> grid_dim;
		wstream.read(reinterpret_cast<char*>(&grid_dim), 3 * sizeof(int));
		int nx = grid_dim[0], ny = grid_dim[1], nz = 4;// grid_dim[2];

		for (int k = 0; k < nz; k++)
```

После:
```cpp
		// grid dimensions
		std::array<size_t, 3> grid_dim;
		wstream.read(reinterpret_cast<char*>(grid_dim.data()), 3 * sizeof(size_t));
		size_t nx = grid_dim[0], ny = grid_dim[1], nz = grid_dim[2];

		for (size_t k = 0; k < nz && k < flowFields.size(); k++)
```

2. В цикле ниже (строки 752-753) `int nt` — оставить как есть, формат записи `SomeFlowField::write` использует `int n`:
```cpp
			int nt;
			wstream.read(reinterpret_cast<char*>(&nt), sizeof(int));
```

3. Внутренние циклы (строки 762-777) используют `nx` и `ny` для выделения памяти и чтения. Типы: `vxField.emplace_back(vector<double>(nx + 1, 0.0))` — `nx` как `size_t` корректно конвертируется в `size_t` аргумент конструктора vector. Аналогично для `ny`.

4. Строка 765: `wstream.read(... sizeof(double) * (nx + 1))` — `nx` теперь `size_t`, умножение корректно (размер в байтах).

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "binary-io"`
- Ожидаемый результат: оба теста зелёные
- Регрессия: `ctest --test-dir build -C Release` — все тесты зелёные, без новых warnings

**Подводные камни:**
- ⚠️ `nz` из файла может быть больше `flowFields.size()` — защита `k < flowFields.size()` предотвращает выход за пределы вектора. Это degraded-mode: часть данных не будет загружена, но программа не упадёт.
- ✅ Тип `size_t` для `nx`, `ny`, `nz` вместо `int` — все использования ниже совместимы (вектор-конструкторы принимают `size_t`, sizeof-умножение корректно).
- ✅ Warning: `for (size_t k = 0; k < nz && k < flowFields.size(); k++)` — оба операнда `size_t`, нет signed/unsigned mismatch.

**Зависимости:**
- Требует: шаг 1 (тесты)
- Блокирует: шаг 3

**Оценка:** ~5 строк изменений, ~10 минут

---

### Шаг 3: Расширить тесты — граничные случаи и regression

**Цель:** убедиться, что фикс работает для различных размерностей сетки и не ломает существующие сценарии.

**Файлы:**
- `tests/unit/reservoir/test_BinaryIO.cpp` (расширить)

**Контекст:**
После шага 2 основной баг исправлен. Нужны дополнительные тесты:
- Различные размерности сетки (1×1×1, 100×200×4, 1×1×1 — минимальный)
- Проверка, что stream position после чтения grid_dim корректна (следующий read получает правильные данные)
- Проверка, что `endDate` и `frameCount` читаются корректно (не смещены)

**Что сделать:**

1. Добавить параметризованный тест round-trip для разных размерностей:

```cpp
TEST_CASE("Binary header round-trip: various grid sizes", "[binary-io]") {
    namespace fs = std::filesystem;
    auto tmpFile = fs::temp_directory_path() / "binary_header_roundtrip.bin";

    auto [nx, ny, nz] = GENERATE(
        std::tuple{size_t(1), size_t(1), size_t(1)},
        std::tuple{size_t(51), size_t(51), size_t(4)},
        std::tuple{size_t(100), size_t(200), size_t(8)},
        std::tuple{size_t(1000), size_t(1000), size_t(1)}
    );

    const double expectedEnd = 365.25;
    const int expectedFrames = 42;

    {
        std::ofstream out(tmpFile, std::ios::binary);
        double sd = 0.0, st = 0.0;
        out.write(reinterpret_cast<const char*>(&sd), sizeof(double));
        out.write(reinterpret_cast<const char*>(&st), sizeof(double));
        out.write(reinterpret_cast<const char*>(&expectedEnd), sizeof(double));
        out.write(reinterpret_cast<const char*>(&expectedFrames), sizeof(int));
        std::array<size_t, 3> dims{nx, ny, nz};
        out.write(reinterpret_cast<const char*>(dims.data()), 3 * sizeof(size_t));
        // Write sentinel to verify stream position
        int sentinel = 0xDEAD;
        out.write(reinterpret_cast<const char*>(&sentinel), sizeof(int));
    }

    {
        std::ifstream in(tmpFile, std::ios::binary);
        double sd, st, endDate;
        int frameCount;
        in.read(reinterpret_cast<char*>(&sd), sizeof(double));
        in.read(reinterpret_cast<char*>(&st), sizeof(double));
        in.read(reinterpret_cast<char*>(&endDate), sizeof(double));
        in.read(reinterpret_cast<char*>(&frameCount), sizeof(int));

        std::array<size_t, 3> grid_dim;
        in.read(reinterpret_cast<char*>(grid_dim.data()), 3 * sizeof(size_t));

        REQUIRE(grid_dim[0] == nx);
        REQUIRE(grid_dim[1] == ny);
        REQUIRE(grid_dim[2] == nz);
        REQUIRE(endDate == expectedEnd);
        REQUIRE(frameCount == expectedFrames);

        // Verify stream position: sentinel must be readable
        int sentinel;
        in.read(reinterpret_cast<char*>(&sentinel), sizeof(int));
        REQUIRE(sentinel == 0xDEAD);
    }

    fs::remove(tmpFile);
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "binary-io"`
- Ожидаемый результат: все тесты зелёные (4 параметрических варианта + 2 из шага 1)
- Регрессия: `ctest --test-dir build -C Release`

**Подводные камни:**
- ✅ `GENERATE` — Catch2 macro для параметрических тестов, доступен в Catch2 v3

**Зависимости:**
- Требует: шаг 2
- Блокирует: шаг 4

**Оценка:** ~50 строк нового кода, ~15 минут

---

### Шаг 4: Обновить vault и GitHub issue

**Цель:** зафиксировать результат в vault и на GitHub.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md` (обновить статус BUG-012)
- `vault/GDM/knowledge/debugging/code-review-2026-06-28-баги-и-корректность.md` (добавить ссылку на issue)

**Что сделать:**

1. В записи BUG-012 (`vault/GDM/roadmap/известные баги и технический долг.md`):
   - Изменить `**Статус:** 🔴 ОТКРЫТ` → `**Статус:** ✅ ИСПРАВЛЕН`
   - Добавить `**Ветка:** fix/bug-012/binary-grid-dim-type-mismatch`

2. Прокомментировать GitHub issue #2:
   ```
   gh issue comment 2 --repo ArturSalamatin/GDM --body "Исправлено: grid_dim читается как size_t, хардкод nz=4 убран. Добавлены unit-тесты round-trip."
   ```

3. Зарегистрировать обнаруженные проблемы (из раздела «Обнаруженные проблемы»):
   - DEBT: дублирование логики записи заголовка в Save-функциях
   - DEBT: отсутствие SomeFlowField::read (асимметрия write/read)
   - DEBT: мёртвый код LoadFlowFieldFromFile (не _bin)

   Проверить, нет ли уже таких записей в реестре. Если нет — добавить со следующими свободными ID.

**Проверка после этого шага:**
- vault обновлён
- GitHub issue прокомментирован
- Новые DEBT зарегистрированы

**Зависимости:**
- Требует: шаг 3 (все тесты зелёные)
- Блокирует: ничего

**Оценка:** ~10 минут

---

## Тестовая стратегия

**Тест 1:** `BUG-012: size_t write vs int read produces wrong grid_dim`
- **Тег:** `[binary-io][bug-012]`
- **Файл:** `tests/unit/reservoir/test_BinaryIO.cpp` (новый)
- **Сценарий:** документирует баг — пишем size_t, читаем int, проверяем что значения расходятся
- **Setup:** запись 3×size_t{51,51,4}, чтение 3×int
- **Ожидание:** grid_dim_int = {51, 0, 51} (а не {51, 51, 4})
- **Предотвращает:** регрессию — если кто-то вернёт `int` в чтение, тест упадёт

**Тест 2:** `BUG-012 fix: size_t round-trip preserves grid_dim`
- **Тег:** `[binary-io][bug-012]`
- **Файл:** `tests/unit/reservoir/test_BinaryIO.cpp`
- **Сценарий:** write size_t → read size_t, проверяем round-trip
- **Setup:** запись 3×size_t{51,51,4}, чтение 3×size_t
- **Ожидание:** grid_dim = {51, 51, 4}
- **Предотвращает:** повторное расхождение типов

**Тест 3:** `Binary header round-trip: various grid sizes`
- **Тег:** `[binary-io]`
- **Файл:** `tests/unit/reservoir/test_BinaryIO.cpp`
- **Сценарий:** параметрический round-trip для 4 размерностей
- **Setup:** {1×1×1, 51×51×4, 100×200×8, 1000×1000×1}
- **Ожидание:** все dimensions и sentinel корректны после read
- **Предотвращает:** edge cases и смещение потока

## Критерии завершения

- [ ] Тест-воспроизводитель (Тест 1) зелёный
- [ ] Тест round-trip (Тест 2) зелёный
- [ ] Параметрический тест (Тест 3) зелёный
- [ ] Все существующие тесты зелёные (`ctest --test-dir build -C Release`)
- [ ] Нет новых compiler warnings
- [ ] Запись BUG-012 в реестре — статус обновлён
- [ ] GitHub issue #2 прокомментирован
- [ ] Обнаруженные DEBT зарегистрированы в реестре

## Связанные vault-заметки

- [[code-review-2026-06-28-баги-и-корректность]] — CR-BUG-005 (хардкод nz=4)
- [[известные баги и технический долг]] — BUG-012
- [[стратегия тестирования GDM]] — раздел «Не мокать I/O»
- [[инвентаризация кодовой базы 2026-06-27]] — ReservoirSimulator: нет прямых юнит-тестов
