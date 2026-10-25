---
tags: [inbox, обнаружено-при-реализации]
date: 2026-07-23
source: VAL-020
---

# AveragePerforationsOut сдвигает close_layer к концу MER-frame

Обнаружено при реализации [[val-020 shutin-restart-close-layer]].

## Суть

`AccumulatedPerforations::AveragePerforationsOut` (SetOfPoints.cpp:445–483) сдвигает операцию закрытия перфорации (`close_layer`) к концу текущего MER-frame вместо того, чтобы оставить её в указанный момент.

## Воспроизведение

`close_layer(0, 150.0)` при MER-расписании с `for_days` по 30.74-дневным записям. MER-frame для t=150: [150, 180.74]. Операция закрытия сдвигается с t=150 на t=180.74. Перфорация в k=0 остаётся открытой ~30 дней дольше, чем задано.

## Причина

Строки 460–461 в SetOfPoints.cpp:
```cpp
if (PerforationsInTime.back().TotalLength() == 0.0)
    PerforationsInTime.back().MoveDate(jobIntervals.back().second.second);
```

Если последняя операция в слое — закрытие (TotalLength == 0), она сдвигается к `endOfCurPeriod`. Логика предполагала что закрытие «не должно происходить посреди MER-периода», но это неверно для точных сценариев (shut-in + restart + close_layer).

## Данные

Sw в ячейке INJ, слой k=0 (21×21 сетка):
- t=150 (ожидание: перфорация закрыта): Sw = 0.633
- t=160: Sw = 0.643 (перфорация всё ещё открыта!)
- t=180: Sw = 0.660
- t=190: Sw = 0.660 (перфорация закрылась между 180 и 190)

## Влияние

ΔSw в k=0 до 0.049 вблизи INJ. VAL-020 тест проходит с margin(0.005) на дальних ячейках, но инвариант «Sw в k=0 не меняется после t=150» не выполняется строго.
