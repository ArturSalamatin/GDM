---
tags:
  - промпт
  - производительность
  - amgcl
  - рефакторинг
date: 2026-06-21
---

# Сессия: шаги 1–4 — iluk в production, комбинации, reuse AMG, OpenMP

Одна сессия. Четыре шага последовательно. После каждого — сборка, тесты, коммит.

## Предварительно

Прочитай `vault/GDM/00-home/текущие приоритеты.md` и `vault/GDM/knowledge/decisions/amgcl конфигурация iluk k1 новый оптимум.md`.

Текущий production-солвер: `amg<aggregation, ilu0> + lgmres` в `LinearProblem.h`, строки 44–49.

---

## Шаг 1: Замена ilu0 → iluk(k=1) в production

### Что менять

**Файл: `HydroSolver/Solver/Math/LinearProblem.h`**

1. Добавить include:
```cpp
#include <amgcl/relaxation/iluk.hpp>
```
   Рядом с существующим `#include <amgcl/relaxation/ilu0.hpp>` (строка 14).

2. Заменить typedef `Solver_AMG`:
```cpp
// БЫЛО:
template<unsigned char B>
using Solver_AMG = amgcl::make_solver<
    amgcl::amg< BBackend<B>, amgcl::coarsening::aggregation, amgcl::relaxation::ilu0>
    ,
    amgcl::solver::lgmres<BBackend<B>>
>;

// СТАЛО:
template<unsigned char B>
using Solver_AMG = amgcl::make_solver<
    amgcl::amg< BBackend<B>, amgcl::coarsening::aggregation, amgcl::relaxation::iluk>
    ,
    amgcl::solver::lgmres<BBackend<B>>
>;
```

3. В `LinearProblem.cpp`, конструктор (`LinearProblem::LinearProblem`, строка ~73): параметр `prm.precond.relax.k` по умолчанию = 1, менять не нужно.

### Что НЕ менять

- `ilu0.hpp` include оставить — он используется в benchmark (`test_amgcl_benchmark.cpp`, серии A–C).
- Benchmark код не трогать.

### Верификация

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Все 35 тестов должны пройти. Если тест 3D completions стал медленнее на > 20% — откатить и разобраться.

### Ожидаемый эффект

По бенчмарку: 84 → 75 с (–11%), 13.3 → 8.3 iters/solve (–38%).

### Коммит

```
perf: замена ilu0 → iluk(k=1) в production-солвере
```

---

## Шаг 2: Бенчмарк комбинаций iluk + лучшие параметры AMG

### Цель

Проверить, дают ли комбинации iluk(k=1) с лучшими параметрами из серий D/F дополнительный выигрыш. Серии D/F/G тестировались с ilu0 — теперь нужно проверить взаимодействие с iluk.

### Конфигурации

Добавить в `test_amgcl_benchmark.cpp` новую серию `[seriesH]`:

| ID | Описание | Параметры |
|---|---|---|
| H1 | iluk(k=1), baseline | defaults (npre=1, npost=1, M=15, K=3) |
| H2 | iluk(k=1) + npre=2 | `prm.precond.npre = 2` |
| H3 | iluk(k=1) + W-cycle | `prm.precond.ncycle = 2` |
| H4 | iluk(k=1) + npre=2, npost=2 | `prm.precond.npre = 2; prm.precond.npost = 2` |
| H5 | iluk(k=1) + K=5 | `prm.solver.K = 5` |
| H6 | iluk(k=2) | `prm.precond.relax.k = 2` |

Все H-конфигурации используют `Solver_AMG<B>` (после шага 1 это уже iluk).

Для H6 нужно явно задать `prm.precond.relax.k = 2`.

### Реализация

```cpp
TEST_CASE("AMGCL benchmark: Series H — iluk combinations",
          "[benchmark][amgcl][seriesH][.slow]")
{
    fs::create_directories("results");
    using S = Solver_AMG<B>;  // после шага 1 это iluk(k=1) + lgmres

    SECTION("H1: iluk(k=1) baseline") {
        S::params prm;
        auto r = run_benchmark<S>("H1_iluk1_baseline", prm);
        report(r); append_csv(csv_path, r);
        CHECK(r.balance_ok);
    }
    // ... аналогично H2–H6
}
```

### Верификация

```powershell
.\build\Release\gdm_benchmark.exe "[seriesH]"
```

### Анализ

Сравнить H1 с H2–H6. Если какая-то комбинация даёт > 5% ускорения — применить в production (обновить конструктор `LinearProblem`).

### Коммит

```
perf: серия H — бенчмарк комбинаций iluk с параметрами AMG/lgmres
```

