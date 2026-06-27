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
- 274 Catch2 tests, 6 standalone examples
- Obsidian knowledge vault for decisions, debugging, validation

## Build

```powershell
cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

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

## Knowledge Vault

The `vault/GDM/` directory is an Obsidian vault containing project knowledge: architecture decisions, debugging logs, validation results, session logs, and roadmap. See `vault/GDM/00-home/index.md` for navigation.

## License

All rights reserved.
