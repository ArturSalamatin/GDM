---
tags:
  - план
  - рефакторинг
date: 2026-07-17
issue: DEBT-026
github: 36
branch: refactor/debt-026/addoffdiagblock-const
status: готов к реализации
audit:
  date: 2026-07-17
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-026: `AddOffDiagBlock` — добавить `const` к `vector<double>&`

## Контекст

`MatrixCSR::AddOffDiagBlock` и `LinearProblem::AddOffDiagBlock` принимают `std::vector<double>&` без `const`, хотя данные не модифицируются. Аналогичный метод `AddDiagBlock` уже принимает `const std::vector<double>&`. Несимметричность вводит в заблуждение и мешает вызывать метод с `const`-объектами.

Внутри `AddOffDiagBlock` вызывается `CopyBlock(offset, dest, data, blocks)`, где параметр `data` уже имеет тип `const std::vector<double>&` — поэтому добавление `const` корректно и не требует каскадных изменений.

### Связанные vault-заметки

- [[технический долг]] — запись DEBT-026
- GitHub issue [#36](https://github.com/ArturSalamatin/GDM/issues/36)

## Подтип

Рефакторинг (const-correctness).

## Анализ

### Текущее состояние

| Файл | Строка | Сигнатура |
|---|---|---|
| `HydroSolver/Solver/Math/MatrixCSR.h` | 52 | `void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);` |
| `HydroSolver/Solver/Math/MatrixCSR.cpp` | 43 | `void MatrixCSR::AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data)` |
| `HydroSolver/Solver/Math/LinearProblem.h` | 79 | `void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);` |
| `HydroSolver/Solver/Math/LinearProblem.cpp` | 170 | `void LinearProblem::AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data)` |

### Целевое состояние

Во всех 4 местах: `std::vector<double>& data` → `const std::vector<double>& data`.

### Call sites

| Файл | Строка | Перегрузка | Влияние |
|---|---|---|---|
| `HydroSolver/Reservoir/JacobianAssembler.cpp` | 137 | `const double*` | нет (другая перегрузка) |
| `tests/unit/math/test_MatrixAssembly.cpp` | 32 | `vector<double>&` | ✅ `voff` — неконстантный `vector`, передаётся по ссылке. Добавление `const` к параметру не ломает вызов: мутабельная ссылка конвертируется в `const`-ссылку |
| `tests/unit/math/test_LinearProblemAssembly.cpp` | 51, 116, 139 | `const double*` | нет (другая перегрузка) |

### Подводные камни

- [x] **Все call sites найдены:** 5 вызовов в 3 файлах — все проверены
- [x] **CopyBlock совместим:** уже принимает `const std::vector<double>&`
- [x] **Тесты:** `test_MatrixAssembly` использует `vector<double>&` перегрузку, но мутабельный аргумент → const-ссылка биндится без изменений
- [x] **Обратная совместимость:** мутабельная ссылка всегда конвертируется в const-ссылку, ни один call site не сломается
- [x] **Вычисления не затронуты:** изменение чисто в сигнатурах, тело функций идентично

### Обнаруженные проблемы

Нет.

## Шаги реализации

### Шаг 1: Создать ветку и зафиксировать baseline

**Цель:** убедиться, что все тесты зелёные до начала изменений.

**Что сделать:**

1. `git checkout -b refactor/debt-026/addoffdiagblock-const experimental`
2. Собрать: `cmake --build build --config Release`
3. Тесты: `ctest --test-dir build -C Release --output-on-failure`
4. Запомнить количество тестов и время

**Проверка:** все тесты зелёные.

### Шаг 2: Добавить `const` в объявление и определение `MatrixCSR::AddOffDiagBlock`

**Цель:** исправить const-correctness на уровне `MatrixCSR`.

**Файлы:**
- `HydroSolver/Solver/Math/MatrixCSR.h:52`
- `HydroSolver/Solver/Math/MatrixCSR.cpp:43`

**Что сделать:**

В `MatrixCSR.h:52`:
```
До:  void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);
После: void AddOffDiagBlock(size_t l, size_t neibIdx, const std::vector<double>& data);
```

В `MatrixCSR.cpp:43`:
```
До:  void MatrixCSR::AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data)
После: void MatrixCSR::AddOffDiagBlock(size_t l, size_t neibIdx, const std::vector<double>& data)
```

**Проверка:**
- `cmake --build build --config Release`
- `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные

**Подводные камни:** нет — `CopyBlock` уже принимает `const vector<double>&`.

### Шаг 3: Добавить `const` в объявление и определение `LinearProblem::AddOffDiagBlock`

**Цель:** исправить const-correctness на уровне `LinearProblem`.

**Файлы:**
- `HydroSolver/Solver/Math/LinearProblem.h:79`
- `HydroSolver/Solver/Math/LinearProblem.cpp:170`

**Что сделать:**

В `LinearProblem.h:79`:
```
До:  void AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data);
После: void AddOffDiagBlock(size_t l, size_t neibIdx, const std::vector<double>& data);
```

В `LinearProblem.cpp:170`:
```
До:  void LinearProblem::AddOffDiagBlock(size_t l, size_t neibIdx, std::vector<double>& data)
После: void LinearProblem::AddOffDiagBlock(size_t l, size_t neibIdx, const std::vector<double>& data)
```

**Проверка:**
- `cmake --build build --config Release`
- `ctest --test-dir build -C Release --output-on-failure`
- Все тесты зелёные

**Подводные камни:** нет — `LinearProblem::AddOffDiagBlock` просто делегирует в `MatrixCSR::AddOffDiagBlock`.

**Зависимости:** требует шаг 2 (MatrixCSR должен быть обновлён первым, иначе делегирующий вызов не скомпилируется).

### Шаг 4: Обновить vault

**Цель:** зафиксировать завершение задачи.

**Что сделать:**
1. В `vault/GDM/roadmap/технический долг.md` — пометить DEBT-026 как завершённый
2. Прокомментировать GitHub issue #36: `gh issue comment 36 --repo ArturSalamatin/GDM --body "Исправлено в ветке refactor/debt-026/addoffdiagblock-const"`

## Тестовая стратегия

Новые тесты не нужны. Изменение чисто в сигнатурах — const-qualifier добавляется к параметру, который уже не модифицировался. Существующие тесты покрывают оба пути:

- `test_MatrixAssembly` — использует `vector<double>&` перегрузку → проверяет, что const-ссылка работает
- `test_LinearProblemAssembly` — использует `const double*` перегрузку → не затронута

## Критерии завершения

- [x] Все 4 сигнатуры обновлены
- [ ] Сборка без warnings
- [ ] Все существующие тесты зелёные
- [ ] Vault обновлён
- [ ] GitHub issue #36 прокомментирован

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-026/addoffdiagblock-const experimental`
3. Собери baseline: `cmake --build build --config Release`
4. Прогони тесты: `ctest --test-dir build -C Release --output-on-failure`
5. Запомни количество тестов — это baseline
6. Выполняй шаги 2–4 последовательно. После каждого: сборка + тесты

## Оценка

- **Файлов:** 4
- **Строк изменений:** 4 (по одному слову `const` в каждой)
- **Время:** ~5 минут
