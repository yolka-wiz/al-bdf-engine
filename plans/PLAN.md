# albdf — Master Project Plan (v2, finalized 2026-08-04)

> **For Hermes/orchestrator:** this is the roadmap. Granular status lives in `db/albdf.db`
> (`python3 scripts/db.py status`). Implementation is delegated to agent roles in
> `agents/roles/`, executing per `AGENTS.md` + `docs/coding-standard.md`.
>
> **Execution constraint (user directive):** max **3 parallel subagents** at any time.
> The orchestrator dispatches ≤3 `delegate_task` workers; anything beyond queues.

**Goal:** a headless PDF editing **library + CLI** for Linux — fork of MIT PDF4QT with
object-level deletion, add-text, and correct **RTL (Arabic/Persian/Hebrew) write + search**.

**Architecture:** fork `Pdf4QtLibCore` + `albdf` (MIT) → extend the CLI with
`delete-object` / `add-text` → add greenfield RTL pipeline (FriBidi + HarfBuzz + ToUnicode)
→ deterministic golden-tested core. GUI explicitly out of scope for v1 (ADR-0002).

**Tech stack:** C++20, Qt 6.8+ (6.10.2 installed), CMake; deps registered in the DB
(`scripts/db.py deps`): FreeType, OpenJPEG, LCMS2, OpenSSL, ZLIB, libjpeg-turbo, libpng,
TBB, blend2d (inherited, via vcpkg), HarfBuzz + FriBidi (to add, M4).

---

## Operating rules (non-negotiable)

1. **Test before you build.** Every milestone starts by TESTING the existing software we
   import (baseline), then ships with tests that prove the new behavior. No untested step.
2. **≤3 parallel subagents.** Dispatch cap. Queue the rest. Never spawn 4+ workers.
3. **Cherry-pick, don't rewrite.** Import upstream PDF4QT code via the vendored tree +
   upstream remote; test what we import; register every imported lib in the DB (`dep-add`).
4. **Steps have checkmarks.** Each milestone has explicit exit criteria (below). A milestone
   is DONE only when its checkboxes are all ticked with evidence (test output + commit sha).
5. **Every step is tested** — build, unit, golden, or CLI smoke. No "it compiles, ship it".
6. **Register imports.** Every third-party lib lands in the `deps` register with license,
   purpose, tested flag. Rejected = not allowed in the core.

---

## Milestones (each = reviewable, testable increment; checkboxes = exit criteria)

### M0 — Infrastructure (DONE 2026-08-04)
- [x] Repo `albdf/`: AGENTS.md, coding-standard, .clang-format, plan, ADRs 0001–0004
- [x] Tracking DB + `scripts/db.py` (components/tasks/decisions/research/questions/skills/deps + FTS5)
- [x] Agent roles (core/cli/rtl/test/research/orchestrator) + vendored Qt skills
- [x] Research brief 001 delivered to Rosetta; 3 research subagents dispatched (PDF4QT deep dive,
      RTL reference impls, skills+AGENTS.md conventions)
- [x] Build env: cmake 4.2 + cmake 3.28/3.31 (blend2d workaround), Qt 6.10.2, apt mirror fixed
- [x] Imported-libs register seeded (13 deps; blend2d build failure documented → vcpkg path)

