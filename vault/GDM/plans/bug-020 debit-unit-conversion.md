---
tags:
  - план
  - баг
date: 2026-07-07
issue: BUG-020
github: 16
branch: fix/bug-020/debit-unit-conversion
status: в процессе
audit:
  date: 2026-07-07
  findings: 0 / 3 / 3
  auto-fixed: 4
  manual-required: 0
---

# BUG-020: отсутствие множителя плотности в скважинном source term и Якобиане

## Как начать работу по этому плану

1. Прочитай этот файл целиком
2. Прочитай связанные vault-заметки:
   - [[несогласованность единиц расхода в формуле Писмана]]
   - [[задача Бакли-Леверетта — аналитический тест для одномерного вытеснения]]
3. Создай ветку: `git checkout -b fix/bug-020/debit-unit-conversion experimental`
4. Собери: `cmake -B build -S . -G "Visual Studio 17 2022"; if ($?) { cmake --build build --config Release }`
5. Прогони тесты (baseline): `ctest --test-dir build -C Release --output-on-failure`
6. Запомни количество тестов и время — это baseline
7. Воспроизведи баг: `ctest --test-dir build -C Release -R "volume balance"`
   → 2 теста `[!mayfail]` — «failed as expected», qt_eff ≈ 0.004 вместо 1.0
8. Начни с шага 1. После каждого шага: сборка + тесты

## Суть бага

### Симптом

VAL-001 Buckley-Leverett: qt_eff ≈ 0.004 м/день при номинальном Q = 1000 кг/день воды (ожидание qt = Q_vol/(A) = 1.0 м/день). Фронт вытеснения проходит ~8 м за 400 дней вместо ожидаемых ~200 м.

### Корневая причина (уточнённая после детального анализа)

Уравнения фильтрации в GDM записаны как **баланс масс** — все слагаемые в кг/день:

```
(M_prev - M_cur)/τ + Σ_neighbors [ρ · f · λ · (A/L) · ΔP] + q_well = 0
```

Межъячеечный поток `ρ · f · λ · (A/L) · ΔP` содержит множитель плотности ρ [кг/м³], что переводит объёмный поток [м³/день] в массовый [кг/день]. Аналогично, Якобиан межъячеечного потока по давлению: `ρ · f · λ · A/L` [кг/(Па·день)].

**Скважинный source term не содержит множитель ρ:**

```cpp
// Wells.cpp:32-33 — rhs
rhsPerPerforation[l][0] = -F_Oil(l) * productions[l];          // [м³/день], нет ρ_oil
rhsPerPerforation[l][1] = -(1 - F_Oil(l)) * productions[l];    // [м³/день], нет ρ_water
```

```cpp
// Wells.cpp:49-53 — Якобиан
matrixBlock[l][0] += factor[l] * F_Oil(l) * dλ/dSw * dP;       // [м³/день], нет ρ_oil
matrixBlock[l][1] += factor[l] * F_Oil(l) * λ;                  // [м³/(Па·день)], нет ρ_oil
matrixBlock[l][2] += factor[l] * (1-F_Oil(l)) * dλ/dSw * dP;   // [м³/день], нет ρ_water
matrixBlock[l][3] += factor[l] * (1-F_Oil(l)) * λ;              // [м³/(Па·день)], нет ρ_water
```

Должно быть:

```
rhs[0] = -ρ_oil   · F_Oil   · production   [кг/день]
rhs[1] = -ρ_water · (1-F_Oil) · production [кг/день]

Якобиан[0] += ρ_oil   · factor · F_Oil · dλ/dSw · dP    [кг/день]
Якобиан[1] += ρ_oil   · factor · F_Oil · λ               [кг/(Па·день)]
Якобиан[2] += ρ_water · factor · (1-F_Oil) · dλ/dSw · dP [кг/день]
Якобиан[3] += ρ_water · factor · (1-F_Oil) · λ            [кг/(Па·день)]
```

### Почему замена oil_m → oil_v НЕ помогает

Первоначальный анализ считал проблемой то, что `CurOverallDebit()` возвращает кг/день, а формула Писмана ожидает м³/день. Но `SetRefWellPressure` устроена так, что `Σ production_l ≡ CurOverallDebit()` — **алгебраическое тождество**, верное при любых единицах Q:

