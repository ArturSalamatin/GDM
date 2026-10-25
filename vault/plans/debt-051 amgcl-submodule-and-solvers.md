---
tags:
  - план
  - миграция
  - amgcl
  - солвер
  - submodule
date: 2026-07-08
issues:
  - DEBT-051
  - DEBT-052
  - DEBT-053
  - FEAT-010
  - FEAT-011
github: 18
branch: refactor/debt-051/amgcl-submodule
status: реализован
audit:
  date: 2026-07-08
  pass: 2
  findings: 0 / 0 / 1
  auto-fixed: 1
  manual-required: 0
---

# DEBT-051..053 + FEAT-010/011: AMGCL submodule, патчи и новые солверы

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки (ссылки внизу)
3. Создай ветку: `git checkout -b refactor/debt-051/amgcl-submodule experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline (ожидание: 294 теста, ~291 сек)
7. Начни с шага 1. После каждого шага: сборка + тесты

## Связанные vault-заметки

- [[локальные патчи AMGCL для GDM]] — два патча: ilu0.hpp fallback, cpr.hpp fallback
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — физика проблемы Sw=0
- [[возможности amgcl для блочных СЛАУ]] — каталог компонентов amgcl
- [[переход с блочного AMG на скалярный CPR в production]] — почему CPR

## Мотивация

AMGCL хранится как локальная копия с двумя ручными патчами (BUG-002). Это блокирует обновление библиотеки и разработку новых методов решения СЛАУ. Основная проблема — zero pivot при Sw=0 в CPR decoupling — не решается выбором Krylov-метода или coarsening, а требует изменения decoupling weights внутри amgcl.

## Стратегия

1. Перевести amgcl на submodule (форк) — инфраструктура
2. Патчи и новые солверы — **две независимые ветки** в форке
3. Сначала протестировать готовые конфигурации amgcl на Sw=0 (SolverFactory)
4. Затем реализовать новый код (threshold fallback, True-IMPES weights)
5. Если новые солверы решают Sw=0 — перейти на них, патчи не нужны

## Граф зависимостей

```
DEBT-051  amgcl → submodule
├── DEBT-052  патчи → experimental/patches (параллельно с 053)
└── DEBT-053  SolverFactory + бенчмарк 5 конфигураций
       │
       ├── точка решения: CPR-DRS решает Sw=0?
       │   да → FEAT-010/011 понижаются в приоритете
       │   нет (ожидаемо) → продолжаем
       │
       ├── FEAT-010  threshold fallback
       └── FEAT-011  True-IMPES weights
```

---

## Текущее состояние (baseline для DEBT-051)

### Структура amgcl в git

- Папка `HydroSolver/AMGSolver/` целиком в `.gitignore` (строка 8: `AMGSolver`)
- Из amgcl git отслеживает **только 2 файла** (force-added поверх .gitignore):
  - `HydroSolver/AMGSolver/amgcl/amgcl/relaxation/ilu0.hpp` — патч fallback D[i]=1
  - `HydroSolver/AMGSolver/amgcl/amgcl/preconditioner/cpr.hpp` — патч fallback d=1
- Физически `HydroSolver/AMGSolver/amgcl/` — полный клон upstream (~670 файлов) со своим `.git`
- `HydroSolver/AMGSolver/Docs/` — справочные PDF (не в сборке, не в git)

### CMake

- `CMakeLists.txt:78`: `target_include_directories(gdm_core PUBLIC ${HYDRO} ${HYDRO}/AMGSolver/amgcl)`
- Include `<amgcl/...>` работает потому что `${HYDRO}/AMGSolver/amgcl` — корень amgcl

### Файлы, включающие amgcl

| Файл | Includes |
|---|---|
| `HydroSolver/Solver/Math/LinearProblem.h` | 9 includes (crs_tuple, make_solver, amg, aggregation, ilu0, as_preconditioner, cpr, lgmres, profiler) |
| `tests/test_amgcl_benchmark.cpp` | 10 includes (те же + iluk, cpr_drs) |
| `examples/ex_benchmark_series_cpr.cpp` | 10 includes (те же + iluk, cpr_drs) |

Все включают через `#include <amgcl/...>` — пути не изменятся при смене include directory.

