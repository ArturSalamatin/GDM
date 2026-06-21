---
tags:
  - промпт
  - производительность
  - assembly
  - профилирование
date: 2026-06-21
---

# Сессия 5: Профилирование и оптимизация assembly

## Предварительно

Прочитай `vault/GDM/00-home/текущие приоритеты.md` и результаты шагов 1–4 из предыдущей сессии.

## Контекст

После шагов 1–4 assembly занимает ~33–40% полного времени (было ~14% до оптимизации солвера — assemble не менялся, но солвер стал быстрее → доля assembly выросла). Это следующее бутылочное горлышко.

Горячий путь:
```
ReservoirSimulator::SingleIteration()
  └─ AssembleMyProblem(loc_tau, nextTimeMoment)  ← prof.tic("assemble")
       └─ MyProblem.ResetProblem()               ← зануление матрицы + rhs
       └─ for l in 0..ActiveCellsNmbr():
            fillMatrixBlockRow(l, loc_tau)        ← основная работа
       └─ AccountForBoundaryConditions()
       └─ for well in Wells:
            well->AddWellToMatrix(t)
```

`fillMatrixBlockRow` (ReservoirSimulator.cpp:537–649):
- Аллоцирует `std::vector<double>` на каждый вызов: `blDiag` (строка 544), `rhsBlock` (545), `blOffDiag` (565) — это горячий цикл, вызывается 10404 × N_newton × N_timesteps раз.
- Вызывает ~20 методов `TwoPhaseFlowCell` — каждый обращается к `DependentFieldProperties[]`, виртуальный вызов.
- `AddDiagBlock` и `AddOffDiagBlock` в `LinearProblem.cpp` — используют `std::transform` (строка 131).

## Этап 1: Детальное профилирование assembly

### Инструментация

Добавить `prof.tic`/`prof.toc` внутрь `AssembleMyProblem`:

```cpp
void ReservoirSimulator::AssembleMyProblem(double loc_tau, double nextTimeMoment)
{
    prof.tic("reset");
    MyProblem.ResetProblem();
    prof.toc("reset");

    prof.tic("fill_rows");
    #pragma omp parallel for
    for (size_t l = 0; l < Grid.ActiveCellsNmbr(); l++)
        fillMatrixBlockRow(l, loc_tau);
    prof.toc("fill_rows");

    prof.tic("boundary");
    AccountForBoundaryConditions();
    prof.toc("boundary");

    prof.tic("wells");
    for (auto& [name, well] : Wells)
    {
        auto [posLocal, matrixBlockPerPerforation, rhsPerPerforation] = 
            well->AddWellToMatrix(nextTimeMoment - loc_tau);
        // ...
    }
    prof.toc("wells");
}
```

Запустить бенчмарк, посмотреть распределение внутри assembly.

## Этап 2: Устранение аллокаций в горячем цикле

### Проблема

`fillMatrixBlockRow` аллоцирует 3 `std::vector<double>` на каждый вызов:
```cpp
std::vector<double> blDiag = std::vector<double>(blockSize, 0);     // строка 544
std::vector<double> rhsBlock = cell.PreviousState_Mass();            // строка 545
std::vector<double> blOffDiag = std::vector<double>(blockSize, 0);  // строка 565, внутри цикла по соседям!
```

На сетке 51×51×4 с ~6 соседями и ~6 Newton-итерациями и ~187 временных шагов:
- `blDiag`: 10404 × 6 × 187 ≈ 11.7M аллокаций
- `blOffDiag`: 10404 × 6 × 6 × 187 ≈ 70M аллокаций

### Решение: stack-allocated arrays

`blockSize` = `B*B` = 4. Это фиксированный compile-time размер. Заменить на `std::array<double, 4>` или `double[4]`:

