---
tags:
  - план
  - баг
date: 2026-07-02
issue: BUG-015
github: 11
branch: fix/bug-015/vector-bool-openmp-race
status: в процессе
audit:
  date: 2026-07-02
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# План: BUG-015 — `std::vector<bool>` под OpenMP — data race в UpdateGrid()

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[известные баги]] (BUG-015)
3. Создай ветку: `git checkout -b fix/bug-015/vector-bool-openmp-race experimental`
4. Собери проект: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

**Baseline (experimental, 2026-07-02):** 287 тестов, ~277 сек Release.

---

## Описание проблемы

### Что происходит

Метод `ReservoirSimulator::UpdateGrid()` проверяет сходимость Newton-итерации для каждой ячейки сетки. Результаты записываются в `std::vector<bool> f` внутри `#pragma omp parallel for`. Специализация `std::vector<bool>` использует bit-packing (1 бит на элемент, 8 элементов в одном байте). Параллельная запись в разные биты одного байта — data race (UB по стандарту C++).

### Цепочка причинно-следственных связей

1. **Триггер:** Newton loop вызывает `UpdateGrid()` для проверки сходимости
2. **Место ошибки:** `ReservoirSimulator.cpp:465` — `std::vector<bool> f` создаётся, затем заполняется в OpenMP parallel for (строки 468-486)
3. **Механизм:** При `B = 2` (constexpr, `LinearProblem.h:46`), элементы `f[2*l]` и `f[2*l+1]` для 4 соседних `l` делят один байт. Два потока, обрабатывающие `l=k` и `l=k+1`, выполняют read-modify-write на одном байте без синхронизации
4. **Распространение:** `UpdateGrid()` возвращает `std::all_of(f.begin(), f.end(), ...)` → `numPrm.update_isSuccesfullNewtonTrial(result)` → влияет на решение о сходимости Newton-петли
5. **Проявление:** ложная сходимость (Newton останавливается раньше — потеря точности) или ложная несходимость (лишние итерации — потеря производительности). Недетерминистично

### Затронутый код

```cpp
// ReservoirSimulator.cpp:462-488
bool ReservoirSimulator::UpdateGrid()
{
    const double tol = 3E-3;
    std::vector<bool> f = std::vector<bool>(B * Grid.ActiveCellsNmbr(), true);
#ifdef USE_PARALLEL
#pragma omp parallel for
#endif
    for (int l = 0; l < Grid.ActiveCellsNmbr(); l++)
    {
        double corr[B];
        MyProblem.UnpackCellCorrections(l, corr);
        Grid[l].UpdateState(corr);

        const std::vector<double>& stateVaiables = Grid[l].GetVariableFieldProperties();

        int i = 0; // saturation
        f[B * l + i] =
            (abs(stateVaiables[i]) < numPrm.NewtonTol() * tol) || (abs(1.0 - stateVaiables[i]) < numPrm.NewtonTol() * tol) ||
            (abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
        i = 1; // pressure
        f[B * l + i] =
            (abs(stateVaiables[i]) < 1E6) ||
            (abs(corr[i]) <= numPrm.NewtonTol() * abs(stateVaiables[i]));
    }
    return std::all_of(f.begin(), f.end(), [](bool x) { return x; });
}
```

Вызывающие `UpdateGrid()`:
- `ReservoirSimulator.cpp:452` — `numPrm.update_isSuccesfullNewtonTrial(UpdateGrid());` (основной Newton loop)
- `tests/test_amgcl_benchmark.cpp:243,385` — `sim.UpdateGrid()` в тестовых Newton loops
- `examples/ex_benchmark_series_ts.cpp:179` — example-файл
- `examples/ex_benchmark_series_cpr.cpp:232,374` — example-файл

### Целевое состояние

`UpdateGrid()` использует `std::vector<char>` вместо `std::vector<bool>`. Каждый элемент занимает свой байт — параллельная запись в разные элементы безопасна (ISO C++ §6.9.2.2). Семантика, результаты вычислений и API не меняются.

---

## Варианты решения

### Вариант A: `std::vector<char>` (рекомендуемый)

**Суть:** заменить `std::vector<bool> f` на `std::vector<char> f`. Значения 0/1 в `char` семантически эквивалентны `false`/`true`.

