---
tags:
  - план
  - баг
date: 2026-07-02
issue: BUG-016
github: 13
branch: fix/bug-016/welldatahandler-memory-leak
status: реализован
audit:
  date: 2026-07-02
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# План: BUG-016 — Утечка памяти `new char[]` без `delete[]` в WellDataHandler

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[известные баги]] (BUG-016), [[code-review-2026-06-28-утечки-ресурсов-и-память]]
3. Создай ветку: `git checkout -b fix/bug-016/welldatahandler-memory-leak experimental`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

**Baseline (experimental, 2026-07-02):** 290 тестов, ~54 сек Release.

---

## Описание проблемы

### Что происходит

Метод `WellDataHandler::DataReader::Read()` читает бинарный файл формата `KPFUBOIL    HOLE`. Для каждой колонки каждой строки файла выделяется `char* var = new char[sizes[j]]` как промежуточный буфер для `memcpy_s` → `reinterpret_cast`. После извлечения значения `delete[] var` не вызывается — утечка при каждой итерации внутреннего цикла.

### Цепочка причинно-следственных связей

1. **Триггер:** вызов `PerfData::Push(path)` (WellDataHandler.cpp:112) или `MerData::Push(path)` (WellDataHandler.cpp:253) при загрузке скважинных данных из бинарных файлов
2. **Место ошибки:** `DataReader::Read()` (WellDataHandler.cpp:43) — `char* var = new char[sizes[j]]` без парного `delete[]`
3. **Распространение:** утечка происходит N_строк × M_колонок раз за один вызов `Read()`. Через `PerfData::Push` и `MerData::Push` — при каждой загрузке перфораций или MER-данных
4. **Проявление:** монотонный рост потребления памяти. На малых тестовых файлах — незаметно. На production-данных (тысячи скважин) — существенная утечка

### Затронутый код

```cpp
// WellDataHandler.cpp:40-68 (внутри DataReader::Read)
for (int j = 0; j < line_size; j++)
{
    //std::vector<char> lbuffer(sizes[j]);     // закомментированная попытка фикса
    char* var = new char[sizes[j]];            // УТЕЧКА: нет delete[]
    memcpy_s(var, sizes[j], &buffer[i + shift], sizes[j]);
    ValueContainer vc;
    if (types[j] == 'A') { vc.Value = *reinterpret_cast<int*>(var); }
    if (types[j] == 'F') { vc.Value = *reinterpret_cast<float*>(var); }
    if (types[j] == 'D') { vc.Value = *reinterpret_cast<double*>(var); }
    if (types[j] == 'I') { vc.Value = *reinterpret_cast<int*>(var); }
    (*(out.end() - 1)).push_back(vc);
    shift += sizes[j];
    // ← delete[] var ОТСУТСТВУЕТ
}
```

Типы:
- `ValueContainer` — структура с `std::variant` или union-like полем `Value` (определена в `WellDataHandler.h`)
- `sizes` — `std::vector<int>` (размеры колонок в байтах)
- `types` — `std::string` (типы колонок: 'A'=int, 'F'=float, 'D'=double, 'I'=int)
- `buffer` — `std::vector<char>` — весь файл уже считан в память (строка 9-14)

### Целевое состояние

`DataReader::Read()` не выделяет raw `new` — использует `std::vector<char>` (RAII). Промежуточный буфер автоматически освобождается при выходе из scope. Семантика и результаты чтения не меняются.

---

## Варианты решения

### Вариант A: `delete[] var` после использования

**Суть:** добавить `delete[] var;` перед `shift += sizes[j]`

**Плюсы:** 1 строка, минимальный diff
**Минусы:** raw `new`/`delete` — хрупко. При добавлении `continue`/`throw`/early return в цикл — снова утечка. Закомментированный `vector<char> lbuffer` говорит о том, что автор уже пытался перейти на RAII
**Риски:** нулевые для текущего кода, но хрупко для будущих изменений
**Совместимость:** полная
**Трудоёмкость:** 1 файл, 1 строка

### Вариант B: `std::vector<char>` вместо `new char[]` (рекомендуемый)

**Суть:** заменить `char* var = new char[sizes[j]]` на `std::vector<char> var(sizes[j])`, использовать `var.data()` вместо `var` в `memcpy_s` и `reinterpret_cast`

```cpp
std::vector<char> var(sizes[j]);
memcpy_s(var.data(), sizes[j], &buffer[i + shift], sizes[j]);
ValueContainer vc;
if (types[j] == 'A') { vc.Value = *reinterpret_cast<int*>(var.data()); }
if (types[j] == 'F') { vc.Value = *reinterpret_cast<float*>(var.data()); }
if (types[j] == 'D') { vc.Value = *reinterpret_cast<double*>(var.data()); }
if (types[j] == 'I') { vc.Value = *reinterpret_cast<int*>(var.data()); }
```

**Плюсы:** RAII, безопасно при исключениях и early return. Удаляет закомментированный мёртвый код (строка 42)
**Минусы:** нет
**Риски:** нулевые — `vector<char>::data()` возвращает `char*`, семантика идентична
**Совместимость:** полная
**Трудоёмкость:** 1 файл, ~6 строк

### Выбор: Вариант B

RAII вместо raw `new`/`delete`. Удаляет закомментированный код. Безопасен при будущих изменениях.

---

## Поиск подводных камней

