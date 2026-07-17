---
tags:
  - план
  - рефакторинг
date: 2026-07-17
issue: DEBT-028
github: 34
branch: refactor/debt-028/neibidx-size-t
status: готов к реализации
audit:
  date: 2026-07-17
  round: 1
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
  note: план проверен по 7 измерениям + мысленный прогон, проблем не найдено
---

# DEBT-028: `int neibIdx` → `size_t` в `LinearProblem::AddOffDiagBlock`

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-028/neibidx-size-t`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Baseline: 312 тестов, все зелёные
6. Начни с шага 1. После каждого шага: сборка + тесты

---

## Текущее состояние

`LinearProblem::AddOffDiagBlock` принимает `int neibIdx`, но вызывает `MatrixCSR::AddOffDiagBlock`, который принимает `size_t neibIdx`. Цепочка типов:

```
JacobianAssembler.cpp:85   size_t neibCount  (цикл по соседям)
JacobianAssembler.cpp:137  static_cast<int>(neibCount)  ← явный каст size_t → int
LinearProblem.h:79-80      int neibIdx  ← объявление
LinearProblem.cpp:170,175  int neibIdx  ← определение
LinearProblem.cpp:172,177  Matrix().AddOffDiagBlock(l, neibIdx, ...)  ← неявный int → size_t
MatrixCSR.h:52-53          size_t neibIdx  ← принимающая сторона
```

Двойное бессмысленное преобразование: `size_t → int → size_t`.

### Downstream-использования `LinearProblem::AddOffDiagBlock`

| Файл | Строка | Контекст |
|---|---|---|
| `JacobianAssembler.cpp` | 137 | `problem.AddOffDiagBlock(l, static_cast<int>(neibCount), blOffDiag)` — единственный production call site |
| `test_LinearProblemAssembly.cpp` | 51 | `lp.AddOffDiagBlock(l, ni, off.data())` — `int ni` |
| `test_LinearProblemAssembly.cpp` | 116 | `lp.AddOffDiagBlock(i, ni, off)` — `int ni` |
| `test_LinearProblemAssembly.cpp` | 139 | `lp.AddOffDiagBlock(i, ni, off)` — `int ni` |

### Downstream-использования `MatrixCSR::AddOffDiagBlock`

| Файл | Строка | Контекст |
|---|---|---|
| `LinearProblem.cpp` | 172, 177 | прокси-вызов — адаптируется автоматически после смены типа |
| `test_MatrixAssembly.cpp` | 32 | `m.AddOffDiagBlock(l, ni, voff)` — `int ni`, неявный `int → size_t` (уже `size_t` в `MatrixCSR`, не наша проблема) |

---

## Целевое состояние

```cpp
// LinearProblem.h:79-80
void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);
void AddOffDiagBlock(size_t l, size_t neibIdx, const double* data);

// JacobianAssembler.cpp:137 — без static_cast:
problem.AddOffDiagBlock(l, neibCount, blOffDiag);

