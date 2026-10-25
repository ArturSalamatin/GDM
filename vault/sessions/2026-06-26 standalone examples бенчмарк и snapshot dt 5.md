---
tags:
  - сессия
  - examples
  - визуализация
date: 2026-06-26
---

# 2026-06-26 Standalone examples: бенчмарк и snapshot_dt=5

## Что сделано

### 1. Бенчмарк 51×51×4 как standalone example
- Создан `examples/ex_benchmark_51x51x4.cpp` — 3D-сценарий из `test_amgcl_benchmark.cpp`
- 51×51×4 = 10 404 ячейки, 6 скважин (3 INJ + 3 PROD), 730 дней
- Использует `MultiLayerCase` + `run_case_3d` с CSV-экспортом по слоям
- Добавлен в CMakeLists.txt: target `ex_benchmark_51x51x4`, `run_examples`, 3D-анимация

### 2. Унификация snapshot_dt = 5 дней во всех примерах
- **Проблема:** адаптивный шаг солвера рос до 7–9 дней в примерах с крупным snapshot_dt
- **Причина:** `CurrentIntegrationStep() = min(schemeTau, timeStepTillNextSaveMomemnt)` — шаг ограничен интервалом между save-моментами, а не абсолютным лимитом
- **Исправлено:**
  - `ex_five_spot.cpp`: 50 → 5 дней
  - `ex_3d_completions.cpp`: 30/20 → 5 дней (все 3 сценария)
  - `ex_variable_debit.cpp`: 10 → 5 дней (все 4 сценария)
  - `SingleInjectorCase.h`: 10 → 5 дней
  - `TwoWellCase.h`: уже было 5 дней

### 3. Разрешения git add в settings.json
- Добавлены правила `Bash(cd * && git add *)`, `Bash(git add *)`, `PowerShell(git add *)` в `.claude/settings.json`

## Файлы изменены
- `examples/ex_benchmark_51x51x4.cpp` — новый
- `examples/ex_five_spot.cpp` — snapshot_dt 50→5
- `examples/ex_3d_completions.cpp` — snapshot_dt 30/20→5
- `examples/ex_variable_debit.cpp` — snapshot_dt 10→5
- `tests/simulation_cases/SingleInjectorCase.h` — snapshot_dt 10→5
- `CMakeLists.txt` — новый target + 3D-анимация
- `.claude/settings.json` — git add permissions

## Связи
- [[план профилирования и оптимизации AMGCL]]
- [[2026-06-24 сессия 9 CPR в production]]
