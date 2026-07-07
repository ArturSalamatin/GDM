---
tags:
  - план
  - инфраструктура
date: 2026-07-07
issue: DEBT-048
github: 17
branch: refactor/debt-048/well-jacobian-dpwell
status: в процессе
audit:
  date: 2026-07-07
  findings: 0 / 0 / 2
  auto-fixed: 2
  manual-required: 0
---

# DEBT-048: Неполный якобиан скважины — добавить ∂P_well/∂P_res

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки: [[BUG-020 несогласованность единиц расхода в формуле Писмана]]
3. Прочитай текущее состояние кода: `HydroSolver/Reservoir/Well/Wells.cpp` (101 строка — можно целиком)
4. Создай ветку: `git checkout -b refactor/debt-048/well-jacobian-dpwell experimental`
5. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
6. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
7. Запомни количество тестов и время — это baseline
8. Прогони benchmark с профилированием Newton (шаг 1). Запомни числа — это baseline сходимости
9. Начни с шага 1. После каждого шага: сборка + тесты

## Связанные vault-заметки

- [[BUG-020 несогласованность единиц расхода в формуле Писмана]] — предыстория: при фиксе BUG-020 обнаружена пустая секция якобиана для ∂P_well
- [[стратегия тестирования GDM]] — паттерн: regression на производительность при структурном изменении солвера
- [[полностью неявная схема линеаризуется методом Ньютона]] — линеаризация J·δy = −F, блочная структура якобиана

---

## Фаза 1: Анализ

### 1.1. Текущее состояние (baseline)

В функции `WellFixedProduction::AddWellToMatrix()` (`HydroSolver/Reservoir/Well/Wells.cpp:19-74`) якобиан скважины содержит три компоненты:

1. **∂/∂Sw** (строка 51-52): производная `OverallMobility(l)` по `Sw` — вклад изменения подвижности
2. **∂/∂P_res** (строка 54-55): прямой вклад `factor[l] · OverallMobility(l)` — как будто `P_well` не зависит от `P_res`
3. **∂F_Oil/∂Sw** (строка 63-65): вклад изменения фракции нефти

**Что отсутствует:** зависимость `P_well` от `P_res(l)`. Забойное давление `P_well` вычисляется в `SetRefWellPressure()` (`Wells.cpp:76-98`):

```
P_well = (−Q_total + Σ_k ρ_mix(k)·PI(k)·P_res(k)) / (Σ_k ρ_mix(k)·PI(k))
```

где `PI(k) = factor[k] · OverallMobility(k)`, `ρ_mix(k) = ρ_oil · F_Oil(k) + ρ_water · (1 − F_Oil(k))`.

Производная по `P_res(l)` (при фиксированных `Sw`):

```
∂P_well/∂P_res(l) = ρ_mix(l)·PI(l) / D,    D = Σ_k ρ_mix(k)·PI(k)
```

Текущий якобиан rhs-строки для нефти (блок [1]):
```
∂rhs_oil/∂P_res(l) = −ρ_oil · F_Oil(l) · factor[l] · λ(l) · (∂P_res/∂P_res − ∂P_well/∂P_res)
```

Сейчас `∂P_well/∂P_res` = 0, поэтому записано просто `ρ_oil · factor · F_Oil · λ`.

### 1.2. Целевое состояние

Добавить вычитаемое `−dPw_dP(l)` в блоки [1] (oil, P) и [3] (water, P):

```
matrixBlockPerPerforation[l][1] -= ρ_oil · factor[l] · F_Oil(l) · OverallMobility(l) · dPw_dP(l)
matrixBlockPerPerforation[l][3] -= ρ_water · factor[l] · (1−F_Oil(l)) · OverallMobility(l) · dPw_dP(l)
```

Ожидаемый эффект: Newton получает точный якобиан для зависимости от давления → **квадратичная сходимость** вместо линейной для скважинных ячеек.

### 1.3. Варианты решения

#### Вариант A: Вычислить D в начале AddWellToMatrix, использовать в цикле якобиана

