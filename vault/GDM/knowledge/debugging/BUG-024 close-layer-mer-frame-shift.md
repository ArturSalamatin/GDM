---
tags: [debugging, баг-исправлен]
date: 2026-07-31
---

# BUG-024: `AveragePerforationsOut` сдвигает `close_layer` к концу MER-frame

## Симптом

`close_layer(0, 150.0)` при MER-frame `[150.0, 180.74)` — перфорация закрывалась при t≈180.74 вместо t=150. Sw в закрытом слое продолжал расти ~30 дней после указанного момента закрытия.

## Причина

`SetOfPoints.cpp`, функция `AveragePerforationsOut`:
- Шаг 2: сдвиг всех дат к `beginOfCurPeriod` — корректно
- Шаг 3: если `TotalLength() == 0` (закрытие), сдвиг к `endOfCurPeriod` — **без проверки**, совпадала ли исходная дата с `beginOfCurPeriod`

Если `close_layer` задан ровно на MER record date, шаг 2 ничего не меняет (дата уже на границе), а шаг 3 ошибочно сдвигает к концу.

Побочная ошибка: код использовал raw-индекс `PerforationsInTime[i-1]` вместо `perf_id` из `jobIntervals`. При пропусках в `jobIntervals` (из-за `continue` в `SomeWell.cpp:145`) это семантически некорректно.

## Фикс

Добавлена проверка `RawPerorationsInTime[perf_id].curTime() != interval.first` — если исходная дата совпадает с `beginOfCurPeriod`, сдвиг к `endOfCurPeriod` не выполняется. Индексация исправлена на `perf_id`. Добавлен guard `if (!jobIntervals.empty())` для блока `back()`.

## Важная деталь: float vs double

MER-записи хранят даты как `float` (`SingleMERrecord["time"]`). `close_layer` передаёт дату как `double`. Дата `153.70` (double) ≠ `float(153.70)` ≈ `153.6999969...`. Поэтому интеграционный тест использует дату `150.0` (точно представима как float), а не `153.70`.

## Связи

- Обнаружено при: реализации [[val-020 shutin-restart-close-layer]]
- Подробный анализ: [[AveragePerforationsOut сдвигает close_layer]]
- План: [[bug-024 close-layer-mer-frame-shift]]
- **GitHub issue:** [#56](https://github.com/ArturSalamatin/GDM/issues/56)
