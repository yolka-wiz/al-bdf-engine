# P3 — Vertical mark offsets (diacritics above the baseline)

Status: **PLANNED** (2026-08-05) · Owner: rtl-agent · Effort: **M**
(per docs/PROBLEMS.md: "needs GPOS-to-Tm or anchor machinery to fix")

## Symptom

Arabic/Persian diacritics (fatha, kasra, damma, sukūn, tashkeel) and Hebrew
niqqud render **at the baseline**, painted over the base letter, instead of
above it. Example: `بِسْم` — the kasra appears inside the `ب` instead of
below the baseline; `مَا` — the fatha overlaps the `ا`.

## Root cause (verified in code, 2026-08-05)

`src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp`:

- Line 356–357: HarfBuzz GPOS anchor offsets **are captured** per glyph:
  ```cpp
  glyph.xOffset = PDFReal(glyphPos[g].x_offset);
  glyph.yOffset = PDFReal(glyphPos[g].y_offset);
  ```
- Line 467–474: the emitter folds `xOffset` into TJ spacing but **discards
  `yOffset`**:
  ```cpp
  // Vertical mark offsets are not expressible as TJ spacing in v1;
  // zero-width marks stay at the baseline (documented limitation).
  ```

Why TJ can't do it: a TJ array's numeric items are **horizontal**
displacements only. There is no per-glyph vertical offset in a TJ array.

## Fix approach — text rise (`Ts`)

PDF has a native per-state vertical baseline offset: **text rise (`Ts`)**,
"the distance, in unscaled text space units, to move the baseline up or down
from its default location" (PDF 32000-1 §9.4.2). It is added to the `ty`
component of the text line matrix and affects only subsequently painted
glyphs until reset to 0.

### Emission strategy (per shaped run, visual order)

For each glyph with non-zero `yOffset`:

1. Flush the current hex run (close the TJ array if open).
2. `rise = yOffset * fontSize / upem` (font units → PDF points).
3. Emit `rise Ts`, then the mark glyph as a single-glyph string, then `0 Ts`.
   ```pdf
   <0017> Tj   % base glyphs...
   6.5 Ts
   <0034> Tj   % mark glyph, painted rise pt above the baseline
   0 Ts
   <0018> Tj   % continue
   ```
4. Continue the run; re-open the TJ array for the remaining glyphs.

Key properties:

- **No Tm boundary** → PDF4QT's flow/extraction does NOT insert a phantom
  space (the code comment at line 438–440 warns Tm splits break extraction;
  `Ts` avoids that entirely).
- **Zero-advance marks keep their inline position** → the mark's x stays at
  the base glyph; only the baseline rises. Text extraction and the
  `characterBoundingRects` mapping are unaffected (the glyph's advance is
  still 0, so the flow sees the same x positions).
- The `/ActualText` wrapper is untouched (logical text still correct).

### Unit conversion to verify during implementation

`glyphPos[g].y_offset` is in **font units** (same space as advances / upem).
Rise in PDF points = `y_offset * fontSize / upem`. **Verify empirically with
a pixel probe** (render `مَا` at 24 pt, assert ink exists in the row band
above the base letter's ascender and none overlapping the base) — the exact
`Ts` unit convention (whether unscaled text space is pre- or post-`Tfs`
multiplication) must be confirmed by the golden-image test, not assumed.

## Files

| File | Change |
|---|---|
| `src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp` | emit `Ts` for non-zero `yOffset` glyphs (lines ~455–478) |
| `src/UnitTests/tst_rtladdtexttest.cpp` | RED test: render mark text, pixel-probe mark band above baseline |
| `docs/PROBLEMS.md` | P3 → resolved |
| `docs/RELEASES.md`, `README.md` | limitation note → feature note |
| `db/seed.py` | close task with evidence sha |

## TDD plan (5 steps, RED → GREEN per commit)

1. **RED** — `tst_rtladdtexttest.cpp::test_verticalMarkOffsets`: add-text
   `مَا` (fatha) + `بِسْم` (kasra) at 24 pt onto `blank.pdf`, render page 1
   to PNG, probe pixels: (a) ink exists in the band above the base letter's
   x-range, (b) no ink where the mark would be at baseline. Expect FAIL
   (marks at baseline → band empty). Commit red.
2. **GREEN** — implement `Ts` emission. Same test passes; full
   `tst_rtladdtexttest` suite + `ctest` 11/11 still green. Commit green.
3. **No-regression** — verify `fetch-text` and `search-text` on the mark PDF:
   - `fetch-text` → logical text intact (`مَا` / `بِسْم`), no phantom spaces.
   - `search-text "ما"` and `"بسم"` (marks stripped by normalization) → 1 match.
   - Render diff: non-mark text golden images unchanged.
   Commit any flow/extraction guard needed.
4. **Docs + DB** — PROBLEMS.md P3 resolved (with sha), README/RELEASES note,
   `db/seed.py` task closed.
5. **Gate + push** — `ci/run-ci.sh` all green (build, ctest, ASAN/UBSAN,
   clang-format), live CLI check, push.

## Risks & mitigations

| Risk | Mitigation |
|---|---|
| `Ts` unit convention wrong (pre/post `Tfs`) | pixel-probe test pins exact value; adjust conversion in step 2 |
| `Ts` causes flow/extraction phantom spacing | `Ts` moves only the baseline, not the text position; verify with fetch-text in step 3; fallback: keep marks inline (status quo) |
| Multi-mark stacking (e.g. fatha + shadda) | each mark has its own `y_offset`; emit per-glyph `Ts` toggles — covered by `مَا`/`بِسْم` probes; add a stacked case if needed |
| Fonts without GPOS mark anchors | `yOffset == 0` → no `Ts` emitted; behavior unchanged (verify with Noto Naskh Arabic which has anchors) |
| Search regression (S#1 interplay) | mark glyph has zero advance and same x → stays in the same flow item; step 3 explicitly tests search on mark text |

## Out of scope

- Vertical offsets for **LTR** runs (Helvetica path has no marks).
- Reordering/stacking beyond GPOS anchor positions (HarfBuzz already
  positions stacked marks; we only need to honor the y).
- Any GUI (ADR-0002).
