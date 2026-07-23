---
tags:
  - навигация
  - vault
date: 2026-06-27
---

# GDM — Гидродинамический симулятор месторождения

Наукоёмкий симулятор для моделирования работы нефтяного месторождения с множеством скважин в двухфазном приближении нефть–вода (обе фазы несжимаемые). Язык разработки — C++23, сборка CMake под Visual Studio 2022.

**Статус проекта:** 310 тестов (Catch2), все проходят в Release (~99 сек). CPR_BICGSTAB с True-IMPES weights, 3D сетки, переменный дебит, 6 standalone examples. AMGCL как git submodule (форк). Текущая фаза: стабилизация ядра.

---

## Навигация по vault

### Roadmap — долгосрочное планирование
- [[roadmap долгосрочный план развития GDM]] — фазы 3–8, критерии завершения, принципы
- [[инвентаризация кодовой базы 2026-06-27]] — 22 исходника, 34 теста, матрица покрытия
- [[известные баги]] — BUG-001..019
- [[технический долг]] — DEBT-002..053
- [[планируемые фичи]] — FEAT-001–011, привязка к фазам roadmap
- [[валидационные кейсы]] — VAL-001..038, аналитика + MRST + конфигурации + неоднородность + boundary + Newton
- [[исследования и эксперименты]] — RES-001–007, численные эксперименты
- [[закрытые баги и решённые проблемы]] — архив закрытых багов и решённого долга
- [[стратегия тестирования GDM]] — пирамида тестов, уровни, метрики
- [[стратегия управления кодовой базой]] — структура vault, правила ветвления, контроль развития

### Plans — детализированные планы реализации
Подробные пошаговые планы для BUG/DEBT/FEAT/VAL/RES задач, готовые к автономной реализации.
Создаются командами `/plan-fix`, `/plan-improve`, `/plan-feature`, `/plan-validate`, `/plan-research`.
Реализуются командой `/implement`.

