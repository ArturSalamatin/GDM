---
tags:
  - план
  - баг
date: 2026-07-02
issue: BUG-010
github: 10
branch: fix/bug-010/const-cast-perforations
status: реализован
audit:
  date: 2026-07-02
  findings: 0 / 0 / 0
  auto-fixed: 0
  manual-required: 0
---

# BUG-010: Устранение `const_cast` на перфорациях в `SomeWell`

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[code-review-2026-06-28-баги-и-корректность]] (секция CR-BUG-006)
3. Создай ветку: `git checkout -b fix/bug-010/const-cast-perforations experimental`
4. Собери проект:
   ```powershell
   cmake -B build -S . -G "Visual Studio 17 2022"
   cmake --build build --config Release
   ```
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Начни с шага 1. После каждого шага: сборка + тесты

**Baseline (ожидаемый):** 287 тестов, все pass, Release ~53 сек.

---

## Суть проблемы

В `SomeWell.cpp` 5 мест используют `const_cast` для мутации объектов `AccumulatedPerforations` и `SetOfPerforations`, полученных через const-геттер `PerforationConfiguration()`.

Метод `PerforationConfiguration()` (строка ~189 в `SomeWell.cpp`) возвращает `const PerforationsOfWell&` — const-ссылку на поле `ItsAccumulatedPerforations`. Конструктор `SomeWell` и методы `BringFirstPerforationToFirstMER()` / `BringLastPerforationToLastMER()` являются non-const и намеренно мутируют перфорации (сдвигают даты, усредняют). Но единственный доступ — через const-геттер, поэтому автор обошёл const через `const_cast`.

Само поле `ItsAccumulatedPerforations` объявлено non-const (`SomeWell.h:144`), поэтому объект де-факто мутабелен, но мутация через `const_cast` на результат const-метода — формально undefined behavior по стандарту C++.

### 5 мест с `const_cast`

1. **Конструктор `SomeWell`, строка ~143:**
   ```cpp
   const_cast<set_of_points::AccumulatedPerforations&>(perfs).AveragePerforationsOut(jobIntervals);
   ```
   `perfs` получен через `for (const auto& [layer_id, perfs] : PerforationConfiguration())`.

2. **`BringFirstPerforationToFirstMER()`, строки ~315–317:**
   ```cpp
   const_cast<set_of_points::SetOfPerforations&>(
       perf.getPerforationsSet().front()).MoveDate(date);
   const_cast<set_of_points::AccumulatedPerforations&>(perf).AssembleJobDates();
   ```
   `perf` получен через `for (auto& [layer_id, perf] : PerforationConfiguration())`.
   Двойной `const_cast`: один на `SetOfPerforations` (через `getPerforationsSet().front()`), один на `AccumulatedPerforations`.

3. **`BringLastPerforationToLastMER()`, строки ~337–339:**
   Аналогично месту 2, но `.back()` вместо `.front()`.

### Цепочка UB

```
SomeWell::SomeWell() [non-const]
  → PerforationConfiguration()               [const метод, возвращает const&]
  → for (const auto& [layer_id, perfs] : ...)
  → const_cast<AccumulatedPerforations&>(perfs)  [UB: мутация через const ref]
    → .AveragePerforationsOut()               [мутирует PerforationsInTime, ItsAllJobsMoments]
```

## Целевое состояние

Все 5 `const_cast` устранены. Мутация перфораций в конструкторе и `BringFirst/LastPerforationToFirstMER` происходит через прямой доступ к полю `ItsAccumulatedPerforations` (non-const, private — все 3 метода являются членами `SomeWell` и имеют доступ). Для `MoveDate` на элементах `PerforationsInTime` добавлена non-const перегрузка `getPerforationsSet()` в `AccumulatedPerforations`.

Поведение программы идентично текущему, но без UB.

## Варианты решения

### Вариант A: прямой доступ к полю + non-const `getPerforationsSet()` (рекомендуемый)

В конструкторе и `BringFirst/LastPerforationToFirstMER` заменить `PerforationConfiguration()` на `ItsAccumulatedPerforations` (прямой доступ к private полю). Для `MoveDate()` / `front()` / `back()` на элементах `PerforationsInTime` добавить non-const перегрузку `getPerforationsSet()` в `AccumulatedPerforations`.

- **Плюсы:** минимальные изменения, не меняет публичный API, не создаёт новых public/protected геттеров
- **Минусы:** нет
- **Файлы:** 3 (SomeWell.cpp, SetOfPoints.h, SetOfPoints.cpp)
- **Строк:** ~15