Если найден лучший набор параметров — отдельный коммит:
```
perf: оптимальные параметры AMG для iluk(k=1)
```

---

## Шаг 3: Reuse AMG-иерархии между Newton-итерациями — ❌ ЗАБЛОКИРОВАН

**Статус:** SIGSEGV. `make_solver::operator()(A, F, X)` передаёт `A` (block_matrix_adapter) в lgmres для SpMV, но lgmres вызывает `backend::spmv(A, ...)`, который не работает корректно с lazy adapter — ожидает `build_matrix`. Результат — segfault при первом вызове residual.

**Альтернативы:**
- Конвертировать adapter → build_matrix и хранить как member. Но build_matrix создаётся внутри make_solver конструктора и не экспортируется.
- CPR.partial_update() — встроенная поддержка reuse, работает с скалярной CRS. См. сессию 6.

Reuse AMG с блочным backend не стоит усилий (< 5% выигрыш). Переходить к CPR.

### Проблема (оригинальная)

`LinearProblem::Solve()` создаёт `Solver_AMG<B>` при каждом вызове. На fine grid setup ~4–7% от total. Внутри одного временного шага Якобиан меняется слабо — AMG hierarchy можно переиспользовать.

### Архитектура изменений

**Файл: `HydroSolver/Solver/Math/LinearProblem.h`**

Добавить member:
```cpp
private:
    std::unique_ptr<Solver_AMG<B>> solver_;
    bool needs_rebuild_ = true;

public:
    void InvalidateSetup() { needs_rebuild_ = true; }
```

**Файл: `HydroSolver/Solver/Math/LinearProblem.cpp`**

Изменить `Solve()`:
```cpp
SolveResult LinearProblem::Solve(int maxIter)
{
    prm.solver.maxiter = maxIter;

    auto A = amgcl::adapter::block_matrix<value_type<B>>(
        std::tie(rhsSize, Matrix().Row(), Matrix().Col(), Matrix().Val()));

    if (needs_rebuild_ || !solver_) {
        prof.tic("setup");
        solver_ = std::make_unique<Solver_AMG<B>>(A, prm);
        prof.toc("setup");
        needs_rebuild_ = false;
    }

    rhs_type<B> const* fptr = reinterpret_cast<rhs_type<B> const*>(&rhs[0]);
    rhs_type<B>* xptr = reinterpret_cast<rhs_type<B>*>(&solutionCorrections[0]);
    amgcl::backend::numa_vector<rhs_type<B>> F(fptr, fptr + cellNmbr);
    amgcl::backend::numa_vector<rhs_type<B>> X(xptr, xptr + cellNmbr);

    prof.tic("solve");
    auto [iters, error] = (*solver_)(A, F, X);  // передаём A для SpMV
    prof.toc("solve");

    std::copy(X.data(), X.data() + X.size(), xptr);
    return { iters, error, true };
}
```

### КРИТИЧЕСКИЙ МОМЕНТ: operator()(A, rhs, x)

`amgcl::make_solver::operator()` имеет перегрузку `(rhs, x)` — использует сохранённую матрицу. Но нам нужна версия `(A, rhs, x)` — использует новую A для Krylov-итераций (SpMV), но старый прекондиционер. **Проверить, что эта перегрузка существует в нашей версии amgcl.**

Искать в `HydroSolver/AMGSolver/amgcl/amgcl/make_solver.hpp`:
```
grep "operator()" make_solver.hpp
```

Если перегрузки `(A, rhs, x)` нет — `operator()(rhs, x)` использует матрицу из конструктора и для SpMV. Тогда reuse только внутри одной Newton-итерации бессмысленен. В этом случае:
- Вариант A: reuse только при ПЕРВОЙ Newton-итерации нового шага (обновлять solver_ на каждом Solve), а benefit — сохранение начального приближения для Krylov (x не обнуляется).
- Вариант B: не делать reuse, перейти к шагу 4.
- Вариант C: реализовать reuse через CPR.partial_update() позже (шаг 6).

### Вызов InvalidateSetup()

**Файл: `HydroSolver/Reservoir/ReservoirSimulator.cpp`**

В `Solve()`, при начале нового временного шага (строка ~406):
```cpp
PerformNewtonLoop(numPrm.CurrentIntegrationStep(), numPrm.NextTimeMoment());
```

Перед `PerformNewtonLoop` добавить:
```cpp
MyProblem.InvalidateSetup();
```

При откате (wasted trial, строка ~419):
```cpp
numPrm.decrease_schemeTau();
solverProfile_.n_wasted_trials++;
MyProblem.InvalidateSetup();  // новый шаг → новая иерархия
continue;
```

### Верификация

