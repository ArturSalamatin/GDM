---
tags:
  - план
  - удаление
date: 2026-07-08
issue: DEBT-008
github: null
branch: refactor/debt-008/eigen-remove-dead-include
status: готов к реализации
audit:
  date: 2026-07-08
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-008: Удалить мёртвый include Eigen

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Создай ветку: `git checkout -b refactor/debt-008/eigen-remove-dead-include experimental`
3. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
4. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
5. Запомни количество тестов и время — это baseline
6. Начни с шага 1. После каждого шага: сборка + тесты

## Связанные vault-заметки

- DEBT-004 (DB-фабрики заглушены) — `gdm_data` целиком является legacy-таргетом, собирается только при наличии PostgreSQL. DEBT-008 не конфликтует: удаляем только мёртвый include, не трогаем таргет

---

## Фаза 1: Анализ

### 1.1. Текущее состояние

Запись в roadmap описывает DEBT-008 как «Eigen не управляется CMake, подключить через FetchContent». Анализ кода показал, что задача проще:

- `#include <Eigen/Eigen>` присутствует **только** в `HydroSolver/Data/wells/Maps.h:10`
- **Ни один тип или функция Eigen не используется** нигде в проекте (grep по `Eigen::`, `MatrixX`, `VectorX`, `Map<`, `Ref<` — ноль вхождений)
- Workaround `_HAS_DEPRECATED_RESULT_OF=1` в `CMakeLists.txt:343` добавлен исключительно для совместимости Eigen с C++23 (Eigen внутри использует `std::result_of`, удалённый в C++23)
- `gdm_data` собирается только при `PostgreSQL_FOUND` (строка 303), PostgreSQL на текущей машине нет → таргет не собирается

### 1.2. Целевое состояние

- `Maps.h` не включает Eigen
- `CMakeLists.txt` не содержит `_HAS_DEPRECATED_RESULT_OF`
- Зависимость от Eigen полностью устранена
- Все 294 теста зелёные (gdm_data не участвует в тестах)

### 1.3. Выбор варианта

Исходная формулировка DEBT-008 предлагала FetchContent. Но FetchContent для библиотеки, которая **не используется** — бессмысленно. Правильное действие: удалить мёртвый include.

### 1.4. Поиск подводных камней

- ✅ **Все call sites найдены:** grep по `Eigen` во всём `HydroSolver/` — единственное вхождение в Maps.h:10
- ✅ **Тесты:** `gdm_data` не собирается и не тестируется (PostgreSQL нет), 294 теста не затронуты
- ✅ **Обратная совместимость:** при появлении PostgreSQL `gdm_data` по-прежнему соберётся — просто без неиспользуемого Eigen
- ✅ **Связь с другими задачами:** DEBT-004 (удаление DB-фабрик) может удалить `gdm_data` целиком, но DEBT-008 не конфликтует — удаляем только include

### 1.5. Обнаруженные проблемы

Нет новых проблем.

---

## Фаза 2: Детализация плана

### Затронутые файлы

| Файл | Строки | Изменение |
|---|---|---|
| `HydroSolver/Data/wells/Maps.h` | 10 | Удалить `#include <Eigen/Eigen>` |
| `CMakeLists.txt` | 342–343 | Удалить комментарий и `_HAS_DEPRECATED_RESULT_OF=1` |

### Шаг 1: Удалить include Eigen и workaround

**Цель:** полностью убрать зависимость от Eigen

**Файлы:** `HydroSolver/Data/wells/Maps.h`, `CMakeLists.txt`

**Контекст:**
`Maps.h` — заголовок класса `Maps` в legacy-таргете `gdm_data` (Data-слой, PostgreSQL + GEOS). Включает `<Eigen/Eigen>`, но **ни один тип Eigen не используется** ни в Maps.h, ни в Maps.cpp, ни в каком-либо другом файле проекта. Include остался от более ранней версии кода. Workaround `_HAS_DEPRECATED_RESULT_OF=1` в CMakeLists.txt нужен только для компиляции этого include в C++23 (Eigen использует `std::result_of`, удалённый в стандарте).

**Что сделать:**

1. В `HydroSolver/Data/wells/Maps.h`, строка 10: удалить `#include <Eigen/Eigen>`
2. В `CMakeLists.txt`, строки 342–343: удалить комментарий `# Maps.h включает системную Eigen...` и строку `target_compile_definitions(gdm_data PUBLIC _HAS_DEPRECATED_RESULT_OF=1)`

**Изменения:**

Maps.h — до:
```cpp
#include <Eigen/Eigen>

#include <geos/geom.h>
```

Maps.h — после:
```cpp
#include <geos/geom.h>
```

CMakeLists.txt — до:
```cmake
    target_link_libraries(gdm_data PUBLIC gdm_core pqxx GEOS::geos)
    # Maps.h включает системную Eigen (std::result_of удалён в C++23)
    target_compile_definitions(gdm_data PUBLIC _HAS_DEPRECATED_RESULT_OF=1)
```

CMakeLists.txt — после:
```cmake
    target_link_libraries(gdm_data PUBLIC gdm_core pqxx GEOS::geos)
```

**Проверка после этого шага:**
- Сборка: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
- Тесты: `ctest --test-dir build -C Release --output-on-failure`
- Все 294 теста зелёные (gdm_data не собирается — PostgreSQL нет)
- Проверить: `grep -r "Eigen" HydroSolver/` — ноль вхождений

**Подводные камни:**
- Нет. Таргет gdm_data не собирается, тесты не затронуты

**Зависимости:** нет

**Оценка:** ~3 строки удалены, ~2 минуты

---

## Тестовая стратегия

Новых тестов не требуется. `gdm_data` не участвует в тестах (PostgreSQL отключён). Верификация: все 294 существующих теста зелёные + grep подтверждает отсутствие Eigen.

---

## Критерии завершения

- [ ] `#include <Eigen/Eigen>` удалён из Maps.h
- [ ] `_HAS_DEPRECATED_RESULT_OF=1` удалён из CMakeLists.txt
- [ ] `grep -r "Eigen" HydroSolver/` — 0 вхождений
- [ ] Все 294 теста зелёные (Release + Debug)
- [ ] Vault обновлён: DEBT-008 → ✅
- [ ] Описание DEBT-008 в roadmap обновлено (было «подключить через FetchContent», стало «удалён мёртвый include»)
