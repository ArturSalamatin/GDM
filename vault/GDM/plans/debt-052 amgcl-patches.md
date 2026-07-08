---
tags:
  - план
  - инфраструктура
  - amgcl
  - патчи
date: 2026-07-08
issues:
  - DEBT-052
github: 19
branch: refactor/debt-052/amgcl-patches
status: реализован
audit:
  date: 2026-07-08
  round: 3
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# DEBT-052: Патчи AMGCL — ветка experimental/patches

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки (ссылки внизу)
3. Создай ветку: `git checkout -b refactor/debt-052/amgcl-patches experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline (ожидание: 293/294, ~291 сек, 1 fail = Grid convergence 41×41)
7. Начни с шага 1. После каждого шага: сборка + тесты

## Связанные vault-заметки

- [[локальные патчи AMGCL для GDM]] — точное содержимое патчей (до/после)
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — физика проблемы
- [[debt-051 amgcl-submodule-and-solvers]] — общий план (этап 2)

## Мотивация

После DEBT-051 amgcl подключён как submodule из форка `ArturSalamatin/vcpkg-amgcl`, ветка `experimental/master` (чистый upstream, коммит `c71ab45`). Два локальных патча BUG-002 утрачены — тест Grid convergence 41×41 падает с "Zero pivot in ILU" (293/294).

Патчи нужно перенести в отдельную ветку форка `experimental/patches` с детальными commit messages. Ветка живёт параллельно `experimental/master` и не мёржится в неё. Если FEAT-010/011 решат проблему Sw=0 — ветка архивируется.

## Текущее состояние (baseline)

### Git

- Submodule `amgcl/` → `ArturSalamatin/vcpkg-amgcl`, ветка `experimental/master`, коммит `c71ab45`
- `.gitmodules` содержит `branch = experimental/master`
- Форк содержит 2 ветки: `master`, `experimental/master`

### Файлы, требующие патчей

В submodule `amgcl/`:

1. **`amgcl/relaxation/ilu0.hpp:156`** — `precondition(!math::is_zero((*D)[i]), "Zero pivot in ILU");`
   Нужно заменить на: `if (math::is_zero((*D)[i])) (*D)[i] = static_cast<value_type>(1);`

2. **`amgcl/preconditioner/cpr.hpp:522`** — `assert(!math::is_zero(d));`
   Нужно заменить на: `if (math::is_zero(d)) { d = static_cast<scalar_type>(1); A[k*B+k] = d; }`
   ⚠️ Запись `A[k*B+k] = d` обязательна — обратный ход (строка 543) читает из массива

### Тесты

- 294 тестов, 293 pass, 1 fail: `integration.Grid convergence: single injector` (Zero pivot in ILU на сетке 41×41)
- После применения патчей ожидается 294/294

---

## Шаг 1: Создать ветку experimental/patches в форке

**Цель:** создать ветку от `experimental/master` для изолированного хранения патчей

**Файлы:** внутри submodule `amgcl/`

**Контекст:**
Ветка `experimental/patches` — параллельная ветка, не мёржится в `experimental/master`. Содержит минимальные правки для обхода zero pivot при Sw=0 и ILU-produced zeros на тонких сетках. Нужна как страховка на время разработки FEAT-010/011.

**Что сделать:**
1. `cd amgcl`
2. `git checkout -b experimental/patches experimental/master`

**Проверка после этого шага:**
- `git branch` в submodule показывает `* experimental/patches`
- `git log --oneline -1` — тот же коммит `c71ab45`

**Оценка:** 0 строк, ~1 мин

---

## Шаг 2: Патч ilu0.hpp — fallback D[i]=1 при zero pivot

**Цель:** заменить `precondition` crash на fallback `D[i]=1` для вырожденных строк ILU0

**Файлы:** `amgcl/amgcl/relaxation/ilu0.hpp` (строка 156)

**Контекст:**
При Sw=0 блок Якобиана водной фазы вырожден: dkrw/dSw → 0, вся строка нулевая. При скалярном CPR decoupling нулевая строка попадает в ILU0, где диагональный элемент D[i] = 0 → `precondition` вызывает exception "Zero pivot in ILU". На тонких сетках (41×41) ILU0 factorization может обнулить pivot через elimination `D[i] -= tl * U[k]`, даже если начальные диагонали ненулевые.

Fallback `D[i]=1` превращает вырожденную строку в identity — это корректно, т.к. для нулевой строки ILU0 не должен модифицировать решение. Convergence деградирует, но солвер не crash-ит.

**Изменения (старый → новый код):**

До (строка 156):
```cpp
                    precondition(!math::is_zero((*D)[i]), "Zero pivot in ILU");
