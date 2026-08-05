# P3 Execution Plan — Vertical Mark Offsets (Orchestrated)

> **For Hermes:** orchestrated development via subagents. Each phase is a
> self-contained delegate_task with its own RED/GREEN gate and commit.
> Status: **START** (2026-08-05). Owner: yolka (orchestrator) + rtl-agent (impl).

**Goal:** Make Arabic/Persian diacritics (tashkeel) and Hebrew niqqud render
**above** the baseline by emitting per-glyph `Ts` (text rise) in the RTL
content-stream emitter, without breaking extraction, search, or golden images.

**Design doc:** `plans/P3-vertical-mark-offsets.md` (root cause verified: lines
356–357 capture `yOffset`, 467–474 discard it; TJ is horizontal-only).

**Tech stack:** C++20, Qt 6.8, CMake + Ninja, vcpkg manifest
(`/home/agent/vcpkg-cache/vcpkg`), HarfBuzz/FriBidi, ctest offscreen.

---

## Baseline facts (verified 2026-08-05)

| Fact | Value |
|---|---|
| Working tree | clean @ `d1f112d` (main, synced with origin) |
| Build dir | `src/build` exists (Release, Ninja) — **not** repo-root `build` |
| Baseline tests | `QT_QPA_PLATFORM=offscreen ctest --test-dir src/build` → **11/11 pass** |
| Emitter | `src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp` (666 lines) |
| Test file | `src/UnitTests/tst_rtladdtexttest.cpp` (runTool helper, offscreen) |
| Fixtures | `src/tests/fixtures/blank.pdf`; fonts `src/tests/fonts/Vazirmatn-Regular.ttf` (has GPOS anchors), `NotoNaskhArabic-Regular.ttf`, `NotoSansHebrew-Regular.ttf` |
| DB task | **#19** `P3: vertical mark offsets via per-glyph Ts` — open, rtl-writer, rtl-agent |
| PROBLEMS.md | P3 row updated → fix planned pointer |

**Environment for subagents:** `source ~/.bashrc`; vcpkg toolchain at
`/home/agent/vcpkg-cache/vcpkg/scripts/buildsystems/vcpkg.cmake`; overlay
`src/vcpkg/overlays`; `QT_QPA_PLATFORM=offscreen` mandatory for all Qt runs.
Configure from `src/` (repo-root has no CMakeLists):
`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
 -DCMAKE_TOOLCHAIN_FILE=... -DVCPKG_OVERLAY_PORTS=... -DALBDF_BUILD_TESTS=ON`

---

## Execution phases

### Phase 1 — RED: pixel-probe test (rtl-agent)

**Objective:** A failing test that pins the exact symptom: marks at baseline.

**Files:**
- Modify: `src/UnitTests/tst_rtladdtexttest.cpp` — add
  `void test_verticalMarkOffsets()` (declaration + definition + `QTest::newRow`).

**Steps:**

1. Read `src/UnitTests/AGENT.md` and the existing test file — copy the
   `runTool` pattern from `test_rtlRenderNoErrors` / `test_persianExtraction`
   (add-text → render → inspect PNG).
2. Write the test:
   - `add-text --page 1 --x 72 --y 700 --size 24 --rtl --lang fa --font
     TEST_FONT_PERSIAN` (Vazirmatn) with text `مَا` (fatha) and separately
     `بِسْم` (kasra), onto `TEST_BLANK_PDF` (blank.pdf), output to tmp dir.
   - `render` page 1 → PNG (`--image-output-dir`, see
     `tst_rtladdtexttest.cpp` for the real flag used today).
   - Load PNG with `QImage`, find the base letter ink band (x-range of any
     non-background pixel row that contains base-letter ink), then assert:
     (a) **ink exists** in the band above the base letter's ascender
     (mark present above baseline), and
     (b) the mark's pixels are **not** painted overlapping the base letter's
     core (band between baseline and x-height shows the mark, not the base
     overpaint).
   - Keep assertions simple and deterministic: compare pixel columns/rows
     between the base glyph and a control glyph without a mark, or probe
     fixed bands. The point is: **this test FAILS today** (marks at baseline,
     so the above-baseline band is empty).
3. Run: `cmake --build build -j$(nproc)` then
   `QT_QPA_PLATFORM=offscreen ctest --test-dir build -R RtlAddText
   --output-on-failure`.
   **Expected:** test FAILS for the right reason (band empty / mark
   overlapping base), other RtlAddText tests still pass.
4. Commit: `test(rtl): RED — vertical mark offset pixel-probe fails at baseline (P3)`.
5. Report: test name, exact failure output, PNG probe numbers, commit sha.

**Gate:** RED observed + committed with failing assertion tied to the symptom.

---

### Phase 2 — GREEN: emit `Ts` for non-zero `yOffset` (rtl-agent)

**Objective:** Make the RED test pass with minimal emitter change.

**Files:**
- Modify: `src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp` (loop at 455–478).
- Modify (as needed): `src/UnitTests/tst_rtladdtexttest.cpp` — only if the
  probe bands were miscalibrated, and then only after re-reading the failure.

**Steps:**

