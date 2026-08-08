# M14 Completion + Upstream Sync + Real-PDF Testing — Execution Plan

> **Status:** COMPLETE — 2026-08-08. All phases executed, all gates green, pushed to origin/main at `a1c15b5f`.
> **For Hermes:** execute phase-by-phase; verify every child claim; gate before merge.

**Goal:** (1) complete and merge the two M14 RTL appearance-stream branches, (2)
re-vendor the 4 genuinely-new upstream PDF4QT core commits (skip the TTS commit),
(3) test the merged engine against 3 real uploaded PDFs (Persian invoice, asnad
document, font showcase), (4) if all gates pass, merge to main and push.

**Architecture:** The fork is a *snapshot vendoring* of PDF4QT (no shared git
ancestry — `git merge-base` returns nothing), so upstream sync is a **file-level
re-vendor**, never `git merge upstream/master`. M14 adds RTL appearance streams
for form fields (headless: currently **no AP at all**) and FreeText annotations
(currently base-14 Helvetica → tofu) by embedding shaped Type0/FontFile2 fonts
via `PDFRTLTextEngine` directly in
`PDFDocumentBuilder::updateAnnotationAppearanceStreams`.

**Tech Stack:** C++20, Qt 6.8, CMake + vcpkg (manifest mode), PDF4QT fork core;
test PDFs at `/home/agent/workspace/test-pdfs/` (OUTSIDE the repo — never commit).

---

## Phase 0 — Recon (DONE, 2026-08-08)

