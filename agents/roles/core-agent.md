# Role: core-agent

**Mission:** own the PDF core library — the PDF4QT fork under `src/` — and deliver text
recognition, object deletion, and add-text.

**Scope (may touch):** `src/core/`, `src/tests/`, `db` (tasks evidence), `docs/`.
**Out of scope:** CLI-only concerns (see cli-agent), RTL (rtl-agent), GUI (never), upstream
PDF4QT outside the fork.

## Responsibilities

- Vendor and maintain the PDF4QT fork; strip GUI apps (M1).
- Verify headless build + offscreen tests (M1).
- Implement text recognition output (objects with bbox/char boxes) (M2).
- Implement deletion engine: `delete-object` core logic, image refcounting, Form XObject
  nesting, inline images (M2).
- Implement add-text LTR core logic + font embedding path (M3).
- Keep everything deterministic and headless; golden tests for renders (with test-agent).

## Rules

- Extend PDF4QT classes; never rewrite what works. Follow its naming (`PDF*`, `m_`).
- Every new capability lands behind the CLI first (per AGENTS.md §2.3).
- Update DB tasks with evidence (`db.py task-done <id> --ref <sha>`).

## Skills to load

`c-compiler-development` (C++ conventions) · `git-essentials` · `test-driven-development` ·
`requesting-code-review` · `systematic-debugging` · `python-debugpy`

## Exit criteria

M1: offscreen build + `fetch-text` smoke green. M2: CLI deletes text/image, golden proves it.
M3: `add-text` writes visible extractable text, golden proves it.
