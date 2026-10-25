---
tags:
  - валидация
  - неактивные-ячейки
  - перфорации
date: 2026-07-28
---

# VAL-022: неактивная ячейка скважины — перфорации фильтруются автоматически

## Результат: ✅ ПРОЙДЕН

## Суть
Скважина перфорирована во всех 3 слоях (k=0, k=1, k=2). Слой k=0 неактивен.
`RemovePerfsAtInactiveCells` автоматически удаляет перфорацию из k=0.

## Отличие от VAL-019
- VAL-019: перфорации задаются *только* в активных слоях (через `WellCompletionBuilder`)
- VAL-022: перфорации задаются *во всех* слоях, фильтрация — автоматическая

## Метрики
- Неактивный слой: P = P_init (epsilon 1e-12), Sw = Sw_init (epsilon 1e-12)
- Рабочие слои: Sw > Sw_init вблизи INJ
- Баланс масс: < 1e-3

## Визуализация
- CSV: `results/val-022/layer_0.csv`, `layer_1.csv`, `layer_2.csv`
- PNG: `results/val-022/val022_comparison.png`
- Скрипт: `scripts/plot_val022.py`

## Связанные заметки
- [[val-019 inactive-layer-multizone-perf]]
- [[val-021 barrier two independent reservoirs]]
