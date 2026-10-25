---
tags:
  - тестирование
  - стратегия
  - roadmap
date: 2026-06-27
---

# Стратегия тестирования GDM

## Пирамида тестов

```
                    ╱╲
                   ╱  ╲
                  ╱ BL ╲          Buckley–Leverett vs аналитика
                 ╱______╲         MRST comparison
                ╱        ╲
               ╱  Visual  ╲      CSV + анимация + grid convergence
              ╱____________╲
             ╱              ╲
            ╱  Integration   ╲    Five-spot, 3D, variable debit
           ╱__________________╲
          ╱                    ╲
         ╱    Unit Level 3–4    ╲  MatrixCSR, TwoPhaseFlowCell, Jacobian
        ╱________________________╲
       ╱                          ╲
      ╱     Unit Level 0–2         ╲  POD, geometry, math, physics
     ╱______________________________╲
```

## Уровни юнит-тестов

Уровни определяются глубиной зависимостей. См. [[юнит-тесты уровень 0 нулевые зависимости]] и далее.

| Уровень | Описание | Примеры классов | Стратегия |
|---|---|---|---|
| 0 | Нулевые зависимости (POD) | Descriptors, GridDescriptors, Point, Segment | Тест всех методов и edge-cases |
| 1 | Одна зависимость от L0 | SetOfPoints, RawHorizon, MER_Data | Тест через фабрику L0-объектов |
| 2 | Средняя глубина | CRSStructure, SparsityPattern, AbstractCells | Мок L0/L1 или реальные объекты |
| 3 | Составные | MatrixCSR, TwoPhaseFlowCell, WellJobs | Реальные зависимости, сложные сценарии |
| 4 | Полная сборка | MatrixAssembly, JacobianAssembly | Полный граф зависимостей, проверка через J·δx ≈ ΔF |

## Интеграционные тесты

| Категория | Что проверяем | Критерий |
|---|---|---|
| Smoke | Запуск, no crash, bounds | S_w ∈ [0,1], P > 0 |
| Stationary pressure | Аналитическое решение | ||P - P_analytic|| < ε |
| Mass balance | Сохранение массы | |ΔM + outflux - debet| < ε_machine |
| Five-spot | Симметрия | S_w(i,j) ≈ S_w(j,i) с точностью ~1e-10 |
| Grid convergence | Сходимость по сетке | ||S_h - S_{h/2}|| убывает |
| Visual | CSV + анимация | Человеческая проверка анимации |

## Правила

1. **Каждый новый функционал — тест ДО кода** (или одновременно). Не after-the-fact.
2. **Визуальная верификация обязательна** для любого физического теста. CSV-экспорт + Python-анимация. Catch2 assertion проверяет bounds, но не обнаруживает неправильную физику. См. [[feedback — visual verification always]].
3. **Скрытые тесты `[.tag]`** — допустимы ТОЛЬКО для документирования известных багов. Каждый скрытый тест должен иметь соответствующий баг в [[известные баги и технический долг]].
4. **Regression на производительность** — при любом структурном изменении солвера прогнать 51×51×4 и сравнить со snapshot-ом.
5. **Не мокать базу / солвер.** Тесты должны использовать реальные объекты. Мок допустим только для I/O.

## Матрица покрытия

### Покрыто хорошо ✅
- Relperm (Corey): boundary, intermediate, monotonicity, derivative
- Fractional flow: boundary, S-shape
- CRS-матрица: AddBlock, toDense, layout conversion
- SparsityPattern: генерация, симметрия
- Jacobian: J·δx ≈ ΔF (second order convergence)
- PI-контроллер: формула, edge-cases, clamping
- Баланс масс: закрытый/открытый пласт

### Покрыто косвенно ⚠️
- ReservoirSimulator (через integration tests)
- OilField (через create_oilfield)
- SomeWell / Wells (через integration tests)
- WellTrajectory (через streamlines)

### Не покрыто 🔴
- Buckley–Leverett симуляция (баг BUG-001)
- PI-контроллер на нестационарных задачах
- Режим BHP (не реализован)
- Модель Peaceman (не реализована)
- Закрытые границы (не реализованы)
- Крупные сетки > 100k ячеек (нет regression-теста на время)
- UniversalSVParser/Writer (исключены из сборки)

## Целевые метрики

| Метрика | Текущее | Цель (фаза 3) | Цель (фаза 6) |
|---|---|---|---|
| Количество тестов | 274 | 300+ | 400+ |
| Время тестов (Release) | ~277 сек | < 300 сек | < 300 сек (без benchmark) |
| Скрытые тесты | 1 ([.wells]) | 0 | 0 |
| Покрытие ядра | ~70% (оценка) | > 85% | > 90% |
