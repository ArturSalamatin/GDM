---
tags:
  - prompt
  - валидация
  - 3D
  - перфорации
date: 2026-06-19
---

# Шаг 6: CMake и сборка

## Задача

Добавить новый тестовый файл в CMakeLists.txt, собрать проект и прогнать все тесты.

## Изменения в CMakeLists.txt

Добавить `tests/test_3d_completions.cpp` в секцию `gdm_tests`:

```cmake
add_executable(gdm_tests
    tests/test_smoke.cpp
    tests/test_stationary_pressure.cpp
    tests/test_mass_balance.cpp
    tests/test_buckley_leverett.cpp
    tests/test_components.cpp
    tests/test_five_spot.cpp
    tests/test_visual_verification.cpp
    tests/test_streamlines.cpp
    tests/test_variable_debit.cpp
    tests/test_3d_completions.cpp        # ← НОВОЕ
)
```

## Новые файлы (только заголовочные, не нужны в CMake)

- `tests/well_completion_builder.h`
- `tests/simulation_cases/MultiLayerCase.h`

Эти файлы включаются через `#include` в `test_3d_completions.cpp` — CMake видит их как зависимости автоматически.

## Порядок сборки

```powershell
# 1. Реконфигурация (нужна из-за нового .cpp)
cmake -B build -S . -G "Visual Studio 17 2022"

# 2. Сборка
cmake --build build --config Release

# 3. Тесты — все
ctest --test-dir build -C Release --output-on-failure

# 4. Если нужны только 3D-тесты
ctest --test-dir build -C Release --output-on-failure -R "3d"

# 5. Если нужен только smoke (без [slow])
ctest --test-dir build -C Release --output-on-failure -R "3d" -E "slow"
```

## Ожидаемый результат

- **Файлы изменены:**
  - `HydroSolver/Reservoir/Well/WellJobs.h` — +1 конструктор (3 строки)
  - `tests/well_schedule_builder.h` — расширение build()/add_to_sim()
  - `CMakeLists.txt` — +1 строка

- **Файлы созданы:**
  - `tests/well_completion_builder.h`
  - `tests/simulation_cases/MultiLayerCase.h`
  - `tests/test_3d_completions.cpp`

- **Тесты:** ~35 тестов (30 существующих + 4-5 новых), все зелёные

## Чеклист перед коммитом

- [ ] Все 30 существующих тестов проходят (регрессия не сломана)
- [ ] Новые 3D-тесты проходят
- [ ] CSV-файлы экспортируются для визуальной верификации
- [ ] Баланс масс < 1e-3 во всех новых тестах
- [ ] 0 ≤ S_w ≤ 1, P > 0 на каждом шаге в каждой ячейке

## Возможные проблемы

### Newton divergence

Те же условия, что вызывали divergence в тесте Buckley–Leverett (см. [[Newton divergence при закачке воды через скважину]]): закачка воды в пласт с S_o = 0.8. Если Newton не сходится на каком-то шаге:
- wasted trial → `ReverseState()` не откатывает `P_Well` → NaN
- Обходной путь: начальная нефтенасыщенность 0.8 (не 1.0) обычно работает
- Если всё-таки diverges: уменьшить начальный tau (`initial_tau()` = 2.0 вместо 5.0)

### Тайм-аут

Тест 7.1 (484 ячейки, 7 скважин, 600 дней) может занять > 60 секунд. Тег `[slow]`. `catch_discover_tests` уже имеет `TIMEOUT 600`.

### Координаты скважин

Скважина должна попадать в сетку. Для сетки 11x11 с шагом hx = hy = 500/11 ≈ 45.45 м:
- (125, 125) → ячейка (2, 2) ✓
- (375, 375) → ячейка (8, 8) ✓
- (250, 250) → ячейка (5, 5) ✓
- (250, 125) → ячейка (5, 2) ✓
- (250, 375) → ячейка (5, 8) ✓

Все координаты внутри области [0, 500] — корректно.