1. Сборка + все 35 тестов
2. Smoke test 3D: `.\build\Release\gdm_tests.exe "[3d]"` — проверить, что AMGCL profiler показывает setup вызванный реже (1 раз на timestep, а не на каждую Newton-итерацию)
3. Бенчмарк: сравнить с H1 baseline

### Коммит

```
perf: reuse AMG-иерархии между Newton-итерациями
```

---

## Шаг 4: OpenMP для assembly и AMGCL backend

### 4a: Подключить OpenMP в CMake

**Файл: `CMakeLists.txt`**

После строки `target_compile_definitions(gdm_core PUBLIC AMGCL_NO_BOOST)` добавить:
```cmake
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(gdm_core PUBLIC OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found: assembly parallelization enabled")
endif()
```

`USE_PARALLEL` уже определён в `HydroSolver/Helpers/Defines.h` (строка 7). Прагмы `#pragma omp parallel for` уже стоят в:
- `ReservoirSimulator.cpp:503–504` (AssembleMyProblem — основной цикл)
- `ReservoirSimulator.cpp:461–462` (MassBalance)

Без `-openmp` флага компилятора эти прагмы игнорировались.

### 4b: Проверить thread-safety assembly

`fillMatrixBlockRow(l, loc_tau)` вызывается для каждой ячейки `l`. Внутри:
- Читает `Grid[l]`, `Grid.GetNeighboursPointer(l)`, `Grid.CommonEdgeArea(l)` — read-only, OK.
- Вызывает `MyProblem.AddOffDiagBlock(l, neibCount, blOffDiag)` и `MyProblem.AddDiagBlock(l, blDiag, rhsBlock)`.

**КРИТИЧЕСКАЯ ПРОБЛЕМА:** `AddDiagBlock` и `AddOffDiagBlock` пишут в общие массивы `value[]` и `rhs[]`. Но каждая ячейка `l` пишет в свой непересекающийся диапазон (блочная структура CRS), поэтому data race отсутствует:
- Диагональный блок ячейки `l` пишется по уникальному смещению, зависящему только от `l`.
- Внедиагональные блоки строки `l` пишутся по уникальным смещениям, определяемым `l` и `neibCount`.
- `rhs` пишется в позиции `l*B .. l*B+B-1`.

Формально safe, но проверить: нет ли в `AddDiagBlock`/`AddOffDiagBlock` побочных эффектов (статических переменных, обращений к общим счётчикам).

Проверить: `std::transform` в `AddDiagBlock` (LinearProblem.cpp:131) — пишет в `rhs[l*B .. l*B+B-1]`, ОК.

### 4c: AMGCL OpenMP backend

AMGCL `backend::builtin` использует OpenMP автоматически, если OpenMP доступен при компиляции. Это ускоряет SpMV и smoother apply внутри solve. На сетке 51×51×4 (10404 ячеек, 20808 скалярных неизвестных) выигрыш может быть скромным из-за overhead.

### Верификация

1. Перегенерировать CMake: `cmake -B build -S . -G "Visual Studio 17 2022"`
2. Собрать: `cmake --build build --config Release`
3. Все 35 тестов
4. Бенчмарк с OpenMP vs без — сравнить общее время

**Проверка OpenMP:** добавить в `main.cpp` или в начало smoke-теста:
```cpp
#ifdef _OPENMP
std::cout << "OpenMP threads: " << omp_get_max_threads() << std::endl;
#endif
```

### Ожидаемый эффект

Assembly: на fine grid ~25 с из 75 с → с 4–8 потоками ускорение 2–3× → -10–15 с → -13–20% total.
AMGCL backend: SpMV ускорение на 20808 неизвестных скромное, ~5–10% от solve.
Суммарно: -15–25% total.

### Коммит

```
perf: подключение OpenMP для assembly и AMGCL backend
```

---

## Итоговый ожидаемый эффект сессии

| Шаг | Изменение | Ожидаемое ускорение |
|---|---|---|
| 1 | iluk(k=1) в production | 84 → 75 с (–11%) |
| 2 | Комбинации (если выигрыш) | 75 → 72 с (–4%) спекулятивно |
| 3 | Reuse AMG | 72 → 69 с (–4%) спекулятивно |
| 4 | OpenMP | 69 → 55 с (–20%) спекулятивно |

Итого: 84 → 55 с (–35%), или 135.6 → 55 с (**2.5× vs Phase 0**). Цифры спекулятивные — каждый шаг верифицируется бенчмарком.

## Связанные заметки

- [[amgcl конфигурация iluk k1 новый оптимум]]
- [[2026-06-21 серии DFGE перебор параметров AMGCL]]
- [[prompt-оптимизация-02-структурные-оптимизации]]