```
P_well = (-Q + Σ PI·λ·P) / (Σ PI·λ)
production_l = PI_l·λ_l·(P_l - P_well)
Σ production_l = Σ PI·λ·P - P_well · Σ PI·λ = Q
```

Поэтому `production = Q` в тех же единицах, что `CurOverallDebit()`, и rhs = F_Oil · Q. Если Q в кг/день → rhs в кг/день → теоретически согласовано с межъячеечным. Если Q в м³/день (после замены на oil_v) → rhs в м³/день → **хуже**, потому что rhs стал в ρ раз меньше.

Экспериментально подтверждено: замена `oil_m` → `oil_v` уменьшила qt_eff с 0.004 до 0.0005 (в ~7.5 раз хуже).

### Почему production = Q, а qt_eff ≠ Q/ρ/A

Проблема в **Якобиане**, а не в rhs.

Newton решает нелинейную систему F(x) = 0 итеративно: J·δx = -F(x), x := x + δx. Сходимость проверяется по `|δx| < tol·|x|` (не по |F(x)|).

Скважинный Якобиан `factor·F_Oil·λ ≈ 3.9e-5` [м³/(Па·день)] — в ~110 раз меньше межъячеечного `ρ·f·λ·A/L ≈ 4.3e-3` [кг/(Па·день)]. Линейный решатель «не видит» скважинный вклад, коррекции δP определяются межъячеечным потоком. Newton «сходится» за 1 итерацию по критерию коррекции, но **решение не удовлетворяет скважинному балансу** — фактический source term в ячейке ≪ Q.

### Единицы внутри солвера

| Величина | Текущие единицы | Нужные единицы |
|---|---|---|
| Accumulation: (M_prev - M_cur)/τ | кг/день | кг/день ✓ |
| Межъячеечный rhs: ρ·f·λ·A/L·ΔP | кг/день | кг/день ✓ |
| Межъячеечный Якобиан: ρ·f·λ·A/L | кг/(Па·день) | кг/(Па·день) ✓ |
| Скважинный rhs: F_Oil·production | **м³/день** | кг/день ✗ |
| Скважинный Якобиан: factor·F_Oil·λ | **м³/(Па·день)** | кг/(Па·день) ✗ |