- [[bug-001 well-state-rollback]] — BUG-001: SIGSEGV при закачке воды — out-of-bounds в AddFlowFieldSnapShot при ny=1
- [[bug-012 binary-grid-dim-type-mismatch]] — BUG-012: несовпадение типов size_t/int при бинарном I/O grid_dim
- [[bug-002 cpr-zero-pivot]] — BUG-002: CPR zero pivot — abort() в cpr.hpp:522 при LU-факторизации блока
- [[bug-019 sparsity-pattern-debug-abort]] — BUG-019: SparsityPattern abort в Debug при пустых массивах
- [[bug-006 solve-converged-always-true]] — BUG-006: Solve() всегда converged=true + CurrentANG_IsAccuracyReached() бессмысленный критерий
- [[bug-007 harmonic-mean-zero-division]] — BUG-007: деление на ноль в гармоническом среднем подвижности
- [[bug-008 well-pressure-zero-mobility]] — BUG-008: деление на ноль в SetRefWellPressure
- [[bug-009 active-cells-filter]] — BUG-009: фильтр ActiveCells не работает — size_t + 1 > 0 всегда true
- [[bug-013 sparsity-pattern-args-order]] — BUG-013: SparsityPattern 3-arg конструктор — аргументы vector\<bool\> перепутаны
- [[bug-003 welljobs-isempty]] — BUG-003: WellJobs::IsEmpty() всегда возвращает false
- [[bug-010 const-cast-perforations]] — BUG-010: const_cast на перфорациях в SomeWell — undefined behavior
- [[bug-015 vector-bool-openmp-race]] — BUG-015: std::vector\<bool\> под OpenMP — data race в UpdateGrid()
- [[bug-016 welldatahandler-memory-leak]] — BUG-016: утечка памяти new char[] без delete[] в WellDataHandler
- [[bug-018 snprintf-size-t-format]] — BUG-018: snprintf с %u для size_t — UB на x64
- [[val-001 buckley-leverett-1d]] — VAL-001: Buckley–Leverett 1D валидация профиля насыщенности
- [[val-002 grid-convergence-1d]] — VAL-002: сходимость на сетке (1D Buckley–Leverett)
- [[val-004 five-spot-grid-convergence]] — VAL-004: five-spot сетчатая сходимость (self-convergence)
- [[bug-020 debit-unit-conversion]] — BUG-020: несогласованность единиц массового и объёмного расхода
- [[debt-048 well-jacobian-dpwell]] — DEBT-048: добавить ∂P_well/∂P_res в якобиан скважины
- [[debt-008 eigen-remove-dead-include]] — DEBT-008: удалить мёртвый include Eigen
- [[debt-051 amgcl-submodule-and-solvers]] — DEBT-051..053 + FEAT-010/011: AMGCL submodule, патчи и новые солверы
- [[debt-052 amgcl-patches]] — DEBT-052: патчи AMGCL (ilu0.hpp, cpr.hpp) в ветку experimental/patches
- [[debt-053 solver-factory]] — DEBT-053: SolverFactory — compile-time выбор конфигурации СЛАУ (CMake + SolverConfig.h)
- [[feat-010 cpr-threshold-fallback]] — FEAT-010: threshold-based fallback в CPR block-LU (near-zero pivot, τ·max_diag)
- [[feat-011 true-impes-weights]] — FEAT-011: True-IMPES weights для CPR decoupling (nullspace ∂F/∂Sw, tag dispatch)
- [[debt-003 refactoring-reservoir-simulator]] — DEBT-003: декомпозиция ReservoirSimulator на JacobianAssembler, NewtonSolver, MassBalanceTracker, TimeIntegrator
- [[debt-054 jacobian-assembler]] — DEBT-054: выделить JacobianAssembler из ReservoirSimulator (этап 1 DEBT-003)
- [[debt-055 newton-solver]] — DEBT-055: выделить NewtonSolver из ReservoirSimulator (этап 2 DEBT-003)
- [[debt-056 mass-balance-tracker]] — DEBT-056: выделить MassBalanceTracker из ReservoirSimulator (этап 3 DEBT-003)
- [[debt-057 time-integrator]] — DEBT-057: выделить TimeIntegrator из ReservoirSimulator (этап 4 DEBT-003)
- [[res-007 bicgstab-vs-lgmres]] — RES-007: CPR+BiCGStab vs CPR+LGMRES — может ли BiCGStab стать default
- [[debt-059 config-json-hiding]] — DEBT-059: устранение name hiding в Config_JSON (перенос инициализации в конструктор)
- [[debt-004 remove-db-factories]] — DEBT-004: удаление мёртвых DB-фабрик и PostgreSQL-зависимого слоя
- [[debt-026 addoffdiagblock-const]] — DEBT-026: const-correctness для AddOffDiagBlock
- [[debt-002 wstring-to-string]] — DEBT-002: wstring → string (UTF-8) в ядре
- [[val-007 checkerboard-inactive-cells]] — VAL-007: шахматная деактивация ячеек (Nz=1)
- [[val-008 barrier-inactive-cells]] — VAL-008: барьер из неактивных ячеек между INJ и PROD
- [[val-009 single-active-layer]] — VAL-009: один активный слой из 4 (Nz=4, k=2) — сравнение с 2D-эталоном
- [[val-010 inactive-boundary-cell]] — VAL-010: неактивная граничная (угловая) ячейка — self-consistency
- [[val-019 inactive-layer-multizone-perf]] — VAL-019: неактивный средний слой + многопластовые перфорации (Nz=3, k=1 off)
- [[val-020 shutin-restart-close-layer]] — VAL-020: закрытие перфорации + shut-in + restart (self-consistency)
- [[debt-009 remove-debug-dumps]] — DEBT-009: убрать отладочные дампы test_*.txt из production-кода
- [[debt-010 wells-unique-ptr]] — DEBT-010: new/delete для скважин → unique_ptr
- [[debt-021 int-size-t-warnings]] — DEBT-021: int переменные цикла итерируют по size_t границам (warnings C4267/C4297)
- [[bug-022 mathroutines-nan-ub]] — BUG-022: return NAN из функции int — UB в LowerPointNonUniformMesh
- [[debt-022 activecellsnmbr-size-t]] — DEBT-022: activeCellsNmbr int → size_t в AbstractGrid
- [[debt-028 neibidx-size-t]] — DEBT-028: int neibIdx → size_t в LinearProblem::AddOffDiagBlock
- [[debt-039 amg-itercount-type]] — DEBT-039: double AMG_maxSolverIterCount → int + дробный аккумулятор
- [[debt-037 override-specifiers]] — DEBT-037: добавить override ко всем переопределённым виртуальным методам
- [[val-039 pi-controller-breakthrough]] — VAL-039: PI-контроллер через breakthrough (1D + five-spot)
- [[debt-006 pi-controller-enable]] — DEBT-006: включить PI-контроллер адаптивного шага по умолчанию
- [[debt-060 long-int-to-ptrdiff]] — DEBT-060: long int → ptrdiff_t в индексации сетки
- [[debt-058 static-cast-cleanup]] — DEBT-058: устранить static_cast — исправить типы, чтобы касты стали ненужны
- [[debt-061 remove-size-t-operator]] — DEBT-061: удалить избыточные operator[](size_t) в SomeGrid
- [[val-040 perf-regression-baseline]] — VAL-040: regression-тест на производительность (51×51×4, baseline + tolerance)