```

После:
```cpp
                    if (math::is_zero((*D)[i]))
                        (*D)[i] = static_cast<value_type>(1);
```

**Что сделать:**
1. Отредактировать файл `amgcl/amgcl/relaxation/ilu0.hpp`
2. Коммит в submodule с сообщением, описывающим проблему:

```
fix: fallback D[i]=1 при zero pivot в ILU0

Проблема: при Sw=0 (чистая нефть) блок Якобиана по насыщенности
вырожден (dkrw/dSw → 0). CPR decoupling передаёт нулевую строку
в ILU0 factorization → D[i]=0 → exception "Zero pivot in ILU".

На тонких сетках (41×41) ILU0 может обнулить pivot через
elimination (D[i] -= tl * U[k]) даже при ненулевых начальных
диагоналях (ILU-produced zero pivots).

Решение: fallback D[i]=1 вместо exception. Identity для
вырожденной строки корректен — ILU0 не должен модифицировать
решение для нулевых строк. Convergence деградирует, но солвер
продолжает работу.

Условия срабатывания:
- Sw=0 (начальная насыщенность) → zero pivot в pressure equation
- Тонкие сетки (41×41+) → ILU-produced zero pivots
- Скалярный CPR (не блочный AMG)

Связано: BUG-002 проявление 1
```

**Проверка после этого шага:**
- `git diff HEAD~1` в submodule показывает замену `precondition` на `if/fallback`
- Файл `ilu0.hpp` строка 156: `if (math::is_zero((*D)[i]))` вместо `precondition`

**Оценка:** ~2 строки, ~3 мин

---

## Шаг 3: Патч cpr.hpp — fallback d=1 при zero pivot в block-LU

**Цель:** заменить `assert` crash на fallback `d=1` для singular 2×2 блоков в CPR

**Файлы:** `amgcl/amgcl/preconditioner/cpr.hpp` (строка 522)

**Контекст:**
CPR decoupling извлекает pressure-часть из 2×2 блока Якобиана [∂F/∂P, ∂F/∂Sw] через block-LU factorization. При Sw=0 блок singular → diagonal element `d = A[k*B+k] = 0` → `assert(!math::is_zero(d))` вызывает abort() в Debug, а в Release — деление на ноль → NaN → GMRES diverges.

Fallback `d=1` пропускает LU-факторизацию singular блока: pressure restrictor получает identity для вырожденного блока. Это безопасно — для нулевого водного уравнения pressure contribution не определён, и identity — разумный default.

**Изменения (старый → новый код):**

До (строки 521-522):
```cpp
                scalar_type d = A[k*B+k];
                assert(!math::is_zero(d));
```

После:
```cpp
                scalar_type d = A[k*B+k];
                if (math::is_zero(d)) {
                    d = static_cast<scalar_type>(1);
                    A[k*B+k] = d;
                }
```

⚠️ Запись `A[k*B+k] = d` **обязательна**: `d` — локальная копия, обратный ход (строка 543: `y[i] /= A[i*B+i]`) читает из массива `A`, не из `d`. Без записи — деление на ноль в обратном ходе.

**Что сделать:**
1. Отредактировать файл `amgcl/amgcl/preconditioner/cpr.hpp`
2. Коммит в submodule с сообщением:

```
fix: fallback d=1 при zero pivot в CPR block-LU

Проблема: CPR decoupling извлекает pressure equation из 2×2
блока Якобиана через block-LU factorization. При Sw=0 блок
singular: ∂F_water/∂Sw = 0, ∂F_water/∂P = 0 → diagonal d=0.

assert(!math::is_zero(d)) вызывает abort() в Debug.
В Release — деление на ноль → NaN → GMRES diverges.

Решение: fallback d=1 с обязательной записью A[k*B+k]=d.
d — локальная копия; обратный ход (строка 543: y[i] /= A[i*B+i])
читает диагональ из массива A, не из d. Без записи — деление
на ноль в обратном ходе upper triangular solve.

Для singular блока LU-факторизация невозможна; identity —
разумный default для pressure restrictor, т.к. water equation
при Sw=0 не содержит pressure contribution.

Условия срабатывания:
- Sw=0 (чистая нефть) → singular 2×2 блок
- Скалярный CPR с block_size=2