### Вариант B: non-const перегрузка `PerforationConfiguration()`

Добавить `PerforationsOfWell& PerforationConfiguration();` как private метод.

- **Плюсы:** сохраняет паттерн доступа через геттер
- **Минусы:** лишний API; если случайно попадёт в public — откроет мутацию наружу; всё равно нужна non-const `getPerforationsSet()`
- **Файлы:** 4 (SomeWell.h, SomeWell.cpp, SetOfPoints.h, SetOfPoints.cpp)
- **Строк:** ~20

### Выбор: вариант A

Все 3 метода (конструктор, `BringFirst`, `BringLast`) — приватные члены `SomeWell`, имеют прямой доступ к полю. Добавлять ещё один геттер — лишняя косвенность без выгоды.

## Поиск подводных камней

- ✅ **Побочные эффекты:** `PerforationConfiguration()` вызывается из 15 мест в `SomeWell.cpp`, но мы НЕ меняем этот метод. Меняем только 3 метода, которые использовали `const_cast`, заменяя на прямой доступ к полю. Остальные 12 мест — read-only, продолжают использовать const-геттер.
- ✅ **Потокобезопасность:** нет `#pragma omp` в `SomeWell.cpp`. Конструктор и `BringFirst/Last` вызываются при инициализации скважины, вне параллельных секций.
- ✅ **Граничные случаи:** поведение не меняется — те же мутации, только без UB. Пустые перфорации — `AveragePerforationsOut` корректно обрабатывает пустой `jobIntervals`.
- ✅ **Производительность:** изменения в коде инициализации (один раз при создании скважины), не в горячем цикле.
- ✅ **Обратная совместимость:** поведение идентично. Публичный API не меняется. `PerforationConfiguration() const` остаётся.
- ✅ **Порядок вызовов:** не меняется. Конструктор → `BringFirst` → `BringLast` → `PrintWell` — тот же порядок.
- ✅ **Состояние при ошибке:** throw в конструкторе (строка ~130) — до мутации перфораций. Не затрагивается.
- ✅ **Численная устойчивость:** не затрагивается (фикс не меняет вычисления).
- ✅ **Связь с другими задачами:** нет конфликта. DEBT-045 (рефакторинг SparsityPattern) и другие открытые баги затрагивают другие файлы.
- ✅ **Зависимости сборки:** добавление non-const `getPerforationsSet()` в `SetOfPoints.h` — все, кто include-ят этот header, видят новый метод, но он не ломает существующий код (перегрузка, не замена).

## Обнаруженные проблемы

Нет.

---

## Затронутые файлы

| Файл | Роль |
|---|---|
| `HydroSolver/Reservoir/Well/SomeWell.cpp` | Устранение 5 `const_cast` в 3 методах |
| `HydroSolver/Reservoir/Well/SetOfPoints.h` | Добавление non-const перегрузки `getPerforationsSet()` |
| `HydroSolver/Reservoir/Well/SetOfPoints.cpp` | Реализация non-const `getPerforationsSet()` |

---

## Шаги реализации

### Шаг 1: Добавить non-const перегрузку `getPerforationsSet()` в `AccumulatedPerforations`

**Цель:** Обеспечить non-const доступ к `PerforationsInTime` для `MoveDate()` на элементах, без `const_cast`.

**Файлы:** `HydroSolver/Reservoir/Well/SetOfPoints.h`, `HydroSolver/Reservoir/Well/SetOfPoints.cpp`

**Контекст:**
`AccumulatedPerforations` хранит вектор `PerforationsInTime` (protected поле, строка ~148 в `SetOfPoints.h`). Текущий геттер `getPerforationsSet()` (строка ~161) возвращает `const vector<SetOfPerforations>&`. Методы `BringFirstPerforationToFirstMER` и `BringLastPerforationToLastMER` в `SomeWell.cpp` вызывают `.front().MoveDate()` и `.back().MoveDate()` на результате этого геттера, используя `const_cast`. Нужна non-const перегрузка.

**Что сделать:**

1. В `SetOfPoints.h`, строка ~161, после существующего const-геттера, добавить non-const перегрузку:

До:
```cpp
		const std::vector<SetOfPerforations>& getPerforationsSet() const;
```

После:
```cpp
		const std::vector<SetOfPerforations>& getPerforationsSet() const;
		std::vector<SetOfPerforations>& getPerforationsSet();
```

2. В `SetOfPoints.cpp`, после существующей реализации (строка ~403–407), добавить реализацию non-const версии:

