# albdf — release notes

## Wave 1 infra (unreleased, feature/wave1-infra) — hosted CI, release packaging, perf benchmark

Date: 2026-08-06
Branch: `feature/wave1-infra` (target: `main`)

- **Hosted CI (W1, DB #25)** — the local gate now also runs on GitHub-hosted
  runners (`.github/workflows/ci.yml`): `gate` / `asan` / `format` jobs shell
  out to `ci/run-ci.sh` with stage-selection flags, so stage logic lives in
  exactly one place. vcpkg deps are pinned (`2026.07.29`) and binary-cached;
  Qt 6.8 comes from the official archives (`d254041`).
- **Release packaging (W7, DB #26)** — `scripts/package.sh` produces a
  deterministic `albdf-<version>-linux-<arch>.tar.gz` from the CMake install
  rules: albdf binary + `libPdf4QtLibCore` + public headers + man page +
  license. The script stages via `cmake --install` into a temp prefix,
  validates the *installed* binary headless (`--version` + `info` on a
  fixture, `QT_QPA_PLATFORM=offscreen`), and tars with reproducible metadata
  (`--sort=name`, root-owned, `SOURCE_DATE_EPOCH` mtime, `gzip -n`) — two
  runs from one build produce identical sha256. Install rules were added
  additively to `src/CMakeLists.txt`, including an `$ORIGIN` INSTALL_RPATH so
  the installed binary never carries a build-tree path. `--deb` additionally
  builds a minimal (skeleton, not policy-complete) `.deb`.
- **Perf benchmark (W8, DB #27)** — the manual M7 perf smoke is promoted to a
  tracked benchmark, `scripts/benchmark.sh`: generates a deterministic
  1000-page document, measures open/info, fetch-text, render, search and
  delete+write timings, prints a stable machine-readable summary
  (`albdf-benchmark-v1` key=value block), and fails if any op exceeds
  `ALBDF_PERF_THRESHOLD_MS` (default 5000 ms). Runs on every PR as a
  separate non-gating CI job (`benchmark`) with the summary uploaded as an
  artifact; locally: `bash scripts/benchmark.sh`.

---

## Post-0.1.0 (main, unreleased) — rename + M8/M8.1

Date: 2026-08-05
Branch: `main` (GitHub: yolka-wiz/al-bdf-engine)

- **Rename:** project and CLI renamed from `pdfedit`/`PdfTool` to **albdf**.
  Binary is now `albdf`; man page `docs/albdf.1`; CMake project `albdf`.
  Upstream library names (`Pdf4QtLibCore`) and the `src/PdfTool/` layout are
  kept for cherry-pick hygiene.
- **License:** authored code relicensed GPL-3.0-or-later (ADR-0005, `68ba8a6`);
  upstream PDF4QT portions keep MIT.
- **M8 — Forms & signatures:** `form-list`, `form-fill`, `sign`
  (PKCS#7 detached, PAdES byte-range), `verify-signatures`; round-trip +
  tamper tests (`3b3e563`, `b8efe4a`, `02b8719`, `6a2a325`). Suite now 11/11.
- **M8.1 — Real-world compatibility sweep:** content-stream floats in
  FixedNotation (strict parsers reject `1.5e-05`, `591dfee`); control-char
  XML sanitization + indirect page resources (`0edbb03`); search duplicate-yeh
  collapse (`615b1bc`); CI format gate exempts upstream-derived files
  (`13cda1c`). All 11 corpus files pass the 7-step battery.

---

# pdfedit 0.1.0 — release notes

> Historical record. This release was tagged **before** the rename to `albdf`
> (see the post-0.1.0 entry above); the CLI was still `PdfTool` and the project
> `pdfedit` at this point. The source layout (`src/PdfTool/`, binary `albdf`)
> applies only to later builds.

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
  `--no-normalize`. Cross-item phrase matching: a phrase split across two
  adjacent text-flow items is found as one occurrence and the Item column
  shows the span range (`2+3`); matches never span lines or columns.

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
- Search matches within a single text item, or across adjacent items split at a word boundary (S#1 resolved: joined per-page visual string; cross-line spans remain out of scope).

## Dependencies (permissive only, ADR-0004)

Qt6 (LGPL), FreeType (FTL), OpenJPEG (MIT), OpenSSL (Apache-2.0), ZLIB,
HarfBuzz (MIT), FriBidi (LGPL-2.1, dynamically linked), blend2d/asmjit
(vcpkg overlay, pinned).

## Commits since fork base

- 327061b M3 add-text (LTR)
- fe016c4 M4 RTL write pipeline
- 9d7d710 fix mirrored RTL rendering (HB>=4 leftmost-first)
- aa92bee M5 RTL search + engine bugfixes (absolute clusters,
  per-instance codes, TJ kerning arrays)
- b6f4bf6/dc45fcb/70e8934 M6 CI + format gate
- 479ebe0 M7 docs (README, man page)
- 0.1.0 tag: this release
