---
tags:
  - решение
  - численные-методы
  - адаптивный-шаг
date: 2026-06-24
---

# PI-контроллер: safety=1 и target=12 для Newton-based timestep control

## Решение

Стандартная формула Soderlind PI-контроллера для адаптивного шага:
```
tau_{n+1} = tau_n * safety * (e_target / e_n)^alpha * (e_{n-1} / e_n)^beta
```

Для нашего Newton-based критерия (e_n = newton_iters / max_iters):
- **safety = 1.0**, не 0.85 как в литературе
- **target_iters = 12** при avg Newton = 7 и max = 65

## Почему safety = 1.0

При safety < 1 и стационарном newton == target: mult = safety < 1 на каждом шаге. Шаг убывает до нуля (deadlock). Soderlind safety предполагает что начальный шаг переоценен (ODE integrators). В PDE с Newton начальный шаг задаётся маленьким и растёт -- safety drain контрпродуктивен.

## Почему target = 12, не 4

`newtonMaxIterNmbr = 65` в кодовой базе. Newton "сходится" через `CurrentANG_IsAccuracyReached()` = `AMG_curError == 0.0`, что происходит при ~7 итерациях в среднем. target должен быть выше avg (чтобы шаг рос при типичной работе) но ниже max (чтобы при проблемах шаг уменьшался).

target ≈ 1.7 * avg_newton = 12 -- эмпирический оптимум на benchmark.

## Связанные заметки

- [[prompt-оптимизация-07-adaptive-timestep]]
- [[2026-06-24 сессия 6b-7 CPR benchmark и PI-контроллер]]
