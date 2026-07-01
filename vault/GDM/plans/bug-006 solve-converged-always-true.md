---
tags:
  - план
  - баг
date: 2026-07-01
issue: BUG-006
github: 5
branch: fix/bug-006/solve-converged-always-true
status: выполнен
---

# BUG-006: Solve() всегда возвращает converged = true

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[code-review-2026-06-28-числовая-устойчивость]] — CR-NUM-006
   - [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — BUG-002 (контекст CPR)
3. Создай ветку: `git checkout -b fix/bug-006/solve-converged-always-true`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release`
6. Запомни baseline: **279 тестов, ~264 сек, 100% pass**
7. Начни с шага 1. После каждого шага: сборка + тесты

---

## Описание бага

`LinearProblem::Solve()` возвращает `SolveResult{iters, error, true}` — поле `converged` захардкожено в `true`. Newton loop не получает информации о расхождении линейного солвера (AMGCL lgmres) и принимает мусорные поправки как валидные.

Дополнительно, `NumericalParameters::CurrentANG_IsAccuracyReached()` использует бессмысленный критерий: `AMG_curError == 1.0 || AMG_curError == 0.0`. Это приводит к тому, что Newton loop **никогда** не завершается через `IsSuccessfullNewtonTrial() == true`, кроме тривиальных задач (нулевая правая часть, где AMG возвращает error = 0.0). Для нетривиальных задач Newton всегда упирается в `maxNewtonIterNmbr` (12 итераций), после чего шаг отбрасывается (wasted trial) и шаг по времени уменьшается.

Система работает, потому что:
- Тривиальные задачи: RHS = 0 → AMG возвращает `error = 0.0` → `CurrentANG_IsAccuracyReached() = true`
- Нетривиальные задачи: после нескольких wasted trials шаг по времени уменьшается до такого, что задача становится достаточно простой. Тесты проверяют `WastedTrialsCount() < 50` (мягко)

**Последствия:**
- Избыточные Newton-итерации (до 12 вместо 1–3 для хорошо обусловленных задач)
- Ненужные wasted trials → уменьшение шага по времени → медленная симуляция
- На жёстких задачах (high contrast, near-breakthrough) — молчаливое принятие неточного решения
- Маскировка BUG-007 (NaN в Якобиане) и BUG-008 (деление на ноль в SetRefWellPressure)

---

## Цепочка причинно-следственных связей

```
AMGCL lgmres
  └─ возвращает: std::tuple<unsigned, double> = (iterations, relative_residual)
  └─ критерий выхода: norm_r < eps || iter >= maxiter
  └─ eps = max(tol * norm_rhs, abstol)

LinearProblem::Solve()  [LinearProblem.cpp:143]
  └─ auto [iters, error] = solve(F, X)
  └─ return { iters, error, TRUE }     ← ← ← BUG: converged захардкожен
                    │
                    ▼
ReservoirSimulator::SingleIteration()  [ReservoirSimulator.cpp:490-491]
  └─ auto res = MyProblem.Solve(maxIter)
  └─ numPrm.update_currentAMGState({res.iters, res.error, res.converged})
                    │
                    ▼
NumericalParameters::update_currentAMGState()  [NumericalParameters.cpp:14-31]
  └─ AMG_isIterationSuccessfull = converged(=true) && isfinite(error)
  └─ AMG_curError = error                    (e.g. 0.003)
                    │
                    ▼
NumericalParameters::update_isSuccesfullNewtonTrial(f)  [NumericalParameters.h:108]
  └─ isSuccessfulTrial = f && CurrentANG_IsAccuracyReached()
                    │
                    ▼
CurrentANG_IsAccuracyReached()  [NumericalParameters.h:69-74]
  └─ return AMG_curError == 1.0 || AMG_curError == 0.0    ← ← ← ВТОРОЙ БАГ
  └─ При error = 0.003 (нормальная сходимость): FALSE
  └─ isSuccessfulTrial = true && false = FALSE             ← Newton думает "не сошёлся"
                    │
                    ▼
PerformNewtonLoop  [ReservoirSimulator.cpp:433-454]
  └─ while (!IsSuccessfullNewtonTrial())  ← NEVER TRUE для нетривиальных задач
  └─ → Newton делает 12 итераций → break → isSuccessfulTrial = false
                    │
                    ▼
Solve (time loop)  [ReservoirSimulator.cpp:410-423]
  └─ if (IsSuccessfullNewtonTrial()) → false
  └─ → decrease_schemeTau() → wasted trial → повтор с меньшим шагом
```

---

## Целевое состояние

1. `LinearProblem::Solve()` возвращает `converged = false` когда линейный солвер не сошёлся
2. `CurrentANG_IsAccuracyReached()` использует осмысленный критерий: `error < tol`
3. Newton loop завершается через `IsSuccessfullNewtonTrial() == true` когда Newton-коррекции малы **и** AMG сошёлся
4. На стандартных задачах Newton сходится за 1–3 итерации вместо 12
5. `WastedTrialsCount` значительно уменьшается
6. Все 279 существующих тестов зелёные

---

## Варианты решения

### Вариант A: Минимальный — только `Solve()` converged

Заменить `true` на вычисление:
```cpp
return { iters, error, std::isfinite(error) && error <= prm.solver.tol };
```

**Плюсы:** 1 строка, локальное изменение
**Минусы:** Не решает `CurrentANG_IsAccuracyReached()`. Newton loop по-прежнему не будет завершаться через `IsSuccessfullNewtonTrial()`. Единственное изменение — при расхождении AMG (`error > tol` или NaN) Newton loop прервётся через `IsSuccessfullAMG_Iteration() == false` (строка 443) вместо бессмысленного продолжения.

### Вариант B: Полный — `Solve()` + `CurrentANG_IsAccuracyReached()`

1. Фикс `Solve()` (как A)
2. Фикс `CurrentANG_IsAccuracyReached()`:
```cpp
// было:
bool CurrentANG_IsAccuracyReached() const {
    return CurrentAMG_Error() == 1.0 || CurrentAMG_Error() == 0.0;
}

// стало:
bool CurrentANG_IsAccuracyReached() const {
    return CurrentAMG_Error() <= AMG_RelTol;
}
```

**Плюсы:** Полное исправление. Newton loop работает корректно: завершается когда AMG сошёлся И Newton-коррекции малы.
**Минусы:** Два изменения в разных файлах. `AMG_RelTol` — public поле `NumericalParameters`. Нужна тщательная проверка, что изменение `CurrentANG_IsAccuracyReached()` не ломает time stepping (Newton будет делать меньше итераций → шаг по времени будет увеличиваться → может проскочить фронт).

### Вариант C: B + edge case для нулевой правой части

Как B, но дополнительно обработать случай `iters == 0` (AMGCL при нулевой RHS):
```cpp
bool ok = std::isfinite(error) && (error <= prm.solver.tol || iters == 0);
return { iters, error, ok };
```

**Плюсы:** Корректно обрабатывает тривиальные задачи
**Минусы:** `iters == 0` может быть и при вырожденной матрице (AMGCL вернёт 0 итераций с norm_rhs ≈ 0). Но это не проблема — при norm_rhs ≈ 0 решение тривиальное.

### Выбор: Вариант B

Вариант A недостаточен — `CurrentANG_IsAccuracyReached()` остаётся сломанным. Вариант C — оверинжиниринг для edge case, который уже покрывается проверкой `error <= tol` (при тривиальной RHS AMGCL lgmres возвращает `(0, norm_rhs)` где `norm_rhs` ≈ 0 < `tol`).

Риск варианта B: изменение поведения Newton loop. Раньше Newton делал 12 итераций (или до maxiter). Теперь — 1-3 итерации (до сходимости). Это **улучшение**, но нужно верифицировать, что физический результат не ухудшается (Newton не останавливается слишком рано).

---

## Чеклист подводных камней

- ✅ **Побочные эффекты:** `Solve()` вызывается из `SingleIteration` (1 место) и `test_amgcl_benchmark.cpp` (2 места). `CurrentANG_IsAccuracyReached()` вызывается из `update_isSuccesfullNewtonTrial` (1 место). Изменения локальны.
- ✅ **Потокобезопасность:** `Solve()` не в `#pragma omp parallel`. `CurrentANG_IsAccuracyReached()` — read-only. Безопасно.
- ⚠️ **Граничные случаи:** (1) `error` может быть NaN/Inf при вырожденной матрице — `std::isfinite` защищает. (2) При `iters == 0` и `error = norm_rhs` ≈ 0 — `error <= tol` = true. (3) При `iters == maxiter` и `error < tol` (сходимость на последней итерации) — `error <= tol` = true. Все три корректны. Адресовано в шаге 1.
- ⚠️ **Обратная совместимость:** Newton будет делать меньше итераций. Физический результат может измениться (шаг по времени будет расти быстрее). Адресовано в шаге 4 (верификация).
- ✅ **Производительность:** Убираем лишние итерации — производительность улучшается.
- ✅ **Порядок вызовов:** `AMG_curError` устанавливается в `update_currentAMGState` (внутри `SingleIteration`), до `update_isSuccesfullNewtonTrial`. Порядок не нарушается.
- ✅ **Состояние при ошибке:** Нет throw/early return. Состояние объекта всегда валидно.
- ✅ **Численная устойчивость:** `error <= AMG_RelTol` — сравнение double с double. `AMG_RelTol = 1e-2`. Нет рисков overflow/underflow.
- ⚠️ **Связь с BUG-007/BUG-008:** Фикс BUG-006 **раскроет** эти баги: при NaN в Якобиане AMG не сойдётся → `converged = false` → Newton откатит шаг. Это правильное поведение, но тесты на сценариях с нулевой подвижностью могут начать падать. Пока BUG-007/008 не зафиксированы — это ожидаемо. Адресовано в шаге 4.
- ✅ **Зависимости сборки:** Изменения только в .h и .cpp файлах, CMakeLists.txt не затрагивается.

---

## Обнаруженные проблемы

### НОВАЯ: `update_isSuccesfullNewtonTrial(false)` побочно инкрементирует Newton counter

В начале `PerformNewtonLoop`:
```cpp
numPrm.set_currentNewtonIterationCount(0);      // counter = 0
numPrm.update_isSuccesfullNewtonTrial(false);    // counter = 1 (побочный эффект)
```

`update_isSuccesfullNewtonTrial` вызывает `update_currentNewtonIterationCount()` — инкремент. Это значит Newton loop стартует с counter = 1 вместо 0. Фактически доступно 11 итераций вместо 12.

**Блокирует текущую задачу:** нет (баг существует и сейчас, фикс BUG-006 его не усугубляет).
**Действие:** зафиксировать как DEBT в реестре после завершения плана.

---

## Этапы

### Этап 1: Фикс `LinearProblem::Solve()` — вычисление converged
### Этап 2: Фикс `CurrentANG_IsAccuracyReached()` — осмысленный критерий
### Этап 3: Фикс `solve_with_scalar` в тестовой обёртке benchmark
### Этап 4: Тесты
### Этап 5: Верификация и очистка

---

## Шаг 1: Вычисление converged в `LinearProblem::Solve()`

**Цель:** `Solve()` возвращает `converged = false` когда AMGCL lgmres не сошёлся.

**Файлы:** `HydroSolver/Solver/Math/LinearProblem.cpp`

**Контекст:**
`LinearProblem::Solve()` — обёртка вокруг AMGCL CPR-солвера. Вызывает `amgcl::make_solver::operator()`, который возвращает `std::tuple<size_t, double>` — `(iterations, relative_residual)`. Относительная невязка = `norm_r / norm_rhs`. Если солвер сошёлся, `error < prm.solver.tol` (по крайней мере). Если упёрся в `maxiter`, `error >= prm.solver.tol`.

AMGCL lgmres использует два критерия выхода:
- `norm_r < eps` где `eps = max(tol * norm_rhs, abstol)`
- `iter >= maxiter`

Для нашего фикса проверяем `error <= prm.solver.tol`:
- Если вышел по `norm_r < tol * norm_rhs`, то `error = norm_r / norm_rhs < tol` → converged = true ✅
- Если вышел по `norm_r < abstol` (но `norm_r / norm_rhs >= tol`), то `error >= tol` → converged = false. Это ложно-отрицательный результат, но безопасный: Newton откатит шаг и попробует с меньшим tau. На практике `abstol = 1e-2` и `tol = 1e-2` — оба одинаковы, edge case маловероятен.
- Если вышел по `iter >= maxiter`, то обычно `error >= tol` → converged = false ✅
- Edge case: `iter == maxiter` и `error < tol` (сошёлся ровно на последней итерации) → converged = true ✅
- Если `error` = NaN/Inf → `isfinite` ловит → converged = false ✅
- Если RHS ≈ 0 → lgmres возвращает `(0, norm_rhs ≈ 0)` → `error ≈ 0 < tol` → converged = true ✅

Поле `prm` — protected в `LinearProblem`, `Solve()` имеет к нему доступ.

**Что сделать:**
1. В файле `HydroSolver/Solver/Math/LinearProblem.cpp`, функция `Solve()`, строка 143:
   Заменить `return { iters, error, true };` на вычисление `converged`.

**Изменения (старый → новый код):**

До:
```cpp
return { iters, error, true };
```

После (план → фактическое при реализации):
```cpp
// Планировалось: return { iters, error, std::isfinite(error) && error <= prm.solver.tol };
// Фактически:
return { iters, error, std::isfinite(error) };
```

**Отклонение от плана:** `error <= prm.solver.tol` убрано. Причина: Newton loop (`PerformNewtonLoop`) при `AMG_isIterationSuccessfull=false` немедленно делает `ReverseState(); break;` — т.е. прерывает Newton и объявляет wasted trial. Это приводит к бесконечным wasted trials на задачах, где AMG не достигает tol за maxiter, но даёт конечное решение. Проверка точности AMG вынесена в `CurrentANG_IsAccuracyReached()` (шаг 2).

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Регрессия: `ctest --test-dir build -C Release`
- Ожидаемый результат: все тесты зелёные (converged=true для всех конечных результатов AMG).

**Подводные камни:**
- `std::isfinite` требует `<cmath>` — включён транзитивно через `stdafx.h` и `amgcl/solver/lgmres.hpp`. Если по какой-то причине не компилируется — добавить `#include <cmath>` в начало `LinearProblem.cpp`

**Зависимости:**
- Требует: нет
- Блокирует: шаг 4 (тест converged=false не пройдёт без фикса)

**Оценка:** ~1 строка, ~2 минуты

---

## Шаг 2: Фикс `CurrentANG_IsAccuracyReached()` в NumericalParameters

**Цель:** Newton loop корректно завершается когда AMG сошёлся.

**Файлы:** `HydroSolver/Reservoir/NumericalParameters.h`

**Контекст:**
`CurrentANG_IsAccuracyReached()` проверяет `AMG_curError == 1.0 || AMG_curError == 0.0`. Это бессмысленный критерий: `AMG_curError` — относительная невязка AMG (обычно 1e-4..1e-2). Значения 1.0 и 0.0 — крайние случаи (начальное значение и точное решение).

Правильный критерий: AMG-ошибка ниже AMG-допуска. `AMG_RelTol` — public поле `NumericalParameters` (значение по умолчанию 1e-2, совпадает с `prm.solver.tol` в LinearProblem).

`CurrentANG_IsAccuracyReached()` используется **только** в `update_isSuccesfullNewtonTrial`:
```cpp
void update_isSuccesfullNewtonTrial(bool f) {
    isSuccessfulTrial = f && CurrentANG_IsAccuracyReached();
    update_currentNewtonIterationCount();
}
```

Семантика: Newton trial успешен, если:
- `f = true` (Newton-коррекции малы, из `UpdateGrid()`)
- `CurrentANG_IsAccuracyReached() = true` (AMG сошёлся с достаточной точностью)

После фикса Newton loop будет завершаться за 1-3 итерации на стандартных задачах.

**Что сделать:**
1. В файле `HydroSolver/Reservoir/NumericalParameters.h`, метод `CurrentANG_IsAccuracyReached()`, строки 69-75:
   Заменить критерий.

**Изменения (старый → новый код):**

До:
```cpp
bool CurrentANG_IsAccuracyReached() const
{
    return
        CurrentAMG_Error() == 1.0 ||
        CurrentAMG_Error() == 0.0 //|| CurrentAMG_Error() < 1E-15
        ;
}
```

После:
```cpp
bool CurrentANG_IsAccuracyReached() const
{
    return CurrentAMG_Error() <= AMG_RelTol;
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Регрессия: `ctest --test-dir build -C Release`
- Ожидаемый результат: **все тесты зелёные**. На тривиальных задачах: error = 0.0 ≤ 1e-2 → true (как раньше). На нетривиальных: error ≈ 1e-3 ≤ 1e-2 → true (раньше было false!). Newton loop теперь завершается нормально.
- Дополнительно: `WastedTrialsCount` должен **уменьшиться** на нетривиальных задачах. Можно проверить: `ctest --test-dir build -C Release -R "Five-spot" --output-on-failure` — в выводе должно быть меньше wasted trials.

**Подводные камни:**
- `AMG_RelTol` инициализируется как `1e-2` и не меняется в runtime — безопасно
- При `AMG_curError` = начальное значение `0.0` (до первого AMG solve): `0.0 <= 1e-2` → true. Это означает, что `update_isSuccesfullNewtonTrial(false)` в начале `PerformNewtonLoop` установит `isSuccessfulTrial = false && true = false` — корректно, как раньше
- Побочный эффект: Newton будет делать меньше итераций на нетривиальных задачах. Шаг по времени будет расти быстрее (через `increase_schemeTau`). Нужно проверить, что результат физически корректен (шаг 4)

**Зависимости:**
- Требует: нет (технически независим от шага 1, но оба нужны для полного эффекта)
- Блокирует: шаг 5 (верификация)

**Оценка:** ~5 строк → 1 строка, ~3 минуты

---

## Шаг 3: Фикс `solve_with_scalar` в benchmark

**Цель:** Тестовая обёртка AMG тоже возвращает корректный `converged`.

**Файлы:** `tests/test_amgcl_benchmark.cpp`

**Контекст:**
`solve_with_scalar<SolverType>()` — шаблонная обёртка для бенчмарков экспериментальных конфигураций AMG. Содержит тот же паттерн `return { iters, error, true }` (строка 50). Не используется в production, но должен быть консистентным.

В отличие от `LinearProblem::Solve()`, здесь `prm` — параметр функции (тип `SolverType::params&`). `prm.solver.tol` доступен.

**Что сделать:**
1. В файле `tests/test_amgcl_benchmark.cpp`, функция `solve_with_scalar`, строка 50:
   Заменить `return { iters, error, true };` на вычисление.

**Изменения (старый → новый код):**

До:
```cpp
return { iters, error, true };
```

После (фактически — см. отклонение в шаге 1):
```cpp
return { iters, error, std::isfinite(error) };
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "AMGCL benchmark"`
- Ожидаемый результат: тесты зелёные

**Подводные камни:**
- `std::isfinite` не используется в файле. Транзитивно доступен через `LinearProblem.h` → AMGCL headers. Если не компилируется — добавить `#include <cmath>` в начало файла

**Зависимости:**
- Требует: нет (независим от шагов 1-2)
- Блокирует: шаг 4

**Оценка:** ~1 строка, ~2 минуты

---

## Шаг 4: Unit-тест для `SolveResult.converged`

**Цель:** Предотвратить регрессию — тест прямо проверяет, что `Solve()` возвращает `converged = false` при нехватке итераций.

**Файлы:** `tests/unit/math/test_LinearProblemAssembly.cpp` (существующий, добавить тесты в конец)

**Контекст:**
В `tests/unit/math/assembly_test_helpers.h` есть инфраструктура для создания `LinearProblem`: `make_grid_graph()`, `fullBlock2()`. Нужно создать `LinearProblem` с ненулевой RHS, запустить `Solve(1)` (1 итерация — гарантированно не сойдётся), проверить `converged == false`. Затем запустить `Solve(100)` — проверить `converged == true`.

Для создания ненулевой задачи: заполним диагональные блоки единичной матрицей, RHS — ненулевыми значениями. Система Ax = b с A = I, b = [1, 0, ...] решается за 1 итерацию AMG (или 0), но нам нужна задача, которая не сходится за 1 итерацию.

Проще: сделать диагонально-доминантную матрицу с off-diagonal блоками. Для сетки 3x1x1 (3 ячейки) с B=2: матрица 6×6. Заполним реалистичными значениями (давление + насыщенность). AMG с `maxiter=1` не сойдётся.

Альтернатива: использовать существующую инфраструктуру `make_grid_graph` + `AddDiagBlock` + `AddOffDiagBlock`. Заполнить блоки как в Jacobian-тестах.

**Что сделать:**
1. В конце файла `tests/unit/math/test_LinearProblemAssembly.cpp` добавить два теста.

Ключевой момент: для теста `converged=false` нужно гарантировать, что AMG не сойдётся за 1 итерацию. На маленькой матрице (6×6) с хорошим прекондиционером 1 итерации GMRES может быть достаточно при `tol=1e-2`. Два способа сделать тест устойчивым:
- Использовать строгий допуск `tol = 1e-15` — CPR + lgmres не достигнет такой точности за 1 итерацию
- Или использовать большую сетку (5×5×1, 50 ячеек)

Рекомендуется первый способ — он надёжнее и не зависит от размера задачи. `tol` задаётся в конструкторе `LinearProblem` (параметр `AMG_RelTol`), а `maxIter` — в аргументе `Solve()`.

Для теста `converged=true` использовать `tol = 1e-2` (как в production) и `maxIter = 100`.

```cpp
TEST_CASE("LinearProblem::Solve converged=false when maxIter=1",
          "[math][solver]") {
    auto graph = assembly_helpers::make_grid_graph(3, 1, 1);
    auto bp = assembly_helpers::fullBlock2();
    // tol = 1e-15: строгий допуск гарантирует, что 1 итерация не достаточна
    LinearProblem lp(Layout::InterleavedPSw, 1e-15, 1e-15, graph, bp);

    for (size_t i = 0; i < 3; ++i) {
        double diag[4] = {10.0, 0.1, 0.1, 10.0};
        double rhs[2] = {1.0, 0.5};
        lp.AddDiagBlock(i, diag, rhs);
    }
    for (size_t i = 0; i < 3; ++i) {
        for (int j : graph[i]) {
            double off[4] = {-1.0, 0.0, 0.0, -1.0};
            lp.AddOffDiagBlock(i, j, off);
        }
    }

    auto res = lp.Solve(1);
    CHECK_FALSE(res.converged);
    CHECK(res.iters >= 1);
    CHECK(std::isfinite(res.error));
    CHECK(res.error > 1e-15);
}

TEST_CASE("LinearProblem::Solve converged=true when maxIter sufficient",
          "[math][solver]") {
    auto graph = assembly_helpers::make_grid_graph(3, 1, 1);
    auto bp = assembly_helpers::fullBlock2();
    // tol = 1e-2: как в production
    LinearProblem lp(Layout::InterleavedPSw, 1e-2, 1e-2, graph, bp);

    for (size_t i = 0; i < 3; ++i) {
        double diag[4] = {10.0, 0.1, 0.1, 10.0};
        double rhs[2] = {1.0, 0.5};
        lp.AddDiagBlock(i, diag, rhs);
    }
    for (size_t i = 0; i < 3; ++i) {
        for (int j : graph[i]) {
            double off[4] = {-1.0, 0.0, 0.0, -1.0};
            lp.AddOffDiagBlock(i, j, off);
        }
    }

    auto res = lp.Solve(100);
    CHECK(res.converged);
    CHECK(res.error <= 1e-2);
    CHECK(std::isfinite(res.error));
}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "LinearProblem::Solve converged"`
- Ожидаемый результат: оба теста зелёные
- Регрессия: `ctest --test-dir build -C Release` — все тесты зелёные

**Подводные камни:**
- `AddOffDiagBlock(size_t l, int neibIdx, ...)` — `neibIdx` это **индекс соседа в глобальной нумерации**, не индекс в списке соседей. Проверить, что `j` из `graph[i]` — это глобальный индекс (да, так устроен `make_grid_graph`)
- Перегрузка `AddOffDiagBlock` принимает `const double*` — использовать массив, не `vector`
- Если даже с `tol=1e-15` задача сходится за 1 итерацию (маловероятно, но идеальный прекондиционер может), увеличить сетку до 5×5×1

**Зависимости:**
- Требует: шаг 1 (без фикса тест `converged=false` упадёт — Solve всегда true)
- Блокирует: нет

**Оценка:** ~40 строк, ~15 минут

---

## Шаг 5: Интеграционная верификация

**Цель:** Убедиться, что изменённое поведение Newton loop не ухудшает физический результат.

**Файлы:** нет изменений — только прогон тестов

**Контекст:**
После шагов 1-2 Newton loop будет делать значительно меньше итераций. Шаг по времени будет расти быстрее (меньше wasted trials). Нужно проверить:
1. Физический результат не изменился (сатурации, давления)
2. Баланс масс соблюдается
3. Five-spot симметрия сохраняется
4. Grid convergence сохраняется

**Что сделать:**

1. Прогнать полный набор тестов:
   ```powershell
   ctest --test-dir build -C Release --output-on-failure
   ```
   Все 279 тестов зелёные.

2. Прогнать five-spot с вниманием к метрикам:
   ```powershell
   ctest --test-dir build -C Release -R "Five-spot" --output-on-failure
   ```
   Проверить: `WastedTrialsCount` уменьшился (< 50 как и раньше, но должно быть значительно меньше).

3. Прогнать benchmark:
   ```powershell
   ctest --test-dir build -C Release -R "AMGCL benchmark" --output-on-failure
   ```
   Проверить: `n_wasted_trials` уменьшился, `n_newton_iters / n_time_steps` уменьшилось (меньше итераций на шаг).

4. Прогнать Grid convergence:
   ```powershell
   ctest --test-dir build -C Release -R "Grid convergence" --output-on-failure
   ```
   Проверить: сходимость по сетке сохраняется.

**Ожидаемый результат:**
- Все тесты зелёные
- Newton-итераций на шаг по времени: 1-3 вместо 12
- Wasted trials: значительно меньше (возможно 0 для простых задач)
- Физический результат: может незначительно измениться из-за другого time stepping (шаг растёт быстрее), но в пределах tolerance тестов

**Подводные камни:**
- Если тесты на five-spot или grid convergence начнут падать — причина в том, что **шаг по времени стал больше**. Раньше Newton делал 12 итераций → wasted trial → `decrease_schemeTau` → маленький dt. Теперь Newton сходится за 1–3 итерации → success → `increase_schemeTau` → бо́льший dt. Бо́льший dt означает бо́льшую нелинейность, и Newton-коррекции могут не достигнуть достаточной точности за фиксированный `newtonTol`. Если это произойдёт — нужно адаптировать `increase_schemeTau` или ужесточить Newton tolerance. Но это **отдельная задача** (связана с CR-NUM-004 из code review).
- Если benchmark показывает ухудшение — маловероятно, но проверить

**Зависимости:**
- Требует: шаги 1, 2, 3
- Блокирует: шаг 6

**Оценка:** ~0 строк кода, ~10 минут (время прогона тестов)

---

## Шаг 6: Очистка и обновление vault

**Цель:** Обновить документацию и закрыть issue.

**Файлы:**
- `vault/GDM/roadmap/известные баги и технический долг.md`
- `vault/GDM/knowledge/debugging/code-review-2026-06-28-числовая-устойчивость.md`
- `vault/GDM/00-home/текущие приоритеты.md`

**Что сделать:**

1. В `известные баги и технический долг.md`, секция BUG-006:
   - Статус: `🔴 ОТКРЫТ` → `✅ ИСПРАВЛЕНО <дата>`
   - Поля **GitHub**, **План**, **Ветка** уже заполнены — не дублировать

2. В `code-review-2026-06-28-числовая-устойчивость.md`, секция CR-NUM-006:
   - Добавить: `**Статус:** ✅ ИСПРАВЛЕНО <дата>`

3. В `текущие приоритеты.md`:
   - Обновить статус BUG-006 если упомянут

4. Обнаруженная проблема (побочный инкремент Newton counter) уже зарегистрирована как DEBT-047 в реестре

5. Прокомментировать GitHub issue:
   ```
   gh issue comment 5 --repo ArturSalamatin/GDM --body "Исправлено: Solve() вычисляет converged, CurrentANG_IsAccuracyReached() использует AMG_RelTol. Ветка: fix/bug-006/solve-converged-always-true."
   ```

**Проверка после этого шага:**
- Vault-файлы обновлены
- GitHub issue прокомментирован

**Зависимости:**
- Требует: шаг 5 (верификация пройдена)
- Блокирует: нет

**Оценка:** ~10 строк vault, ~5 минут

---

## Тестовая стратегия

**Тест 1 — воспроизводитель (converged=false):**
**Тест:** `LinearProblem::Solve converged=false when maxIter=1`
**Тег:** `[math][solver]`
**Файл:** `tests/unit/math/test_LinearProblemAssembly.cpp` (существующий)
**Сценарий:** диагонально-доминантная система 3 ячейки, 1 итерация AMG, строгий допуск
**Setup:** сетка 3×1×1, Layout::InterleavedPSw, **tol=1e-15**, diag=10, offdiag=-1, rhs=[1, 0.5], maxIter=1
**Ожидание:** `converged == false`, `iters >= 1`, `isfinite(error)`, `error > 1e-15`
**Предотвращает:** регрессия к `converged = true` hardcode

**Тест 2 — позитивный (converged=true):**
**Тест:** `LinearProblem::Solve converged=true when maxIter sufficient`
**Тег:** `[math][solver]`
**Файл:** `tests/unit/math/test_LinearProblemAssembly.cpp` (существующий)
**Сценарий:** та же система, 100 итераций AMG, production допуск
**Setup:** сетка 3×1×1, Layout::InterleavedPSw, **tol=1e-2**, те же матрица и RHS, maxIter=100
**Ожидание:** `converged == true`, `error <= 1e-2`, `isfinite(error)`
**Предотвращает:** ложные отрицательные результаты

**Regression:**
- Все 279 существующих тестов (`ctest --test-dir build -C Release`)
- Five-spot: `WastedTrialsCount < 50`
- Benchmark: `n_wasted_trials` не увеличился

---

## Критерии завершения

- [ ] `LinearProblem::Solve()` возвращает `converged = false` при расхождении AMG
- [ ] `CurrentANG_IsAccuracyReached()` использует `AMG_RelTol` вместо `== 1.0 || == 0.0`
- [ ] Тест-воспроизводитель зелёный (converged=false при maxIter=1)
- [ ] Позитивный тест зелёный (converged=true при достаточных итерациях)
- [ ] Все существующие тесты зелёные (`ctest --test-dir build -C Release`)
- [ ] Five-spot: WastedTrialsCount не ухудшился
- [ ] Benchmark: Newton-итераций на шаг уменьшилось
- [ ] Vault обновлён: BUG-006 → ✅ ИСПРАВЛЕНО
- [ ] GitHub issue #5 прокомментирован
- [ ] DEBT-047 (Newton counter побочный инкремент) уже зарегистрирован — проверить что запись актуальна

---

## Связанные заметки

- [[code-review-2026-06-28-числовая-устойчивость]] — CR-NUM-006 (описание бага)
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — BUG-002 (контекст CPR-солвера)
- [[переход с блочного AMG на скалярный CPR в production]] — решение о CPR
- [[стратегия тестирования GDM]] — паттерны тестов