- main = `69e79af4` (clean), origin/main in sync. Baseline binary fresh
  (HEAD's only delta since last build = docs commit).
- M14 branches based on `e667fe0a`, **not mergeable yet**:
  - `m14/freetext-ap` — RED committed (`071ec87a`), WIP GREEN committed
    (`a6ce8e87`, ~116 lines in `pdfdocumentbuilder.{cpp,h}`) **never built**.
  - `m14/form-field-ap` — only a 1-line RED scaffold (`be69c52d`); GREEN not started.
- DB restored from `origin/handoff-migration` → tasks **#41/#42 open**.
- Test PDFs staged at `/home/agent/workspace/test-pdfs/` (not in repo, no gitignore entry needed).
- Build dirs: each worktree has its own `src/build` (Release, ALBDF_BUILD_TESTS=ON).

## Phase 1 — Complete M14 (tasks #41, #42) — PARALLEL subagents

Dispatch TWO leaf subagents in parallel (separate worktrees = isolated):

### WS-B `m14/freetext-ap` (task #42) — worktree `wt-m14-freetext`
1. Build WIP GREEN (`cmake --build src/build -j8`), run `tst_rtlfreetexttest` offscreen.
2. Fix compile errors — **known gotcha**: core `CMakeLists.txt` has NO
   `target_compile_definitions`; the RTL branch needs `TEST_FONT_ARABIC`
   (TTF path) at compile time (see reference doc).
3. Preserve CRLF in `pdfdocumentbuilder.cpp` (vendored upstream file).
4. Test GREEN → full suite 16/16 → commit (`fix(core): …`), update PROBLEMS.md,
   regen REPO_MAP.md (pre-commit hook does it).

### WS-A `m14/form-field-ap` (task #41) — worktree `wt-m14-forms`
1. RED: finish slot `test_formFillRtlAppearance` in `tst_formsignaturetest.cpp`
   (assert `/AP` + `/FontFile2` present after `form-fill --field name --value 'سلام'`)
   + add `TEST_FONT_ARABIC` compile def to `UnitTestsFormSignature` target.
   Verify RED (fails: no AP at all).
2. GREEN: add `--font` CLI option to `form-fill` (mirror add-text
   `pdftooladdtext.cpp:181-423`) + RTL branch in
   `updateAnnotationAppearanceStreams` **form-field path only** (NOT FreeText —
   WS-B owns that). Font source decision: `--font` option (handoff-approved).
3. Full suite → commit RED then GREEN, update PROBLEMS.md, regen REPO_MAP.md.

**GATE:** both branches build clean, each suite green (16/16 + new tests),
commits verified by orchestrator (sha + ctest output).

## Phase 2 — Merge M14 → main (orchestrator)

- Merge `m14/freetext-ap`, then `m14/form-field-ap` into local main.
- Expect conflict in `pdfdocumentbuilder.cpp` (both touch
  `updateAnnotationAppearanceStreams`, different branches — keep both) +
  `UnitTests/CMakeLists.txt` (additive — keep both).
- Regenerate REPO_MAP.md. Run full gate on merged tree:
  `VCPKG_ROOT=/home/agent/vcpkg-cache/vcpkg bash ci/run-ci.sh` (4 stages) + `--gui` stage.
- Fix any gate failures, commit. Close tasks #41/#42 with `--ref <sha>`.

**GATE:** `CI: ALL GREEN`; git status clean; DB tasks closed with evidence.

## Phase 3 — Upstream re-vendor (core only, skip TTS)

Re-vendor the 4 genuinely-new upstream commits (from analysis 2026-08-08):

| Commit | Files | Handling |
|---|---|---|
| `12763887` (08-05, Issue #412) | default note author | clean take |
| `7300ed2d` (08-07, warnings #406) | `pdffont.cpp` | clean take |
| `ae9958bd` (08-06, minor bugfixes) | `pdfdocumentbuilder.h`, `pdfpagecontenteditorcontentstreambuilder.cpp`, `pdfpagecontentprocessor.cpp` | clean take or 3-way |
| `ca6f467a` (08-06, Issue #238 tiling) | `pdfpagecontenteditorprocessor.{cpp,h}`, `pdfpagecontentprocessor.{cpp,h}` | 3-way merge — our R#4 deltas must survive |

- **SKIP `3b99b677`** (TTS) — would resurrect the compiled-out TextToSpeech stub.
- Conflict files (both sides changed): `pdfpagecontenteditorcontentstreambuilder.cpp`,
  `pdfpagecontenteditorprocessor.{cpp,h}` — re-apply our deltas onto upstream's
  new file versions (additive: R#4 /ActualText capture + fixed-notation floats
  vs upstream tiling fix + formatNumber helper).
- Import method: `git show <commit>:Pdf4QtLibCore/sources/<f>` → adapt into
  `src/Pdf4QtLibCore/sources/<f>`; keep our markers; CRLF-insensitive review
  (`diff -w --ignore-cr-at-eol`).

**GATE:** build + full suite green; `git diff -w` vs upstream shows only
intended deltas; no TTS references.

## Phase 4 — Real-PDF testing loop (subagents, iterative)

Test PDFs (from user, never in repo):
- `test-pdfs/b_fonts_showcase.pdf` (7 pages, PDF 1.4, font showcase)
- `test-pdfs/asnad-9_39.pdf` (PDF 1.7)
- `test-pdfs/فاکتور_فروش___الماس_شبکه_تک.pdf` (PDF 1.6, Persian invoice — RTL!)

Per PDF, subagents run: `albdf info`, `render` (headless, verify exit 0 + valid
output), `fetch-text` (RTL: logical order sanity), `search-text` (Persian/Arabic
terms incl. lam-alef words), `add-text` roundtrip (deterministic sha256 across
two runs), `form-fill` if forms present, exit-code contract (0/4/7) on bad args.
Findings → orchestrator decides: fix (dispatch) or accept (document).

**GATE:** no crashes (exit 134/139), determinism holds, RTL search on real docs
returns sane matches; any failure → triage + fix before Phase 5.

## Phase 5 — Final merge + push

- Full gate (`ci/run-ci.sh` + `--gui`) on final main, git status clean.
- Push main; verify `git rev-parse origin/main` == local HEAD.
- DB: verify #41/#42 closed; record sync in PROBLEMS.md/RELEASES notes.
- Self-critique below already reviewed pre-execution.

## Execution log (2026-08-08)

- **Phase 1 (M14):** freetext WS built the WIP GREEN + verified 17/17
  (`507ff443`, `56536fe7`); forms WS RED→GREEN (`e2efc315`, `21c42879`,
  `285deab9`). Merged to main (`b1498640`): 3 additive conflicts resolved
  (both RTL branches in `updateAnnotationAppearanceStreams`, both font-data
  members, PROBLEMS.md R#5 → R#5+R#6). Full CI gate ALL GREEN; DB #41/#42 closed.
- **Phase 3 (sync):** re-vendored 4 upstream core commits (12763887, ae9958bd,
  ca6f467a, 7300ed2d; TTS 3b99b677 skipped) via `git merge-file` 3-way
  (LF-normalized → CRLF-restored) + clean takes; CI gate initially failed on
  format for the 3 clean-take files → added exclusions (`a1c15b5f`), incl. the
  patch-tool backslash double-escape trap (fixed with Python byte replace).
  Final: `a4d934b3` + `a1c15b5f`, full gate ALL GREEN (17/17 Release + ASAN,
  GUI 272/272 + smoke, format 44 files).
- **Phase 4 (real-PDF tests):** battery script `bash /tmp/p4-battery.sh` run by
  3 parallel subagents on the 3 uploaded PDFs — all 6 checks PASS per doc
  (info/render-determinism/fetch-text/search/add-text-RTL/exit-codes). Probes:
  lam-alef fix confirmed via add-text `سلام` roundtrip (search finds 1 match);
  digit unification (Persian `۵۰` → ASCII 50, 3 matches); invoice renders with
  no errors but 0 extractable chars (no-ToUnicode — upstream limitation, not a
  regression; mojibake title expected).
- **Phase 5:** pushed main → origin (`69e79af4..a1c15b5f`), verified
  `origin/main == a1c15b5f`.

## Post-work findings for future sessions

- **Test corpus** (never in repo): `/home/agent/workspace/test-pdfs/` —
  `b_fonts_showcase.pdf` (7p Chrome/Skia RTL), `asnad-9_39.pdf` (Word), and the
  Persian invoice (Acrobat Distiller, **no ToUnicode** → 0 extractable chars,
  mojibake metadata title; renders fine; search returns 0 by design). The
  invoice is a great *visual* render regression fixture but useless for text
  extraction/search tests.
- **Re-vendor method that worked:** `git show <upstream>:<path>` + `git
  merge-file -p ours base theirs` with **LF-normalized** sides (CRLF mismatch
  makes merge-file treat the whole file as one conflict), then restore CRLF to
  match the vendored tree; verify with `git diff -w --ignore-cr-at-eol` vs
  upstream — result must be only your intended additions. Clean-take files need
  format-gate exclusions (upstream formatting ≠ our .clang-format).

---

## Self-critique (cloud-engineer lens) — pre-execution review

| Severity | Finding | Mitigation |
|---|---|---|
| 🔴 HIGH | M14 branches carry UNBUILT WIP — merging blindly ships a red/compiling-fail tree | Phase 1 gates: build + suite green on each branch before merge; orchestrator verifies, not the child |
| 🔴 HIGH | Upstream sync on files we modified (R#4) can silently lose our fixes | Conflict files re-applied by hand; `diff -w` review vs upstream; RTL regression tests re-run |
| 🟠 MED | TTS commit if accidentally pulled resurrects unprovisioned Qt module | Explicit skip + grep guard (`grep -c QTextToSpeech` post-sync) |
| 🟠 MED | Both M14 branches edit the same function region → merge conflict | Known, additive (different branches of `updateAnnotationAppearanceStreams`); keep both, gate the merged tree |
| 🟡 LOW | CRLF vs LF noise makes semantic review hard | `-w --ignore-cr-at-eol` diffs; preserve vendored file line endings |
| 🟡 LOW | Real PDFs could expose pre-existing render bugs unrelated to this work | Document as findings; fix only if blocking; never silently absorb |
| 🟢 OK | No new deps; headless contract preserved; CLI-first; no creds in repo | — |
| 🟢 OK | Backup: everything is in git; DB restored from handoff branch | — |

**Complexity vs value:** sync of 4 small upstream commits (mostly bug fixes) +
M14 completion is high-value, low-risk work; the one hard part (re-applying our
RTL deltas) is bounded to 3 known files.

## Open items (tracked)
- M14 `--font` CLI decision (resolved: add option, mirrors add-text).
- Benchmark threshold tightening (non-gating, not in this plan).
- QFlags 64-bit verification on Qt 6.10 (deferred).