### Submodules (существующие)

```
[submodule "GridEngine"]
    path = GridEngine
    url = http://10.8.0.1:9999/pcnac_development/gridengine.git
[submodule "Eigen"]
    path = Eigen
    url = https://gitlab.com/libeigen/eigen.git
```

### Форк

Уже создан: https://github.com/ArturSalamatin/vcpkg-amgcl/tree/master

---

## Этап 1: DEBT-051 — amgcl как git submodule

### Предусловия (выполняет пользователь вручную)

1. В форке `ArturSalamatin/vcpkg-amgcl` создать ветку `experimental/master` от коммита `c71ab45` (не от HEAD master!)
   - HEAD форка (`28296c2`, Merge PR #315 — ROCm/hipSPARSE) **новее** нашей локальной копии. Между ними могут быть API-breaking changes
   - Наша локальная amgcl зафиксирована на `c71ab45` ("Small fix for mpi")
   - Команда: `git checkout -b experimental/master c71ab45`
2. Убедиться, что ветка запушена на GitHub

---

### Шаг 1: Удалить tracked-файлы amgcl из git

**Цель:** убрать 2 пропатченных файла из git index, не трогая физические файлы (они будут заменены submodule)

**Файлы:** `HydroSolver/AMGSolver/amgcl/amgcl/relaxation/ilu0.hpp`, `HydroSolver/AMGSolver/amgcl/amgcl/preconditioner/cpr.hpp`

**Контекст:**
Git отслеживает только эти 2 файла из всей папки amgcl (force-added поверх .gitignore `AMGSolver`). Удаляем их из index, чтобы git не конфликтовал при добавлении submodule.

**Что сделать:**
1. `git rm --cached HydroSolver/AMGSolver/amgcl/amgcl/relaxation/ilu0.hpp`
2. `git rm --cached HydroSolver/AMGSolver/amgcl/amgcl/preconditioner/cpr.hpp`

Флаг `--cached` удаляет из index, но оставляет файлы на диске (они уже в .gitignore, так что останутся ignored).

**Проверка после этого шага:**
- `git ls-files HydroSolver/AMGSolver/amgcl` — пусто
- `git status` показывает deleted в staged
- Сборка: `cmake --build build --config Release` — успешно (файлы на диске)
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — 294 pass

**Подводные камни:**
- ✅ Физические файлы остаются — сборка не сломается
- ✅ `.gitignore` содержит `AMGSolver` — файлы не появятся обратно в untracked

---

### Шаг 2: Добавить amgcl как git submodule

**Цель:** подключить форк amgcl как submodule в корне проекта

**Файлы:** `.gitmodules`, `amgcl/` (новая директория)

**Контекст:**
Submodule добавляется аналогично Eigen и GridEngine — в корень проекта. Ветка `experimental/master` — чистый upstream без патчей.

**Что сделать:**
1. `git submodule add -b experimental/master https://github.com/ArturSalamatin/vcpkg-amgcl.git amgcl`
2. `cd amgcl && git checkout experimental/master && cd ..`
3. `git add amgcl` (зафиксировать конкретный коммит submodule)

Примечание: `-b experimental/master` записывает `branch = experimental/master` в `.gitmodules` и задаёт default branch для `git submodule update --remote`. Начальный checkout всё равно нужно выставить вручную на нужный коммит (шаг 2).

**Проверка после этого шага:**
- `git submodule status` показывает 3 submodule: GridEngine, Eigen, amgcl
- `.gitmodules` содержит запись:
  ```
  [submodule "amgcl"]
      path = amgcl
      url = https://github.com/ArturSalamatin/vcpkg-amgcl.git
      branch = experimental/master
  ```
- Директория `amgcl/` существует, содержит amgcl headers

**Подводные камни:**
- ⚠️ Если `amgcl/` уже существует как директория — submodule add не сработает. Решение: удалить/переименовать конфликтующую директорию
- ✅ Имя submodule `amgcl` не конфликтует с `AMGSolver` в .gitignore

---

### Шаг 3: Обновить CMakeLists.txt — include path

**Цель:** переключить include path amgcl с локальной копии на submodule

**Файлы:** `CMakeLists.txt` (строка 78)

**Контекст:**
Include path должен указывать на корень submodule, потому что все includes в коде — `#include <amgcl/...>`. Корень submodule `amgcl/` содержит папку `amgcl/` с headers, поэтому include path = `${CMAKE_SOURCE_DIR}/amgcl`.

**Изменения (старый → новый код):**

До:
```cmake
target_include_directories(gdm_core PUBLIC ${HYDRO} ${HYDRO}/AMGSolver/amgcl)
```

После:
```cmake
target_include_directories(gdm_core PUBLIC ${HYDRO} ${CMAKE_SOURCE_DIR}/amgcl)
```

**Проверка после этого шага:**
- Сборка: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }` — успешно
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — 294 pass
- Убедиться, что amgcl headers берутся из submodule, а не из старой копии: в выводе cmake должен быть путь `d:/Lessons/Grants/2027/GDM/amgcl`

**Подводные камни:**
- ⚠️ Старая копия `HydroSolver/AMGSolver/amgcl/` физически остаётся на диске. Если include path не обновлён — компилятор найдёт headers в старой копии. Проверить: временно переименовать старую папку → сборка должна пройти
- ✅ Все `#include <amgcl/...>` в коде — relative, не зависят от полного пути