### 00-home — статус и навигация
- [[текущие приоритеты]] — текущая фаза, приоритеты, snapshot метрик

### Atlas — архитектура проекта
- [[математическая модель двухфазной фильтрации]]
- [[численные методы решения уравнений фильтрации]]
- [[структура проекта и конвенции кода]]
- [[ревизия legacy-кода декабрь 2022]]
- [[модуль линий тока Anomaly FlowField]]
- [[граф зависимостей классов gdm_core]]

### Knowledge — база знаний

#### Физика пласта (`knowledge/physics/`)
- [[несжимаемость фаз упрощает уравнение неразрывности до дивергенции скорости]]
- [[относительные проницаемости задаются степенными моделями Кори]]
- [[капиллярное давление можно пренебречь в первом приближении]]
- [[дебит скважины определяется формулой Дюпюи с радиусом Писмана]]

#### Численные методы (`knowledge/numerics/`)
- [[уравнение давления с несжимаемыми фазами даёт эллиптическую задачу]]
- [[полностью неявная схема линеаризуется методом Ньютона]]
- [[возможности amgcl для блочных СЛАУ]]
- [[RES-007 результаты bicgstab vs lgmres]]

#### Архитектурные решения (`knowledge/decisions/`)
- [[схема дискретизации полностью неявная а не IMPES]]
- [[капиллярное давление отброшено и давление единое для обеих фаз]]
- [[переменные задачи — нормированная насыщенность и давление]]
- [[upstream-взвешивание для доли фазы и гармоническое среднее проницаемости на гранях]]
- [[сетка 3D структурированная с линейной индексацией]]
- [[два режима скважин — фиксированное давление и фиксированный дебит]]
- [[линейный солвер — AMG через amgcl]]
- [[отвязка от PostgreSQL и GEOS через заглушки и синтетический main]]
- [[знаковая конвенция баланса масс accumDebet минус интеграл дебита]]
- [[закрытые границы требуют корректной регуляризации давления]]
- [[amgcl конфигурация lgmres ilu0 aggregation]]
- [[amgcl конфигурация iluk k1 новый оптимум]]
- [[CPR требует перестановки переменных или col percent B == 0 будет Sw]]
- [[layout абстракция отделяет топологию сетки от CRS маппинга]]
- [[PI-контроллер safety=1 и target=12 для Newton-based timestep control]]
- [[вертикальные перетоки исключены из модели]]
- [[переход с блочного AMG на скалярный CPR в production]]
- [[план профилирования и оптимизации AMGCL]]
- [[локальные патчи AMGCL для GDM]]
- [[регуляризация диагонали Якобиана для всех строк а не только Sw]]
- [[true-impes weights корректны через анализ layout InterleavedPSw]]
- [[BUG-002 ILU0 fallback не нужен при корректной регуляризации]]
- [[команда issue для создания GitHub issues]]
- [[команда plan-fix для планирования исправления багов]]
- [[команда plan-improve для планирования рефакторинга]]
- [[команда plan-feature для планирования новых фич]]
- [[команда plan-validate для планирования валидации]]
- [[команда plan-research для планирования экспериментов]]
- [[команда implement для реализации планов]]
- [[переход с LGMRES на BiCGStab как default солвер]]
- [[индексация через size_t и ptrdiff_t а не long]]

