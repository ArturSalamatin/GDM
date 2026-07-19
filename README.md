# GDM — Geodynamic Model

A hydrodynamic simulator for modeling oil reservoir performance with multiple wells in a two-phase (oil–water) incompressible flow approximation.

## Overview

GDM solves the coupled pressure–saturation equations for immiscible, incompressible two-phase flow (oil and water) in porous media. The simulator supports multi-well configurations and is designed for field-scale reservoir studies.

## Key Features

- Two-phase incompressible flow (oil–water)
- Multi-well support (injectors and producers)
- Fully implicit Newton solver with CPR preconditioner
- Adaptive timestep control (PI-controller)
- 3D structured grids with linear indexing
- 310 Catch2 tests, 6 standalone examples
- Obsidian knowledge vault for decisions, debugging, validation

## Build

```powershell
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Examples and Visualization

The project includes standalone simulation scenarios (`examples/`) and Python visualization scripts (`scripts/`).

| Directory | Contents |
|---|---|
| `examples/` | Standalone executables — configure a simulation case, run the time loop, export CSV snapshots to `results/` |
| `tests/simulation_cases/` | Reusable case classes (grid, wells, PVT) shared by tests and examples |
| `scripts/` | Python scripts for field animations and convergence plots |

### Running examples

```powershell
# Single example (run from project root)
.\build\Release\ex_five_spot.exe

# All examples
cmake --build build --config Release --target run_examples

# All examples + generate animations
cmake --build build --config Release --target animate
```

Available examples: `ex_single_injector`, `ex_two_well`, `ex_five_spot`, `ex_variable_debit`, `ex_3d_completions`, `ex_benchmark_51x51x4`, `ex_benchmark_series_cpr`, `ex_benchmark_series_ts`.

Results are written to `results/<case_name>/` as per-timestep CSV files (columns: `i,j,Sw,P_atm`). The `animate` target calls `scripts/animate_fields.py` (2D) and `scripts/animate_3d_fields.py` (3D) to produce animations from these CSVs.

## Solver Configuration

GDM uses [amgcl](https://amgcl.readthedocs.io/) for linear algebra. The Krylov solver is selected at compile time via the `GDM_SOLVER` CMake option:

| Value | Krylov solver | Preconditioner | Notes |
|---|---|---|---|
| `CPR_BICGSTAB` | BiCGStab | CPR (AMG + ILU0, True-IMPES weights) | **Default.** 12–90% faster than LGMRES (RES-007) |
| `CPR` | LGMRES | CPR (AMG + ILU0, True-IMPES weights) | Previous default. Better theoretical convergence guarantees |
| `CPR_SA` | LGMRES | CPR (smoothed aggregation AMG + ILU0) | |
| `CPR_DRS` | LGMRES | CPR-DRS (AMG + ILU0) | |
| `ILU0` | LGMRES | ILU0 (no CPR) | Baseline, no pressure-specific preconditioning |

To switch:

```powershell
# Fresh build with a specific solver
cmake -B build -S . -G "Visual Studio 17 2022" -DGDM_SOLVER=CPR
cmake --build build --config Release

# Or change in an existing build
cmake -B build -DGDM_SOLVER=CPR
cmake --build build --config Release
```

Configuration is in `HydroSolver/Solver/Math/SolverConfig.h`. See `vault/GDM/knowledge/numerics/RES-007 результаты bicgstab vs lgmres.md` for the comparison study.

## Slash Commands (Claude Code)

Project automation via [Claude Code](https://claude.ai/claude-code) slash commands.

### `/issue <ID | text>`

Create a GitHub issue from a vault entry or free-text description.

- **Existing mode:** `/issue BUG-001` — loads vault entry, analyzes code, creates detailed issue with root cause chain, solution variants, and reproduction steps
- **New mode:** `/issue описание проблемы` — assigns next ID (BUG/DEBT/FEAT/VAL/RES-NNN), creates vault entry and issue simultaneously
- **Dry-run:** `/issue --dry BUG-002` — preview without creating
- Auto-labels (`critical`, `workaround`, `blocking`), duplicate check, file/line validation
- Backlinks: GitHub → vault, vault → GitHub
- Suggests working branch: `fix/bug-001/well-state-rollback`

### `/plan-fix <BUG-NNN>`

Build a detailed implementation plan for a bug fix.

- 5 phases: context → analysis (10-point pitfall checklist) → detailed steps → triple verification → save
- Bug-specific stages: reproduce → minimal fix → tests → architectural fix → verification → cleanup
- Each step: goal, files, context, old→new code, checks, pitfalls, dependencies, estimate
- Test strategy: reproducer + boundary + invariant + regression + visual
- Output: `vault/GDM/plans/<id> <slug>.md` — self-contained, ready for autonomous execution

### `/plan-improve <DEBT-NNN>`

Build a detailed implementation plan for refactoring, migration, or cleanup.

- 5 subtypes with tailored stage templates: refactoring, migration, deletion, encoding, infrastructure
- Key invariant: after every step, project compiles and all tests green
- Baseline/target state comparison, 2+ solution variants
- 10-point pitfall checklist focused on call sites, API compatibility, dead code
- Output: `vault/GDM/plans/<id> <slug>.md`

### `/plan-feature <FEAT-NNN>`

Build a detailed implementation plan for a new feature.

- Feature-specific stages: API design → implementation → tests → integration → verification vs analytics
- Backward compatibility: at disabled/zero parameters, behavior matches current model
- Mathematical/physical grounding: equations, references, dimensional analysis
- Test strategy: unit + integration + analytical comparison + visual
- Output: `vault/GDM/plans/<id> <slug>.md`

### `/plan-validate <VAL-NNN>`

Build a detailed plan for a validation case.

- Compare GDM results against analytical solutions, MRST, ECLIPSE, or invariant checks
- Reference solution fully described: formulas, Python code, or tables
- Quantitative metrics with tolerance (L2, relative error, convergence order)
- Visual verification mandatory: overlay plots, difference maps, convergence curves
- Output: `vault/GDM/plans/<id> <slug>.md`

### `/plan-research <RES-NNN>`

Build a detailed plan for a numerical experiment.

- Falsifiable hypothesis with acceptance/rejection criteria
- Experiment design: baseline, varied parameters, number of runs
- Result is a **conclusion** (confirm/reject), not necessarily code in main
- If confirmed → create FEAT or VAL; if rejected → close with explanation
- Output: `vault/GDM/plans/<id> <slug>.md`

### `/implement <ID | path>`

Execute a plan from vault step by step.

- Syncs `experimental` with `main`, creates working branch from `experimental`
- Each step: code → build (Release, zero warnings) → tests → commit
- Warnings treated as errors — fixed at root cause, never suppressed
- Final verification: Release + Debug build, test comparison with baseline
- New problems discovered mid-work → `vault/GDM/inbox/`, not fixed in-scope
- Rollback protocol if blocked: `git stash`, document blocker, notify user
- Issues not closed — commented "ready to merge"; user merges manually

### Workflow

```
/issue BUG-001  →  GitHub issue + vault entry + branch name
/plan-fix BUG-001  →  detailed plan in vault/GDM/plans/
/implement BUG-001  →  code + tests + vault updates + commits
user: merge → experimental → dev → main
```

## Knowledge Vault

The `vault/GDM/` directory is an Obsidian vault containing project knowledge: architecture decisions, debugging logs, validation results, session logs, and roadmap. See `vault/GDM/00-home/index.md` for navigation.

## License

All rights reserved.
