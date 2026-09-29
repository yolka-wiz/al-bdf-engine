# Changelog

All notable changes to `albdf` are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Nothing yet.

## [0.4.1] - 2026-09-29

**Patch release: the first one that actually ships binaries.** `0.4.0` published
zero assets — three of its four matrix legs failed and the publisher was gated
all-or-nothing, so even the leg that built was discarded. This release fixes
those legs and that gate. It is the first tag since `0.4.0` (2026-08-15), which
is also why a month of merged work reaches users only now: the release workflow
fires on a `0.*` tag and nothing had been tagged.

### Fixed

- **Release binaries ship.** `linux-x86_64`, `linux-aarch64`, `macos-arm64` and
  `macos-x86_64` tarballs (plus `.sha256`) are attached to this release.
- The publisher job no longer discards the whole release when one platform leg
  fails. It was gated on `needs.build.result == 'success'` — the *aggregate* of
  the matrix — so one red leg skipped the job and threw away the artifacts the
  other legs had just built; that is exactly how `0.4.0` shipped with zero
  assets. It now publishes whatever built and names the platforms that did not.
- macOS legs compile: the `std::execution::seq` / `::par` uses in the vendored
  headers are guarded behind `__cpp_lib_execution` (libc++ ships `<execution>`
  without the PSTL policies; libstdc++ has them), with a serial fallback.
- `linux-aarch64` obtains Qt: aqtinstall is given the `linux_arm64` host, so
  Qt 6.8 resolves for the `linux_gcc_arm64` arch instead of reading the
  arm64-less `linux_x64` index.
- Packaging bundles the plugins of the Qt the binary actually links, not the
  runner's default Qt, so the tarball no longer depends on the runner image.
- macOS tarballs are self-contained: the Qt frameworks, fontconfig and the
  platform plugins the binary uses are bundled and referenced relatively.
- `albdf` starts with no display server, `--help-all` is treated as help, and
  CLI exceptions are guarded with the documented exit-code contract enforced
  and regression-tested.

### Changed

- A gating `gate-macos` job in `ci.yml` builds + ctests on macOS for every pull
  request, so a macOS-only break fails on the PR instead of at tag time.
- The GUI AppImage job is gone — this repository ships CLI binaries.
- The `windows-x86_64` matrix leg is **dropped for now**: the MSVC runner has no
  `pkg-config` and `src/CMakeLists.txt` requires it, so the leg died at
  configure after ~26 minutes. Tracked as R6.4.

### Also in this build (merged since 0.4.0, internal)

- `PDFPageContentRewriter` (R3.1/R3.2): one core page write-back path shared by
  `add-text` and `delete-object`; `Q_ASSERT`-as-validation replaced by explicit
  bounds checks and error codes.
- `PDFBidi` / `PDFShaper` (R4.1/R4.2): the RTL seam that owns the only FriBidi
  and HarfBuzz includes and the only visual↔logical inversion logic.

### Known limitations

- Linux binaries need the distribution Qt listed in
  `share/doc/albdf/PREREQUISITES.md` (Qt 6.8 or newer; Ubuntu 24.04's Qt 6.4 is
  too old).
- There is no Windows artifact: see R6.4.
- The `0.4.0` release stays asset-less — its tag predates every fix above.

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

[0.4.1]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.4.1
[0.4.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.4.0
[0.3.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.3.0
[0.2.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.2.0
[0.1.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.1.0