#### Тестирование (`knowledge/decisions/`)
- [[юнит-тесты уровень 0 нулевые зависимости]]
- [[юнит-тесты уровень 1 зависимости от уровня 0]]
- [[юнит-тесты уровень 2 средняя глубина зависимостей]]
- [[юнит-тесты уровень 3 составные классы]]

#### Debugging (`knowledge/debugging/`)
- [[Newton divergence при закачке воды через скважину]]
- [[SIGSEGV в MatrixCSR ResetMatrix вызванном из конструктора]]
- [[zero pivot в ILU0 при скалярном CPR на двухфазном Якобиане]]
- [[CopyBlock mutex сериализовал OpenMP и убивал параллелизм]]
- [[code-review-2026-06-28-баги-и-корректность]]
- [[code-review-2026-06-28-утечки-ресурсов-и-память]]
- [[code-review-2026-06-28-числовая-устойчивость]]
- [[SparsityPattern abort в Debug при пустых массивах]]
- [[code-review-2026-06-28-мёртвый-код-и-гигиена]]
- [[code-review-2026-06-28-архитектурные-ограничения]]
- [[BUG-006 solve converged always true]]
- [[BUG-007 harmonic mean zero division]]
- [[BUG-008 well pressure zero mobility]]
- [[BUG-009 active cells filter]]
- [[BUG-013 sparsity pattern args order]]
- [[BUG-010 const cast perforations]]
- [[BUG-015 vector bool openmp race]]
- [[BUG-003 welljobs isempty always false]]
- [[BUG-016 welldatahandler memory leak]]
- [[BUG-018 snprintf size_t format]]
- [[BUG-020 несогласованность единиц расхода в формуле Писмана]]

#### Валидация (`knowledge/validation/`)
- [[задача Бакли-Леверетта — аналитический тест для одномерного вытеснения]]
- [[одномерная задача требует плоского источника а не точечного]]
- [[five-spot сравнение с MRST]]
- [[таблица единиц GDM vs MRST]]
- [[val-010 неактивная граничная ячейка]] — self-consistency: деактивация угловых/бортовых ячеек

#### Литература (`knowledge/literature/`)
- [[Wallis 1983 Incomplete Gaussian Elimination as Preconditioning for CPR]] — CPR-прекондиционер, True-IMPES decoupling weights
- [[Cao 2002 Development of Techniques for General Purpose Simulators]] — PhD thesis, фундамент CPR-framework, три decoupling strategy
- [[Lacroix 2003 Decoupling Preconditioners in IPARS]] — обобщение decoupling на многокомпонентные задачи
- [[Cao 2005 Parallel Scalable Unstructured CPR-Type Linear Solver]] — quasi-IMPES vs True-IMPES vs ABF, параллельная реализация
- [[Cao 2009 A Fully Coupled Two-Phase Flow CPR Preconditioner]] — мета-заметка: какая публикация что покрывает в серии Cao–Tchelepi
- [[Gries 2014 System-AMG Approach for Fully Coupled CPR]] — System-AMG как альтернатива decoupling-based CPR

