# AGENTS.md — Binding contract for every agent working in this repo

> Read this file completely before doing anything. It overrides general coding habits.
> If a task conflicts with this file, STOP and ask the orchestrator (Yolka) — do not improvise.

## 1. Project identity

- **What we build:** `albdf` — a headless PDF editing **library** + **CLI** for Linux,
  focused on Middle Eastern languages (Arabic/Persian/Hebrew RTL).
- **What we do NOT build by default:** any GUI. The headless default (no QWidgets, no
  QML, no web frontend) is preserved. Since M12 a **vendored, optional GUI** exists:
  the PDF4QT GUI dirs (`Pdf4QtLibWidgets`, `Pdf4QtLibGui`, `Pdf4QtEditor`,
  `Pdf4QtViewer`, `Pdf4QtPageMaster`) build only with `-DALBDF_BUILD_GUI=ON`, and
  **core + CLI + tests must stay headless and deterministic regardless of that flag**
  (see ADR-0002 addendum). All work must be testable from a terminal with no display.
- **Base:** fork of PDF4QT (`Pdf4QtLibCore` + `PdfTool`, CLI shipped as `albdf`), MIT. We extend it, we don't
  rewrite it. Our fork lives under `src/`; upstream is a git remote.
- **Differentiators:** RTL (Arabic/Persian/Hebrew) write + search; object deletion; add-text.

## 2. Ground rules

1. **Determinism above all.** The CLI and library must be byte-deterministic for a
   given input: no timestamps in output streams unless asked, no random IDs, no
   `QDateTime::currentDateTime()` in document output. Tests depend on this.
2. **Headless everything.** All tests must pass with `QT_QPA_PLATFORM=offscreen`
   and no display. Never write a test that needs a window.
3. **CLI-first.** Any capability must be reachable via the CLI before (or with) any
   library API change. If you can't demonstrate it from a shell command, it doesn't exist.
4. **Test-driven.** Every bug fix gets a failing test first. Every feature gets tests
   that fail before the feature lands. Golden-image tests for anything that renders.
5. **Small commits.** One logical change per commit. Conventional Commits format.
   Commit messages explain WHY, not just what.
6. **No scope creep.** Do exactly what the task says. Note anything extra in a comment
   or the tracking DB as a proposal — do not implement it.
7. **Never fake results.** If a test fails, it fails. If you can't build, say so.
   Never mark a task done in the DB without a real `git log` + passing test as evidence.
8. **Document as you go.** Update `docs/` and the tracking DB (`scripts/db.py`) when
   you change a component's status, add a decision, or discover a risk.

## 3. The tracking database is the source of truth for status

Everything is tracked in `db/albdf.db` (SQLite + FTS5). CLI: `python3 scripts/db.py`.

```bash
python3 scripts/db.py status          # full status by component
python3 scripts/db.py tasks --open    # open tasks
python3 scripts/db.py search "rtl"    # FTS5 search across tasks/decisions/notes
python3 scripts/db.py task-done <id> --ref <commit-sha>   # close a task with evidence
```

**Rules:**
- Every task you work on must exist in the DB (create it if missing: `db.py task-add`).
- Closing a task REQUIRES `--ref <commit-sha>`; no evidence, no close.
- Decisions are recorded as ADRs in `docs/decisions/` AND summarized in the DB.

## 4. Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Golden tests: `python3 scripts/db.py` does not build; use `src/tests/golden/run.sh`.

## 5. Code style

Full standard in `docs/coding-standard.md`. Non-negotiable highlights:

- C++20, Qt 6.8+, CMake. Follow existing PDF4QT naming (`PDF*` classes, `m_` members).
- `clang-format` must be clean: `clang-format --dry-run --Werror` on every touched file.
- No `using namespace std;`. No exceptions across API boundaries without docs.
- Public API in headers gets Doxygen comments.
- License header on every new file: `SPDX-License-Identifier: GPL-3.0-or-later` +
  the standard GPLv3 notice, "Copyright (c) 2026 albdf contributors". The project is
  GPL-3.0-or-later (ADR-0005); upstream PDF4QT portions keep MIT.

## 6. Git workflow

- Branch per milestone/task: `m<milestone>/<slug>`.
- Conventional Commits: `feat:`, `fix:`, `test:`, `docs:`, `refactor:`, `chore:`.
- Every commit must compile (no "wip" commits that break the build — use `git stash`
  or work in a branch).
- Merge via rebase, keep history linear. Squash only under explicit instruction.
- **No push to main until fully tested** — `ci/run-ci.sh` green (see CONTRIBUTING.md).
- The pre-commit hook (`.githooks/pre-commit`, install with
  `scripts/install-hooks.sh`) regenerates `REPO_MAP.md` and gates markdown
  structure on staged docs. `git commit --no-verify` bypasses it, but the same
  checks run in CI.

## 7. If you are unsure

- Check the DB, the plan, the ADRs, and the upstream PDF4QT source in that order.
- Ask the orchestrator before: changing public API shape, adding a dependency,
  changing the license posture, or diverging from the fork strategy.

## 8. Quick map of AGENT.md guides

Orientation first: `REPO_MAP.md` (auto-generated index: quick start, doc
index, key files) and `CONTRIBUTING.md` (the five binding contribution rules).
Per-folder onboarding guides supplement this file. Load the one for whatever
you are touching:

| Guide | Purpose |
|---|---|
| `src/AGENT.md` | Source tree layout, build (CMake+vcpkg), RTL pipeline location, custom CLI tools, exit-code contract, pitfalls |
| `src/Pdf4QtLibCore/AGENT.md` | The core PDF library: naming, subsystems, registering new sources, ABI macro, documented RTL quirks |
| `src/PdfTool/AGENT.md` | How to add a CLI command, static registration, output formatter + exit-code contracts |
| `src/UnitTests/AGENT.md` | How to add a unit/integration test, the QProcess `runTool` helper, compile definitions |
| `src/tests/AGENT.md` | Fixtures, fonts (OFL only), golden images, smoke.sh |
| `db/AGENT.md` | Using the tracking DB: commands, evidence-gated task completion, FTS5 search, gitignore discipline |
| `docs/AGENT.md` | Writing docs: layout, ADR vs research vs coding-standard, man page, RELEASES.md, context7 vendoring |