До:
```cpp
	const std::vector<SetOfPerforations>& 
		AccumulatedPerforations::getPerforationsSet() const
	{
		return PerforationsInTime;
	}
```

После (добавить ниже):
```cpp
	const std::vector<SetOfPerforations>& 
		AccumulatedPerforations::getPerforationsSet() const
	{
		return PerforationsInTime;
	}

	std::vector<SetOfPerforations>&
		AccumulatedPerforations::getPerforationsSet()
	{
		return PerforationsInTime;
	}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release --output-on-failure` (все тесты зелёные, новая перегрузка пока не вызывается)

**Подводные камни:**
- Non-const перегрузка перегружает по const-квалификации `this`. При вызове на non-const объекте будет выбрана non-const версия, на const — const версия. Все текущие вызовы `getPerforationsSet()` в `SetOfPoints.cpp` (строки ~412, 420, 422, 423, 426, 433, 434, 435, 436) происходят из const-методов (`getPerforations`, `getPerforations_future`) — для них по-прежнему будет выбрана const-перегрузка.

**Зависимости:**
- Требует: —
- Блокирует: шаг 2

**Оценка:** ~5 строк, ~5 минут

---

### Шаг 2: Устранить `const_cast` в конструкторе `SomeWell`

**Цель:** Заменить `const_cast` на прямой доступ к полю `ItsAccumulatedPerforations` в конструкторе.

**Файлы:** `HydroSolver/Reservoir/Well/SomeWell.cpp`

**Контекст:**
В конструкторе `SomeWell` (строка ~99–149 в `SomeWell.cpp`) цикл на строке ~113 итерирует перфорации через `PerforationConfiguration()` (const-геттер). На строке ~143 результат мутируется через `const_cast`. Поскольку конструктор — non-const член класса, он имеет прямой доступ к private полю `ItsAccumulatedPerforations` (объявлено в `SomeWell.h:144`).

**Что сделать:**

1. В `SomeWell.cpp`, строка ~113: заменить `PerforationConfiguration()` на `ItsAccumulatedPerforations`:

До:
```cpp
			for (const auto& [layer_id, perfs] : PerforationConfiguration())
```

После:
```cpp
			for (auto& [layer_id, perfs] : ItsAccumulatedPerforations)
```

2. В `SomeWell.cpp`, строка ~143: убрать `const_cast`:

До:
```cpp
				const_cast<set_of_points::AccumulatedPerforations&>(perfs).AveragePerforationsOut(jobIntervals);
```

После:
```cpp
				perfs.AveragePerforationsOut(jobIntervals);
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release --output-on-failure` (все тесты зелёные)
- Проверить: integration-тесты с перфорациями (`test_3d_completions`, `test_variable_debit`)

**Подводные камни:**
- `perfs` теперь non-const ref. Внутри цикла (строки 116–141) `perfs` используется только для чтения (`AllJobMoments()`, `EarliestJobDate()` — const-методы), а на строке 143 — для мутации. Non-const ref не ломает read-only вызовы.

**Зависимости:**
- Требует: —
- Блокирует: —

**Оценка:** ~2 строки, ~5 минут

---

### Шаг 3: Устранить `const_cast` в `BringFirstPerforationToFirstMER`

**Цель:** Заменить `const_cast` на прямой доступ к полю и non-const `getPerforationsSet()`.

**Файлы:** `HydroSolver/Reservoir/Well/SomeWell.cpp`

**Контекст:**
Метод `BringFirstPerforationToFirstMER()` (строка ~297–321) сдвигает дату самой ранней перфорации к дате первой записи MER. На строке ~301 читает перфорации (const), на строке ~312 мутирует их через `const_cast`. Два `const_cast`: один на `SetOfPerforations` для вызова `MoveDate()`, один на `AccumulatedPerforations` для вызова `AssembleJobDates()`.

**Что сделать:**

1. Строка ~301: заменить `PerforationConfiguration()` на `ItsAccumulatedPerforations`:

До:
```cpp
			for (const auto& [layer_id, perf] : PerforationConfiguration())
```

После:
```cpp
			for (const auto& [layer_id, perf] : ItsAccumulatedPerforations)
```

2. Строка ~312: заменить `PerforationConfiguration()` на `ItsAccumulatedPerforations`:

До:
```cpp
				for (auto& [layer_id, perf] : PerforationConfiguration())
```

После:
```cpp
				for (auto& [layer_id, perf] : ItsAccumulatedPerforations)
```

3. Строки ~315–317: убрать оба `const_cast`, использовать non-const `getPerforationsSet()`:

