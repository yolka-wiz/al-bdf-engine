# Changelog

All notable changes to `albdf` are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Nothing yet.

## [0.4.0] - 2026-08-15

RTL appearance streams (M14) plus multi-platform release binaries.

### Added

- FreeText annotation RTL appearance streams: shaped Type0 fonts are embedded
  in the annotation `/Resources`, so RTL text renders in viewers that draw the
  appearance stream instead of the annotation contents.
- Form-field RTL appearance streams, generated via `PDFRTLTextEngine` when an
  RTL font is supplied with `form-fill --font` (upstream `drawFormField` is a
  no-op headless).
- Release workflow building Linux x86_64/aarch64 and macOS arm64/x86_64
  artifacts; `scripts/package.sh` is now cross-platform (`.dylib`/`@loader_path`
  on macOS, `.so`/`$ORIGIN` on Linux).
- Re-vendored four upstream PDF4QT core commits into the fork.

### Fixed

- R#5: the form-field and FreeText appearance-stream "tofu" gap is closed.

## [0.3.0] - 2026-08-07

GUI restore (M12) and RTL text wiring (M13).

### Added

- Optional GUI build (`-DALBDF_BUILD_GUI=ON`) restoring the PDF4QT Viewer,
  Editor, and PageMaster apps against the modified core; the headless default
  is preserved.
- GUI search routed through the RTL-aware `PDFTextSearchEngine`, plus RTL
  clipboard re-inversion to logical order.

### Fixed

- R#4: the content editor now preserves `/ActualText` marked content on
  re-edit, so mixed RTL+LTR `add-text` keeps full lam-alef ligatures.
- S#3: document-level RTL search now finds lam-alef words such as `سلام`.

## [0.2.0] - 2026-08-07

First public albdf-branded release (project renamed from `pdfedit`).

### Added

- Forms and signatures: `form-list`, `form-fill`, `sign` (PKCS#7 detached,
  PAdES byte-range), and `verify-signatures`, with round-trip and tamper tests.
- Page operations CLI: `rotate`, `move-page`, `delete-page`.
- RTL extraction fidelity: `/ActualText` carries full cluster text so lam-alef
  ligatures survive extraction; decomposed marks are deduplicated.
- Cross-line RTL search for paragraph-like line gaps.
- Hosted GitHub Actions CI (`gate`/`asan`/`format`), deterministic release
  packaging, a tracked perf benchmark, and a seeded CLI fuzz harness.
- Redaction wired and pixel-verified, with byte-determinism.

### Fixed

- `PDFDocumentBuilder` no longer stamps the current date into
  `CreationDate`/`ModDate`; output is now byte-reproducible via
  `SOURCE_DATE_EPOCH`.
- Render argument validation: out-of-range page ranges and extreme DPI values
  are rejected with exit 7 instead of aborting or hanging.
- Page-ops flags no longer overflow 32-bit `QFlags` on Qt 6.8/6.10.

## [0.1.0] - 2026-08-04

Initial release, tagged as `pdfedit` before the rename.

### Added

- `add-text` (LTR) with standard Helvetica and zero font embedding.
- `add-text --rtl`: FriBidi directional runs, HarfBuzz shaping, and an embedded
  Type0/Identity-H TrueType font with ToUnicode CMap and `/ActualText`.
- `recognize-text` (content objects with bounding boxes) and `delete-object`.
- RTL-aware `search-text` with normalization (tashkeel, ZWNJ/ZWJ,
  presentation forms, lam-alef, and Persian/Arabic digit unification).
- CLI smoke suite and a golden-image render harness.

[0.4.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.4.0
[0.3.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.3.0
[0.2.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.2.0
[0.1.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.1.0