---

### Шаг 4: Проверить сборку без старой копии

**Цель:** убедиться, что проект собирается исключительно из submodule

**Что сделать:**
1. Переименовать старую копию: `Rename-Item HydroSolver\AMGSolver\amgcl HydroSolver\AMGSolver\amgcl_backup`
2. Полная пересборка: `Remove-Item -Recurse -Force build; cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
3. Тесты: `ctest --test-dir build -C Release --output-on-failure` — 294 pass
4. Если всё зелёное — удалить backup: `Remove-Item -Recurse -Force HydroSolver\AMGSolver\amgcl_backup`

**Подводные камни:**
- ⚠️ Submodule использует **чистый upstream** без патчей. Тесты с Sw_init=0.001 должны проходить (патчи нужны только при Sw=0). Если какой-то тест неожиданно падает — значит он зависит от патчей, и нужно разобраться
- ✅ Profiler `amgcl::prof` — определён в `LinearProblem.h:23`, не зависит от патчей

---

### Шаг 5: Обновить .gitignore

**Цель:** строка `AMGSolver` в .gitignore больше не нужна в текущем виде — amgcl подключён как submodule, а `HydroSolver/AMGSolver/Docs/` можно оставить ignored

**Файлы:** `.gitignore` (строка 8)

**Изменения:**

До:
```
AMGSolver
```

После:
```
HydroSolver/AMGSolver/
```

**Контекст:**
Более явный паттерн. `AMGSolver` без пути игнорировал любую папку с таким именем в любом месте. `HydroSolver/AMGSolver/` — только конкретная legacy-папка. Submodule `amgcl/` в корне не затрагивается.

**Проверка после этого шага:**
- `git status` — submodule `amgcl` не появляется в untracked
- Папка `HydroSolver/AMGSolver/` остаётся ignored

---

### Критерии завершения DEBT-051

- [ ] `git submodule status` показывает amgcl рядом с Eigen и GridEngine
- [ ] `.gitmodules` содержит запись для amgcl с branch = experimental/master
- [ ] `CMakeLists.txt` ссылается на `${CMAKE_SOURCE_DIR}/amgcl`
- [ ] Старые tracked файлы удалены из git index (`git ls-files HydroSolver/AMGSolver/amgcl` — пусто)
- [ ] Сборка проходит **без** старой локальной копии amgcl
- [ ] Все 294 теста зелёные в Release
- [ ] Старая копия amgcl удалена с диска или переименована

---

## Этап 2: DEBT-052 — патчи в ветку experimental/patches

### Ветка amgcl: `experimental/patches` (от `experimental/master`)

### Шаги

1. В репозитории форка (`ArturSalamatin/vcpkg-amgcl`) создать ветку `experimental/patches` от `experimental/master`
2. Коммит 1: fallback в `amgcl/relaxation/ilu0.hpp`
   - Заменить `precondition(!math::is_zero((*D)[i]), "Zero pivot in ILU")` на:
     ```cpp
     if (math::is_zero((*D)[i]))
         (*D)[i] = static_cast<value_type>(1);
     ```
   - Commit message: описание проблемы (Sw=0, вырожденный блок Якобиана водной фазы, dkrw/dSw → 0, zero pivot в ILU0), физическая причина, при каких условиях срабатывает
3. Коммит 2: fallback в `amgcl/preconditioner/cpr.hpp`
   - Заменить `assert(!math::is_zero(d))` на fallback `if (math::is_zero(d)) d = 1;`
   - Commit message: аналогично — CPR block-LU при singular 2×2 блоке Якобиана

### Проверка
- В GDM: переключить submodule на `experimental/patches`, сборка + тесты
- Five-spot Sw_init=0 проходит (как и раньше с локальными патчами)

### Связь с основной веткой
- `experimental/patches` **не мёржится** в `experimental/master`
- Живёт параллельно как страховка
- Если FEAT-010/011 решают проблему — ветка архивируется

---

## Этап 3: DEBT-053 — SolverFactory и бенчмарк

### Ветка GDM: `refactor/debt-053/solver-factory`

### Предыстория

В `test_amgcl_benchmark.cpp` уже протестированы 4 конфигурации через дублирование `using`:
- CPR1: CPR<AMG<aggregation, ilu0>, ilu0> + lgmres (production)
- CPR2: CPR<AMG<aggregation, iluk>, ilu0> + lgmres
- CPR3: CPR-DRS<AMG<aggregation, ilu0>, ilu0> + lgmres
- CPR4: ILU0 (без AMG) + lgmres

SolverFactory обобщает этот паттерн: CMake-опция переключает `using`-алиасы в `LinearProblem.h`.

### Шаги

1. Создать `HydroSolver/Solver/Math/SolverConfig.h` с `#ifdef`-блоками:
   - `GDM_SOLVER_CPR_LGMRES` — текущий production (default)
   - `GDM_SOLVER_CPR_BICGSTAB` — BiCGStab вместо LGMRES
   - `GDM_SOLVER_CPR_SA_LGMRES` — smoothed_aggregation вместо aggregation
   - `GDM_SOLVER_CPRDRS_LGMRES` — CPR-DRS
   - `GDM_SOLVER_ILU0_LGMRES` — голый ILU0