**Конкретные изменения:**
- Строка 465: `std::vector<bool> f = std::vector<bool>(...)` → `std::vector<char> f(B * Grid.ActiveCellsNmbr(), 1);`
- Строка 487: `[](bool x) { return x; }` → `[](char x) { return x != 0; }`

**Плюсы:**
- 2 строки, минимальный diff
- Гарантированная потокобезопасность
- Семантика идентична
- Нет структурных изменений

**Минусы:**
- Расход памяти ×8 (1 байт vs 1 бит на элемент). Для 100K ячеек: 200 KB вместо 25 KB. Несущественно для выделения на стеке в рамках одного вызова

**Риски:** нулевые. Типы совместимы, `bool` → `char` для 0/1 — нет потери данных.

**Совместимость:** полная. Возвращаемый тип `bool` не меняется. Вызывающий код (`numPrm.update_isSuccesfullNewtonTrial(bool)`) не затронут.

**Трудоёмкость:** 1 файл, 2 строки.

### Вариант B: OpenMP reduction

**Суть:** убрать массив `f`, использовать `#pragma omp parallel for reduction(&&: converged)`.

**Конкретные изменения:**
```cpp
bool converged = true;
#pragma omp parallel for reduction(&&: converged)
for (int l = 0; l < Grid.ActiveCellsNmbr(); l++) {
    // ...
    bool sat_ok = (abs(stateVaiables[0]) < numPrm.NewtonTol() * tol) || ...;
    bool pres_ok = (abs(stateVaiables[1]) < 1E6) || ...;
    converged = converged && sat_ok && pres_ok;
}
return converged;
```

**Плюсы:** zero allocation, идиоматический OpenMP.

**Минусы:** теряется информация о конкретных несошедшихся ячейках (диагностика). Структурное изменение — больший diff, больший риск.

**Трудоёмкость:** 1 файл, ~15 строк.

### Выбор: Вариант A

Минимальное изменение, нулевой риск регрессии, сохраняет возможность future-расширения (диагностика по ячейкам). Вариант B — потенциальная оптимизация, но для DEBT, не для BUG-fix.

---

## Поиск подводных камней

- ✅ **Побочные эффекты:** `UpdateGrid()` вызывается из 5 мест (Newton loop + тесты + examples). Все передают возвращаемый `bool` — тип не меняется. Безопасно
- ✅ **Потокобезопасность:** именно это и фиксим. `std::vector<char>` — каждый элемент в своём байте, параллельная запись безопасна
- ✅ **Граничные случаи:** пустая сетка (`ActiveCellsNmbr() == 0`) — `vector<char>` пуст, `all_of` возвращает `true` (как и `vector<bool>`). Одна ячейка — один поток, race невозможен. Корректно
- ✅ **Производительность:** аллокация `vector<char>(2*N)` вместо `vector<bool>(2*N)` — разница ~200 KB для 100K ячеек. Выделяется один раз за Newton-итерацию, вне горячего цикла assembly/solve. Несущественно
- ✅ **Обратная совместимость:** семантика `char` 0/1 идентична `bool`. `all_of` с `[](char x) { return x != 0; }` — тот же результат
- ✅ **Порядок вызовов:** не меняется
- ✅ **Состояние при ошибке:** `UpdateGrid()` не бросает исключений, нет early return
- ✅ **Численная устойчивость:** фикс не затрагивает вычисления — только контейнер для результатов. Формулы сходимости не меняются
- ✅ **Связь с другими задачами:** нет активных веток, затрагивающих `UpdateGrid()`. BUG-006 (Solve converged always true) — затрагивает `numPrm`, не `UpdateGrid()`. Конфликта нет
- ✅ **Зависимости сборки:** нет новых `#include`, нет изменений в CMake

---

## Обнаруженные проблемы

Нет. Единственное место `vector<bool>` под OpenMP в проекте — `ReservoirSimulator.cpp:465`. Остальные `vector<bool>` (`ActiveCells` в строке 135, `blockPattern` в CRSStructure/LinearProblem/SparsityPattern, `IsActiveCell` в WellJobs) — не используются в OpenMP-параллельных секциях.

---

## Затронутые файлы

