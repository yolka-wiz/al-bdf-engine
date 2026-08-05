# P3 Execution Plan — Vertical Mark Offsets (Orchestrated)

> **For Hermes:** orchestrated development via subagents. Each phase is a
> self-contained delegate_task with its own RED/GREEN gate and commit.
> Status: **BLOCKED-ROOT-CAUSE-FOUND** (2026-08-05). Owner: yolka (orchestrator) + rtl-agent (impl).

**Goal:** Make Arabic/Persian diacritics (tashkeel) and Hebrew niqqud render
**above** the baseline by emitting per-glyph `Ts` (text rise) in the RTL
content-stream emitter, without breaking extraction, search, or golden images.

**Design doc:** `plans/P3-vertical-mark-offsets.md` (root cause verified: lines
356–357 capture `yOffset`, 467–474 discard it; TJ is horizontal-only).

**Tech stack:** C++20, Qt 6.8, CMake + Ninja, vcpkg manifest
(`/home/agent/vcpkg-cache/vcpkg`), HarfBuzz/FriBidi, ctest offscreen.

---

## ⚠️ CRITICAL FINDING (2026-08-05, verified by orchestrator with Ghostscript)

**The CIDToGIDMap array emitted by pdfrtltextengine.cpp is invalid per PDF spec
and causes WRONG GLYPHS in every strict renderer — including albdf's own.**

- The emitter writes `/CIDToGIDMap [0 681]` or `[0 1173 728 1266]` — a short
  **array** (one entry per used glyph). PDF 32000-1 §9.7.4.3 requires
  **65536 entries** for a 2-byte-CID font, or a **stream**.
- Strict renderers (Ghostscript) reject the short array → Identity fallback
  (CID == GID) → code 1 = GID 1 = `.null` (invisible), code 2 = GID 2 =
  **Latin 'A'**. Single "ا" renders as NO INK; "مَا" renders as 'A' shapes.
- PDF4QT's parser reads CIDToGIDMap only when it `isStream()` (pdffont.cpp
  ~2354) — so albdf's own renderer ALSO ignores the array. **The "14px mark
  blob" in earlier Phase-2 probes was literally Latin 'A' (gid 2), not the
  fatha.**
- **Proof:** hand-patched alef.pdf with a proper 65536-entry (Flate or raw)
  CIDToGIDMap stream → Ghostscript paints the alef correctly (2×16px stroke).
- Why tests missed it: golden tests never pixel-verify albdf's own RTL output;
  fetch-text/search use ToUnicode + /ActualText (correct regardless of glyph
  mapping); RTL render tests only assert exit code / no FreeType errors.

**Consequence:** P3's Ts emission is *mechanically correct* (gid 728 + `Ts 2`
rises 2px in Ghostscript), but it can never be validated by pixel probes until
the CIDToGIDMap emission is fixed. Fix order is therefore:

1. **Fix CIDToGIDMap emission** → emit a 65536-entry stream (2 bytes per CID,
   code→gid, unused = 0), Flate-compressed if the factory supports it, or raw
   131072-byte stream (matches existing uncompressed ToUnicode style). Keep the
   per-instance code scheme (codeToGid). Verify with Ghostscript + albdf render.
2. **Re-apply + verify Ts emission** (kasra sign to be pinned empirically with
   correct glyph mapping — the earlier ±3.93 confusion was measuring 'A').
3. No-regression: fetch-text/search/golden — **golden images may legitimately
   change** (they embedded wrong glyphs); regenerate deliberately with evidence.
4. Docs + DB: PROBLEMS.md gets a new P# entry for the CIDToGIDMap bug (spec
   compliance, affects ALL RTL output), P3 → resolved with sha, DB tasks closed.

---

## Baseline facts (verified 2026-08-05)

| Fact | Value |
|---|---|
| Working tree | clean @ `dc3e1d8` (RED test committed) |
| Build dir | `src/build` exists (Release, Ninja) — **not** repo-root `build` |
| Baseline tests | `QT_QPA_PLATFORM=offscreen ctest --test-dir src/build` → **11/11 pass** (RED test excluded from baseline; now 1 fail expected) |
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

## Execution phases (REVISED)

### Phase 2a — Fix CIDToGIDMap emission (rtl-agent) ← NEW, do FIRST

**Objective:** Spec-valid glyph mapping so every renderer paints the right glyph.

**Files:**
- Modify: `src/Pdf4QtLibCore/sources/pdfrtltextengine.cpp` (CIDToGIDMap block,
  lines ~604–615).