2. CMake option: `set(GDM_SOLVER "CPR_LGMRES" CACHE STRING "Solver configuration")`
3. `LinearProblem.h`: `#include "SolverConfig.h"`, убрать захардкоженные `using`
4. Бенчмарк: прогнать все 5 конфигураций на five-spot 21×21:
   - С Sw_init=0.001 (должны проходить все)
   - С Sw_init=0 (ожидание: все падают без патчей)
5. Зафиксировать результаты в vault

### Точка решения после бенчмарка

- **Если CPR-DRS решает Sw=0** (маловероятно — DRS улучшает обусловленность, но не устраняет singular блок) → FEAT-010/011 понижаются
- **Если ни одна конфигурация не решает Sw=0** (ожидаемо) → переходим к FEAT-010

---

## Этап 4: FEAT-010 — threshold-based fallback в CPR

### Ветка amgcl: `experimental/solvers` (от `experimental/master`)

Реализация нового кода в `amgcl/preconditioner/cpr.hpp`. Замена exact-zero проверки на threshold-based: `|d| < τ·max(|A_row|)`.

Подробности: [[планируемые фичи]] → FEAT-010

---

## Этап 5: FEAT-011 — True-IMPES weights

### Ветка amgcl: `experimental/solvers` (поверх FEAT-010 или отдельный коммит)