Связано: BUG-002 проявление 3
```

**Проверка после этого шага:**
- `git diff HEAD~1` в submodule показывает замену `assert` на `if/fallback` + запись `A[k*B+k] = d`
- Файл `cpr.hpp` строка 522: `if (math::is_zero(d))` вместо `assert`
- Файл `cpr.hpp`: `A[k*B+k] = d;` — обязательная запись обратно в массив

**Оценка:** ~4 строки, ~3 мин

---

## Шаг 4: Запушить ветку experimental/patches в форк

**Цель:** сохранить патчи в remote-репозитории

**Что сделать:**
1. `cd amgcl`
2. `git push -u origin experimental/patches`

**Проверка после этого шага:**
- `git branch -r` в submodule содержит `origin/experimental/patches`
- На GitHub `ArturSalamatin/vcpkg-amgcl` видна ветка `experimental/patches` с 2 коммитами поверх `experimental/master`

**Оценка:** ~1 мин

---

## Шаг 5: Переключить submodule GDM на experimental/patches

**Цель:** GDM использует пропатченный amgcl, все тесты зелёные

**Файлы:** `amgcl` (submodule pointer в основном репо)

**Контекст:**
Submodule `amgcl` уже на правильной ветке (`experimental/patches`) после шагов 1-3. Нужно зафиксировать это в основном репо GDM: `git add amgcl` запишет новый commit hash.

Примечание: `.gitmodules` пока содержит `branch = experimental/master`. Это нормально — `branch` в `.gitmodules` влияет только на `git submodule update --remote`. Для DEBT-052 не меняем — submodule привязан к конкретному коммиту, а не к branch tip.

⚠️ Если кто-то выполнит `git submodule update --remote`, submodule откатится на `experimental/master` (без патчей). Обычно используется `git submodule update --init` (без `--remote`), что безопасно — берёт коммит из tree, не из remote branch tip.

**Что сделать:**
1. `cd ..` (вернуться в корень GDM)
2. `git add amgcl`
3. Сборка: `cmake --build build --config Release`
4. Тесты: `ctest --test-dir build -C Release --output-on-failure`

**Проверка после этого шага:**
- `git submodule status` — amgcl указывает на коммит из `experimental/patches` (не `c71ab45`)
- Сборка без ошибок
- **294/294 тестов зелёные** (Grid convergence 41×41 проходит)

**Подводные камни:**
- ⚠️ CMake может использовать кэш старых headers. Если тест всё ещё падает после патча — полная пересборка: `Remove-Item -Recurse -Force build; cmake -B build -S . -G "Visual Studio 17 2022"; cmake --build build --config Release`

**Оценка:** ~5 мин (включая пересборку)

---

## Критерии завершения DEBT-052

- [ ] Ветка `experimental/patches` существует в форке `ArturSalamatin/vcpkg-amgcl`
- [ ] Ветка содержит ровно 2 коммита поверх `experimental/master`:
  - fallback D[i]=1 в ilu0.hpp
  - fallback d=1 в cpr.hpp
- [ ] Commit messages содержат описание проблемы, условия срабатывания, физическую причину
- [ ] Submodule в GDM указывает на HEAD ветки `experimental/patches`
- [ ] Сборка Release без ошибок
- [ ] **294/294 тестов зелёные** (включая Grid convergence 41×41)
- [ ] Vault обновлён: статус DEBT-052, приоритеты
- [ ] GitHub issue #19 прокомментирован

## Чеклист подводных камней

- [x] **Call sites:** патчи внутри amgcl, GDM код не затрагивается
- [x] **Потокобезопасность:** не затрагивается — ILU0 и CPR block-LU однопоточные внутри amgcl
- [x] **Зависимости сборки:** include path не меняется (`${CMAKE_SOURCE_DIR}/amgcl`)
- [x] **Обратная совместимость API:** API amgcl не меняется, только внутреннее поведение
- [x] **Тесты:** 293 теста не зависят от патчей; 1 тест (Grid convergence 41×41) должен начать проходить
- [x] **Производительность:** fallback `D[i]=1` может увеличить число итераций GMRES для near-singular случаев, но не влияет на нормальную работу (Sw > 0)
- [x] **Связь с другими задачами:** DEBT-053 (SolverFactory) и FEAT-010/011 зависят от DEBT-051, не от DEBT-052. Ветка `experimental/patches` не конфликтует с будущей `experimental/solvers`
- [x] **Третий assert в amgcl:** `amgcl/detail/inverse.hpp:66` содержит `assert(!math::is_zero(d))` — НЕ на нашем code path. Используется только в `deflated_solver`, `builtin backend`, `chebyshev`. CPR использует собственную `invert()` в `cpr.hpp:515`, а не `detail/inverse.hpp`. Патч не нужен
- [x] **`git submodule update --remote`:** `.gitmodules` содержит `branch = experimental/master`, но submodule будет на `experimental/patches`. `--remote` откатит на upstream. Безопасно: `--init` (без `--remote`) берёт коммит из tree

## Связанные заметки

- [[технический долг]] — DEBT-052
- [[локальные патчи AMGCL для GDM]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[debt-051 amgcl-submodule-and-solvers]] — общий план
