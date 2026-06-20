---
tags:
  - промпт
  - производительность
  - amgcl
  - архитектура
date: 2026-06-20
updated: 2026-06-20
---

# Фаза 2: Структурные оптимизации (не параметрические)

## Цель

Архитектурные изменения, которые могут дать существенное ускорение помимо выбора конфигурации AMGCL.

---

## 2.1: Reuse AMG-иерархии — разделение preconditioner и solver

### Проблема

Сейчас `Solver_AMG` (`make_solver<Precond, IterativeSolver>`) создаётся заново в каждом вызове `LinearProblem::Solve()`. Внутри `make_solver` хранит P (preconditioner) и S (iterative solver) как **значения** (не указатели). Setup AMG-иерархии — дорогая операция, которая не нужна при каждой Ньютоновской итерации.

### Решение: unique_ptr + раздельные preconditioner и solver

API amgcl позволяет вызывать Krylov-солвер с **другой матрицей** при фиксированном предобуславливателе:
```cpp
// make_solver::operator()(const Matrix &A, const Vec1 &rhs, Vec2 &&x)
// — решает с матрицей A, но предобуславливателем, построенным при конструировании
```

Реализация в `LinearProblem`:

```cpp
// Вместо локальной переменной в Solve():
// Solver_AMG<B> solve(A, prm);  // ← каждый раз

// Хранить как member:
std::unique_ptr<Solver_AMG<B>> solver_;
bool needs_rebuild_ = true;

void InvalidateSetup() { needs_rebuild_ = true; }

const std::tuple<int, double, bool> Solve(int maxIter) {
    auto A = amgcl::adapter::block_matrix<value_type<B>>(...);
    
    if (needs_rebuild_ || !solver_) {
        solver_ = std::make_unique<Solver_AMG<B>>(A, prm);
        needs_rebuild_ = false;
    }
    
    // Решаем с ТЕКУЩЕЙ матрицей A, но КЕШИРОВАННЫМ preconditioner-ом:
    auto [iters, error] = (*solver_)(A, F, X);
    // ↑ operator()(A, rhs, x) — использует A для SpMV, precond от конструирования
    
    return { iters, error, true };
}
```

Вызов `InvalidateSetup()`:
- В `ReservoirSimulator::Solve()` — при начале **нового временного шага** (после `Grid.AcceptState()`)
- НЕ при каждой Ньютоновской итерации

### Стратегия частоты пересборки

Три варианта для сравнения:
1. **Каждый временной шаг** — пересоздавать solver_ при первой Newton-итерации каждого шага
2. **Каждые N шагов** — N = 5, 10, 20 (frozen preconditioner)
3. **Адаптивно** — пересоздавать, если число GMRES-итераций выросло > порога (например, > 2× avg)

### Для CPR: partial_update()

CPR имеет встроенный `partial_update(K)` — обновляет ILU-часть, оставляя AMG-иерархию нетронутой. Это идеальный вариант для reuse:

```cpp
std::unique_ptr<CPRSolver> solver_;

// При каждой Newton-итерации:
solver_->precond().partial_update(K, /* update_transfer_ops = */ false);
// При каждом новом временном шаге:
solver_->precond().partial_update(K, /* update_transfer_ops = */ true);
// Полная пересборка — только при деградации сходимости
```

---

## 2.2: Начальное приближение для Krylov-солвера

### Проблема

В `ResetProblem()`:
```cpp
solutionCorrections = std::vector<double>(cellNmbr * B, 0.0); // аллокация + обнуление
```

Каждая Ньютоновская итерация начинает с нулевого начального приближения. Если поправки мало меняются между итерациями, можно передать предыдущие corrections как x0.

### Решение

1. НЕ обнулять `solutionCorrections` в `ResetProblem()` (оставлять значения от предыдущего solve)
2. GMRES/BiCGStab принимают x0 как входной вектор — если он близок к решению, нужно меньше итераций
3. Обнулять только при **первой** Newton-итерации нового временного шага

