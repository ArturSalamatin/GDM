---
tags:
  - prompt
  - валидация
  - 3D
  - перфорации
date: 2026-06-19
---

# Шаг 5: Тесты 3D-перфорации

## Задача

Создать файл `tests/test_3d_completions.cpp` с тестами для 3D-задачи (4 пласта, 7 скважин). Тег `[3d]`, `[completions]`.

## Контекст

### Физическая модель

- Область: 500 x 500 x 40 м, сетка 11 x 11 x 4 = 484 ячейки
- hz = 10 м, perm = 100 мД, φ = 0.2, P_init = 200 атм, S_o = 0.8

### Общий код тестового файла

Использовать паттерн из `test_variable_debit.cpp`:
- `run_case()` или аналог `run_case_3d()` — запуск SimulationCase с проверками
- CSV-экспорт: для 3D-задачи экспортировать **каждый слой отдельно** (`snapshot_000_layer_0.csv`, ...)
- `metadata.json` включает `Nz`, `hz`

### Экспорт по слоям

Для 3D-задачи поля `Sw` и `P` содержат `Nx*Ny*Nz` значений. Индексация:
```
cell(i, j, k) = Nx * Ny * k + Nx * j + i
```

Для экспорта слоя k: извлечь `Sw[Nx*Ny*k .. Nx*Ny*(k+1))`, записать как 2D CSV.

```cpp
void write_layer_csv(const std::string& path,
                     size_t nx, size_t ny, size_t layer_k,
                     const std::vector<double>& Sw,
                     const std::vector<double>& P) {
    std::ofstream ofs(path);
    ofs << "i,j,Sw,P_atm\n";
    size_t offset = nx * ny * layer_k;
    for (size_t j = 0; j < ny; ++j)
        for (size_t i = 0; i < nx; ++i) {
            size_t idx = offset + j * nx + i;
            ofs << i << "," << j << ","
                << std::setprecision(8) << Sw[idx] << ","
                << P[idx] / 101325.0 << "\n";
        }
}
```

## Тесты

### 7.1: 3D smoke — 7 скважин работают без падения

Тег: `[3d][completions][smoke]`

Все 7 скважин из таблицы (см. обзорный файл). Время: 600 дней.

Настройка каждой скважины:

```cpp
// INJ-1: пласты 0,1 с t=0, Q=40
auto c_inj1 = WellCompletionBuilder(4, hz)
    .open_layer(0, 0.0).open_layer(1, 0.0);
WellScheduleBuilder(L"INJ-1", 125.0, 125.0)
    .set_completions(c_inj1)
    .inject_water(40.0).for_days(600.0);

// INJ-2: пласт 0 с t=0, пласт 2 добавлен при t=200, Q=30
auto c_inj2 = WellCompletionBuilder(4, hz)
    .open_layer(0, 0.0).open_layer(2, 200.0);
WellScheduleBuilder(L"INJ-2", 375.0, 375.0)
    .set_completions(c_inj2)
    .inject_water(30.0).for_days(600.0);

// PROD-1: все 4 пласта с t=0, Q=25
auto c_prod1 = WellCompletionBuilder(4, hz)
    .open_layer(0, 0.0).open_layer(1, 0.0)
    .open_layer(2, 0.0).open_layer(3, 0.0);
WellScheduleBuilder(L"PROD-1", 375.0, 125.0)
    .set_completions(c_prod1)
    .produce_oil(25.0).for_days(600.0);

// PROD-2: только пласт 3, Q=15
auto c_prod2 = WellCompletionBuilder(4, hz)
    .open_layer(3, 0.0);
WellScheduleBuilder(L"PROD-2", 125.0, 375.0)
    .set_completions(c_prod2)
    .produce_oil(15.0).for_days(600.0);

// PROD-3: пласты 1,2 с t=0, закрытие пласта 1 при t=300, Q=20
auto c_prod3 = WellCompletionBuilder(4, hz)
    .open_layer(1, 0.0).open_layer(2, 0.0)
    .close_layer(1, 300.0);
WellScheduleBuilder(L"PROD-3", 250.0, 250.0)
    .set_completions(c_prod3)
    .produce_oil(20.0).for_days(600.0);

// INJ-3: появляется при t=150, пласты 0,1, Q=35
auto c_inj3 = WellCompletionBuilder(4, hz)
    .open_layer(0, 150.0).open_layer(1, 150.0);
WellScheduleBuilder(L"INJ-3", 250.0, 125.0)
    .set_completions(c_inj3)
    .shut_in().for_days(150.0)
    .inject_water(35.0).for_days(450.0);

// PROD-4: появляется при t=300, пласты 2,3, Q=15
auto c_prod4 = WellCompletionBuilder(4, hz)
    .open_layer(2, 300.0).open_layer(3, 300.0);
WellScheduleBuilder(L"PROD-4", 250.0, 375.0)
    .set_completions(c_prod4)
    .shut_in().for_days(300.0)
    .produce_oil(15.0).for_days(300.0);
```