Суть: перед циклом якобиана пройти по перфорациям и вычислить `D = Σ ρ_mix·PI`. Затем в цикле якобиана для каждой перфорации вычислить `dPw_dP(l) = ρ_mix(l)·PI(l)/D` и вычесть из блоков [1] и [3].

Изменения:
- `Wells.cpp:38`: перед циклом якобиана добавить 6 строк для вычисления D
- `Wells.cpp:55-56`: после текущих строк добавить 4 строки для dPw_dP

Плюсы: минимальный diff (~10 строк), логика в одном месте, `D` вычисляется один раз.
Минусы: дублирование цикла вычисления `D` (то же самое делает `SetRefWellPressure`).

#### Вариант B: Кэшировать D в SetRefWellPressure как поле класса

Суть: `SetRefWellPressure` уже вычисляет `denom = Σ ρ_mix·PI`. Сохранить его в поле `denom_pw_` и использовать в якобиане.

Изменения:
- `SomeWell.h`: добавить `double denom_pw_ = 0;`
- `Wells.cpp SetRefWellPressure`: `denom_pw_ = denom;`
- `Wells.cpp AddWellToMatrix`: использовать `denom_pw_` вместо пересчёта

Плюсы: нет дублирования цикла.
Минусы: связывание через mutable state, порядок вызовов (`SetRefWellPressure` до `AddWellToMatrix`) становится критичным, усложняется отладка.

#### Выбор: Вариант A

Обоснование:
1. Дублирование 6-строчного цикла — дешевле, чем ошибка из-за нарушения порядка вызовов
2. Минимальный diff = минимальный риск
3. `D` в `SetRefWellPressure` и `D` в якобиане вычисляются из одних и тех же данных (OverallMobility, factor, F_Oil, rho) в одном и том же состоянии — расхождения быть не может
4. Если в будущем потребуется `∂P_well/∂Sw` (дополнительный вклад) — цикл легко расширить

### 1.4. Поиск подводных камней

- ✅ **Побочные эффекты:** `AddWellToMatrix` вызывается из `ReservoirSimulator.cpp:527`. Результат — CellNumericalData, передаётся в сборку матрицы. Изменение блоков [1] и [3] влияет только на якобиан, не на rhs → баланс масс не меняется. Но сходимость Newton меняется → число итераций может измениться
- ✅ **Потокобезопасность:** скважины обрабатываются последовательно в цикле `for (auto& [name, well] : Wells)` (ReservoirSimulator.cpp:524). Нет OpenMP
- ✅ **Граничные случаи:**
  - `D = 0`: невозможно, если хотя бы одна ячейка имеет ненулевую подвижность. Если все подвижности нулевые, `SetRefWellPressure` уже обрабатывает этот случай (fallback на avg_P). В якобиане `factor[l]·OverallMobility(l)` тоже ноль → вклад в блоки [1] и [3] ноль. Но для защиты от деления на ноль — использовать `if (D > 0)` (аналогично `SetRefWellPressure`)
  - Одна перфорация: `dPw_dP(0) = 1` — корректно: P_well = P_res (при Q=0 и одной перфорации)
  - `dP < 0` (инжекция в ячейке): не влияет — dPw_dP не зависит от знака dP
- ✅ **Производительность:** один дополнительный цикл по NmbrOfOpenedCells() (обычно 1-10 ячеек) — пренебрежимо мало по сравнению с solve
- ✅ **Обратная совместимость:** изменение якобиана влияет на скорость сходимости Newton, не на решение. Конечный результат (давления, насыщенности) не должен измениться при достаточно малой Newton tolerance
- ✅ **Порядок вызовов:** `SetRefWellPressure()` вызывается ДО цикла якобиана в той же функции AddWellToMatrix (строка 22). Значит OverallMobility, factor, F_Oil — актуальны
- ⚠️ **Число итераций Newton:** точный якобиан обычно улучшает сходимость, но в нелинейных системах бывают исключения. **Необходим замер до/после** — требование пользователя
- ✅ **Связь с другими задачами:** блок `else { // derivative of balanced oil }` (строка 69-70) — незаполненный. Это отдельная задача (∂F_OilWellBalance/∂Sw для инжектирующих перфораций). DEBT-048 не конфликтует

