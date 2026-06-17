---
tags:
  - промпт
  - валидация
  - MRST
  - benchmark
  - 2D
  - catch2
date: 2026-06-17
---

# Промпт: Валидация #05 — сравнение с MRST (2D five-spot)

Скопируй этот промпт в новую сессию Claude Code целиком. Проект находится в `d:\Lessons\Grants\2027\GDM`.

---

## Контекст проекта

GDM — двухфазный (нефть–вода) гидродинамический симулятор. C++23, CMake + Visual Studio 2022, Windows 10.

**Правила работы описаны в `CLAUDE.md` — прочитай его перед началом.**
Прочитай `vault/GDM/00-home/текущие приоритеты.md`.

### Тестовый фреймворк

Catch2 v3 настроен (промпт #00). Executable: `gdm_tests`.

**ВАЖНО:** тесты #01–#03 должны быть пройдены. Этот тест НЕ запускает MRST — только готовит сценарий для GDM и MATLAB-скрипт для MRST.

```powershell
cmake --build build --config Debug
.\build\Debug\gdm_tests.exe [five-spot]
```

## Цель

Подготовить **идентичный** тестовый сценарий для GDM и MRST. Catch2-тест запускает GDM и записывает результаты в CSV. MATLAB-скрипт запускает MRST и записывает аналогичные CSV. Третий скрипт сравнивает.

## Параметры Five-Spot

```
Сетка: 21×21×1 (уменьшено для скорости Debug)
Размер: 500×500×10 м
k = 100 мД, φ = 0.2
S_wc = 0, S_or = 0
μ_w = 2.0 мПа·с, ρ_w = 1000 кг/м³
μ_o = 4.3 мПа·с, ρ_o = 800 кг/м³
k_rw = S_w³, k_ro = (1-S_w)³ (Corey n=3)

P_init = 200 атм, S_w_init = 0
Инжектор: центр (10,10), Q_w = 50 м³/день
Продюсеры: углы (0,0),(0,20),(20,0),(20,20), по 12.5 м³/день
T = 500 дней
```

## Задание

### Шаг 1. Создай `tests/test_five_spot.cpp`

```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"
#include <fstream>

using Catch::Matchers::WithinAbs;

TEST_CASE("Five-spot: runs without crash", "[five-spot][2d][benchmark]") {
    constexpr size_t Nx = 21, Ny = 21, Nz = 1;
    constexpr double Lx = 500.0, Ly = 500.0, hz = 10.0;

    auto horizon = test_helpers::make_uniform_horizon(
        Nx, Ny, Nz, Lx, Ly, hz, 100.0, 0.2, 200.0, 1.0);
    auto numPrm = test_helpers::default_num_params();

    reservoir_simulator::ReservoirSimulator sim{
        numPrm, horizon, horizon.oil, horizon.water, horizon.other};
    sim.RefPressure = 200.0 * 101325.0;
    sim.numPrm.set_initial_schemeTau(5.0);
    sim.numPrm.set_currentMoment(0.0);

    // Добавить 5 скважин через add_simple_well (см. test_helpers.h)
    // Инжектор в центре: water_rate = -50000 кг/день (отрицательный = закачка)
    // 4 продюсера в углах: oil_rate = ... (положительный)

    // Для несжимаемых фаз: ΣQ_prod = Q_inj (объёмно)
    // Q_inj = 50 м³/день → каждый продюсер = 12.5 м³/день
    // Массовый расход продюсера зависит от water cut в ячейке

    // ... добавить скважины

    // Запуск
    std::vector<double> timeMoments = {0.0};
    for (double t = 50.0; t <= 500.0; t += 50.0)
        timeMoments.push_back(t);
    sim.Solve(timeMoments);

    // Базовые проверки
    REQUIRE(sim.numPrm.WastedTrialsCount() < 50);

    auto Sw = sim.GetWaterSaturationField();
    auto P = sim.GetPressureField();

    SECTION("Saturation bounds") {
        for (size_t i = 0; i < Sw.size(); ++i) {
            CHECK(Sw[i] >= -1e-10);
            CHECK(Sw[i] <= 1.0 + 1e-10);
            CHECK(P[i] > 0.0);
        }
    }

    SECTION("Symmetry: quarter-symmetry of saturation") {
        // Five-spot имеет 4-кратную симметрию
        // S_w(i,j) ≈ S_w(Nx-1-i, j) ≈ S_w(i, Ny-1-j) ≈ S_w(Nx-1-i, Ny-1-j)
        for (size_t i = 0; i < Nx / 2; ++i) {
            for (size_t j = 0; j < Ny / 2; ++j) {
                double s00 = Sw[j * Nx + i];
                double s10 = Sw[j * Nx + (Nx - 1 - i)];
                double s01 = Sw[(Ny - 1 - j) * Nx + i];
                double s11 = Sw[(Ny - 1 - j) * Nx + (Nx - 1 - i)];
                REQUIRE_THAT(s00, WithinAbs(s10, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s01, 1e-6));
                REQUIRE_THAT(s00, WithinAbs(s11, 1e-6));
            }
        }
    }
}

TEST_CASE("Five-spot: export results for MRST comparison",
          "[five-spot][benchmark][.export]") {
    // Тег [.export] = скрытый, запускается только явно
    // Этот тест записывает CSV-файлы для сравнения

    // ... аналогичная настройка

    // Запуск с сохранением в контрольные моменты
    std::vector<double> save_times = {0.0, 100.0, 300.0, 500.0};

    // Для каждого save_time записать:
    // results/five_spot_Sw_t{time}.csv — карта насыщенности
    // results/five_spot_P_t{time}.csv — карта давления

    // Формат CSV:
    // i,j,Sw,P
    // 0,0,0.000000,20260000.0
    // ...

    // + results/five_spot_production.csv — кривые добычи
    // time_days,oil_mass_kg,water_mass_kg
}
```

### Шаг 2. Создай MATLAB-скрипт

`MatLab/test_five_spot_mrst.m` — шаблон для MRST с идентичными параметрами.

```matlab
%% Five-Spot: MRST reference for comparison with GDM
% Requires MRST (https://www.sintef.no/projectweb/mrst/)

mrstModule add incomp

G = cartGrid([21, 21, 1], [500, 500, 10]);
G = computeGeometry(G);
rock = makeRock(G, 100*milli*darcy, 0.2);

fluid = initSimpleFluid('mu',  [2, 4.3]*centi*poise, ...
                        'rho', [1000, 800], ...
                        'n',   [3, 3]);

% Wells
W = addWell([], G, rock, sub2ind([21,21,1], 11, 11, 1), ...
    'Type', 'rate', 'Val', 50/day, 'Comp_i', [1,0], 'Name', 'INJ');
corners = {[1,1],[1,21],[21,1],[21,21]};
for k = 1:4
    c = corners{k};
    W = addWell(W, G, rock, sub2ind([21,21,1], c(1), c(2), 1), ...
        'Type', 'rate', 'Val', -12.5/day, 'Comp_i', [0,1], ...
        'Name', sprintf('P%d',k));
end

state = initResSol(G, 200*atm, [0, 1]);

% Time loop
dT = 10*day; nsteps = 50;
for n = 1:nsteps
    state = incompTPFA(state, G, computeTrans(G, rock), fluid, 'wells', W);
    state = implicitTransport(state, G, dT, rock, fluid, 'wells', W);
end

% Export CSV matching GDM format
% csvwrite('results/mrst_five_spot_Sw_t500.csv', ...
%     [(1:G.cells.num)', state.s(:,1), state.pressure]);
```

**ВАЖНО:** это шаблон. MRST API может отличаться, пользователь адаптирует.

### Шаг 3. Создай скрипт визуализации

`MatLab/compare_gdm_mrst.m`:
```matlab
%% Compare GDM vs MRST
% Reads CSV files from both, plots side-by-side, computes L2 error
```

### Шаг 4. Зарегистрируй в CMakeLists.txt

```cmake
target_sources(gdm_tests PRIVATE tests/test_five_spot.cpp)
```

## Единицы — таблица конвертации

| Величина | GDM | MRST | Конвертация |
|----------|-----|------|-------------|
| Давление | Па | Па | 1:1 |
| Время | дни | секунды | ×86400 |
| Вязкость | Па·день (внутр.) | Па·с | ×86400 |
| Проницаемость | м² | м² | 1:1 |
| Дебит (MER) | кг/день | м³/с | ÷ρ÷86400 |

## Критерии успеха

1. `gdm_tests.exe [five-spot]` — тесты PASSED (без [.export])
2. Физические ограничения выполняются
3. Четвертная симметрия карты насыщенности
4. CSV-файлы записываются (при запуске с [.export])
5. MATLAB-скрипт подготовлен

## По завершении

1. Создай `vault/GDM/knowledge/validation/five-spot сравнение с MRST.md`
2. Создай `vault/GDM/knowledge/validation/таблица единиц GDM vs MRST.md`
3. Обнови `vault/GDM/00-home/текущие приоритеты.md`