### M0.5 — BASELINE: test the software we want to fork (core-agent) ← user directive
**Goal: prove upstream PDF4QT builds and works BEFORE we change a single line of it.**
- [x] Build pristine PDF4QT (core lib + albdf CLI only, GUI stripped) on this container
- [x] Run upstream `UnitTests/` — record pass/fail baseline
- [x] CLI smoke: `fetch-text`, `render`, `info`, `unite` on a generated test PDF — record outputs
- [x] Golden-baseline: render a fixed corpus → PNGs, store as reference for regression
- [x] Register result in DB: task #2 (`--ref <sha>`), deps marked `tested=1`
- **Exit:** baseline doc `docs/research/baseline-upstream.md` with commands + outputs;
  `ctest` green on pristine tree; any upstream failures listed (so we know they're NOT ours)

### M1 — Fork & vendor into `src/` (core-agent)
- [x] Copy vendored tree into `src/`, strip GUI apps (Viewer/Editor/PageMaster/Diff/LaunchPad/plugins)
- [x] Add upstream git remote (user Q3: cherry-pick policy) — decided: keep for cherry-picks
- [x] Verify headless build: `QT_QPA_PLATFORM=offscreen` + `fetch-text` smoke
- [x] Confirm baseline outputs still match M0.5 (no behavior change from stripping)
- **Exit:** fork builds; baseline diff = empty; commit with evidence

### M2 — Text recognition + object deletion via CLI (core-agent, cli-agent)
- [x] Text recognition output: page objects → JSON/XML with bbox, text, char boxes (reuse pdfoutputformatter)
- [x] `delete-object` command: wire `PDFDocumentTextFlowEditor::removeItem` + content-stream write-back
- [x] Deletion safety: image XObject refcount, Form XObject nesting, inline images (task #5)
- [x] Golden tests: deleted text gone from extraction; rest of page unchanged
- **Exit:** CLI lists objects, deletes text run/image, saves; tests green

### M3 — Add-text (LTR) via CLI (core-agent, cli-agent)
- [x] `add-text` command: reuse `PDFTextLayoutGenerator` + content-stream builder; font embed path — `327061b`
- [x] Golden test: inserted text visible + extractable
- **Exit:** `add-text "hello" --page 1 --x .. --y ..` works; test green

### M4 — RTL write pipeline (rtl-agent) — the differentiator
- [x] HarfBuzz + FriBidi deps in build (registered in DB) — `fe016c4`
- [x] Bidi runs → HarfBuzz shaping → visual-order `Tj` emission (absolute `Tm` positioning) — `fe016c4`
- [x] Type0/Identity-H font embedding + `/W` advances — `fe016c4`
- [x] ToUnicode CMap from HarfBuzz clusters (subset-GID pitfall) + `/ActualText` — `fe016c4`
- [x] Golden images + pdftotext extraction checks for Arabic/Persian/Hebrew — `fe016c4`
- **Exit:** `add-text --rtl "سلام دنیا"` renders connected, extractable in correct order — DONE (7/7 RTL tests; Hebrew exact round-trip; ligature degradation documented)

### M5 — RTL search (rtl-agent)
- [x] Normalization: tashkeel, presentation forms, lam-alef, Persian↔Arabic, digits, ZWNJ — `aa92bee`
- [x] Bidi inversion of extracted visual-order text; substring match + highlight geometry — `aa92bee`
- [x] RTL corpus tests (ZWNJ/lam-alef/digits/mixed-bidi edge cases) — `aa92bee`
- [x] **Exit:** search finds RTL strings in our own add-text output (foreign-PDF pass = fetch-text normalization, same engine) — `aa92bee`

### M6 — Test hardening + CI (test-agent, parallel from M2)
- [x] Golden-image harness (deterministic render diff) — earlier
- [x] RTL corpus fixtures committed — earlier (blank.pdf + fonts + README)
- [x] CI: offscreen `ctest` + ASAN/UBSAN + clang-format gate — `b6f4bf6`, `dc45fcb`
- [x] **Exit:** CI green on bare container — `ci/run-ci.sh` all-green incl. ASAN 10/10

### M7 — Polish & release (all agents)
- [x] CLI docs (`--help` complete, man page), deterministic saves — `479ebe0` (README + `docs/albdf.1`)
- [x] Perf smoke: 1000-page doc open/render/delete — info 0.03s, fetch 0.04s, render 0.03s, search 0.04s (1000 matches), delete 0.02s
- [x] **Exit:** release candidate; version tag — **0.1.0** (`aef2575`)

### M8 — Forms & signatures (feature/forms-signatures, merged `382dc4b`)
- [x] `form-list` — enumerate AcroForm fields (name, type, value, page, rect, readonly) — `3b3e563`
- [x] `form-fill` — set field values (text/button/choice), regenerate appearance, write new doc — `b8efe4a`
- [x] `sign` — PKCS#7 detached digital signature (invisible or visible widget), PAdES byte-range flow — `02b8719`
- [x] `verify-signatures` (upstream tool) validates signed docs; one-byte tamper → `Signature: Error` — tested
- [x] `UnitTestsFormSignature` — form-list/form-fill/sign/verify/tamper round-trip — `6a2a325`; suite **11/11**
- [x] ADR-0006, README section, DB synced — `b2276d7`
- [x] **Exit:** forms+signatures scriptable headlessly; CI all green on merged main

### M8.1 — Real-world compatibility sweep (compat agents, merged `382dc4b`)
- [x] `fix(add-text)`: content-stream floats in FixedNotation (precision 8) — strict parsers reject scientific notation — `591dfee`
- [x] S#1 field observation documented (dense-page RTL phrase split is flow geometry, not add-text defect)
- [x] CI format gate exempts upstream-derived content-stream builder
- [x] **Exit:** 11-file corpus (testing-temp) exercised; agent-a interrupted before committing (work lost), agent-b's fix merged

---

## Dependency graph

```
M0 ──► M0.5 ──► M1 ──► M2 ──► M3 ──► M4 ──► M5
                            ▲        ▲
M6 (tests) ─────────────────┴────────┘ (parallel from M2)
```

M0.5 is the new gate: we cannot touch upstream code until the baseline is recorded.

## Execution cadence (per milestone)

1. Orchestrator: mark milestone in_progress in DB; assign to role agent(s); ≤3 parallel.
2. Each task: subagent reads AGENTS.md + role file + task; writes failing test first; implements;
   runs test; commits with evidence; updates DB (`task-done --ref <sha>`).
3. Orchestrator: spec-compliance review → code-quality review (qt-cpp-review skill + lint).
4. Milestone exit criteria checked; ADRs updated; next milestone starts.

## Key decisions (ADRs in `docs/decisions/`)

| ADR | Decision | Status |
|---|---|---|
| 0001 | Fork PDF4QT (MIT) as base | accepted |
| 0002 | No GUI in v1 — library+CLI only | accepted |
| 0003 | RTL write+search is greenfield differentiator | accepted |
| 0004 | License posture: MIT fork + permissive deps only | superseded (ADR-0005) |
| 0005 | Relicense to GPL-3.0-or-later | accepted |
| 0006 | Forms and digital signatures via CLI | accepted |

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| blend2d build on this container (CMake recursion bugs) | vcpkg (in progress); documented in deps register #11 |
| PDF4QT upstream drift | upstream remote + cherry-picks; baseline (M0.5) anchors behavior |
| RTL pipeline bugs (subset GIDs, lam-alef, ZWNJ) | golden corpus (M6); /ActualText belt-and-braces |
| ≤3 subagent cap slows parallel work | queue tasks; batch by dependency; tests written early |
| Scope creep (GUI, OCR, edit-existing-text) | explicitly out; DB questions track proposals |
| License contamination | ADR-0004 + deps register: every import vetted before approval |

## Open questions (DB `questions`)

All answered as of 2026-08-05: name **albdf**, hosting GitHub
(`yolka-wiz/al-bdf-engine`), upstream remote kept for cherry-picks,
context7 key received and wired via Hermes MCP, CLI commands preserved
(~30 kept + new added), agent roles confirmed, skills installed, Rosetta
delivered research briefs.