### 1.5. Обнаруженные проблемы

**Проблема 1:** `∂P_well/∂Sw(l)` тоже отсутствует в якобиане. `P_well` зависит от `Sw` через `OverallMobility(l)` и `F_Oil(l)` (через `rhoMix`). Это означает, что блоки [0] и [2] тоже неполные. Однако этот вклад сложнее (требует `dOverallMobility/dSw`, `dF_Oil/dSw`, и `drhoMix/dSw`) и является **отдельной задачей**. Не блокирует DEBT-048.

**Проблема 2:** незаполненный блок `else { // derivative of balanced oil }` (строка 69-70) — для инжектирующих перфораций (dP < 0) производная `F_OilWellBalance` по `Sw` не учтена. Отдельная задача.

Обе проблемы не блокируют DEBT-048 и будут зарегистрированы после завершения плана.

---

## Фаза 2: Детализация плана

### Шаг 1: Baseline — зафиксировать текущие метрики Newton

**Цель:** получить baseline для сравнения числа итераций Newton до и после изменения якобиана

**Файлы:** нет изменений в коде

**Контекст:**
Benchmark-тесты (`tests/test_amgcl_benchmark.cpp`) уже собирают `n_newton_iters`, `n_time_steps`, `n_amg_solves`, `t_total`, `t_assembly`, `t_solve`. Тесты помечены `[.slow]` — запускаются только явно. Нам нужен baseline от секции CPR1 (production CPR) — это стандартная конфигурация.

Тест `test_3d_completions.cpp` тоже пишет `solver_profile.csv` с Newton iterations.

**Что сделать:**

1. Собрать проект: `cmake --build build --config Release`
2. Запустить benchmark CPR1:
   ```
   ctest --test-dir build -C Release -R "Series CPR" --output-on-failure
   ```
3. Запустить 3D completions:
   ```
   ctest --test-dir build -C Release -R "3D completions" --output-on-failure
   ```
4. Записать в этот файл (раздел «Baseline» ниже) значения: `n_newton_iters`, `n_time_steps`, `t_total`
5. Прогнать все тесты: `ctest --test-dir build -C Release --output-on-failure` — записать количество и время

**Проверка после этого шага:**
- Все тесты зелёные (294 тестов по состоянию на 2026-07-07)
- Benchmark-числа записаны

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 3 (сравнение)

**Оценка:** ~0 строк кода, ~5 минут

### Шаг 2: Реализация — добавить ∂P_well/∂P_res в якобиан

**Цель:** дополнить якобиан скважины точной производной забойного давления по пластовым давлениям

**Файлы:** `HydroSolver/Reservoir/Well/Wells.cpp`

**Контекст:**

Забойное давление `P_well` вычисляется в `SetRefWellPressure()` как:

```
P_well = (−Q_total + Σ_k ρ_mix(k)·PI(k)·P_res(k)) / D
D = Σ_k ρ_mix(k)·PI(k)
```

Дебит перфорации:
```
production(l) = factor(l) · λ(l) · (P_res(l) − P_well)
```

rhs нефтяной фазы:
```
rhs_oil(l) = −ρ_oil(l) · F_Oil(l) · production(l)
```

Якобиан по P_res(l):
```
∂rhs_oil/∂P_res(l) = −ρ_oil · F_Oil · factor · λ · (1 − ∂P_well/∂P_res(l))
```

Сейчас записано `−ρ_oil · F_Oil · factor · λ · 1` (строки 54-55), т.е. `∂P_well/∂P_res` считается нулевым.

Производная:
```
∂P_well/∂P_res(l) = ρ_mix(l) · PI(l) / D
```
где `PI(l) = factor(l) · OverallMobility(l)`, `ρ_mix(l) = ρ_oil(l) · F_Oil(l) + ρ_water(l) · (1 − F_Oil(l))`.

