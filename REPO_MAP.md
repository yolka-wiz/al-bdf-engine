# REPO_MAP.md — albdf repository orientation index

> **Purpose:** one-page orientation for humans and AI agents: what this
> repo is, how to build/test it, and where every document lives. This
> file is **auto-generated** by `scripts/gen-repo-map.py` from the
> pre-commit hook (`.githooks/pre-commit`). Do not edit by hand; the
> next commit regenerates it. Deterministic: no timestamps, sorted.

> **Binding contracts:** read `AGENTS.md` first — it overrides general
> coding habits. The tracking DB (`db/albdf.db`, gitignored) is the
> source of truth for component/task status; rebuild with
> `python3 scripts/db.py init && python3 db/seed.py` on a fresh clone.

## Quick start (build & test)

```bash
source ~/.bashrc
cd src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \\
    -DVCPKG_OVERLAY_PORTS=$PWD/vcpkg/overlays -DALBDF_BUILD_TESTS=ON
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen bash src/tests/smoke.sh src/build/bin/albdf src/tests/fixtures
bash ci/run-ci.sh   # full gate: Release + ctest + ASAN/UBSAN + clang-format
```

> Build dir is `src/build` (repo root has no CMakeLists). vcpkg manifest
> deps: `$VCPKG_ROOT` (vcpkg manifest mode). Aliyun mirrors everywhere.
> Headless: `QT_QPA_PLATFORM=offscreen` is mandatory for all Qt runs.

## Top-level directory map

| Path | Purpose |
|---|---|
| `.githooks/` | Git hooks: pre-commit regenerates REPO_MAP.md + markdown structure gate |
| `agents/` | Per-role agent contracts (core, cli, rtl, test, research, orchestrator) |
| `ci/` | CI gate: run-ci.sh (build + ctest + ASAN/UBSAN + clang-format) |
| `db/` | Tracking DB: schema.sql + seed.py committed; albdf.db gitignored (db.py CLI) |
| `docs/` | PROBLEMS.md, RELEASES.md, ADRs (decisions/), research notes, man page, coding standard |
| `plans/` | PLAN.md roadmap (M0–M8.1) + per-problem fix plans (P3-execution.md, …) |
| `scripts/` | Tooling: db.py (tracking DB), gen-repo-map.py (this file), install-hooks.sh |
| `skills/` | Vendored Qt Company agent skills (qt-cmake-project, qt-cpp-docs, qt-cpp-review) |
| `src/` | The fork: Pdf4QtLibCore (engine) + PdfTool (CLI) + UnitTests + tests |
| `agents/` | *(unmapped — add to scripts/gen-repo-map.py)* |
| `ci/` | *(unmapped — add to scripts/gen-repo-map.py)* |
| `db/` | *(unmapped — add to scripts/gen-repo-map.py)* |
| `docs/` | *(unmapped — add to scripts/gen-repo-map.py)* |
| `plans/` | *(unmapped — add to scripts/gen-repo-map.py)* |
| `scripts/` | *(unmapped — add to scripts/gen-repo-map.py)* |
| `skills/` | *(unmapped — add to scripts/gen-repo-map.py)* |

## Document index (markdown, except README.md)