```cpp
void ReservoirSimulator::fillMatrixBlockRow(size_t l, double loc_tau)
{
    constexpr int blockSize = B * B;  // = 4
    double blDiag[blockSize] = {};
    
    auto prevMass = Grid[l].PreviousState_Mass();
    double rhsBlock[B] = { 
        (prevMass[0] - Grid[l].OilMass()) / loc_tau,
        (prevMass[1] - Grid[l].WaterMass()) / loc_tau 
    };
    // ...
    for (int neibCount = 0; ...) {
        double blOffDiag[blockSize] = {};
        // ...
    }
}
```

Также проверить `PreviousState_Mass()` — если возвращает `std::vector`, рассмотреть возврат `std::array<double, B>`.

### Изменения в API

`AddDiagBlock` и `AddOffDiagBlock` принимают `const std::vector<double>&`. Нужно либо:
- Перегрузить для `double*` + `size_t`
- Или использовать `std::span<const double>` (C++20)
- Или оставить `std::vector` и создавать его один раз как thread-local

Рекомендация: перегрузка для `const double*`:
```cpp
void AddDiagBlock(size_t l, const double* data, const double* dataRHS);
void AddOffDiagBlock(size_t l, size_t neibIdx, const double* data);
```

Это минимальное изменение — внутри `AddDiagBlock` всё равно обращается к `data[i]`.

## Этап 3: Устранение виртуальных вызовов

`TwoPhaseFlowCell` наследует от `SomeProcessCell_TimeDependent<Dim3Cell>`. Методы `DensityOil()`, `MobilityOverall()`, `F_Oil()` и др. обращаются к `DependentFieldProperties[i]` — это `std::vector<double>` (или массив?) в базовом классе.

Проверить:
1. `DependentFieldProperties` — это вектор или массив?
2. Методы виртуальные или inline?
3. Можно ли сделать `DependentFieldProperties` `std::array` фиксированного размера?

Если `DependentFieldProperties` — `std::vector`, каждое обращение `cell.DensityOil()` = pointer chase (vector → heap data). Замена на `std::array<double, 10>` устранит indirection.

### Предупреждение

Изменение иерархии ячеек — это рефакторинг с большим blast radius. Все тесты должны пройти. Менять базовый класс только если профилирование подтверждает, что это бутылочное горлышко.

## Этап 4: ResetProblem — profiling zero-fill

`ResetProblem()` вызывает:
- `matrix->ResetMatrix()` — `std::fill` на `value[]` (nnz × blockSize doubles)
- `std::fill` на `rhs[]` (cellNmbr × B doubles)
- `std::fill` на `solutionCorrections[]` (cellNmbr × B doubles)

На 10404 ячейках с ~6 соседями: `value[]` ~ 10404 × 7 × 4 = 291K doubles = 2.3 MB. `std::fill` на 2.3 MB — ~0.5 мс, ничтожно. Но проверить.

Альтернатива: вместо зануления → перезаписывать напрямую в `fillMatrixBlockRow`. Это сложнее, но устраняет лишний проход по памяти. Оставить как оптимизацию третьего эшелона.

## Порядок

1. Инструментация → профиль → определить, где реальное время
2. Stack-allocated arrays в `fillMatrixBlockRow`
3. `AddDiagBlock`/`AddOffDiagBlock` с `double*` (если шаг 2 требует)
4. Виртуальные вызовы (только если профиль подтверждает)
5. `PreviousState_Mass()` → `std::array`

## Верификация

После каждого изменения: сборка, все 35 тестов, бенчмарк. Сравнить:
- Полное время
- Время assembly (из amgcl profiler)
- Правильность: balance_ok, saturation bounds

## Коммиты

```
perf: инструментация assembly — prof.tic/toc для reset, fill_rows, boundary, wells
perf: stack-allocated arrays в fillMatrixBlockRow
perf: AddDiagBlock/AddOffDiagBlock с double* перегрузкой
```

## Связанные заметки

- [[prompt-оптимизация-04-iluk-reuse-openmp-adaptive]]
- [[amgcl конфигурация iluk k1 новый оптимум]]