#### Пока пусто
- `knowledge/testing/` — методология и паттерны тестирования
- `knowledge/performance/` — результаты профилирования и бенчмарки
- `knowledge/architecture/` — API-контракты, паттерны, зависимости модулей

### Sessions — логи рабочих сессий

#### Промпты валидации (Catch2, выполнять в указанном порядке)
0. [[prompt-валидация-00-настройка-catch2]]
1. [[prompt-валидация-01-стационарное-давление]]
2. [[prompt-валидация-02-баланс-масс]]
3. [[prompt-валидация-03-buckley-leverett]]
4. [[prompt-валидация-04-компонентные-тесты]]
5. [[prompt-валидация-05-сравнение-MRST]]

#### 3D-перфорации (фаза 2.8)
- [[prompt-валидация-06-3D-перфорации-обзор]]
- [[prompt-валидация-06a-WellCompletionBuilder]]
- [[prompt-валидация-06b-WellJobs-конструктор]]
- [[prompt-валидация-06c-интеграция-WellScheduleBuilder]]
- [[prompt-валидация-06d-MultiLayerCase]]
- [[prompt-валидация-06e-тесты-3D-перфорации]]
- [[prompt-валидация-06f-CMake-и-сборка]]

#### Профилирование и оптимизация AMGCL (фаза 3.1)
- [[план профилирования и оптимизации AMGCL]]
- [[prompt-оптимизация-00-инструментирование]]
- [[prompt-оптимизация-01-перебор-конфигураций-AMGCL]]
- [[prompt-оптимизация-02-структурные-оптимизации]]
- [[prompt-оптимизация-03-CPR-прекондиционер]]
- [[prompt-очистка-benchmark-артефактов]]
- [[prompt-оптимизация-04-iluk-reuse-openmp-adaptive]]
- [[prompt-оптимизация-05-profiling-assembly]]
- [[prompt-оптимизация-06-CPR-прекондиционер]]
- [[prompt-оптимизация-06b-CPR-benchmark]]
- [[prompt-оптимизация-07-adaptive-timestep]]
- [[prompt-оптимизация-08-matrix-format]]
- [[prompt-оптимизация-09-CPR-в-production]]

#### Сессии (хронологически)
- [[2026-06-16 дедупликация legacy и поиск актуальной версии]]
- [[2026-06-17 визуальная верификация single injector]]
- [[2026-06-17 исследование закрытых границ и pin pressure]]
- [[2026-06-17 двускважинный сценарий INJ PROD]]
- [[2026-06-18 восстановление модуля линий тока]]
- [[2026-06-18 тесты скважин с переменным дебитом]]
- [[2026-06-20 инструментация AMGCL и baseline профиль]]
- [[2026-06-20 оптимизация AMGCL солвера lgmres ilu0]]
- [[2026-06-21 переход на amgcl profiler]]
- [[2026-06-21 серии DFGE перебор параметров AMGCL]]
- [[2026-06-21 сессия 4 iluk openmp]]
- [[2026-06-21 сессия 5 оптимизация assembly]]
- [[2026-06-23 сессия 6a layout абстракция]]
- [[2026-06-24 сессия 6b-7 CPR benchmark и PI-контроллер]]
- [[2026-06-24 сессия 9 CPR в production]]
- [[2026-06-25 тесты сборки матрицы Якобиана]]
- [[2026-06-26 standalone examples бенчмарк и snapshot dt 5]]
- [[2026-06-27 ревизия техдолга и команда issue]]
- [[2026-06-27 система plan-команд и консистентность]]
- [[план-сессия-6-CPR-прекондиционер]]
- [[2026-07-08 amgcl-patches-debt-052]]
- [[2026-07-09 solver-factory-debt-053]]
- [[2026-07-13 feat-011-sw0-diagonal-regularization]]
- [[2026-07-15 res-007-bicgstab-vs-lgmres]]
- [[2026-07-15 debt-004-remove-db-factories]]
- [[2026-07-22 val-007 checkerboard-inactive-cells]]
- [[2026-07-23 val-003-mrst-отложено]]