| Document | Title | One-liner |
|---|---|---|
| `AGENTS.md` | AGENTS.md — Binding contract for every agent working in this repo | > Read this file completely before doing anything. It overrides general coding habits. > If a task conflicts with this file, STOP and ask the orchestrator (Yolka) — do not… |
| `CONTRIBUTING.md` | CONTRIBUTING.md — albdf contribution rules | > These are the binding rules for every contributor — human or AI agent. > The short version is also summarized in `REPO_MAP.md`; this file is the > full contract. If a task… |
| `REPO_MAP.md` | REPO_MAP.md — albdf repository orientation index | > **Purpose:** one-page orientation for humans and AI agents: what this > repo is, how to build/test it, and where every document lives. This > file is **auto-generated** by… |
| `SECURITY.md` | Security Policy | Please **do not open a public issue** for security vulnerabilities. Report privately to the maintainers via GitHub's **Security Advisory** flow: 1. Open the repository on GitHub. |
| `agents/roles/README.md` | Agent Roles | This project is written by an agent team. Each role is a **contract**: when the orchestrator dispatches a subagent for a task, the subagent receives the role definition + task +… |
| `agents/roles/cli-agent.md` | Role: cli-agent | **Mission:** own the CLI surface (fork of upstream PDF4QT `PdfTool`, shipped as the `albdf` binary) — command dispatch, output formatting, determinism, and the user-facing… |
| `agents/roles/core-agent.md` | Role: core-agent | **Mission:** own the PDF core library — the PDF4QT fork under `src/` — and deliver text recognition, object deletion, and add-text. **Scope (may touch):** `src/Pdf4QtLibCore/`,… |
| `agents/roles/orchestrator.md` | Role: orchestrator (Yolka) | **Mission:** run the project — planning, dispatch, review, tracking, and keeping every agent unblocked. This role is executed by the primary agent (Yolka) in this session, not by… |
| `agents/roles/research-agent.md` | Role: research-agent (Rosetta) | **Mission:** deep research that unblocks feature agents — API discovery, library evaluation, spec reading, skill sourcing. Runs as a separate research sandbox (hermes-sandbox),… |
| `agents/roles/rtl-agent.md` | Role: rtl-agent | **Mission:** the differentiator — correct Arabic/Persian/Hebrew **write + search** in PDF. This is the most research-heavy role: it turns the RTL pipeline in ADR-0003 into code.… |
| `agents/roles/test-agent.md` | Role: test-agent | **Mission:** make "done" mean something — deterministic test infrastructure that proves every feature works and keeps working. **Scope (may touch):** `src/tests/`, `scripts/`… |
| `db/AGENT.md` | AGENT.md — the tracking database (source of truth for status) | > Everything about the state of `albdf` lives in `db/albdf.db`. If you > changed a component's status, closed a task, added a decision, found a risk, > or imported a dependency,… |
| `docs/AGENT.md` | AGENT.md — writing documentation in this repo | > How to document `albdf`. The binding code rules live in > `docs/coding-standard.md` (referenced, not restated here). This guide is > about the docs/ *layout*, when to write… |
| `docs/PROBLEMS.md` | albdf — Known Problems, Future Outlook & Catches | > Living document for agents (and humans) working on this repo. This is the > institutional memory of the sharp edges discovered while building v0.1.0. > If you hit something new,… |
| `docs/RELEASES.md` | albdf — release notes | Date: 2026-08-06 Branch: `feature/wave1-infra` (target: `main`) - **Hosted CI (W1, DB #25)** — the local gate now also runs on GitHub-hosted |
| `docs/branch-protection.md` | Branch protection — `main` | > How the repository is guarded so nothing reaches `main` (or a release) > without passing the gates. This describes the *intended* settings; the > actual GitHub settings must… |
| `docs/coding-standard.md` | albdf Coding Standard | Applies to ALL code in this repo, human- or agent-written. Binding — see `AGENTS.md` §5. When in doubt, follow the surrounding PDF4QT code style; this standard codifies it. --- |
| `docs/context7/README.md` | Context7 MCP usage notes (albdf) | Fetched 2026-08-04 via native `mcp__context7__*` tools (wired by user on host). 1. `resolve-library-id` with BOTH args: - `libraryName`: official name ("HarfBuzz", not "harfbuzz") |
| `docs/context7/freetype.md` | FreeType docs (Context7, fetched 2026-08-04) | Source: https://context7.com/freetype/freetype — resolved via MCP `resolve-library-id` (`/freetype/freetype`, High reputation, 149 snippets). Library ID for future `query-docs`… |
| `docs/context7/fribidi.md` | GNU FriBidi docs (Context7, fetched 2026-08-04) | Source: https://context7.com/fribidi/fribidi — resolved via MCP `resolve-library-id` (`/fribidi/fribidi`, High reputation, 119 code snippets). Library ID for future `query-docs`… |
| `docs/context7/harfbuzz.md` | HarfBuzz docs (Context7, fetched 2026-08-04) | Source: https://context7.com/harfbuzz/harfbuzz — resolved via MCP `resolve-library-id` (`/harfbuzz/harfbuzz`, High reputation, 2024 code snippets). Library ID for future… |
| `docs/decisions/0001-fork-pdf4qt.md` | ADR-0001: Fork PDF4QT (MIT) as the base library + CLI | **Status:** accepted (2026-08-04) We need a headless PDF editing library + CLI for Linux with RTL write/search and object deletion. Research (`pdf-editor-research/report.md`)… |
| `docs/decisions/0002-no-gui-in-v1.md` | ADR-0002: No GUI in v1 — library + CLI only | **Status:** accepted (2026-08-04) User directive: "forget about the gui, we only care about the library for now." The project is a big agent-written codebase; the core must be… |
| `docs/decisions/0003-rtl-differentiator.md` | ADR-0003: RTL write + search is the differentiator and is greenfield | **Status:** proposed (2026-08-04) No OSS native PDF app does correct RTL (Arabic/Persian/Hebrew) write + search: Okular bug 353300 open since ~2015, pdf.js search has no bidi… |
| `docs/decisions/0004-license-posture.md` | ADR-0004: License posture — MIT fork + permissive deps only | **Status:** accepted (2026-08-04) The fork inherits MIT from PDF4QT. The project must stay clean for permissive distribution. No AGPL/GPL contamination (research report §2.4:… |
| `docs/decisions/0005-gpl-license.md` | ADR-0005: Relicense to GPL-3.0-or-later | **Status:** accepted (2026-08-04) **Supersedes:** ADR-0004 (MIT fork + permissive deps only) The project's license posture was MIT (inherited from the PDF4QT fork, ADR-0004), |
| `docs/decisions/0006-forms-signatures-cli.md` | ADR-0006: Forms and digital signatures via CLI | - Status: accepted - Date: 2026-08-04 - Deciders: user, Yolka |
| `docs/research/001-pdf4qt-deepdive.md` | 001 — PDF4QT deep dive (CLI extension, text-flow write-back, fonts, determinism) | - **Date:** 2026-08-04 - **Author:** research subagent (Rosetta brief 001, task 0) - **Status:** verified against source ([V]) unless flagged [unverified] |
| `docs/research/002-rtl-reference-implementations.md` | 002 — RTL reference implementations (for PDF4QT fork) | - **Date:** 2026-08-04 - **Author:** research subagent (Rosetta brief 001, task 1) - **Status:** source-verified against fpdf2 master ([V]); behavioral claims that could not be |
| `docs/research/003-skills-and-agent-conventions.md` | 003 — Skills Sourcing & AGENTS.md Conventions | - **Date:** 2026-08-04 - **Author:** research-agent (Rosetta brief #003) - **Status:** draft — findings for orchestrator review; no skills were installed by this brief |
| `docs/research/004-gui-architecture.md` | GUI Architecture — how a desktop frontend consumes albdf | > Status: **THINKING / design note** (2026-08-05, yolka). Not a decision, not a > plan to execute — ADR-0002 still defers the GUI. This records how the binary > would be consumed… |
| `docs/research/baseline-upstream.md` | Baseline — Pristine PDF4QT on this Container (M0.5) | **Date:** 2026-08-04 **Status:** DONE — recorded BEFORE any fork modifications (M0.5 gate) **Upstream:** JakubMelka/PDF4QT 1.6.0.0 (master, depth-1 clone 2026-08-03) |
| `docs/research/brief-001-rosetta.md` | Research Brief 001 — for Rosetta (research agent) | **From:** Yolka (orchestrator), albdf project **Date:** 2026-08-04 **Context:** We are forking PDF4QT (MIT) into a headless PDF editing library + CLI for Linux |
| `docs/setup-context7.md` | context7 MCP wiring (Hermes side) | > How to configure the context7 MCP server for the Hermes agent runtime. > The API key is a **secret** — it must never be committed to this repo. The `mcp_servers` config lives in… |
| `plans/P3-execution.md` | P3 Execution Plan — Vertical Mark Offsets (Orchestrated) | > **For Hermes:** orchestrated development via subagents. Each phase is a > self-contained delegate_task with its own RED/GREEN gate and commit. > Status:… |
| `plans/P3-vertical-mark-offsets.md` | P3 — Vertical mark offsets (diacritics above the baseline) | Status: **RESOLVED** (2026-08-05) · Owner: rtl-agent · Effort: **M** Fixed by `109a4af` (Ts emission) + `9a4587d` (flow phantom-space guard) + `4ed7ab2` (kasra band calibration);… |
| `plans/PLAN.md` | albdf — Master Project Plan (v2, finalized 2026-08-04) | > **For Hermes/orchestrator:** this is the roadmap. Granular status lives in `db/albdf.db` > (`python3 scripts/db.py status`). Implementation is delegated to agent roles in >… |
| `plans/m10-fix-render-args-execution.md` | F#1/F#2 Render Argument Validation Fixes — Execution Plan (DB #32/#33) | > Binding context: repo root AGENTS.md (read fully first), src/AGENT.md, > src/PdfTool/AGENT.md (exit-code contract!), src/UnitTests/AGENT.md, > docs/PROBLEMS.md (F#1/F#2 entries). |
| `src/AGENT.md` | AGENT.md — albdf Source Tree Guide | Onboarding guide for AI agents working in `src/`. This is the buildable source tree of the **albdf** project — a fork of MIT-licensed **PDF4QT** delivering a headless PDF editing… |
| `src/Pdf4QtLibCore/AGENT.md` | AGENT.md — Pdf4QtLibCore (Core PDF Library) | Guide to `Pdf4QtLibCore/` — the fork of the MIT-licensed **PDF4QT** core, packaged as a shared library. This is where the PDF document model, text flow, fonts, and our RTL… |
| `src/Pdf4QtLibCore/liberation-fonts-ttf/README.md` | src/Pdf4QtLibCore/liberation-fonts-ttf/README.md | Liberation Fonts ================= The Liberation Fonts is font collection which aims to provide document layout compatibility as usage of Times New Roman, Arial, Courier New.… |
| `src/PdfTool/AGENT.md` | AGENT.md — albdf CLI (`src/PdfTool/`) | Onboarding guide for AI agents adding or modifying CLI commands in the `albdf` fork. Read [`AGENTS.md`](../../AGENTS.md) (binding contract) and… |
| `src/README.upstream.md` | PDF4QT | [![CI](https://github.com/JakubMelka/PDF4QT/actions/workflows/ci.yml/badge.svg)](https://github.com/JakubMelka/PDF4QT/actions/workflows/ci.yml) **(c) Jakub Melka 2018-2025**… |
| `src/UnitTests/AGENT.md` | AGENT.md — Unit / Integration tests (`src/UnitTests/`) | Onboarding guide for agents adding or modifying tests in the `albdf` fork. Read [`AGENTS.md`](../../AGENTS.md) and [`docs/coding-standard.md`](../../docs/coding-standard.md)… |
| `src/tests/AGENT.md` | AGENT.md — Test fixtures, fonts, goldens & smoke (`src/tests/`) | Onboarding guide for agents working with the deterministic test corpus of the `albdf` fork. Read [`AGENTS.md`](../../AGENTS.md) and… |
| `src/tests/README.md` | albdf test harness (M2) | Deterministic regression harness for the fork. Everything here is committed; fixtures and goldens are reproducible from `scripts/`. \| Path \| Contents \| |
| `src/tests/fonts/README.md` | Test fonts (all permissive licenses — ADR-0004) | Bundled for deterministic RTL integration tests. Licenses: \| Font \| Purpose \| License \| Source \| \|---\|---\|---\|---\| |

## Key source files

| File | Role |
|---|---|
| `src/Pdf4QtLibCore/sources/pdfdocumenttextflow.cpp` | text flow factory (fetch-text / search input) |
| `src/Pdf4QtLibCore/sources/pdfpagecontentprocessor.cpp` | content stream processor (render + flow) |
| `src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp` | RTL write pipeline: FriBidi + HarfBuzz shaping + Type0 embed + Ts mark offsets |
| `src/Pdf4QtLibCore/sources/pdfrtltextnormalizer.cpp` | RTL normalization (tashkeel, presentation forms, digits) |
| `src/Pdf4QtLibCore/sources/pdftextlayout.cpp` | text layout + phantom-space heuristic (P3 guard) |
| `src/Pdf4QtLibCore/sources/pdftextsearchengine.cpp` | search engine: joined visual string + span mapping (S#1) |
| `src/PdfTool/main.cpp` | CLI entry point (albdf binary) |
| `src/PdfTool/pdftooladdtext.cpp` | add-text command (LTR + RTL) |
| `src/PdfTool/pdftooldeleteobject.cpp` | delete-object command |
| `src/PdfTool/pdftoolformfill.cpp` | form-fill command |
| `src/PdfTool/pdftoolformlist.cpp` | form-list command (AcroForm) |
| `src/PdfTool/pdftoolsearchtext.cpp` | search-text command (RTL-aware) |
| `src/PdfTool/pdftoolsign.cpp` | sign command (PKCS#7 / PAdES) |

## Tracking DB status

```text
== Components ==
  [32mdone[0m add-text               Add text (LTR + RTL) via CLI
                owner: core-agent  dir: src/Pdf4QtLibCore/
  [32mdone[0m cli                    CLI surface (albdf binary)
                owner: cli-agent  dir: src/PdfTool/
  [32mdone[0m fork-base              PDF4QT fork: Pdf4QtLibCore + PdfTool
                owner: core-agent  dir: src/
  [32mdone[0m forms-signatures       Forms (AcroForm) + digital signatures
                owner: core-agent  dir: src/PdfTool/
  [32mdone[0m object-deletion        Whole-object deletion (text runs, images, elements)
                owner: core-agent  dir: src/Pdf4QtLibCore/
  [32mdone[0m research               Research stream (Rosetta)
                owner: rosetta  dir: docs/research/
  [32mdone[0m rtl-search             RTL-aware search
                owner: rtl-agent  dir: src/Pdf4QtLibCore/
  [32mdone[0m rtl-writer             RTL write pipeline (Arabic/Persian/Hebrew)
                owner: rtl-agent  dir: src/Pdf4QtLibCore/
  [32mdone[0m tests                  Test infrastructure: unit + golden + CLI
                owner: test-agent  dir: src/tests/
  [32mdone[0m text-recognition       Text recognition as objects
                owner: core-agent  dir: src/Pdf4QtLibCore/

== Tasks ==
  #1    [32mdone[0m [fork-base] Vendor PDF4QT fork into src/, strip GUI apps (Viewer/Editor/PageMaster/Diff/LaunchPad)
        evidence: a52c18c
  #2    [32mdone[0m [fork-base] Verify albdf headless run: QT_QPA_PLATFORM=offscreen build + fetch-text smoke test
        evidence: 9ca9c38
  #3    [32mdone[0m [cli] Add delete-object CLI command (wire TextFlowEditor::removeItem + write-back)
        evidence: f354005
  #4    [32mdone[0m [cli] Add add-text CLI command (LTR)
        evidence: 327061b
  #5    [32mdone[0m [object-deletion] Image XObject reference-counting + Form XObject nesting for deletion
        evidence: f354005
  #6    [32mdone[0m [rtl-writer] Add HarfBuzz + FriBi
```

## Contributing rules (summary)

Full rules: `CONTRIBUTING.md`. The five binding rules:

1. **Branch per task**; commit each major step (Conventional Commits).
2. **Update docs** about what you change, as you change it.
3. **No push to main until fully tested** (`ci/run-ci.sh` green).
4. **Update PROBLEMS.md** for any problem your change fixes/creates.
5. **No unnecessary files, secrets, or data** in the repo.