1. In the glyph loop, for each glyph with non-zero `yOffset`:
   - `flushHex()`; close/open TJ array as needed (the plan: emit the mark
     glyph as its own single-glyph string).
   - `rise = yOffset * fontSize / upem` (font units → PDF points; verify
     empirically — see Risks).
   - Emit `"<rise> Ts"`, then the mark hex string `Tj`, then `"0 Ts"`.
   - Continue the run (re-open TJ array with remaining glyphs).
2. **Unit-convention probe:** the exact `Ts` semantics (pre/post-`Tfs`
   scaling) MUST be verified by the pixel test, not assumed. If the first
   attempt puts the mark too far/not far enough, adjust the conversion
   factor and re-run. Document the final formula in a code comment.
3. Build + run the RED test → **GREEN**. Then full:
   `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure`
   → all 11 tests pass.
4. Commit: `feat(rtl): emit Ts for GPOS vertical mark offsets (P3 green)`.
5. Report: exact emitted content-stream snippet for `مَا`, the final
   conversion formula, ctest summary, commit sha.

**Gate:** RED test green + full ctest green + comment documents formula.

---

### Phase 3 — No-regression: extraction & search (rtl-agent)

**Objective:** Prove `Ts` did not break flow/extraction/search (the Tm-split
phantom-space trap; `Ts` should avoid it by design).

**Steps:**

1. On the mark PDF from Phase 2:
   - `fetch-text` → logical text intact (`مَا` / `بِسْم`), no phantom spaces.
   - `search-text "ما"` and `"بسم"` (normalization strips marks) → 1 match each.
2. Golden images: run `src/tests/golden/run.sh` (or the UnitTestsGolden
   ctest) — non-mark fixtures byte-identical to baseline.
3. If `fetch-text`/`search` regress: STOP, do not hack around — report the
   exact output. (Design doc says `Ts` must not move the pen; if PDF4QT's
   flow sees a gap, we need a different emission shape — this is the risky
   branch.)
4. Commit only if a guard is genuinely needed (design doc says none should
   be): `fix(rtl): <what> (P3 extraction guard)`.
5. Report: fetch-text output, search match counts, golden results, commits.

**Gate:** extraction + search + golden all unchanged vs baseline.

---

### Phase 4 — Docs, DB, gate, push (orchestrator, yolka)

**Objective:** Close the loop: PROBLEMS.md P3 → resolved, DB #19 closed with
evidence, CI gate, push.

**Steps:**

1. `docs/PROBLEMS.md`: P3 row → **Fixed** with commit sha; move note under
   the "Design decision" if still relevant.
2. `docs/RELEASES.md` + `README.md`: limitation note → feature note
   (per design doc Files table).
3. DB: `python3 scripts/db.py task-done 19 --ref <green-sha>`; update
   `db/seed.py` TASKS entry status→done + evidence_ref.
4. Gate: `ci/run-ci.sh` all green (Release + ASAN/UBSAN + clang-format).
5. Commit: `docs: P3 resolved — vertical mark offsets (Ts)`; push to origin.

**Gate:** DB shows #19 done with sha; CI green; pushed.

---

## Risks & debugging protocol

| Risk | Handling |
|---|---|
| `Ts` unit convention wrong (pre/post `Tfs`) | **Ranked hypotheses, test one at a time:** H1 `rise = yOffset*fontSize/upem`; H2 `rise = yOffset/upem` (Ts unscaled); H3 needs `* 1000/upem`. Pixel probe disambiguates. Never stack multiple fixes. |
| Phantom space from `Ts` in PDF4QT flow | Phase 3 gate; if it appears, this is an architectural flag → STOP, report, do not cargo-cult a Tm split (design doc warns it breaks extraction). |
| Font lacks GPOS anchors → `yOffset==0` | No `Ts` emitted, behavior unchanged; Vazirmatn has anchors (verified in design doc). |
| Multi-mark stacking (fatha+shadda) | Per-glyph `Ts` toggles; add a stacked case in Phase 1 if the probe needs it. |
| S#1 search interplay | Mark glyph has zero advance → same flow item; Phase 3 explicitly tests search on mark text. |

**Debugging rules (systematic-debugging):**
- Phase 1 first, always. No GREEN without a witnessed RED.
- One variable per change; re-run the tight loop (`-R RtlAddText`) after every
  edit.
- Temporary logs tagged `[DEBUG-p3]`; removed before commit.
- If 3+ fix attempts fail → stop, question the emission architecture, report
  to orchestrator.

---

## Subagent dispatch plan (orchestrator)

1. **Phase 1** → `delegate_task(goal="Implement P3 RED test", context=<repo
   facts + this plan>)`. Leaf, toolsets terminal+file.
2. After Phase 1 returns (sha verified), **Phase 2** → fresh delegate.
3. After Phase 2 returns, **Phase 3** → fresh delegate.
4. Orchestrator does Phase 4 (docs+DB+gate+push) directly — it needs no
   subagent and requires repo-level judgment.

Each delegate gets: repo path, AGENTS.md is binding, build commands,
QT_QPA_PLATFORM=offscreen, TDD rules, commit conventions (Conventional
Commits, one logical change per commit), and the exact Phase gate. Child
summaries are self-reports — verify shas/tests/pushes ourselves.