// Тесты: int ni → size_t ni, убрать static_cast<int>() в циклах
```

---

## Варианты решения

### Вариант A (рекомендуемый): `int neibIdx` → `size_t neibIdx` в `LinearProblem`

- **Суть:** тип параметра в `LinearProblem` приводится к типу в `MatrixCSR`
- **Конкретные изменения:** 4 файла, ~10 строк
- **Плюсы:** минимальный diff, устраняет двойной каст, типы согласованы по всей цепочке
- **Минусы:** нет
- **Риски:** нет — `neibIdx` используется только как индекс в массив, всегда ≥ 0
- **Трудоёмкость:** ~5 минут

### Вариант B: добавить `size_t` overload, deprecate `int` версию

- **Суть:** сохранить обратную совместимость, добавить новый overload
- **Плюсы:** постепенная миграция
- **Минусы:** дублирование API, лишняя сложность для внутреннего кода
- **Трудоёмкость:** ~15 минут

### Выбор: Вариант A

Это внутренний API (не вызывается из внешнего кода), обратная совместимость не нужна. Минимальный diff = минимальный риск.

---

## Подводные камни

- ✅ **Все call sites найдены:** 1 production (`JacobianAssembler.cpp:137`), 3 тестовых (`test_LinearProblemAssembly.cpp:51,116,139`)
- ✅ **Потокобезопасность:** `AddOffDiagBlock` вызывается из последовательного цикла в `JacobianAssembler`, не из OpenMP-секции
- ✅ **Зависимости сборки:** изменения в `.h` + `.cpp`, CMake не затрагивается
- ✅ **Обратная совместимость API:** внутренний API, внешних пользователей нет
- ✅ **Тесты:** `test_LinearProblemAssembly.cpp` и `test_MatrixAssembly.cpp` — адаптируются в том же шаге
- ✅ **Связь с другими задачами:** DEBT-026 (`vector<double>&` → `const`) затрагивает тот же метод, но не конфликтует — меняется другой параметр. DEBT-021 (int в циклах) — остатки `int ni` в тестах закроются попутно
- ✅ **Производительность:** нет изменений — `int → size_t` на x64 одинаковый регистр

---

## Шаги

### Шаг 1: `int neibIdx` → `size_t neibIdx` + адаптация call sites

**Цель:** устранить двойное преобразование `size_t → int → size_t` в цепочке `JacobianAssembler → LinearProblem → MatrixCSR`.

**Файлы:** `HydroSolver/Solver/Math/LinearProblem.h`, `HydroSolver/Solver/Math/LinearProblem.cpp`, `HydroSolver/Reservoir/JacobianAssembler.cpp`, `tests/unit/math/test_LinearProblemAssembly.cpp`

**Что сделать:**

1. `LinearProblem.h:79`: `int neibIdx` → `size_t neibIdx` (overload с `vector<double>&`)
   ```
   До:  void AddOffDiagBlock(size_t l, int neibIdx, std::vector<double>& data);
   После: void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);
   ```

2. `LinearProblem.h:80`: `int neibIdx` → `size_t neibIdx` (overload с `const double*`)
   ```
   До:  void AddOffDiagBlock(size_t l, int neibIdx, const double* data);
   После: void AddOffDiagBlock(size_t l, size_t neibIdx, const double* data);
   ```

3. `LinearProblem.cpp:170`: `int neibIdx` → `size_t neibIdx`
   ```
   До:  void LinearProblem::AddOffDiagBlock(size_t l, int neibIdx, std::vector<double>& data)
   После: void LinearProblem::AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data)
   ```

4. `LinearProblem.cpp:175`: `int neibIdx` → `size_t neibIdx`
   ```
   До:  void LinearProblem::AddOffDiagBlock(size_t l, int neibIdx, const double* data)
   После: void LinearProblem::AddOffDiagBlock(size_t l, size_t neibIdx, const double* data)
   ```

5. `JacobianAssembler.cpp:137`: убрать `static_cast<int>()`
   ```
   До:  problem.AddOffDiagBlock(l, static_cast<int>(neibCount), blOffDiag);
   После: problem.AddOffDiagBlock(l, neibCount, blOffDiag);
   ```

6. `test_LinearProblemAssembly.cpp:46`: `int ni` → `size_t ni`, убрать `static_cast<int>()`
   ```
   До:  for (int ni = 0; ni < static_cast<int>(graph[l].size()); ni++) {
   После: for (size_t ni = 0; ni < graph[l].size(); ni++) {
   ```

7. `test_LinearProblemAssembly.cpp:114`: аналогично
   ```
   До:  for (int ni = 0; ni < static_cast<int>(graph[i].size()); ++ni) {
   После: for (size_t ni = 0; ni < graph[i].size(); ++ni) {
   ```

8. `test_LinearProblemAssembly.cpp:137`: аналогично
   ```
   До:  for (int ni = 0; ni < static_cast<int>(graph[i].size()); ++ni) {
   После: for (size_t ni = 0; ni < graph[i].size(); ++ni) {
   ```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 312 тестов должны остаться зелёными

**Подводные камни:**
- `test_MatrixAssembly.cpp:27` — `int ni` остаётся, т.к. вызывает `MatrixCSR::AddOffDiagBlock` напрямую (уже `size_t`), неявный `int → size_t` допустим и существовал до DEBT-028. Можно почистить, но это DEBT-021 остаток, не скоуп текущей задачи

**Оценка:** 8 изменений в 4 файлах, ~5 минут

---

### Шаг 2: проверка чистоты warnings

**Цель:** убедиться, что изменения не породили новые warnings.

**Что сделать:**

1. Полная пересборка:
   ```powershell
   cmake --build build --config Release -- /t:Rebuild 2>&1 | Select-String "warning C" | Where-Object { $_.Line -notmatch "amgcl|Catch2|Eigen|xutility|xmemory|xstring|numeric" }
   ```

2. Ожидание: 0 warnings из проектного кода

**Оценка:** ~3 минуты (ожидание rebuild)

---

## Критерии завершения

- [ ] `size_t neibIdx` в объявлении и определении `LinearProblem::AddOffDiagBlock`
- [ ] `static_cast<int>(neibCount)` убран в `JacobianAssembler.cpp`
- [ ] `size_t ni` в тестовых циклах `test_LinearProblemAssembly.cpp`
- [ ] Все 312 тестов зелёные
- [ ] 0 новых warnings из проектного кода
- [ ] Vault обновлён (статус DEBT-028)
- [ ] GitHub issue #34 прокомментирован

---

## Обнаруженные проблемы

1. **`test_MatrixAssembly.cpp:27`** — `for (int ni = 0; ni < static_cast<int>(graph[l].size()); ni++)` — остаток DEBT-021, `int ni` вызывает `MatrixCSR::AddOffDiagBlock(size_t, size_t, ...)` с неявным `int → size_t`. Не блокирует DEBT-028

2. **`test_JacobianAssembly.cpp:54,193`** — `for (int ni = 0; ...)` — остаток DEBT-021, не связан с `AddOffDiagBlock` API. Не блокирует

---

## Связанные заметки

- [[известные баги и технический долг]] — секция DEBT-028
- Related: #31 (DEBT-021, закрыт), #33 (DEBT-022, закрыт)
- DEBT-026 — `AddOffDiagBlock` `vector<double>&` → `const` (тот же метод, другой параметр)
- DEBT-058 — общая задача типизации int/size_t