Замена quasi-IMPES decoupling (обращение 2×2 блока) на True-IMPES weights из nullspace ∂F/∂Sw.

Подробности: [[планируемые фичи]] → FEAT-011

Если True-IMPES решает Sw=0 → переключить production submodule на `experimental/solvers`, патчи (`experimental/patches`) архивировать.

---

## Чеклист подводных камней (фаза 2.5)

- [x] **Все call sites найдены:** 3 файла включают amgcl (LinearProblem.h, test_amgcl_benchmark.cpp, ex_benchmark_series_cpr.cpp). Все через `#include <amgcl/...>` — не зависят от полного пути
- [x] **Потокобезопасность:** не затрагивается — меняется только путь include
- [x] **Зависимости сборки:** CMakeLists.txt строка 78 — единственная точка изменения
- [x] **Обратная совместимость API:** API не меняется
- [x] **Тесты:** 294 теста, все используют Sw_init ≥ 0.001 → не зависят от патчей
- [x] **Кодировки:** не затрагиваются
- [x] **Платформозависимость:** submodule работает одинаково на Windows/Linux
- [x] **Мёртвый код:** `HydroSolver/AMGSolver/Docs/` — справочные PDF, не в сборке, не в git — можно оставить
- [x] **Связь с другими задачами:** DEBT-052/053 зависят от DEBT-051, конфликтов нет
- [x] **Производительность:** headers идентичны upstream — производительность не меняется

## Обнаруженные проблемы (аудит 2026-07-08, pass 1+2)

### 🔴 Несовпадение версий upstream

Форк `ArturSalamatin/vcpkg-amgcl` HEAD (`28296c2`) **новее** локальной копии (`c71ab45`). Ветку `experimental/master` необходимо создавать от `c71ab45`, иначе возможны API-breaking changes.

**Фикс:** добавлено в предусловия — `git checkout -b experimental/master c71ab45`.

### 🟡 Baseline тестов может быть устаревшим

План указывает 294 теста. Текущие приоритеты указывают 294, но после DEBT-008 (Eigen) количество могло измениться. Baseline нужно зафиксировать при старте реализации.

**Фикс:** добавлена инструкция в «Как начать работу» — запомнить фактическое количество тестов.

### 🔵 Шаг 2: `git submodule add -b` не гарантирует checkout

Флаг `-b` задаёт default branch для `git submodule update --remote`, но начальный checkout может не совпасть. Нужен явный `cd amgcl && git checkout experimental/master`.

**Фикс:** обновлена команда в шаге 2.

### 🔵 Шаг 2 (pass 2): команда submodule add без `-b`, а проверка ожидает `branch =`

Команда `git submodule add` не содержала `-b experimental/master`, но проверка ожидала `branch = experimental/master` в `.gitmodules`. Без `-b` этой записи не будет.

**Фикс:** добавлен `-b experimental/master` в команду `git submodule add`.

---

## Связанные заметки

- [[технический долг]] — DEBT-007, DEBT-051, DEBT-052, DEBT-053
- [[планируемые фичи]] — FEAT-010, FEAT-011
- [[локальные патчи AMGCL для GDM]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[возможности amgcl для блочных СЛАУ]]
- [[переход с блочного AMG на скалярный CPR в production]]