- ✅ **Побочные эффекты:** `DataReader::Read()` вызывается из `PerfData::Push` (строка 99) и `MerData::Push` (строка 234). Возвращаемый тип `vector<vector<ValueContainer>>` не меняется. Безопасно
- ✅ **Потокобезопасность:** `DataReader::Read()` — статический метод, без shared state. Не вызывается из OpenMP. Безопасно
- ✅ **Граничные случаи:** `sizes[j] == 0` — `vector<char>(0)` корректен, `memcpy_s(data(), 0, ...)` — no-op. Безопасно
- ✅ **Производительность:** `DataReader::Read()` вызывается при загрузке скважинных данных (startup), не в горячем цикле. Overhead `vector` vs `new` — несущественен
- ✅ **Обратная совместимость:** семантика чтения не меняется. `vector<char>::data()` возвращает тот же `char*`
- ✅ **Порядок вызовов:** не зависит от порядка инициализации
- ✅ **Состояние при ошибке:** при `throw` (строка 74, 78) — `vector` автоматически освобождается. Улучшение по сравнению с `new`
- ✅ **Численная устойчивость:** не применимо (чтение данных, не вычисления)
- ✅ **Связь с другими задачами:** нет конфликтов. BUG-012 (бинарный I/O grid_dim) — другой файл и другая функция
- ✅ **Зависимости сборки:** `<vector>` уже включён через другие заголовки. Нет CMake-изменений

---

## Обнаруженные проблемы

Нет.

---

## Затронутые файлы

| Файл | Роль |
|---|---|
| `HydroSolver/Utils/WellDataHandler.cpp` | Замена `new char[]` на `vector<char>` в `DataReader::Read()` |

---

## Шаги реализации

### Шаг 1: Заменить `new char[]` на `std::vector<char>` в `DataReader::Read()`

**Цель:** устранить утечку памяти — заменить raw `new char[]` на RAII-контейнер `std::vector<char>`.

**Файлы:** `HydroSolver/Utils/WellDataHandler.cpp`

**Контекст:**
`WellDataHandler::DataReader::Read()` (строки 3-84) читает бинарный файл формата `KPFUBOIL    HOLE`, парсит заголовок (типы и размеры колонок), затем в двойном цикле (по строкам × по колонкам) извлекает значения. Для каждой колонки выделяется промежуточный буфер `char* var = new char[sizes[j]]` (строка 43), в который копируется фрагмент из общего буфера `buffer`. Затем через `reinterpret_cast` извлекается значение нужного типа (int, float, double). После извлечения `delete[] var` не вызывается — утечка.

На строке 42 есть закомментированный `std::vector<char> lbuffer(sizes[j])` — предыдущая попытка фикса, не доведённая до конца.

**Что сделать:**

1. В файле `HydroSolver/Utils/WellDataHandler.cpp`, функция `DataReader::Read()`, строки 42-43:
   удалить закомментированный `lbuffer`, заменить `char* var = new char[sizes[j]]` на `std::vector<char> var(sizes[j])`

2. Строки 44, 49, 53, 57, 61:
   заменить `var` на `var.data()` в `memcpy_s` и `reinterpret_cast`

**Изменения (старый → новый код):**

До:
```cpp
					//std::vector<char> lbuffer(sizes[j]);
					char* var = new char[sizes[j]];
					memcpy_s(var, sizes[j], &buffer[i + shift], sizes[j]);
					ValueContainer vc;
					//преобразуем переменную
					if (types[j] == 'A')
					{
						vc.Value = *reinterpret_cast<int*>(var);
					}
					if (types[j] == 'F')
					{
						vc.Value = *reinterpret_cast<float*>(var);
					}
					if (types[j] == 'D')
					{
						vc.Value = *reinterpret_cast<double*>(var);
					}
					if (types[j] == 'I')
					{
						vc.Value = *reinterpret_cast<int*>(var);
					}
```

После:
```cpp
					std::vector<char> var(sizes[j]);
					memcpy_s(var.data(), sizes[j], &buffer[i + shift], sizes[j]);
					ValueContainer vc;
					//преобразуем переменную
					if (types[j] == 'A')
					{
						vc.Value = *reinterpret_cast<int*>(var.data());
					}
					if (types[j] == 'F')
					{
						vc.Value = *reinterpret_cast<float*>(var.data());
					}
					if (types[j] == 'D')
					{
						vc.Value = *reinterpret_cast<double*>(var.data());
					}
					if (types[j] == 'I')
					{
						vc.Value = *reinterpret_cast<int*>(var.data());
					}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — все 290 тестов зелёные
- Debug: `cmake --build build --config Debug` + тесты — все зелёные

**Подводные камни:** нет (см. чеклист выше)

**Зависимости:**
- Требует: нет
- Блокирует: нет

**Оценка:** ~6 строк изменений (2 новые, 5 модифицированных, 1 удалена), ~3 минуты

---

## Тестовая стратегия

### Тест-воспроизводитель

Утечка памяти не ловится unit-тестом без специальной инфраструктуры (MSVC CRT debug heap, AddressSanitizer). Тест-воспроизводитель не создаётся — фикс верифицируется инспекцией кода (RAII вместо raw `new`) и полным прогоном регрессионных тестов.

### Regression

Все 290 существующих тестов. `DataReader::Read()` не покрыт unit-тестами напрямую (файл помечен «Исключён» в инвентаризации кодовой базы), но вызывается через `PerfData::Push` и `MerData::Push` в integration-тестах, которые загружают скважинные данные.

### Visual

Не применимо.

---

## Критерии завершения

- [ ] `new char[]` заменён на `std::vector<char>` в `DataReader::Read()`
- [ ] Закомментированный `lbuffer` удалён
- [ ] Все существующие тесты зелёные в Release (290)
- [ ] Все тесты зелёные в Debug
- [ ] Ноль новых warnings при сборке (оба конфига)
- [ ] Vault обновлён: запись в [[известные баги]] → статус ✅
- [ ] Заметка в `knowledge/debugging/` создана
- [ ] GitHub issue #13 прокомментирован с результатом
- [ ] `vault/GDM/00-home/index.md` обновлён