### Риск

Если предыдущая поправка — плохое начальное приближение, может увеличить число итераций. Нужно замерить оба варианта.

---

## 2.3: std::fill вместо аллокации в ResetProblem

### Проблема

```cpp
void LinearProblem::ResetProblem() {
    matrix->ResetMatrix();
    rhs = std::vector<double>(cellNmbr * B, 0.0);              // аллокация!
    solutionCorrections = std::vector<double>(cellNmbr * B, 0.0); // аллокация!
}
```

Каждый вызов выделяет новый вектор (malloc + free). Для 20k элементов это ~160 KB — быстро, но бессмысленно.

### Решение

```cpp
void LinearProblem::ResetProblem() {
    matrix->ResetMatrix();
    std::fill(rhs.begin(), rhs.end(), 0.0);
    std::fill(solutionCorrections.begin(), solutionCorrections.end(), 0.0);
}
```

То же для `MatrixCSR::ResetMatrix()` — проверить, делает ли он `value = std::vector<double>(nnz, 0.0)` (аллокация) или `std::fill` (in-place).

---

## 2.4: Адаптивный шаг по времени — PI-контроллер

### Текущая стратегия

```
increase: tau *= (1 + factor)     // factor = 0.15 → ×1.15
decrease: tau *= (1 - 2*factor)   // → ×0.70
```

Грубая эвристика. Если Newton сходится за 1 итерацию — шаг увеличивается слишком медленно.

### PI-контроллер

```
tau_new = tau * (target_iters / actual_iters)^{k_P} * (prev_iters / actual_iters)^{k_I}
```

Типичные значения: k_P = 0.075, k_I = 0.175, target_iters = 3.

### Шаги

1. Из baseline определить число wasted trials
2. Если > 5% шагов — реализовать PI-контроллер
3. Если < 5% — эта оптимизация не приоритетна

---

## 2.5: OpenMP-параллелизм в assembly

### Текущее состояние

В `AssembleMyProblem` есть `#pragma omp parallel for` под `#ifdef USE_PARALLEL`. Проверить:
1. Определён ли `USE_PARALLEL` в `CMakeLists.txt`?
2. Включён ли OpenMP в cmake?
3. Если нет — добавить:
```cmake
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_compile_definitions(gdm_core PRIVATE USE_PARALLEL)
    target_link_libraries(gdm_core PRIVATE OpenMP::OpenMP_CXX)
endif()
```

### Ожидаемый эффект

Assembly — embarrassingly parallel (каждая ячейка независима). На 4–8 ядрах ожидается ускорение 2–4× для assembly-части.

---

## 2.6: Preonly-солвер (AMG как прямой метод)

### Идея

Если AMG-предобуславливатель достаточно хорош, `solver::preonly` — один V-cycle без Krylov-обёртки. Экономит ортогонализацию и хранение.

### Когда проверять

После серий B–C: если с лучшим coarsening+relaxation avg_iters = 1, стоит попробовать preonly. Если avg_iters > 2 — preonly не подходит.

---

## Итоговый чеклист фазы 2

- [ ] 2.1: Reuse AMG setup через unique_ptr (+ partial_update для CPR)
- [ ] 2.2: Начальное приближение для Krylov (не обнулять corrections)
- [ ] 2.3: std::fill вместо аллокации в ResetProblem
- [ ] 2.4: PI-контроллер шага (если wasted trials > 5%)
- [ ] 2.5: OpenMP для assembly (если assembly > 30% total)
- [ ] 2.6: Preonly-солвер (если avg_iters = 1)

## Порядок выполнения

2.3 → 2.1 → 2.2 → 2.5 → 2.6 → 2.4

Начать с самого дешёвого (std::fill), потом самое перспективное (reuse), потом остальное по убыванию ожидаемого эффекта.
