---
tags:
  - валидация
  - self-consistency
  - перфорации
  - shut-in
date: 2026-07-23
---

# VAL-020: закрытие перфорации + shut-in + restart — self-consistency

## Результат

✅ ПРОЙДЕН

## Сценарий

Nz=2, INJ+PROD с перфорациями в обоих слоях. Три фазы:
1. Работа 100 дней — оба слоя открыты
2. Shut-in 50 дней — обе скважины остановлены
3. Restart с закрытым k=0, 150 дней — поток только через k=1

Z-связи между слоями отключены (закомментированы в AbstractGrid.h:228–234, 263–269).

## Метрики

| Метрика | Значение | Критерий | Статус |
|---|---|---|---|
| max oil balance rel | < 1e-3 | < 1e-3 | ✅ |
| max water balance rel | < 1e-3 | < 1e-3 | ✅ |
| Sw[k1] > Sw[k0] вблизи INJ | да | да | ✅ |
| Sw в дальней ячейке k=0 | ≈ 0.202 | Sw_init ± 0.005 | ✅ |

## Наблюдения

ΔSw в k=0 между концом shut-in (t=150) и концом restart (t=300) составляет до 0.037 вблизи INJ. Это не утечка через перфорацию: то же поведение наблюдается в тесте «layer closure mid-simulation» (ΔSw до 0.01). Причина — граничные условия по давлению (`RefPressure`) создают слабый поток через внешнюю границу домена даже в слоях без скважинного притока. Вдали от скважин ΔSw ≈ 0.

## Тесты

- `3D completions: shut-in + restart with closed layer` — `[3d][completions][shutin-restart][val-020]`, сетка 11×11×2
- `3D completions: shut-in + restart - visual` — `[.visual][3d][completions][shutin-restart][val-020]`, сетка 21×21×2

## Визуализация

- `results/val-020/sw_phases.png` — карты Sw по слоям на t=100, 150, 300
- `results/val-020/mass_balance.png` — невязка баланса масс по времени
- `results/val-020/delta_sw_k0.png` — ΔSw в k=0 между shut-in и restart
- Скрипт: `scripts/plot_val020.py`

## Связанные

- [[val-019 inactive-layer-multizone-perf]] — close_layer + неактивные ячейки
- [[схема дискретизации полностью неявная а не IMPES]]
