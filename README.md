# pdfedit

Headless PDF editing library + CLI for Linux. The reader/editor core behind a future GUI.

**Status:** planning / scaffolding. No implementation yet.

## What this is

A fork-and-extend of **PDF4QT** (MIT, `Pdf4QtLibCore` + `PdfTool`) into a standalone
headless library and CLI, with our own additions:

- **RTL (Arabic/Persian/Hebrew) text write + search** — the differentiator; PDF4QT has none.
- **Object-level deletion** (text runs, images, other content elements) exposed via CLI.
- **Add-text** (LTR + RTL) exposed via CLI.
- Deterministic, agent-testable core: golden-image tests, headless CLI, stable output.

**GUI is explicitly out of scope.** This repo is the library + CLI only. A future GUI
(thin Qt shell or PDF4QT's existing apps) will consume this library.

## Repo layout

```
docs/            design, decisions (ADRs), coding standard, research notes
plans/           master plan + milestone breakdowns
db/              schema + seed for the SQLite tracking database
scripts/         db.py (tracking DB CLI) + other tooling
agents/          role definitions for the agent team that writes this code
skills/          skills vendored/pinned for agents
src/core/        the PDF editing library (fork of Pdf4QtLibCore + additions)
src/cli/         the CLI (fork of PdfTool + new commands)
src/tests/       unit + golden tests
tools/           helper binaries/scripts for dev workflows
```

## How to work here (for agents AND humans)

1. **Read `AGENTS.md`** at the repo root first — it is the binding contract for every agent.
2. **Read `docs/coding-standard.md`** — style, naming, testing, commit rules.
3. **Check the tracking DB** — every component, task, decision, and question is tracked:
   `python3 scripts/db.py status` (see `scripts/db.py --help`).
4. **Follow the plan** — `plans/PLAN.md` is the master roadmap. Work in milestones.

## Quick reference

```bash
# Tracking database
python3 scripts/db.py status            # everything, by component
python3 scripts/db.py search "rtl"      # searchable (FTS5)
python3 scripts/db.py tasks --open      # open tasks

# Build (once the fork lands)
cmake -S . -B build && cmake --build build -j
```

## License

MIT (inherited from PDF4QT) unless the ADRs decide otherwise. Third-party deps:
Qt (LGPL), FreeType (FTL), OpenJPEG (MIT), OpenSSL (Apache-2.0), ZLIB, plus
HarfBuzz (MIT) and FriBidi (LGPL-2.1) to be added for RTL.
