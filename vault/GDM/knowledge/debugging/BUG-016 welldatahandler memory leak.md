---
tags:
  - debugging
  - баг
  - память
date: 2026-07-02
issue: BUG-016
---

# BUG-016: утечка памяти new char[] без delete[] в DataReader::Read()

## Симптом

`WellDataHandler::DataReader::Read()` выделяет `new char[sizes[j]]` в цикле парсинга бинарного файла (перфорации, MER-данные). `delete[]` не вызывается — утечка N_строк × M_колонок аллокаций за каждый вызов `Read()`.

## Причина

Промежуточный буфер `char* var = new char[sizes[j]]` использовался для `memcpy_s` → `reinterpret_cast` при извлечении значений из бинарного файла. После извлечения буфер не освобождался. На строке выше был закомментирован `std::vector<char> lbuffer(sizes[j])` — предыдущая попытка фикса, не доведённая до конца.

## Фикс

Замена `char* var = new char[sizes[j]]` на `std::vector<char> var(sizes[j])`. Использование `var.data()` вместо `var` в `memcpy_s` и `reinterpret_cast`. Удалён закомментированный `lbuffer`.

RAII — буфер автоматически освобождается при выходе из scope. Безопасно при исключениях и early return.

## Файлы

- `HydroSolver/Utils/WellDataHandler.cpp` — `DataReader::Read()`, строки 42–62

## Связанные заметки

- [[code-review-2026-06-28-утечки-ресурсов-и-память]]
- [[bug-016 welldatahandler-memory-leak]]