| Файл | Роль |
|---|---|
| `HydroSolver/Reservoir/ReservoirSimulator.cpp` | Замена `vector<bool>` на `vector<char>` в `UpdateGrid()` |

---

## Шаги реализации

### Шаг 1: Заменить `std::vector<bool>` на `std::vector<char>` в `UpdateGrid()`

**Цель:** устранить data race при параллельной записи в вектор флагов сходимости Newton-итерации.

**Файлы:** `HydroSolver/Reservoir/ReservoirSimulator.cpp`

**Контекст:**
Метод `UpdateGrid()` (строки 462-488) проверяет сходимость Newton-итерации для каждой ячейки сетки. Результаты (converged/not converged) записываются в `std::vector<bool> f` внутри `#pragma omp parallel for`. Специализация `std::vector<bool>` использует bit-packing — запись в разные биты одного байта из разных потоков = data race (UB). Замена на `std::vector<char>` устраняет проблему: каждый `char` занимает свой байт, параллельная запись в разные элементы безопасна по стандарту C++.

`B = 2` — глобальная constexpr для количества переменных (насыщенность + давление), определена в `HydroSolver/Solver/Math/LinearProblem.h:46`.

**Что сделать:**

1. В файле `HydroSolver/Reservoir/ReservoirSimulator.cpp`, функция `UpdateGrid()`, строка ~465:
   заменить объявление вектора — `std::vector<bool>` на `std::vector<char>`, инициализация значением `1` (эквивалент `true`)

2. В строке ~487: в `std::all_of` заменить тип параметра lambda с `bool` на `char` и сравнение `x != 0` вместо неявного `return x`

**Изменения (старый → новый код):**

До:
```cpp
	std::vector<bool> f = std::vector<bool>(B * Grid.ActiveCellsNmbr(), true);
```

После:
```cpp
	std::vector<char> f(B * Grid.ActiveCellsNmbr(), 1);
```

До:
```cpp
	return std::all_of(f.begin(), f.end(), [](bool x) { return x; });
```

После:
```cpp
	return std::all_of(f.begin(), f.end(), [](char x) { return x != 0; });
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — все 287 тестов зелёные
- Ожидаемый результат: идентичное поведение. Результаты симуляции не меняются — фикс устраняет UB, но на практике race мог не проявляться на текущих тестовых сетках (малый размер)
- Debug: `cmake --build build --config Debug; ctest --test-dir build -C Debug --output-on-failure` — все тесты зелёные

**Подводные камни:**
- Побочные эффекты ✅ — возвращаемый тип `bool` не меняется
- Потокобезопасность ✅ — `vector<char>` гарантирует безопасную параллельную запись

**Зависимости:**
- Требует: нет
- Блокирует: нет

**Оценка:** ~2 строки, ~5 минут

---

## Тестовая стратегия

### Тест-воспроизводитель

Data race по своей природе недетерминистична — нельзя создать тест, который гарантированно падает без фикса на малых сетках. Баг проявляется стохастически на больших сетках при нескольких OpenMP-потоках. Тест-воспроизводитель не создаётся — фикс верифицируется инспекцией кода (замена `vector<bool>` на `vector<char>`) и полным прогоном регрессионных тестов.

### Regression

Все 287 существующих тестов — регрессионная проверка. Ключевые:
- Тесты из `test_amgcl_benchmark.cpp` (используют `sim.UpdateGrid()` напрямую) — линии 243, 385
- Все standalone examples (`ex_benchmark_series_ts.cpp`, `ex_benchmark_series_cpr.cpp`)
- Five-spot, Buckley-Leverett — integration-тесты, которые прогоняют полный Newton loop

### Visual

Не применимо — фикс не меняет численные результаты (устраняет UB, которое на малых сетках может не проявляться).

---

## Критерии завершения

- [ ] `std::vector<bool>` заменён на `std::vector<char>` в `UpdateGrid()`
- [ ] Все 287 тестов зелёные в Release
- [ ] Все тесты зелёные в Debug
- [ ] Ноль новых warnings при сборке (оба конфига)
- [ ] Vault обновлён: запись в [[известные баги]] → статус ✅
- [ ] Заметка в `knowledge/debugging/` создана
- [ ] GitHub issue #11 прокомментирован с результатом
- [ ] `vault/GDM/00-home/index.md` обновлён если создана новая заметка