**Физический смысл:** когда давление `P_res(l)` в перфорированной ячейке `l` растёт на δP, забойное давление `P_well` тоже растёт, но на долю δP, пропорциональную «весу» перфорации `l` в общей формуле Писмана. Поэтому чистое давление-перепад `(P_res(l) − P_well)` меняется на `(1 − dPw_dP)·δP`, а не на `δP`.

**Свойство:** `Σ_l dPw_dP(l) = D/D = 1` — сумма весов равна единице. Для отладки: если сумма отклоняется от 1 — ошибка в D_pw.

**Что сделать:**

1. В файле `Wells.cpp`, функция `AddWellToMatrix()`, **перед циклом якобиана** (перед строкой 39 `for (size_t l = 0; l < NmbrOfOpenedCells(); l++)`), добавить вычисление знаменателя D:

2. В том же цикле якобиана, **после строк 54-55** (текущий вклад ∂/∂P) и **перед строкой 57** (`// derivative of P_Well`), добавить вычисление dPw_dP и вычитание из блоков [1] и [3]

**Изменения (старый → новый код):**

До (строки 38-58):
```cpp
			// fill in the matrix blocks of the linear problem
			for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
			{
				if (!isfinite(P_Reservoir(l)) || !isfinite(P_Well[l])
					|| !isfinite(OverallMobility(l)) || !isfinite(Derivative_F_Oil(l))
					|| !isfinite(factor[l]) || !isfinite(DerivativeOverallMobility(l)))
					throw std::runtime_error("well production is not determined. Date:" + std::to_string(nextTimeMoment));

				double dP = P_Reservoir(l) - P_Well[l];
				double rhoO = CellDensityOil(l);
				double rhoW = CellDensityWater(l);

				// derivative of OverallMobility with S_Water
				matrixBlockPerPerforation[l][0] += rhoO * factor[l] * F_Oil(l) * DerivativeOverallMobility(l) * dP;
				matrixBlockPerPerforation[l][2] += rhoW * factor[l] * (1 - F_Oil(l)) * DerivativeOverallMobility(l) * dP;
				// derivative of P_Reservoir
				matrixBlockPerPerforation[l][1] += rhoO * factor[l] * F_Oil(l) * OverallMobility(l);
				matrixBlockPerPerforation[l][3] += rhoW * factor[l] * (1 - F_Oil(l)) * OverallMobility(l);

				// derivative of P_Well
				/////!!!!!!!!!!!////////////
```

После:
```cpp
			// denominator for dP_well/dP_res: D = Σ ρ_mix(k)·PI(k)
			double D_pw = 0.0;
			for (size_t k = 0; k < NmbrOfOpenedCells(); k++)
			{
				double f = F_Oil(k);
				double rhoMix = CellDensityOil(k) * f + CellDensityWater(k) * (1 - f);
				D_pw += rhoMix * factor[k] * OverallMobility(k);
			}

			// fill in the matrix blocks of the linear problem
			for (size_t l = 0; l < NmbrOfOpenedCells(); l++)
			{
				if (!isfinite(P_Reservoir(l)) || !isfinite(P_Well[l])
					|| !isfinite(OverallMobility(l)) || !isfinite(Derivative_F_Oil(l))
					|| !isfinite(factor[l]) || !isfinite(DerivativeOverallMobility(l)))
					throw std::runtime_error("well production is not determined. Date:" + std::to_string(nextTimeMoment));

				double dP = P_Reservoir(l) - P_Well[l];
				double rhoO = CellDensityOil(l);
				double rhoW = CellDensityWater(l);

				// derivative of OverallMobility with S_Water
				matrixBlockPerPerforation[l][0] += rhoO * factor[l] * F_Oil(l) * DerivativeOverallMobility(l) * dP;
				matrixBlockPerPerforation[l][2] += rhoW * factor[l] * (1 - F_Oil(l)) * DerivativeOverallMobility(l) * dP;
				// derivative of P_Reservoir (direct contribution)
				matrixBlockPerPerforation[l][1] += rhoO * factor[l] * F_Oil(l) * OverallMobility(l);
				matrixBlockPerPerforation[l][3] += rhoW * factor[l] * (1 - F_Oil(l)) * OverallMobility(l);

				// derivative of P_well w.r.t. P_res(l): dPw/dP = ρ_mix(l)·PI(l) / D
				if (D_pw > 0.0)
				{
					double f = F_Oil(l);
					double rhoMix = rhoO * f + rhoW * (1 - f);
					double dPw_dP = rhoMix * factor[l] * OverallMobility(l) / D_pw;
					matrixBlockPerPerforation[l][1] -= rhoO * factor[l] * F_Oil(l) * OverallMobility(l) * dPw_dP;
					matrixBlockPerPerforation[l][3] -= rhoW * factor[l] * (1 - F_Oil(l)) * OverallMobility(l) * dPw_dP;
				}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release` — без ошибок и без warnings
