---
tags:
  - сессия
  - amgcl
  - CPR
  - submodule
date: 2026-07-08
---

# 2026-07-08 — AMGCL patches (DEBT-052)

## Что сделано

### DEBT-052: патчи AMGCL в ветку experimental/patches — ✅ реализовано

1. **План** (`/plan-improve DEBT-052`) — 5 шагов: создание ветки, патч ilu0.hpp, патч cpr.hpp, переключение submodule, тесты
2. **Аудит** (`/audit-plan DEBT-052`) — 3 раунда:
   - Раунд 1: найдены мелкие проблемы, автофикс
   - Раунд 2: **критическая находка** — в cpr.hpp:522 патч был без `A[k*B+k] = d` (write-back в массив). Строка 543 (`y[i] /= A[i*B+i]`) читает из массива, не из локальной `d`. Без write-back — деление на ноль. Исправлено в плане
   - Раунд 3: верификация — все проблемы закрыты
3. **Реализация** (`/implement DEBT-052`):
   - Ветка `experimental/patches` в форке `ArturSalamatin/vcpkg-amgcl` от `experimental/master` (c71ab45)
   - Коммит 1: `ilu0.hpp:156` — fallback `D[i]=1` при zero pivot (вместо `precondition(!math::is_zero(...))`)
   - Коммит 2: `cpr.hpp:521-522` — fallback `d=1` + обязательная запись `A[k*B+k]=d` при zero pivot в block-LU
   - Submodule в GDM переключён на `experimental/patches`
4. **Тесты:** Release 294/294, Debug 294/294 (был 293/294 — Grid convergence 41×41 теперь проходит)

### Branch protection

- Настроена защита ветки `experimental/master` в форке через GitHub API
- Цель: предотвратить случайный merge `experimental/patches` → `experimental/master`
- Patches — это локальные правки upstream-кода, они не должны попадать в clean upstream

### Merge в experimental

- Ветка `refactor/debt-052/amgcl-patches` смёржена в `experimental`
- Merge в GDM НЕ затрагивает ветки форка — GDM хранит только commit hash submodule
- Issue #19 закрыт

### Обновление приоритетов

- Зафиксирована последовательность: DEBT-053 → FEAT-010 → FEAT-011
- DEBT-053: SolverFactory — compile-time выбор конфигурации СЛАУ (следующая задача)
- FEAT-010: Threshold-based fallback (near-zero pivot) — после DEBT-053
- FEAT-011: True-IMPES / ABF weights — после FEAT-010

## Ключевые решения

1. **Стратегия веток в форке amgcl:**
   - `experimental/master` — чистый upstream (branch protection)
   - `experimental/patches` — патчи upstream-файлов (ilu0.hpp, cpr.hpp)
   - `experimental/solvers` (будущее) — новый код (CPR-DRS, True-IMPES weights)

2. **Не менять `.gitmodules` branch:**
   - `.gitmodules` хранит `branch = experimental/master` (по дизайну)
   - `git submodule update --remote` обновит до HEAD of `experimental/master`, что потеряет патчи
   - Submodule фиксируется по commit hash, не по branch name

3. **Write-back `A[k*B+k] = d` обязателен:**
   - cpr.hpp:515-543 — `invert()` делает block-LU in-place
   - Локальная переменная `d` получает fallback `d=1`, но без `A[k*B+k]=d` строка 543 (`y[i] /= A[i*B+i]`) читает из массива исходный ноль
   - Vault-заметка [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] предупреждала об этом

## Связанные заметки

- [[debt-052 amgcl-patches]] — план реализации
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]] — анализ проблемы
- [[локальные патчи AMGCL для GDM]] — решение о патчах
- [[debt-051 amgcl-submodule-and-solvers]] — общий план DEBT-051..053

## Следующая задача

**DEBT-053: SolverFactory — compile-time выбор конфигурации СЛАУ**
- 5 конфигураций: CPR+lgmres, CPR+bicgstab, CPR<smoothed_aggr>+lgmres, CPR-DRS+lgmres, ILU0+lgmres
- После DEBT-053 — тестирование готовых конфигураций amgcl на Sw=0