**Steps:**
1. Replace the array emission with a **stream** of 65536 2-byte big-endian
   entries: entry[0] = 0 (.notdef), entry[code] = gid from `codeToGid`, all
   other entries = 0. 131072 bytes raw. If the project's PDFStream/factory
   supports FlateDecode (check how other streams are written), use it — zeros
   compress to near nothing; otherwise raw stream with `/Length` is acceptable
   and matches the existing uncompressed ToUnicode style.
2. **Verify glyph correctness with Ghostscript** (independent, spec-strict):
   ```
   gs -q -dNOPAUSE -dBATCH -sDEVICE=png16m -r72 -sOutputFile=/tmp/x.png <out.pdf>
   ```
   - single "ا" → a vertical stroke (INK present), NOT empty, NOT 'A'
   - "مَا" → alef + meem glyphs + small fatha mark, NOT Latin 'A' shapes
3. Verify albdf's own render matches (same shapes, not 'A').
4. Full ctest still 11/11 except the RED P3 test (which may still fail —
   glyph mapping is fixed but Ts is not yet re-applied).
5. Commit: `fix(rtl): emit spec-valid 65536-entry CIDToGIDMap stream (P6)`.

**Gate:** Ghostscript + albdf both paint correct Arabic glyphs; no regressions.

### Phase 2b — Re-apply + pin Ts emission (rtl-agent)

**Objective:** Make the RED test GREEN with correct glyph mapping.

**Steps:**
1. Re-apply the Ts emission from the earlier attempt (flush TJ → `rise Ts` →
   mark `<hex> Tj` → `0 Ts` → reopen TJ), with the stale comments updated.
2. **Pin the formula and kasra sign EMPIRICALLY** with Ghostscript on the real
   output: render mark PDF, measure where fatha ink lands (must be above base
   top) and kasra ink (must be below base bottom). Try `rise = -yOffset *
   fontSize / upem` first, then variants, ONE at a time. The earlier kasra
   confusion (±3.93) was measuring Latin 'A' — re-derive with correct glyphs.
3. RED test must pass (all three assertions: fatha above, no core overpaint,
   kasra below). Full ctest 11/11.
4. Commit: `feat(rtl): emit Ts for GPOS vertical mark offsets (P3 green)`.

**Gate:** RED test green + full ctest green + Ghostscript confirms glyphs.

### Phase 3 — No-regression: extraction & search (rtl-agent)

**Objective:** Prove glyph-map fix + Ts did not break extraction/search/golden.

**Steps:**
1. On mark PDF: `fetch-text` → logical text intact; `search-text` → 1 match.
2. Golden images: **expect changes where goldens embedded wrong glyphs** —
   regenerate via the golden harness with evidence, do not hand-edit.
3. Commit as needed: `fix(rtl): regenerate goldens for correct glyph mapping (P6)`.
4. Report: fetch-text output, search counts, golden diff summary, commits.

**Gate:** extraction + search unchanged; golden diffs are deliberate glyph fixes.

### Phase 4 — Docs, DB, gate, push (orchestrator, yolka)

**Steps:**
1. PROBLEMS.md: new entry **P6** CIDToGIDMap array invalid (spec §9.7.4.3) →
   Fixed with sha; P3 row → Fixed with sha; note P3 was unverifiable until P6.
2. README/RELEASES limitation note → feature note.
3. DB: `task-done 19 --ref <green-sha>`; add+close P6 task in seed.py.
4. Gate: `ci/run-ci.sh` all green; push.

**Gate:** DB shows #19 + P6 done with shas; CI green; pushed.

---

## Risks & debugging protocol

| Risk | Handling |
|---|---|
| CIDToGIDMap stream too large (131KB raw per font) | Flate it if supported; verify size in real output; if factory lacks Flate, accept raw and note as follow-up |
| `Ts` unit convention wrong (pre/post `Tfs`) | Ghostscript pixel probe pins it; ranked hypotheses H1/H2, one at a time |
| Kasra sign ambiguity | Re-derive with CORRECT glyphs (earlier data was polluted by 'A') |
| Golden images "break" | Expected — they embedded wrong glyphs; regenerate with evidence |
| PDF4QT renderer still differs from Ghostscript | Compare both on same PDF; PDF4QT reads stream maps so should now match |

**Debugging rules (systematic-debugging):**
- Phase 2a FIRST — no Ts work until glyphs map correctly (symptom-fix trap).
- One variable per change; tight loop = Ghostscript pixel probe + `-R RtlAddText`.
- Temporary logs tagged `[DEBUG-p3]`; removed before commit.
- If 3+ fix attempts fail → stop, question the emission architecture, report.