- Тесты: `ctest --test-dir build -C Release --output-on-failure` — все 294 теста зелёные
- Регрессия: числа массового баланса не должны ухудшиться (тесты с tolerance на баланс)

**Подводные камни:**
- Переменная `f` в блоке `if (D_pw > 0.0)` — локальная, не конфликтует с `f` в цикле D_pw (разные скоупы)
- `D_pw = 0` при нулевых подвижностях — защищено `if (D_pw > 0.0)`, аналогично `SetRefWellPressure`
- Для одной перфорации: `dPw_dP = 1`, значит `∂/∂P_res` обнуляется — блоки [1] и [3] = 0. Это корректно: если одна перфорация, P_well следит за P_res, и изменение P_res не меняет dP

**Зависимости:**
- Требует: шаг 1 (baseline)
- Блокирует: шаг 3 (верификация)

**Оценка:** ~13 строк, ~10 минут

### Шаг 3: Верификация — сравнить метрики Newton с baseline

**Цель:** убедиться, что точный якобиан не ухудшает сходимость Newton и сравнить с baseline

**Файлы:** нет изменений в коде

**Контекст:**
Пользователь ожидает, что изменение якобиана может ухудшить сходимость. Теоретически точный якобиан должен улучшить сходимость (квадратичная вместо линейной), но на практике нелинейные эффекты могут давать неожиданные результаты, особенно при крупных временных шагах. Замер обязателен.

**Что сделать:**

1. Запустить те же benchmark-тесты, что и в шаге 1:
   ```
   ctest --test-dir build -C Release -R "Series CPR" --output-on-failure
   ctest --test-dir build -C Release -R "3D completions" --output-on-failure
   ```
2. Записать `n_newton_iters`, `n_time_steps`, `t_total`
3. Сравнить с baseline:
   - `n_newton_iters`: ожидаем уменьшение (или без изменений). Если увеличение > 10% — исследовать
   - `n_time_steps`: не должно измениться (PI-контроллер реагирует на Newton iterations, но tolerance тот же)
   - `t_total`: ожидаем уменьшение или без изменений. Если увеличение > 5% — исследовать
4. Прогнать все 294 теста: `ctest --test-dir build -C Release --output-on-failure`

**Действия при ухудшении:**
- Если `n_newton_iters` вырос: проверить, не стала ли матрица хуже обусловленной. Возможная причина: для одной перфорации диагональ якобиана обнуляется. Это корректно математически, но может ухудшить обусловленность. Варианты:
  - Ослабить вклад: `dPw_dP *= 0.8` (damping) — но это ad hoc
  - Проверить, только ли для одной перфорации проблема — если да, можно добавить `if (NmbrOfOpenedCells() > 1)`
  - Откатить изменение и зарегистрировать как RES (требует исследования)
- Если `t_total` вырос: скорее всего из-за Newton iterations, см. выше

**Проверка после этого шага:**
- Все 294 теста зелёные
- Метрики Newton записаны
- Сравнение с baseline задокументировано

