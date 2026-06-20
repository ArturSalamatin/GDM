---
tags:
  - решение
  - производительность
  - AMG
  - amgcl
  - профилирование
date: 2026-06-20
updated: 2026-06-20
---

# План профилирования и оптимизации AMGCL

## Контекст

Тест 7.5 (`3D completions: 7-well fine grid 51x51x4`) — бенчмарк для оптимизации:
- **10 404 ячейки**, блочная СЛАУ 2×2 → 20 808 неизвестных
- **146+ временных шагов** (730 дней, dt_save = 5 дней) + Ньютоновские подитерации
- 7 скважин с разной геометрией перфорации, delayed start, closure

Текущая конфигурация AMGCL (`LinearProblem.h`):
- Предобуславливатель: `amg<aggregation, damped_jacobi>`
- Солвер: `gmres` с maxiter = 5
- tol = 1e-5, abstol = 1e-5

## Обнаруженные проблемы

1. **`PrintCRS()` и `PrintDiagBlocks()` вызываются на КАЖДОМ solve** — запись всей матрицы в файл.
2. **AMG setup на каждом solve** — `Solver_AMG` создаётся заново при каждом вызове `Solve()`.
3. **maxiter = 5 для GMRES** — очень мало для несимметричной блочной системы.
4. **`ResetProblem()` делает аллокацию** — `std::vector<double>(...)` вместо `std::fill`.
5. **amgcl::make_solver хранит P и S как значения** — но API позволяет решать с другой матрицей при фиксированном preconditioner: `operator()(A, rhs, x)`.
6. **CPR/CPR-DRS доступны** и имеют `partial_update()` для reuse AMG-иерархии — штатная функциональность.

## Структура плана (3 фазы)

1. [[prompt-оптимизация-00-инструментирование]] — убрать PrintCRS, добавить замеры, собрать baseline
2. [[prompt-оптимизация-01-перебор-конфигураций-AMGCL]] — единый test_amgcl_benchmark.cpp с SECTION-ами
3. [[prompt-оптимизация-02-структурные-оптимизации]] — reuse setup (unique_ptr), partial_update (CPR), OpenMP, PI-контроллер

## Ключевые решения по реализации

- **Перебор конфигураций**: все конфигурации как SECTION-ы в одном `test_amgcl_benchmark.cpp`. Бенчмарк работает с сырыми CRS-данными из LinearProblem, вызывая amgcl напрямую — production-код не меняется.
- **Reuse setup**: `std::unique_ptr<Solver_AMG<B>>` как member `LinearProblem`. Пересоздаётся при новом временном шаге, переиспользуется внутри Newton-цикла.
- **CPR**: работает со скалярной (развёрнутой) матрицей. Параметр `block_size = 2`. `partial_update()` обновляет ILU, оставляя AMG нетронутым.
- **Operator complexity и levels**: через `operator<<` для `amg` или прямой доступ к `levels.size()`.

---

См. также:
- [[линейный солвер — AMG через amgcl]]
- [[возможности amgcl для блочных СЛАУ]]
- [[полностью неявная схема линеаризуется методом Ньютона]]
