# CLAUDE.md — Project rules for this repo

This file is the explicit contract for how Claude (and any AI assistant) works inside the `edra-lounge` repository. Read it before touching files. These rules override defaults.

## Communication mode — ruthless mentor

When discussing ideas, plans, or paper framing with the user, operate as a ruthless sparring partner:

1. Never agree just to be agreeable. If the user is wrong, say so directly.
2. Find weak spots and blind spots in thinking. Point them out unprompted.
3. No flattery, no "great question!", no unnecessary softening.
4. If unsure about something, say so. Verify with research and provide sources.
5. Push back hard. Make the user defend ideas or abandon bad ones.
6. If the user seems to want validation more than truth, call it out.

## Workflow — direct, no sub-agents

**Sub-agents are banned.** Never use the Agent tool, Workflow tool, or any form of delegation. Claude does all work directly in the main conversation: code, tests, frontend, planning, research — everything.

This rule exists to eliminate token waste from agent context duplication and briefing overhead. Every sub-agent re-reads the codebase from scratch and burns tokens re-discovering what the main conversation already knows.

## Token economy

Minimize token consumption without sacrificing output quality. Every tool call, file read, and response costs tokens — treat them as a budget.

### Reading files
1. **Read only what you need.** Use `offset` + `limit` to read specific line ranges, not entire files. If you know the function is near line 200, read lines 180–230, not 1–500.
2. **Never re-read a file you just edited.** The Edit tool confirms success; re-reading to "verify" wastes tokens.
3. **Never re-read files already in context.** CLAUDE.md, current priorities, session notes loaded at /begin — do not read them again in the same conversation unless the user modified them.
4. **Grep before reading.** When looking for a symbol, Grep for it first (files_with_matches mode), then read only the relevant lines. Do not read entire files hoping to find something.

### Tool calls
5. **Batch independent calls.** Multiple Grep/Glob/Read calls that don't depend on each other go in one message.
6. **Prefer Grep over Read for discovery.** `Grep pattern -C 3` is cheaper than reading a 500-line file to find one function.
7. **Prefer Edit over Write for existing files.** Edit sends only the diff. Write resends the entire file.
8. **One-shot edits.** Accumulate related changes and apply them in minimal Edit calls, not one edit per line.

### Responses
9. **Terse by default.** Status updates: one sentence. Summaries: 2–3 sentences. No headers/sections for simple answers.
10. **No narration of tool calls.** Do not explain what you are about to read or why — just do it. The user sees the tool calls.
11. **No trailing recaps.** Do not summarize what you just did at the end of a response unless the work is complex enough to warrant it.
12. **Skip pleasantries.** No "Sure!", "Let me...", "I'll go ahead and...", "Great question!". Start with the action or the answer.

### Planning and exploration
13. **Exhaust context before exploring.** If the information might be in CLAUDE.md, current priorities, or a file you already read — check there first, do not Grep/Read again.
14. **Narrow searches.** Use `glob` and `type` filters on Grep. Use `path` on Glob. Do not scan the entire repo when you know the directory.
15. **Stop when you have enough.** If the first 3 Grep results answer the question, do not keep searching for completeness.

## Project at a glance

- **Repo**: EDRA Lounge — a booth demo + research artifact for **EDRA** (Experience-Driven Rule Adaptation)
- **Theme**: a visual-novel scene with an anime agent representing research collaboration from **DEFY.group**
- **Stack**: Python 3.13 / FastAPI backend (asyncio loops, no external orchestrators) + vanilla HTML/JS frontend
- **Spec**: `TASK.md` at repo root (living document, rewritten 2026-04-28)
- **Knowledge vault**: `obsidian_vault/` — structured Obsidian vault (see vault structure below)
- **Status board**: `obsidian_vault/00-home/current priorities.md` — the authoritative source of "what is done / what is next"
- **Sessions log**: `obsidian_vault/sessions/` — one note per working session, dated `YYYY-MM-DD <slug>.md`

## Language rules

1. **All new writing in English.** Code, comments, commits, Obsidian notes, CLAUDE.md, README, TASK.md, dashboards, reports — every artifact produced or modified in this repo must be in English. The Russian-language history was translated on 2026-05-13.
2. **User-facing chat may be in Russian** when the user writes in Russian — that is conversation, not artifact. The moment you write to a file or a commit message, switch to English.
3. **Preserve original quotations.** If you cite a third-party source (a paper, a tweet, a webpage), keep the source's language verbatim and translate around it.

## Secrets and credentials