**Зависимости:**
- Требует: шаг 1 (baseline), шаг 2 (реализация)
- Блокирует: шаг 4 (документация)

**Оценка:** ~0 строк кода, ~5 минут

### Шаг 4: Документация — обновить vault и прокомментировать issue

**Цель:** зафиксировать результат в vault и на GitHub

**Файлы:**
- `vault/GDM/roadmap/технический долг.md` — обновить статус DEBT-048
- `vault/GDM/00-home/текущие приоритеты.md` — обновить если DEBT-048 был в приоритетах

**Что сделать:**

1. В `vault/GDM/roadmap/технический долг.md`, секция DEBT-048:
   - Изменить `**Статус:** 🟡 ОТКРЫТ` → `**Статус:** ✅ исправлено 2026-07-XX`
2. Обновить `vault/GDM/00-home/текущие приоритеты.md` если DEBT-048 упомянут
3. Прокомментировать GitHub issue #17:
   ```
   Реализовано в ветке refactor/debt-048/well-jacobian-dpwell.
   Newton iterations: <baseline> → <после> (Δ<N>%).
   Тесты: <N> passed. Готово к merge.
   ```

**Проверка после этого шага:**
- Vault обновлён
- GitHub issue прокомментирован

**Зависимости:**
- Требует: шаг 3 (метрики для комментария)
- Блокирует: ничего

**Оценка:** ~5 строк в vault, ~5 минут

---

## Baseline

_Заполняется при выполнении шага 1._

| Метрика | Baseline | После DEBT-048 | Δ |
|---|---|---|---|
| Тесты (Release) | | | |
| n_newton_iters (CPR1) | | | |
| n_time_steps (CPR1) | | | |
| t_total (CPR1) | | | |
| n_newton_iters (3D completions) | | | |

---

## Тестовая стратегия

### Regression

Все 294 существующих теста должны оставаться зелёными. Изменение якобиана не меняет rhs и не меняет решение — только скорость сходимости Newton. Если какой-то тест начнёт падать, это означает, что точный якобиан раскрыл скрытую нестабильность — исследовать.

Ключевые тесты, чувствительные к скважинному якобиану:
- `[balance]` — баланс масс. Не должен ухудшиться
- `[buckley_leverett]` — аналитика. Tolerance не меняется
- `[benchmark]` — Newton iterations. Замер до/после
- `[five_spot]` — симметрия. Не должна нарушиться
- `[3d_completions]` — 3D скважины. solver_profile.csv

### Инвариантный

Баланс масс остаётся прежним (rhs не меняется, только якобиан). Если баланс ухудшился — это регрессия, не связанная с DEBT-048.

### Visual

Визуальная верификация не требуется — изменение чисто численное (скорость сходимости), не физическое. Если benchmark покажет аномалию, тогда визуализировать.

---

## Критерии завершения

- [ ] Все существующие тесты зелёные (Release и Debug)
- [ ] Benchmark Newton iterations: не хуже baseline (или объяснено почему хуже)
- [ ] Vault обновлён: DEBT-048 закрыт
- [ ] GitHub issue #17 прокомментирован
- [ ] Warnings: ноль (оба конфига)
- [ ] План обновлён: `status: реализован`

---

## Обнаруженные проблемы

1. **∂P_well/∂Sw(l) отсутствует** — P_well зависит от Sw через OverallMobility и rhoMix. Это делает блоки [0] и [2] неполными. Отдельная задача, сложнее чем DEBT-048 (требует dOverallMobility/dSw, dF_Oil/dSw, drhoMix/dSw — все уже есть, но формула длиннее). Зарегистрировать как DEBT-049.

2. **Незаполненный блок ∂F_OilWellBalance/∂Sw** — строки 69-70 в Wells.cpp: `else { // derivative of balanced oil }`. Для инжектирующих перфораций (dP < 0) производная F_OilWellBalance по Sw не учтена. Зарегистрировать как DEBT-050.
