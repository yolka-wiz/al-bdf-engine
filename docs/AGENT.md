# AGENT.md — writing documentation in this repo

> How to document `albdf`. The binding code rules live in
> `docs/coding-standard.md` (referenced, not restated here). This guide is
> about the docs/ *layout*, when to write which kind of doc, and how to keep
> the man page, release notes, and vendored context7 docs current.

## Table of contents

- [The docs/ layout](#the-docs-layout)
- [Choosing the right kind of doc](#choosing-the-right-kind-of-doc)
- [ADRs (decisions/)](#adrs-decisions)
- [Research notes (research/)](#research-notes-research)
- [Vendoring context7 library docs](#vendoring-context7-library-docs)
- [The man page (albdf.1)](#the-man-page-pdftool1)
- [Release notes (RELEASES.md)](#release-notes-releasesmd)
- [Markdown style](#markdown-style)

## The docs/ layout

```
docs/
  coding-standard.md   # binding code rules (style, tests, agents) — referenced by AGENTS.md
  RELEASES.md          # per-release notes, newest on top
  albdf.1            # the man page (troff/groff, NOT markdown)
  setup-context7.md    # host-side MCP wiring for the context7 tools
  decisions/           # ADR-0001..0006 (fork, no-gui, rtl, license, gpl, forms/signatures)
  research/            # 001/002/003 briefs + findings, baseline-upstream.md, baseline-out/
  context7/            # vendored library docs: harfbuzz.md, fribidi.md, freetype.md, README.md
  specs/               # reserved for formal specs (currently empty)
```

## Choosing the right kind of doc

| You are... | Write it in... | And also |
|---|---|---|
| Making a design/project decision (fork, no GUI, licensing, new dependency) | `docs/decisions/` **ADR** | Add a row to the DB (`db.py decision-add`) |
| Investigating a library, upstream behavior, or feasibility | `docs/research/` **note** | `db.py research-add` |
| Changing code rules (style, naming, tests, git workflow) | `docs/coding-standard.md` | — (it's the binding contract) |
| Documenting an API/CLI the user actually runs | **man page** `albdf.1` | — |
| Shipping a release | `docs/RELEASES.md` | — |

Rule of thumb: a **decision** is "we chose X and why" (record it as an ADR,
keep it short, link to evidence). A **research note** is "we looked at Y and
found Z" (data, references, unresolved gaps). If you're changing *how all code
is written*, that belongs in `coding-standard.md`, not a one-off note.

## ADRs (decisions/)

- One file per decision, numbered sequentially: `docs/decisions/0007-<slug>.md`.
- Every ADR file gets a matching `decisions` row via
  `db.py decision-add --file docs/decisions/0007-x.md --title "..." --summary "..."`.
- Keep statuses in sync: `proposed` → `accepted` / `superseded` / `rejected`.
- Existing numbering starts at 0001-fork-pdf4qt → 0006-forms-signatures-cli. The next
  free number is **0007**.

## Research notes (research/)

- Format: `docs/research/NNN-<slug>.md` for numbered briefs/findings; descriptive
  names (`baseline-upstream.md`) are fine for artifacts tied to a specific run.
- Record the source (URL / brief file / `rosetta`) and register it:
  `db.py research-add --topic "..." --source <url> --file docs/research/NNN-x.md`.
- If a research step produces rendered output (e.g. a baseline PDF), keep it in
  `docs/research/baseline-out/` and reference it from the note.

## Vendoring context7 library docs

`docs/context7/` stores up-to-date third-party library documentation fetched
via the context7 MCP tools (`mcp__context7__*`), so agents don't guess library
APIs.

- One markdown file per library: `harfbuzz.md`, `fribidi.md`, `freetype.md`.
- `docs/context7/README.md` logs how to query (`resolve-library-id` with BOTH
  `libraryName` + `query`; `query-docs` one concept per call) and the discovered
  library IDs (`/harfbuzz/harfbuzz`, `/fribidi/fribidi`, `/freetype/freetype`).
- Note: PDF4QT itself is **not** on context7 — treat the upstream clone in
  `vendor-upstream-pdf4qt/` (gitignored) as the authority for PDF4QT API.
- When you vendor new docs, update the README's library-ID table and note the
  fetch date. Host-side MCP wiring lives in `docs/setup-context7.md`.

## The man page (albdf.1)

- Written in **troff/groff** format (`.TH`, `.SH`, `.TP`, `.PP`), NOT markdown.
- It's the user-facing reference for the CLI. Update it whenever you add or
  change a user-facing command or option.
- After editing, verify it renders cleanly:

```bash
groff -man -Tutf8 docs/albdf.1 | less     # or pipe to a pager / file
```

  Fix any formatting warnings or broken macro output before committing. The
  header line (`.TH ALBDF 1 "date" "albdf 0.1.0" "User Commands"`) carries
  the date and version — keep them current.

## Release notes (RELEASES.md)

- Newest release on top, one `# <version> — <title>` section each.
- Each entry covers: what it is, highlights (features in user terms), quality
  gates (test/ASAN/format status), known limitations, and the commits since the
  prior base. Follow the existing 0.1.0 entry as the template.
- Update `RELEASES.md` when a milestone lands or at tag time; the commit list
  comes from `git log` over the milestone range.

## Markdown style

- **TOC for long docs:** any guide longer than ~40 lines starts with a
  `## Table of contents` list of `#section-anchor` links (as this file does).
- **Tables** for structured comparisons (statuses, ADRs, dep licenses, library IDs).
- **Exact paths** everywhere — never say "the schema file", say `db/schema.sql`.
- Keep one logical topic per file; link across files instead of duplicating.
- Commands are `fenced` code blocks with the repo-root invocation, e.g.
  `python3 scripts/db.py status`, `groff -man -Tutf8 docs/albdf.1`.
- Follow the same determinism/headless/no-fake-results ground rules as code:
  dates and commit shas in docs must be real.
