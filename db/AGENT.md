# AGENT.md — the tracking database (source of truth for status)

> Everything about the state of `pdfedit` lives in `db/pdfedit.db`. If you
> changed a component's status, closed a task, added a decision, found a risk,
> or imported a dependency, you must record it here. The DB is the project's
> single source of truth for *what exists and how it's going*.

## Table of contents

- [What the DB tracks](#what-the-db-tracks)
- [CLI basics](#cli-basics)
- [Commands with examples](#commands-with-examples)
- [The evidence-gated completion rule](#the-evidence-gated-completion-rule)
- [Searching (FTS5)](#searching-fts5)
- [gitignore discipline](#gitignore-discipline)

## What the DB tracks

| Table | Tracks | Key fields |
|---|---|---|
| `components` | Units of work (fork-base, cli, rtl-writer, ...) | `id` (kebab), `status`, `owner_agent`, `source_dir` |
| `tasks` | Concrete work items tied to a component | `status`, `priority`, `evidence_ref`, `assignee` |
| `decisions` | Index of ADRs in `docs/decisions/` | `adr_file`, `status`, `summary` |
| `research` | Findings from Rosetta / web / briefs | `source`, `file` |
| `questions` | Open questions for the user or another agent | `asked_to`, `status`, `answer` |
| `skills` | Skills pinned to agent roles | `role`, `source`, `vetting` |
| `deps` | Third-party libraries imported/linked | `license`, `vendored`, `tested`, `status` |
| `search_index` | FTS5 virtual table auto-synced by triggers | `kind`, `ref_id`, `title`, `body` |

Every insert/update keeps the FTS5 `search_index` in sync via SQL triggers —
you never touch `search_index` directly.

## CLI basics

```bash
python3 scripts/db.py <subcommand>
```

Run from the repo root. If the DB doesn't exist, first:

```bash
python3 scripts/db.py init        # create db/pdfedit.db from db/schema.sql
python3 db/seed.py                # load known components/tasks/decisions/questions
```

All commands are idempotent-ish; re-running the seed is safe for components
(`INSERT OR IGNORE`) but re-inserts tasks fresh — run it once.

## Commands with examples

```bash
# Status
python3 scripts/db.py status                       # everything, by component
python3 scripts/db.py status --component rtl-writer

# Tasks
python3 scripts/db.py tasks --open                 # open + in_progress, by priority
python3 scripts/db.py tasks --component cli
python3 scripts/db.py task-add --component cli --title "Add delete-object CLI" \
    --priority 1 --estimate "2-4d" --assignee core-agent --notes "wire TextFlowEditor::removeItem"
python3 scripts/db.py task-open 3 --assignee core-agent
python3 scripts/db.py task-block 3 --why "blocked on M2 build"
python3 scripts/db.py task-done 3 --ref 942d55a    # REQUIRES --ref (see below)

# Components
python3 scripts/db.py comp-add --id rtl-search --name "RTL-aware search" --owner rtl-agent
python3 scripts/db.py comp-status rtl-writer done

# Decisions (ADR) — keep docs/decisions/ and DB in sync
python3 scripts/db.py decision-add --file docs/decisions/0005-x.md \
    --title "..." --summary "..." --status proposed

# Research
python3 scripts/db.py research-add --topic "PDF4QT text extraction" \
    --source "docs/research/001-pdf4qt-deepdive.md" --file docs/research/001-pdf4qt-deepdive.md

# Questions
python3 scripts/db.py question-add --question "Repo hosting?" --asked-to user
python3 scripts/db.py question-answer 1 --answer "local for now"
python3 scripts/db.py questions --open

# Skills
python3 scripts/db.py skill-add --name qt-cmake-project --role core-agent --source local
python3 scripts/db.py skill-vet 1 approved

# Deps
python3 scripts/db.py dep-add --name harfbuzz --version 8.3.0 --license MIT \
    --source github --purpose "RTL shaping" --vendored
python3 scripts/db.py dep-status 1 approved --tested 1
python3 scripts/db.py deps

# Backup / portability
python3 scripts/db.py dump
```

**Matching components by display title, not slug:** a few commands take a
component id (e.g. `task-add --component`). Pass the *id* (kebab-case) column,
not the human `name` — `--component rtl-writer` is right; `--component "RTL
write pipeline (Arabic/Persian/Hebrew)"` is wrong.

## The evidence-gated completion rule

A task may only be marked `done` with real evidence:

```bash
python3 scripts/db.py task-done <id> --ref <commit-sha>
```

- `--ref` is **required** by the CLI — there is no way to close a task without
  it. The value is stored in the `evidence_ref` column.
- The ref must be a real `git log` commit sha (and, per AGENTS.md ground rules,
  backed by a passing test). **Never** fabricate a sha to "unblock" a close.
- If you cannot produce a sha because the work isn't landed or tests fail,
  leave the task `in_progress` or `blocked` and say so. No evidence, no close.

This is the mechanism that keeps "done" meaningful across parallel agents.

## Searching (FTS5)

```bash
python3 scripts/db.py search "rtl"          # matches task/component/decision/research/question/skill
python3 scripts/db.py search "harfbuzz OR fribidi"
python3 scripts/db.py search "\"delete-object\""
```

Uses SQLite FTS5 over `search_index` — same query syntax as any FTS5 index
(`AND` default, `OR`, quoted phrases, prefix `dele*`). Use it instead of
grep-ping the docs when you need to know what's already decided, planned, or
researched before starting work.

## gitignore discipline

- `db/pdfedit.db` (and its `-wal`/`-shm` sidecars) is **gitignored** — never
  commit it.
- What IS committed: `db/schema.sql` (the schema) and `db/seed.py` (the seed).
  These two make the DB fully **rebuildable**:
  `db.py init && python3 db/seed.py`.
- If you change the schema, edit `db/schema.sql` (committed) and re-init. If
  you add known components/tasks, prefer extending `db/seed.py` so the state is
  reproducible. Never rely on a hand-edited live `.db` surviving a checkout.
