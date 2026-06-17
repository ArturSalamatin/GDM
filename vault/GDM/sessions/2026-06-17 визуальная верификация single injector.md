---
tags:
  - сессия
  - верификация
  - визуализация
  - баланс-масс
date: 2026-06-17
---

# Визуальная верификация: single injector + массовый баланс

## Что сделано

### Ядро солвера
- Добавлен `WaterContourFlux()` — отток воды через открытую границу (по аналогии с `OilContourFlux()`)
- Расширен `MassBalance()` тремя водными аккумуляторами: `accumWater`, `accumWaterOutFlux`, `accumWaterDebet`
- `GetOverallBalance()` возвращает 7 элементов (было 4)

### Тестовая инфраструктура
- [[SimulationCase]] — абстрактная фабрика для тестовых сценариев (`tests/simulation_cases/`)
- `SingleInjectorCase` — 21×21×1, одна нагнетательная скважина в центре, ГУ Дирихле (RefPressure)
- `test_visual_verification.cpp` — пошаговый Solve с экспортом снапшотов и проверкой баланса на каждом шаге

### Визуализация
- `scripts/animate_fields.py` — анимация S_w + P (gif/mp4) + график баланса масс (png)
- Результаты в `results/single_injector/` (gitignored)

## Обнаруженное

### Знаковая конвенция в MassBalance
`accumDebet = -∫(Debit·dt)`, где `Debit > 0` = добыча, `< 0` = закачка.

Правильное тождество баланса: `ΔM + outflux - debet_accum = 0`.

Для нефти (без скважин): `accumOil + accumOilOutFlux - 0 = 0` — работало всегда.
Для воды (с инжектором): `accumWater + accumWaterOutFlux - accumWaterDebet = 0` — разница знаков.

### Результаты расчёта
- Нефтяной баланс: невязка < 1e-15 (машинная точность)
- Водный баланс: невязка ~ 1e-4 кг при M ~ 1e8 кг (< 1e-12 relative)
- S_w растёт от инжектора к контуру, P повышается вокруг инжектора на ~35 атм
- Фронт воды не достигает границы за 200 дней (water outflux мал)
- Newton сходится за 5–7 итераций на каждом шаге, 0 wasted trials

## Коммиты
1. `feat: WaterContourFlux + водные аккумуляторы в MassBalance`
2. `test: visual verification — single injector, mass balance обеих фаз`
3. `feat: Python-скрипт анимации полей P, S_w и графика баланса масс`