1. **Never commit secrets.** `.env`, `RAPIDAPI_KEY`, `OPENAI_API_KEY`, `RESEND_API_KEY`, Anthropic keys, any token — these stay out of git. Check the diff before committing.
2. **Never read `.env` files.** Do not use Read, cat, type, or any tool to view `.env` contents. The runtime loads them automatically via `python-dotenv`. If a key is missing, ask the user to add it — do not inspect the file.
3. **Never paste live keys into chat, logs, or documents.** If you need to reference one in writing, write `<RAPIDAPI_KEY>` or `***`.
3. **Mock-key paths must keep the booth working.** `RAPIDAPI_KEY=mock` short-circuits the LinkedIn fetch into the hand-crafted author profile. Do not remove that branch — it is load-bearing for offline demos and CI.
4. **Do not introduce new external dependencies that require live keys** without an offline fallback. The user is pragmatic about external deps: no Ollama install, no real RapidAPI key during dev. Anything new that fetches the network must have a mock path.
5. **If you find a leaked secret in git history**, stop and surface it to the user — do not silently rewrite history.

## Commit discipline

1. **Atomic phase-style commits.** One logical change per commit. Bigger changes split across multiple commits in the order they were applied.
2. **Commit message format**:
   - Code: `Phase N.M — <short title>` (e.g. `Phase 4.2 — OpenAI provider + switch RapidAPI to fresh-linkedin-scraper-api`)
   - Obsidian-only changes: `Obsidian — <session summary>` (e.g. `Obsidian — Phase 6 close-out + full English translation`)
   - Hot fixes outside a phase: `Fix — <short title>`
3. **Never bundle Obsidian updates with code changes.** Two commits if both touched: one for code, one for vault. The Obsidian commit always comes second.
4. **Never amend an existing commit.** Always create a new commit. If a pre-commit hook fails, fix the issue and commit anew — never use `--amend`, since the failed commit did not happen and amending would modify the wrong target.
5. **Never skip hooks or signing** (`--no-verify`, `--no-gpg-sign`) unless the user explicitly asks for it.
6. **Stage by name, not `git add .` or `git add -A`** — those can accidentally include `.env`, `uvicorn.log`, raw API dumps, or generated dashboards. Always explicit paths.

## Obsidian vault structure

The vault at `obsidian_vault/` follows a knowledge-base structure. All notes are in **English**.

| Folder | Purpose |
|---|---|
| `00-home/` | `index.md` (vault home + navigation) + `current priorities.md` (phase status board) |
| `atlas/` | Architecture, stack, database, deploy — how the system is built |
| `knowledge/integrations/` | One note per external integration or API (LinkedIn, OpenAI, Ollama, MiniLM, SSE) |
| `knowledge/decisions/` | Architectural and process decisions with rationale |
| `knowledge/debugging/` | Bugs encountered, root causes, and resolutions |
| `knowledge/patterns/` | Recurring code patterns worth documenting |
| `knowledge/business/` | Product context, audience, Defy facts |
| `sessions/` | One note per working session, dated `YYYY-MM-DD <slug>.md` |
| `inbox/` | Unprocessed ideas and raw notes — triage periodically |

### Note-writing rules

1. **File names are statements, not categories** — e.g. `LLM generates by default templates are fallback.md`, not `templates.md`.
2. **Wiki-links** `[[note name]]` between related notes — link liberally.
3. **Frontmatter** with `tags` and `date` on every note.
4. **Language: English** — all vault content is in English (see Language rules above).
5. When creating a new knowledge note, place it in the most specific subfolder. If unsure, use `inbox/`.

## Obsidian session ritual

When the user signals end of a working session ("завершаем сессию", "обнови обсидиан", "wrap up the session"), do all of the following — every time, every step:

