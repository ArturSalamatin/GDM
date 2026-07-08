---
tags: [inbox, обнаружено-при-реализации]
date: 2026-07-08
source: DEBT-051
---
# ExceptionFactory.h — неверный относительный путь к stdafx.h

Обнаружено при реализации [[debt-051 amgcl-submodule-and-solvers]].

`HydroSolver/Data/ExceptionFactory.h` включал `../../stdafx.h` — путь для уровня вложенности 2 (Reservoir/Well/, Solver/Math/), а `Data/` — уровень 1. Правильный путь: `../stdafx.h`.

Раньше работало случайно: include directory `${HYDRO}/AMGSolver/amgcl` создавал дополнительный base path, от которого `../../stdafx.h` резолвился в `HydroSolver/stdafx.h`.

Исправлено в рамках DEBT-051.