**GitHub issue:** [#16](https://github.com/ArturSalamatin/GDM/issues/16)

## Анализ вариантов решения

### Вариант A: добавить ρ в скважинный rhs и Якобиан (выбранный)

**Суть:** в `AddWellToMatrix` (Wells.cpp) умножить rhs и Якобиан каждой фазы на соответствующую плотность. `WellEnvironment` получает доступ к `DensityOil(l)` и `DensityWater(l)` через `cells[ItsCurCellIDs[l]]`.

**Изменения:**
1. `SomeWell.h/cpp` — добавить методы `DensityOil(l)`, `DensityWater(l)` в `WellEnvironment`
2. `Wells.cpp:32-33` — rhs: `ρ_oil · F_Oil · production`, `ρ_water · (1-F_Oil) · production`
3. `Wells.cpp:49-53` — Якобиан ∂/∂Sw и ∂/∂P: добавить множители ρ_oil / ρ_water
4. `Wells.cpp:61-63` — Якобиан ∂F_Oil/∂Sw: добавить ρ_oil / ρ_water

**Плюсы:**
- Скважинный source и Якобиан в кг/день и кг/(Па·день) — согласованно с межъячеечным
- MER не меняется (Q остаётся в кг/день)
- BalanceOil не нужно менять (F_OilWellBalance — безразмерная доля, её вычисление не зависит от ρ)
- SetRefWellPressure не нужно менять (P_well используется только для вычисления production = Q, а ρ добавляется при формировании rhs)

**Минусы:**
- Доступ к ρ из скважинного кода — через `cells[ItsCurCellIDs[l]]->DensityOil()`. Нужно добавить 2 метода в WellEnvironment.
- Слагаемое `productions[l] · Derivative_F_Oil(l)` (строка 61) содержит `production` в м³/день и dF_Oil/dSw — нужно разделить на нефтяную и водную компоненты с разными ρ.

**Трудоёмкость:** ~3 файла, ~25 строк.

### Вариант B: перевести production в кг/день

**Суть:** в `SetProductions` или `SetRefWellPressure` умножить production на ρ_mix. Тогда rhs и Якобиан автоматически в кг/день.
**Проблема:** `production = PI·λ·ΔP` — объёмный расход смеси. Умножение на ρ_mix требует знания доли фаз (которая определяется в BalanceOil, вызываемой **после** SetProductions). Циклическая зависимость.
**Отвергнут.**

### Вариант C: перевести всё в объёмные единицы

**Суть:** убрать ρ из межъячеечных потоков — перейти к объёмному балансу вместо массового.
**Проблема:** масштабное изменение солвера, затрагивает fillMatrixBlockRow, accumulation, UpdateGrid. Несоразмерно задаче.
**Отвергнут.**

### Обоснование выбора варианта A

Минимальный скоуп: добавить ρ в одном месте (AddWellToMatrix). Не меняет MER, SetRefWellPressure, BalanceOil, SetProductions. Все изменения в одном файле (Wells.cpp) + 2 accessor-метода в SomeWell.

## Поиск подводных камней

- ✅ **Побочные эффекты:** rhs и Якобиан скважины формируются **только** в `AddWellToMatrix`. Умножение на ρ не затрагивает другие call sites.
- ✅ **BalanceOil:** вычисляет `F_OilWellBalance` как отношение `posProduction / |negProduction|` — обе величины в м³/день, ρ сокращается. Не нужно менять.
- ✅ **SetRefWellPressure:** вычисляет P_well из Q и PI·λ·P_res. Алгебраическое тождество production = Q. ρ не участвует. Не нужно менять.
- ✅ **CurOilDebit_Num/CurWaterDebit_Num:** используются в ReservoirSimulator.cpp:378,388 для отчётности (OilDebitTotal, WaterDebitTotal). Они вычисляют `CurOverallDebit() · F_OilWellBalance` — в кг/день. Это **отчётные** величины, не участвуют в СЛАУ. Не затрагиваются.
- ✅ **Потокобезопасность:** AddWellToMatrix вне OpenMP.
- ⚠️ **Граничные случаи:** при λ=0 (Sw=0): denom=0 в SetRefWellPressure → P_well = avg_P_res (строка 89-95). Production = PI·λ·ΔP = 0. rhs = ρ·F_Oil·0 = 0. Корректно: нулевой расход при нулевой подвижности. Адресовано: не меняет поведение.
- ✅ **Производительность:** DensityOil/DensityWater — чтение из массива DependentFieldProperties, O(1).
- ✅ **Обратная совместимость:** P_well станет физически осмысленным (десятки атм вместо сотен ГПа). Все потребители P_well (только SetProductions) получат корректный ΔP.
- ⚠️ **BalanceOil при нескольких перфорациях с разными ρ:** в текущем коде BalanceOil складывает productions[l] с CurOverallDebit(). Productions в м³/день, CurOverallDebit() в кг/день — несоразмерные. Но BalanceOil определяет только F_OilWellBalance (безразмерную долю), и при INJ с чистой водой (f_oil_in=0) результат всегда 0. Для PROD — F_Oil берётся из ячейки, а не из F_OilWellBalance. Проблема проявится при многопластовых скважинах с перфорациями, где production < 0 в одних слоях и > 0 в других. Зарегистрировать как отдельный DEBT. Не блокирует текущий фикс.
- ✅ **Связь с другими задачами:** BUG-002 (Newton divergence при Sw=0) обходится через Sw_init=0.2 — не затрагивается.

## Целевое состояние

После фикса:
- Скважинный rhs: `ρ_oil · F_Oil · production` и `ρ_water · (1-F_Oil) · production` [кг/день]
- Скважинный Якобиан: `ρ · factor · ... · λ` [кг/(Па·день)]
- Согласовано с межъячеечным (кг/день)
- Newton корректно балансирует скважинный и межъячеечный вклады
- P_well физически осмысленное (~десятки атм)
- Тест BL: qt_eff ≈ 1.0 м/день
- Тесты `[!mayfail]` → зелёные, тег снят

---

## Этап 1: Минимальный фикс

### Шаг 1: Добавить методы DensityOil(l), DensityWater(l) в WellEnvironment

**Цель:** обеспечить доступ к плотностям фаз из скважинного кода для умножения rhs и Якобиана.

**Файлы:** `HydroSolver/Reservoir/Well/SomeWell.h`, `HydroSolver/Reservoir/Well/SomeWell.cpp`

**Контекст:**
`WellEnvironment` (SomeWell.h) — базовый класс скважинных типов. Содержит `cells` — вектор указателей на `TwoPhaseFlowCell`. Через `cells[ItsCurCellIDs[l]]` уже доступны `P()`, `MobilityOverall()`, `F_Oil()`, `Derivative_F_Oil()`. Нужно добавить аналогичные accessor-ы для `DensityOil()` и `DensityWater()` — они уже есть в TwoPhaseFlowCell (TwoPhaseFlowCell.cpp:50-51).

**Что сделать:**

1. В `SomeWell.h`, после строки ~30 (рядом с объявлениями `OverallMobility`, `DerivativeOverallMobility`):
   добавить два объявления методов.

2. В `SomeWell.cpp`, после функции `Derivative_F_Oil` (~строка 34):
   добавить реализации.

**Изменения (SomeWell.h):**

До:
```cpp
		double OverallMobility(size_t l) const;
		double DerivativeOverallMobility(size_t l) const;
		double Derivative_F_Oil(size_t l) const;
```

После:
```cpp
		double OverallMobility(size_t l) const;
		double DerivativeOverallMobility(size_t l) const;
		double Derivative_F_Oil(size_t l) const;
		double CellDensityOil(size_t l) const;
		double CellDensityWater(size_t l) const;
```

**Изменения (SomeWell.cpp):**

До:
```cpp
	double WellEnvironment::Derivative_F_Oil(size_t l) const
	{
		return cells[ItsCurCellIDs[l]]->Derivative_F_Oil();
	}
```

После:
```cpp
	double WellEnvironment::Derivative_F_Oil(size_t l) const
	{
		return cells[ItsCurCellIDs[l]]->Derivative_F_Oil();
	}
	double WellEnvironment::CellDensityOil(size_t l) const
	{
		return cells[ItsCurCellIDs[l]]->DensityOil();
	}
	double WellEnvironment::CellDensityWater(size_t l) const
	{
		return cells[ItsCurCellIDs[l]]->DensityWater();
	}
```

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Ожидаемый результат: компилируется без ошибок (методы объявлены, но пока не вызываются).

**Подводные камни:**
- Имена `CellDensityOil`/`CellDensityWater` вместо `DensityOil`/`DensityWater` — чтобы не конфликтовать с потенциальными полями скважины.

**Зависимости:**
- Требует: ничего
- Блокирует: шаг 2

**Оценка:** ~10 строк, ~3 минуты

---

### Шаг 2: Добавить множитель ρ в скважинный rhs и Якобиан в AddWellToMatrix

**Цель:** привести скважинный source term и Якобиан к единицам кг/день и кг/(Па·день), согласованно с межъячеечным.

**Файлы:** `HydroSolver/Reservoir/Well/Wells.cpp`

**Контекст:**
`AddWellToMatrix` (Wells.cpp:19-78) формирует rhs и Якобиан скважинного слагаемого. Сейчас rhs = `F_Oil · production` [м³/день], Якобиан = `factor · F_Oil · λ` [м³/(Па·день)]. Межъячеечные — в кг/день и кг/(Па·день). Несогласованность приводит к тому, что Newton неправильно балансирует вклады, и фактическое вытеснение в ~275 раз меньше номинального.

Нужно добавить ρ_oil к нефтяным компонентам (индексы [0], [1]) и ρ_water к водным ([2], [3]).

Паттерн — тот же, что в межъячеечных потоках (ReservoirSimulator.cpp:597-612):
- rhs[0] ← ρ_oil · ... → кг/день
- rhs[1] ← ρ_water · ... → кг/день
- Якобиан ∂/∂Sw (блоки [0],[2]) ← ρ · ...
- Якобиан ∂/∂P (блоки [1],[3]) ← ρ · ...

**Что сделать:**

1. В блоке rhs (Wells.cpp:32-33): умножить на ρ.
2. В блоке Якобиана ∂λ/∂Sw (Wells.cpp:49-50): умножить на ρ.
3. В блоке Якобиана ∂/∂P (Wells.cpp:52-53): умножить на ρ.
4. В блоке Якобиана ∂F_Oil/∂Sw (Wells.cpp:61-63): использовать ρ_oil для [0] и ρ_water для [2].

**Изменения:**

До (rhs, строки 32-33):
```cpp
					rhsPerPerforation[l][0] = -F_Oil(l) * productions[l];
					rhsPerPerforation[l][1] = -(1 - F_Oil(l)) * productions[l];
```

После:
```cpp
					double rho_o = CellDensityOil(l);
					double rho_w = CellDensityWater(l);
					rhsPerPerforation[l][0] = -rho_o * F_Oil(l) * productions[l];
					rhsPerPerforation[l][1] = -rho_w * (1 - F_Oil(l)) * productions[l];
```

До (Якобиан ∂λ/∂Sw и ∂/∂P, строки 45-53):
```cpp
					double dP = P_Reservoir(l) - P_Well[l];

					// derivative of OverallMobility
					// with S_Water
					matrixBlockPerPerforation[l][0] += factor[l] * F_Oil(l) * DerivativeOverallMobility(l) * dP;
					matrixBlockPerPerforation[l][2] += factor[l] * (1 - F_Oil(l)) * DerivativeOverallMobility(l) * dP;
					// derivative of P_Reservoir
					matrixBlockPerPerforation[l][1] += factor[l] * F_Oil(l) * OverallMobility(l);
					matrixBlockPerPerforation[l][3] += factor[l] * (1 - F_Oil(l)) * OverallMobility(l);
```

После:
```cpp
					double dP = P_Reservoir(l) - P_Well[l];
					double rho_o = CellDensityOil(l);
					double rho_w = CellDensityWater(l);

					// derivative of OverallMobility
					// with S_Water
					matrixBlockPerPerforation[l][0] += rho_o * factor[l] * F_Oil(l) * DerivativeOverallMobility(l) * dP;
					matrixBlockPerPerforation[l][2] += rho_w * factor[l] * (1 - F_Oil(l)) * DerivativeOverallMobility(l) * dP;
					// derivative of P_Reservoir
					matrixBlockPerPerforation[l][1] += rho_o * factor[l] * F_Oil(l) * OverallMobility(l);
					matrixBlockPerPerforation[l][3] += rho_w * factor[l] * (1 - F_Oil(l)) * OverallMobility(l);
```

До (Якобиан ∂F_Oil/∂Sw, строки 59-63):
```cpp
					if (dP > 0)
					{
						double temp = productions[l] * Derivative_F_Oil(l);
						matrixBlockPerPerforation[l][0] += temp;
						matrixBlockPerPerforation[l][2] += -temp;
					}
```

После:
```cpp
					if (dP > 0)
					{
						double dF = productions[l] * Derivative_F_Oil(l);
						matrixBlockPerPerforation[l][0] += rho_o * dF;
						matrixBlockPerPerforation[l][2] -= rho_w * dF;
					}
```

Примечание по scope: `rho_o` и `rho_w` объявляются **внутри** каждого цикла for:
- В цикле rhs (строки 26-34): объявить `rho_o`/`rho_w` перед строками 32-33.
- В цикле Якобиана (строки 37-69): объявить `rho_o`/`rho_w` после `double dP = ...` (строка 45). Блок `if(dP > 0)` (строка 59) находится **внутри** этого же цикла → те же `rho_o`/`rho_w` видны, повторное объявление НЕ нужно.

**Проверка после этого шага:**
- Сборка: `cmake --build build --config Release`
- Тест: `ctest --test-dir build -C Release -R "buckley-leverett"` — ожидаем значительное изменение qt_eff
- Тест: `ctest --test-dir build -C Release -R "volume balance"` — ожидаем qt_eff ≈ 1.0
- Регрессия: `ctest --test-dir build -C Release --output-on-failure`

**Подводные камни:**
- ⚠️ Два цикла for (rhs и Якобиан) используют `rho_o`/`rho_w` — не забыть объявить в обоих.
- ⚠️ Слагаемое `productions[l] · Derivative_F_Oil(l)` (∂F_Oil/∂Sw) в текущем коде имеет `+temp` для [0] и `-temp` для [2]. После фикса: `+ρ_oil·dF` для [0] и `-ρ_water·dF` для [2]. Знаки: [0] — нефть (рост F_Oil увеличивает source нефти), [2] — вода (рост F_Oil уменьшает source воды). Корректно.
- Переменная `temp` переименована в `dF` для ясности (productions·dF_Oil/dSw — это не финальный temp, а промежуточная dF).
- Проверить, что `CellDensityOil(l)`/`CellDensityWater(l)` не возвращают 0 или NaN при Sw ∈ (0,1). В GDM плотности слабо зависят от давления (`ρ(P) = ρ_fixed · (1 + c · (P - P_fixed))`, c_oil ≈ 1e-9, c_water = 0), де-факто константы (~800 и ~1000 кг/м³). Межъячеечный код (ReservoirSimulator.cpp:600-612) тоже не учитывает ∂ρ/∂P в Якобиане — осознанный паттерн. Безопасно.

**Зависимости:**
- Требует: шаг 1
- Блокирует: шаги 3, 4, 5

**Оценка:** ~15 строк, ~10 минут (включая тестирование)

---

## Этап 2: Тесты

### Шаг 3: Снять [!mayfail] и обновить тесты баланса объёмов

**Цель:** тесты баланса объёмов становятся обязательными — BUG-020 исправлен.

**Файлы:** `tests/test_buckley_leverett.cpp`

**Контекст:**
Два теста с тегом `[!mayfail]` документировали BUG-020:
1. «BL validation: qt_eff consistent across meshes» — проверяет вариацию qt_eff < 15% по сеткам.
2. «BL validation: absolute volume balance» — проверяет qt_eff ≈ qt_nominal (1.0 м/день) с tolerance 10%.

После фикса qt_eff должен быть ≈ 1.0 м/день (с точностью до PI-зависимости от сетки).

**Что сделать:**
1. Убрать `[!mayfail]` из обоих TEST_CASE.
2. Если tolerance 10% слишком жёсткий (qt_eff зависит от r_app/PI) — ослабить до 20-25%.

**Изменения:**

До:
```cpp
TEST_CASE("BL validation: qt_eff consistent across meshes",
          "[buckley-leverett][validation][!mayfail]")
```

После:
```cpp
TEST_CASE("BL validation: qt_eff consistent across meshes",
          "[buckley-leverett][validation]")
```

До:
```cpp
TEST_CASE("BL validation: absolute volume balance",
          "[buckley-leverett][validation][!mayfail]")
```

После:
```cpp
TEST_CASE("BL validation: absolute volume balance",
          "[buckley-leverett][validation]")
```

**Проверка после этого шага:**
- Сборка + тесты: `ctest --test-dir build -C Release -R "buckley-leverett"`
- Ожидаемый результат: все BL тесты зелёные, нет «failed as expected».
- Если qt_eff ≈ 1.0 но не проходит tolerance — адаптировать epsilon.

**Подводные камни:**
- qt_eff зависит от PI (r_app зависит от hx). Tolerance 10% может быть недостаточен. Если нужно ослабить — 25% с комментарием.

**Зависимости:**
- Требует: шаг 2
- Блокирует: ничего

**Оценка:** ~4 строки, ~5 минут

---

### Шаг 4: Обновить тест MER_Data (если нужен)

**Цель:** проверить, что тест «MER_Data: debitPartial returns mass rate» не затронут фиксом.

**Файлы:** `tests/unit/descriptors/test_MER_Data.cpp`

**Контекст:**
Фикс не меняет MER_Descriptor. `curFluidDebit` по-прежнему читает `oil_m`/`water_m` и возвращает кг/день. Тест «debitPartial returns mass rate» проверяет `debit.oil == Approx(360/30)` = 12 кг/день — **не нужно менять**.

**Что сделать:** убедиться, что тест проходит без изменений. Если проходит — этот шаг пропускается.

**Проверка:**
- `ctest --test-dir build -C Release -R "MER_Data"`
- Ожидаемый результат: все MER_Data тесты зелёные.

**Зависимости:**
- Требует: шаг 2
- Блокирует: ничего

**Оценка:** 0 строк, ~1 минута

---

## Этап 3: Верификация и очистка

### Шаг 5: Визуальная верификация профиля Sw

**Цель:** убедиться визуально, что фронт Sw(x, t_final) после фикса сместился на десятки-сотни метров (не ~8 м как до фикса).

**Файлы:** нет изменений в коде

**Что сделать:**
1. Запустить: `build\Release\gdm_tests.exe "BL validation: CSV export for visual check"`
2. Открыть `bl_validation_profile.csv` — фронт GDM должен быть при x ≈ 50-200 м (зависит от PI и Swf).
3. Аналитика (Sw_analytical) должна совпадать с GDM по форме и положению фронта.

**Проверка:**
- Фронт не вырожден (не при x~8 м как до фикса).
- Sw_GDM ≈ Sw_analytical (форма профиля).

**Зависимости:**
- Требует: шаг 2
- Блокирует: ничего

**Оценка:** 0 строк кода, ~5 минут

---

### Шаг 6: Обновить vault-заметки

**Цель:** vault отражает исправленный анализ BUG-020.

**Файлы:**
- `vault/GDM/knowledge/debugging/несогласованность единиц расхода в формуле Писмана.md`
- `vault/GDM/knowledge/validation/задача Бакли-Леверетта — аналитический тест для одномерного вытеснения.md`

**Что сделать:**
1. В debugging-заметке: обновить анализ — проблема не в MER (кг vs м³), а в отсутствии ρ в скважинном rhs и Якобиане. Добавить раздел «Уточнённый анализ» и «Исправление».
2. В validation-заметке: обновить результаты — qt_eff ≈ 1.0 м/день после фикса, L2 и порядок сходимости могут измениться (фронт на другом месте → другие L2).
3. В roadmap-записи BUG-020 (`vault/GDM/roadmap/известные баги.md`, строки 195-205): обновить поле **Причина** — текущий текст описывает старый (неверный) анализ про `SetRefWellPressure` и `debitConversion`. Заменить на: «`AddWellToMatrix` формирует rhs и Якобиан скважины без множителя плотности ρ (м³/день), в то время как межъячеечные потоки — в кг/день. Якобиан скважины в ~1000× слабее межъячеечного → Newton игнорирует скважинный вклад.»
4. GitHub issue #16: добавить комментарий с результатом фикса. Старый комментарий описывает Вариант C (oil_m→oil_v) — устарел.

**Зависимости:**
- Требует: шаг 5
- Блокирует: ничего

**Оценка:** ~40 строк, ~10 минут

---

## Обнаруженные проблемы

### DEBT-NEW-1: BalanceOil складывает productions (м³/день) с CurOverallDebit (кг/день)

**Файл:** `HydroSolver/Reservoir/Well/SomeWell.cpp:392-396`

**Суть:** `BalanceOil` вычисляет `F_OilWellBalance` из `productions[l]` (м³/день) и `CurOverallDebit()` (кг/день). Несоразмерные величины. При ρ ≈ 800-1000 ошибка ≈ 3 порядка. Проявляется только при многопластовых скважинах с перфорациями, где часть слоёв закачивает (production < 0) и часть отбирает (production > 0) одновременно. Для простых INJ/PROD с одной перфорацией — не проявляется (f_oil_in=0 для INJ, F_Oil берётся из ячейки для PROD).

**Блокирует ли BUG-020:** нет. F_OilWellBalance для INJ = 0 (f_oil_in=0), для PROD F_Oil берётся из ячейки.

**Действие:** зарегистрировать в `vault/GDM/roadmap/известные баги и технический долг.md`.

### BUG-021 (уже зарегистрирован): WellDataHandler — water_v для инжекторов содержит тонны, не м³

Не блокирует текущий фикс (MER не меняется).

### Замечание: Якобиан не учитывает ∂P_well/∂P_res

**Файл:** `HydroSolver/Reservoir/Well/Wells.cpp:55-56`

Строки 55-56: `// derivative of P_Well //!!!!!!!!!!!//` — Якобиан скважинного слагаемого не содержит производную P_well по P_res. Это означает, что для скважин с несколькими перфорациями Якобиан неполный. При одной перфорации ∂production/∂P_res = PI·λ·(1-1) = 0 — проблемы нет. При нескольких — Newton может сходиться медленнее.

Это **известное упрощение**, отмеченное в коде (`!!!!!`). Не блокирует текущий фикс, но стоит зарегистрировать как DEBT.

---

## Тестовая стратегия

### Тест-воспроизводитель

**Тест:** «BL validation: absolute volume balance»
**Тег:** `[buckley-leverett][validation]` (снят `[!mayfail]`)
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Сценарий:** qt_eff из баланса масс GDM ≈ qt_nominal (1.0 м/день)
**Setup:** 1D, Nx=100, Lx=100м, INJ -1000 кг/день воды, Sw_init=0.2, 400 дней
**Ожидание:** `qt_eff == Approx(1.0).epsilon(0.1)` (10% tolerance)
**Предотвращает:** регрессию — возвращение к скважинному rhs без ρ

### Внутренняя согласованность

**Тест:** «BL validation: qt_eff consistent across meshes»
**Тег:** `[buckley-leverett][validation]` (снят `[!mayfail]`)
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Сценарий:** qt_eff одинаков ±15% для Nx = 25, 50, 100, 200
**Ожидание:** max/min вариация < 15%
**Предотвращает:** зависимость qt_eff от сетки (PI-зависимость)

### Сетчатая сходимость (regression)

**Тест:** «BL validation: grid convergence of Sw profile»
**Тег:** `[buckley-leverett][validation]`
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Ожидание:** p > 0.5 для всех пар
**Предотвращает:** регрессию порядка сходимости

### MER unit test (regression)

**Тест:** «MER_Data: debitPartial returns mass rate»
**Тег:** `[unit][level2][descriptors][MER_Data]`
**Файл:** `tests/unit/descriptors/test_MER_Data.cpp` (существующий, без изменений)
**Ожидание:** без изменений — curFluidDebit по-прежнему кг/день

### Visual

**Тест:** «BL validation: CSV export for visual check» (тег `[.]`)
**Файл:** `tests/test_buckley_leverett.cpp` (существующий)
**Ожидание:** фронт при x ≈ 50-200 м (не при ~8 м)

---

## Критерии завершения

- [ ] `SomeWell.h/cpp` — добавлены `CellDensityOil(l)`, `CellDensityWater(l)`
- [ ] `Wells.cpp` — rhs умножен на ρ_oil / ρ_water
- [ ] `Wells.cpp` — Якобиан умножен на ρ_oil / ρ_water
- [ ] Тесты `[!mayfail]` → зелёные, тег снят
- [ ] Все существующие тесты зелёные (`ctest --test-dir build -C Release`)
- [ ] Debug-сборка без ошибок и warnings
- [ ] Визуальная верификация: фронт BL сместился на десятки-сотни метров
- [ ] Vault обновлён: debugging + validation заметки
- [ ] Roadmap-запись BUG-020 обновлена (причина: старый анализ про SetRefWellPressure неверен)
- [ ] GitHub issue #16 прокомментирован с результатом (старый комментарий про Вариант C устарел)
- [ ] DEBT для BalanceOil зарегистрирован

---

## Связанные заметки

- [[несогласованность единиц расхода в формуле Писмана]]
- [[задача Бакли-Леверетта — аналитический тест для одномерного вытеснения]]
- [[val-001 buckley-leverett-1d]]
- [[r_app vs r_well мелкие сетки]]
- [[дебит скважины определяется формулой Дюпюи с радиусом Писмана]]