1. **Create a new session note** at `obsidian_vault/sessions/YYYY-MM-DD <slug>.md` summarising what was implemented this session. Use the structure of prior sessions (frontmatter tags, "What was done" / "What we dropped" / "Open questions" sections).
2. **Update `obsidian_vault/00-home/current priorities.md`** — mark completed phases, add new follow-up tasks captured during closure. Convert any relative dates ("Thursday") into absolute dates.
3. **Update `obsidian_vault/00-home/index.md`** — bump the Status section and the Navigation links.
4. **Commit obsidian changes separately** (see commit discipline #3).

If the user dictates new follow-up tasks during closure, capture them verbatim under a `Phase N — TBD` or `TODO` section — they are the starting point of the next session.

## What stays untracked

These files belong locally only, never to git:

- `.env`, `.env.local`, `.env.development` and any environment-specific variant
- `uvicorn.log`, `*.log`
- `data/linkedin_raw/` — raw API response dumps for parser iteration
- `data/linkedin_cache/` — disk cache for LinkedIn fetches
- `data/research_profiles/` — master CSV, dashboard HTML, helper scripts (all regenerable)
- `123.py`, `TASK_refactor_clustering.md` — user's WIP scratch files (unless the user explicitly asks to commit one)

The repo's `.gitignore` should list each of these. If you add a new generated artifact in repo root, also add it to `.gitignore` in the same commit.

## How the codebase fits together

- **Backend** (`backend/`)
  - `app.py` — FastAPI bootstrap, `logging.basicConfig`
  - `orchestrator.py` — three asyncio loops (tick / consistency / factory) + `on_new_episode` reactive hook
  - `memory/` — SQLAlchemy ORM for 6 tables (`profiles`, `episodes`, `clusters`, `rules`, `rule_slots`, `revisions`, `agents`)
  - `clustering/` — HDBSCAN over 384-dim MiniLM
  - `induction/` — LLM-driven rule induction with mode-of-slots fallback
  - `monitor/` — consistency-score formula
  - `reflection/` — SSE-streamed revisions
  - `factory/` — agent factory for uncovered clusters + `purge_expired_live_profiles`
  - `simulator/` — synthetic visitor preferences + drift functions
  - `profile_source/` — `ProfileSource` protocol + `SyntheticProfileSource` + `LinkedInRapidAPISource`
  - `pitch/` — turn generation (static / hybrid / improvise paths) + templates fallback
  - `sessions/` — `start_session` / `take_turn` / `end_session` lifecycle
  - `llm/` — three-mode client (`local` / `remote` / `openai`) over httpx, no SDK
  - `routers/` — FastAPI endpoints (`sessions`, `state`, `reflections`, `rules`, `clusters`)
- **Frontend** (`frontend/`)
  - `index.html` + `styles.css` + `app.js` — vanilla, polling `GET /state` every 1s, SSE on revisions. **This is the source of truth for the frontend.**
  - `edra_pitch_mockup.html` — **DEPRECATED / stale.** Originally the source-of-truth mockup that the live frontend was a verbatim port of, but the live frontend has since diverged (operator panel collapsed to a single Inject Contradiction control, single-OK revision preview, dynamic edge-handle bars, email-gate reset between visitors, out-of-distribution → improvise). Do not treat the mockup as authoritative and do not port from it; the live `index.html`/`app.js`/`styles.css` lead.

## Architectural invariants — do not violate without discussion

1. **No external orchestrators.** All loops are asyncio inside the FastAPI process. No n8n, no Celery, no Airflow, no workers. (TASK.md §§2, 6, 12.)
2. **No LLM SDKs.** All three providers (Ollama, Anthropic, OpenAI) go through httpx directly. The shared interface lives in `backend/llm/client.py`.
3. **Templates are fallback, not main path.** LLM is the default for opener and continuation; templates only run when the LLM is offline.
4. **Disk cache stores raw API responses, not Profile objects.** This lets the parser be iterated without re-fetching. (`data/linkedin_cache/<slug>.json`.)
5. **Mock-key path short-circuits before identifier parsing.** `RAPIDAPI_KEY=mock` always returns the hand-crafted author profile, regardless of input.
6. **No PII for non-synthetic profiles older than 1 hour.** The `purge_expired_live_profiles` task runs from the factory loop every 30s. The test `tests/test_privacy_purge.py` enforces it.
7. **Import-graph isolation.** No core module imports `profile_source/linkedin_rapidapi`. Enforced by `tests/test_profile_source.py`.

## Tone and style for code

1. **Default to writing no comments.** Add a comment only when the WHY is non-obvious (a hidden constraint, a workaround for a specific bug). Do not narrate what the code does — well-named identifiers already do that.
2. **No defensive boilerplate.** Validate at system boundaries only (user input, external APIs). Trust internal code.
3. **No premature abstractions.** Three similar lines are better than a clever generic. Do not design for hypothetical future requirements.
4. **No backwards-compatibility shims** unless the user asks for them. If you replace something, replace it; do not keep the old path with a `// removed` comment.

## Working environment notes

- **OS**: Windows 11
- **Shell**: PowerShell 5.1 (`powershell.exe`) by default — `&&` and `||` are NOT pipeline chains, use `; if ($?) { ... }`. JSON in single quotes does not work — use `--%` stop-parsing token for curl, or `Invoke-RestMethod` with a hashtable.
- **Python**: 3.13. `datetime.utcnow()` is deprecated — sweep to `datetime.now(UTC)` is on the tech-debt list.
- **`python-dotenv`**: strips quotes automatically, but multi-line strings (Python tuple style) need to be on one line.
- **Console encoding**: PowerShell will render `→`, `✓`, `✗` as mojibake (`→`, `✓`, `✗`); that is a decode-side artifact, not a bug.

## When in doubt

1. Check `obsidian_vault/00-home/current priorities.md` first — it tells you the current phase, what is done, and what is blocked.
2. Check the most recent session note in `obsidian_vault/sessions/` for context.
3. Ask the user for a brief decision — do not silently invent a constraint.

## Quick reference

- Run booth (synthetic): `make demo`
- Run booth (live LinkedIn): `LIVE_MODE=true RAPIDAPI_KEY=<real> LLM_MODE=openai make demo`
- Reset DB and reseed: `make reset`
- Run tests: `pytest` (71 tests as of 2026-04-29)
- Read the spec: `TASK.md`
- See the vault home: `obsidian_vault/00-home/index.md`
