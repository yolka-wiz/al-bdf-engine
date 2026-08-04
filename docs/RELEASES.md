# pdfedit 0.1.0 — release notes

Date: 2026-08-04
Branch: `main` (local-only hosting)

## What this is

Headless PDF editing library + CLI for Linux, forked from PDF4QT (MIT).
CLI-first: no GUI in v1 (ADR-0002). The differentiator is RTL
(Arabic/Persian/Hebrew) text writing and search — upstream PDF4QT has neither.

## Highlights

- **add-text (LTR)** — standard Helvetica, zero embedding; `--page/--x/--y/--text/--size`.
- **add-text --rtl** — full RTL write pipeline:
  FriBidi directional runs (UAX #9) -> HarfBuzz shaping
  (cluster_level=2) -> Type0/Identity-H embedded TrueType with explicit
  /CIDToGIDMap (per-instance codes), hmtx /W, ToUnicode CMap, and
  /ActualText marked content. Kerning emitted as TJ spacing arrays.
- **recognize-text** — lists page content objects (text/image/path) with
  bounding boxes; index space for delete-object.
- **delete-object** — whole-object deletion by page + index, safe
  write-back via PDFDocumentModifier.
- **search-text (RTL-aware)** — logical-order queries matched against
  visual-order PDF text via FriBidi inversion, with normalization:
  tashkeel strip, presentation-form/lam-alef folding, ZWNJ/ZWJ strip,
  Persian<->Arabic letter unification, Persian/Arabic-Indic digit
  unification. Reports page/item/bbox/matched text. `--case-sensitive`,
  `--no-normalize`.

## Quality gates (all green)

- 10/10 test targets: unit, font encoding, recognize, delete, add-text,
  RTL add-text (incl. mirror-regression vs Qt bidi ground truth),
  RTL search (Hebrew/digits/ZWNJ/tashkeel/mixed-bidi corpus), golden-image
  render harness, CLI smoke.
- ASAN/UBSAN build: 10/10 clean.
- clang-format gate: 24 authored files (vendored upstream exempt).
- `ci/run-ci.sh` — one-command gate; green on the dev container.
- Perf smoke on 1000-page doc (lazy-load): info 0.03s, fetch-text 0.04s,
  render 0.03s, search 0.04s (1000 matches), delete+write 0.02s.

## Known limitations (v1, documented in code)

- ToUnicode CMap destinations are one UTF-16 unit: ligatures degrade in
  fetch-text (full logical text in /ActualText; search still works via
  normalization).
- Decomposed marks duplicate their base letter in extraction.
- Vertical mark offsets (diacritic height) dropped in rendering.
- Search matches within a single text item (no cross-item spans).

## Dependencies (permissive only, ADR-0004)

Qt6 (LGPL), FreeType (FTL), OpenJPEG (MIT), OpenSSL (Apache-2.0), ZLIB,
HarfBuzz (MIT), FriBidi (LGPL-2.1, dynamically linked), blend2d/asmjit
(vcpkg overlay, pinned).

## Commits since fork base

- 942d55a M3 add-text (LTR)
- ca6313c M4 RTL write pipeline
- 23dc377 fix mirrored RTL rendering (HB>=4 leftmost-first)
- d516e4e M5 RTL search + engine bugfixes (absolute clusters,
  per-instance codes, TJ kerning arrays)
- f3d20e7/c433dfe/138619d M6 CI + format gate
- 717d1d4 M7 docs (README, man page)
- 0.1.0 tag: this release
