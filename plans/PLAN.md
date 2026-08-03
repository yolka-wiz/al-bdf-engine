# pdfedit — Master Project Plan

> **For Hermes/orchestrator:** this is the roadmap. Track granular status in `db/pdfedit.db`
> (`python3 scripts/db.py status`). Implementation is delegated to agent roles defined in
> `agents/roles/`, executing per `AGENTS.md` and `docs/coding-standard.md`.

**Goal:** a headless PDF editing **library + CLI** for Linux — fork of MIT PDF4QT with
object-level deletion, add-text, and correct **RTL (Arabic/Persian/Hebrew) write + search**.

**Architecture:** fork `Pdf4QtLibCore` + `PdfTool` (MIT) → extend the CLI with
`delete-object` / `add-text` → add greenfield RTL pipeline (FriBidi + HarfBuzz + ToUnicode)
→ deterministic golden-tested core. GUI explicitly out of scope for v1.

**Tech stack:** C++20, Qt 6.8+, CMake; deps: FreeType, OpenJPEG, OpenSSL, ZLIB, TBB, blend2d
(inherited), HarfBuzz (MIT), FriBidi (LGPL-2.1) (added).

---

## Milestones (each = a reviewable, testable increment)

### M0 — Infrastructure (this repo) ✅ scaffolding done, seeds live
- Repo `pdfedit/`, `AGENTS.md`, `docs/coding-standard.md`, `.clang-format`.
- Tracking DB `db/pdfedit.db` + `scripts/db.py` (components/tasks/decisions/research/questions/skills + FTS5).
- Agent role definitions (`agents/roles/`).
- **Exit criteria:** `db.py status` shows all components; plan + standard committed.

### M1 — Fork & baseline build (core-agent)
**Goal: we can build the fork headlessly and run its existing CLI.**
- Tasks #1–#2 in DB: vendor PDF4QT fork into `src/`, strip GUI apps, keep `Pdf4QtLibCore` +
  `PdfTool` + `UnitTests`; verify `QT_QPA_PLATFORM=offscreen` build + `fetch-text` smoke test.
- Add upstream remote for cherry-picking (pending user answer Q3).
- Write ADR-0001 (fork decision) final status + ADR-0002 (no GUI).
- **Exit criteria:** `cmake --build build -j` green; `./build/bin/pdfedit fetch-text sample.pdf`
  prints text with no display.

### M2 — Text recognition + object deletion via CLI (core-agent)
**Goal: "recognize text as objects, delete the object" — user's core requirement.**
- Task #3: `delete-object` command — wire `PDFDocumentTextFlowEditor::removeItem` +
  content-stream write-back; CLI takes page + object id / rect.
- Task #5: deletion safety — image XObject refcounting, Form XObject nesting, inline images.
- Expose text recognition output (JSON/XML via existing `pdfoutputformatter`): object list with
  bbox, text, char boxes per page — feeds future GUI + tests.
- **Exit criteria:** CLI can list objects, delete a text run/image, save; golden tests prove
  the deleted text is gone from extraction and render unchanged elsewhere.

### M3 — Add-text (LTR) via CLI (core-agent)
**Goal: insert new text into a page.**
- Task #4: `add-text` command — reuse `PDFTextLayoutGenerator` + content-stream builder;
  font embedding path; position + size + color options.
- **Exit criteria:** `add-text "hello" --page 1 --x .. --y .. --size 12` writes visible,
  extractable text; golden test.

### M4 — RTL write pipeline (rtl-agent) — the differentiator
**Goal: correct Arabic/Persian/Hebrew text insertion.**
- Task #6: add HarfBuzz + FriBidi deps.
- Task #7: bidi (FriBidi) → per-run shaping (HarfBuzz) → visual-order `Tj` emission with
  absolute `Tm` positioning; Type0/Identity-H font embedding; width arrays from advances.
- Task #8: ToUnicode CMap from HarfBuzz clusters (subset-GID pitfall) + `/ActualText`.
- **Exit criteria:** `add-text --rtl "سلام دنیا"` renders connected, extractable via
  `pdftotext`, copy-paste order correct; golden images for Arabic/Persian/Hebrew.

### M5 — RTL search (rtl-agent)
**Goal: find RTL text in arbitrary PDFs — beats every OSS incumbent (Okular bug 353300).**
- Task #9: normalization (tashkeel, presentation forms, lam-alef, Persian↔Arabic, digits, ZWNJ).
- Task #10: bidi inversion of extracted visual-order text; substring match with highlight geometry.
- **Exit criteria:** search finds Persian/Arabic/Hebrew strings incl. ZWNJ/digit/lam-alef edge
  cases in foreign PDFs and in our own add-text output.

### M6 — Test hardening + CI (test-agent, parallel with M2–M5)
- Task #11: golden-image harness (deterministic render diff).
- Task #12: RTL corpus fixtures (ZWNJ, lam-alef, tashkeel, digits, mixed bidi).
- Task #13: CI — offscreen `ctest` + ASAN/UBSAN job.
- **Exit criteria:** CI green; corpus covers the §5 research pitfalls.

### M7 — Polish & release (all agents)
- CLI docs (`--help` complete, man page), `--deterministic-id`-style stable saves.
- Performance smoke: 1000-page doc open/render/delete.
- Flatpak/AppImage packaging only if requested later (GUI phase).

---

## Dependencies between milestones

```
M1 ──► M2 ──► M3 ──► M4 ──► M5
                ▲       ▲
M6 (tests) ─────┴───────┘ (parallel from M2 on)
```

M4 (RTL writer) depends on M3 (add-text LTR machinery). M5 (RTL search) depends on M2's
text-recognition output + M4's ToUnicode/ActualText. M6 runs in parallel from M2.

## Key decisions (tracked as ADRs in `docs/decisions/`)

| ADR | Decision | Status |
|---|---|---|
| 0001 | Fork PDF4QT (MIT) as base | proposed |
| 0002 | No GUI in v1 — library+CLI only | proposed |
| 0003 | RTL write+search is greenfield differentiator | proposed |
| 0004 | License posture: MIT fork + permissive deps only | proposed |

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| PDF4QT single-maintainer upstream drift | Keep upstream remote; cherry-pick; we own the fork (MIT) |
| RTL pipeline bugs (subset GIDs, lam-alef, ZWNJ) | Golden corpus from M6; /ActualText belt-and-braces |
| Fork learning curve for agents | Research stream (Rosetta) + AGENTS.md + ADRs |
| Scope creep (GUI, OCR, edit-existing-text) | Explicitly out; DB questions track proposals |
| License contamination | ADR-0004: no AGPL/GPL deps; vet every added dep |

## Open questions (also in DB)

See `python3 scripts/db.py questions` — name, hosting, upstream-remote policy, context7 keys,
CLI preservation policy, agent-role set. All asked to user; answers gate specific milestones.

## How execution works

1. Orchestrator (Yolka) picks the next milestone/task from the DB, reads the research notes.
2. Dispatches a fresh subagent per task with full context (role definition + task + plan ref).
3. Subagent follows `AGENTS.md`: TDD, small commits, DB evidence on completion.
4. Spec-compliance + code-quality review passes (per `requesting-code-review`).
5. Status + ADRs updated in DB. Proceed to next task.