Проверки:
- Newton сходится (нет wasted trials > 10)
- 0 ≤ S_w ≤ 1, P > 0 во всех 484 ячейках на каждом шаге
- Баланс масс: relative residual < 1e-3
- CSV-экспорт: 4 файла на каждый snapshot (по слоям)

### 7.2: Отложенный запуск скважины

Тег: `[3d][completions][delayed-start]`

Упрощённая задача: 2 скважины.
- INJ (125, 250): пласт 0, с t=0, Q=30
- PROD (375, 250): пласт 0, **появляется при t=200**, Q=20

Время: 400 дней.

Проверки:
- До t=200: только закачка, давление растёт, S_w около INJ увеличивается
- После t=200: PROD включается, баланс масс корректен
- Общий баланс < 1e-3

### 7.3: Закрытие пласта в процессе работы

Тег: `[3d][completions][layer-closure]`

Упрощённая задача: 1 скважина, 2 пласта (Nz=2).
- INJ (250, 250): пласты 0 и 1 с t=0, закрытие пласта 1 при t=200, Q=30

Время: 400 дней, сетка 11x11x2.

Проверки:
- 0 ≤ S_w ≤ 1, P > 0 во всех ячейках
- Баланс масс < 1e-3
- После t=200: закачка идёт только в пласт 0. Визуально (CSV) — S_w в пласте 1 не растёт после закрытия, а перераспределяется за счёт вертикальных потоков

### 7.4: Частичное вскрытие

Тег: `[3d][completions][partial]`

Упрощённая задача: 1 скважина, 1 пласт (Nz=1), hz=10 м.
- INJ (250, 250): вскрытие (0, 5) — только верхняя половина, Q=30

Время: 200 дней, сетка 11x11x1.

Проверки:
- Длина перфорации = 5 м (половина hz)
- Сравнить с полным вскрытием (0, 10): давление на скважине при частичном вскрытии **выше** (больше гидравлическое сопротивление → меньше factor в формуле Дюпюи)
- Баланс масс < 1e-3

### 7.5 (опционально): Частичное закрытие в процессе работы

Тег: `[3d][completions][partial-closure]`

1 скважина, 1 пласт, hz=10 м.
- INJ (250, 250): вскрытие (0, 10) с t=0, закрытие (4, 6) при t=100, Q=30

Время: 200 дней.

Проверки:
- После t=100: длина перфорации = 8 м (из 10)
- Баланс масс < 1e-3

## Маркировка тестов

Используй тег `[slow]` для теста 7.1 (7 скважин, 484 ячейки — длительный расчёт).
Остальные тесты (7.2-7.5) — малые сетки, должны быть быстрыми.

## Структура экспорта

```
results/
  3d_smoke/
    snapshot_000_layer_0.csv
    snapshot_000_layer_1.csv
    snapshot_000_layer_2.csv
    snapshot_000_layer_3.csv
    snapshot_001_layer_0.csv
    ...
    mass_balance.csv
    metadata.json
  3d_delayed_start/
    ...
  3d_layer_closure/
    ...
```