До:
```cpp
						const_cast<set_of_points::SetOfPerforations&>(
							perf.getPerforationsSet().front()).MoveDate(date);
						const_cast<set_of_points::AccumulatedPerforations&>(perf).AssembleJobDates();
```

После:
```cpp
						perf.getPerforationsSet().front().MoveDate(date);
						perf.AssembleJobDates();
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release --output-on-failure` (все тесты зелёные)

**Подводные камни:**
- `perf` в цикле строки ~312 объявлен `auto&`. Поскольку `ItsAccumulatedPerforations` — non-const `std::map<size_t, AccumulatedPerforations>`, structured binding даст `size_t& layer_id, AccumulatedPerforations& perf` — оба non-const. `perf.getPerforationsSet()` вызовет non-const перегрузку (добавленную в шаге 1), возвращающую `vector<SetOfPerforations>&`, и `.front()` даст non-const `SetOfPerforations&` — `MoveDate` вызывается корректно.
- Первый цикл (строка ~301) оставлен `const auto&` — он только читает (`EarliestJobDate()` — const-метод).

**Зависимости:**
- Требует: шаг 1 (non-const `getPerforationsSet()`)
- Блокирует: —

**Оценка:** ~6 строк, ~5 минут

---

### Шаг 4: Устранить `const_cast` в `BringLastPerforationToLastMER`

**Цель:** Аналогично шагу 3 — устранить `const_cast` в `BringLastPerforationToLastMER`.

**Файлы:** `HydroSolver/Reservoir/Well/SomeWell.cpp`

**Контекст:**
Метод `BringLastPerforationToLastMER()` (строка ~323–343) сдвигает дату последней перфорации к дате последней записи MER. Структурно идентичен `BringFirstPerforationToFirstMER`: два цикла (read-only + mutating), два `const_cast`.

**Что сделать:**

1. Строка ~326: заменить `PerforationConfiguration()` на `ItsAccumulatedPerforations`:

До:
```cpp
			for (const auto& [layer_id, perf] : PerforationConfiguration())
```

После:
```cpp
			for (const auto& [layer_id, perf] : ItsAccumulatedPerforations)
```

2. Строка ~334: заменить `PerforationConfiguration()` на `ItsAccumulatedPerforations`:

До:
```cpp
				for (auto& [layer_id, perf] : PerforationConfiguration())
```

После:
```cpp
				for (auto& [layer_id, perf] : ItsAccumulatedPerforations)
```

3. Строки ~337–339: убрать оба `const_cast`:

До:
```cpp
						const_cast<set_of_points::SetOfPerforations&>(
							perf.getPerforationsSet().back()).MoveDate(date);
						const_cast<set_of_points::AccumulatedPerforations&>(perf).AssembleJobDates();
```

После:
```cpp
						perf.getPerforationsSet().back().MoveDate(date);
						perf.AssembleJobDates();
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release --output-on-failure` (все тесты зелёные)
- Grep: `const_cast` в `SomeWell.cpp` — 0 результатов

**Подводные камни:**
- Аналогично шагу 3. `.back()` вместо `.front()` — единственное отличие.

**Зависимости:**
- Требует: шаг 1 (non-const `getPerforationsSet()`)
- Блокирует: —

**Оценка:** ~6 строк, ~5 минут

---

## Тестовая стратегия

### Тест-воспроизводитель

Этот баг — latent UB, не вызывающий crash на текущем компиляторе. Прямого unit-теста для воспроизведения UB нет (UBSan недоступен на MSVC). Верификация — через статический анализ: после фикса `grep const_cast SomeWell.cpp` должен вернуть 0 результатов.

### Regression

Поведение не меняется — те же мутации, без UB. Все существующие тесты должны остаться зелёными:

- `test_3d_completions` — integration-тест с перфорациями, выполняет конструктор `SomeWell` и `BringFirst/LastPerforationToFirstMER`
- `test_variable_debit` — скважины с переменным дебитом, аналогично инициализирует перфорации
- Все остальные 287 тестов

### Визуальная верификация

Не требуется — фикс не меняет вычисления, только устраняет UB в пути доступа.

---

## Критерии завершения

- [ ] 0 `const_cast` в `SomeWell.cpp` (`grep const_cast SomeWell.cpp` → пусто)
- [ ] Non-const `getPerforationsSet()` добавлена в `AccumulatedPerforations`
- [ ] Все существующие тесты зелёные (Release + Debug)
- [ ] Нет новых warnings при сборке
- [ ] Vault обновлён: BUG-010 → ✅ исправлено, заметка в debugging
- [ ] GitHub issue #10 прокомментирован с результатом
