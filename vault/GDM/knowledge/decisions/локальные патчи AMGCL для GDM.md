---
tags:
  - amgcl
  - зависимости
  - решения
date: 2026-06-27
---

# Локальные патчи AMGCL для GDM

Библиотека AMGCL (header-only, MIT, автор Denis Demidov) хранится как локальная копия в `HydroSolver/AMGSolver/amgcl/`. Upstream: `https://github.com/ddemidov/amgcl`. Локальная версия — 2022.

## Изменённые файлы

### `amgcl/relaxation/ilu0.hpp` (единственное изменение)

**Оригинал (upstream):**
```cpp
precondition(c == i, "No diagonal value in system matrix");
precondition(!math::is_zero((*D)[i]), "Zero pivot in ILU");
(*D)[i] = math::inverse((*D)[i]);
```

**Наша правка:**
```cpp
precondition(c == i, "No diagonal value in system matrix");
if (math::is_zero((*D)[i]))
    (*D)[i] = static_cast<value_type>(1);
(*D)[i] = math::inverse((*D)[i]);
```

**Причина:** при Sw=0 (чистая нефть) блок Якобиана по насыщенности вырожден → zero pivot в ILU0 → exception. Fallback `D[i]=1` позволяет солверу продолжить работу, хотя convergence деградирует. См. [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]].

### Другие отличия от legacy (2020 → 2022)

Это изменения самого upstream amgcl между версиями, не наши патчи:
- Добавлены `col_type`, `ptr_type` typedefs в backend
- Zero-stripping в L/U факторах после ILU0 факторизации
- `static_cast<ptrdiff_t>` в циклах по U
- Допуск параметра `k` в `check_params`

## План миграции на FetchContent

1. Подключить amgcl через `FetchContent_Declare` из GitHub
2. Наш патч (fallback D[i]=1) оформить как `.patch` файл и применять через `PATCH_COMMAND` в FetchContent, либо через wrapper-header
3. Проверить, не исправлен ли zero pivot в новых версиях amgcl
4. Прямые изменения файлов amgcl запрещены

## Связанные заметки

- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[известные баги и технический долг]]
