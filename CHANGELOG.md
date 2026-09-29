# Changelog

All notable changes to `albdf` are documented here. The format is based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

Nothing yet.

## [0.5.0] - 2026-09-29

Release binaries that actually ship, plus the shared write-back path (R3) and
the RTL backend seam (R4).

### Added

- **Release artifacts for five platforms** — `linux-x86_64`, `linux-aarch64`,
  `macos-arm64`, `macos-x86_64` and `windows-x86_64` (MSVC 2022) — built,
  validated and attached to the GitHub Release by
  `.github/workflows/release.yml` on a version tag. Each tarball carries the
  CLI, `Pdf4QtLibCore`, the headers, the man page, the license, and
  `PREREQUISITES.md` + `THIRD_PARTY_NOTICES.md`.
- A gating `gate-macos` job in `ci.yml`: a macOS-only compile break now fails on
  the pull request instead of surfacing at tag time — the gap that let `0.4.0`
  ship with zero assets.
- `docs/PREREQUISITES.md`: what a *prebuilt* tarball needs on the machine that
  runs it, shipped inside the tarball, including why those libraries are
  prerequisites rather than bundled.
- `PDFPageContentRewriter` (R3.1): one core helper for page write-back
  (replace resources → compress content → build content/page dicts → merge →
  finalize), with tests for `/Contents` arrays and indirect resources.
- `PDFBidi` / `PDFShaper` (R4.1/R4.2): the internal RTL seam that owns the only
  FriBidi and HarfBuzz includes in core and the only visual↔logical inversion
  logic, unit-tested without the full engine.

### Changed

- `add-text` (LTR and RTL) and `delete-object` now share the `R3.1` write-back
  path, so page updates behave identically across both commands.
- `Q_ASSERT`-as-validation in those commands (R3.2) is replaced by explicit
  bounds checks and error codes.
- macOS tarballs are self-contained: the Qt frameworks, fontconfig and the
  platform plugins the binary uses are bundled and referenced relatively, so
  they run on a Mac that has never had Homebrew Qt installed.

### Fixed

- **A failing release leg no longer discards the whole release.** The `release`
  job was gated on `needs.build.result == 'success'` — the *aggregate* result of
  the matrix — so a single red platform skipped the job and threw away the
  artifacts the other legs had just built. That is exactly how `0.4.0` shipped
  with zero assets. The job now publishes whatever built and names the platforms
  that did not in the release body.
- Packaging bundles the plugins of the Qt the binary actually links, not the
  runner's default Qt, so the tarball no longer depends on the runner image.
- macOS builds: the `std::execution::seq` / `::par` uses in the vendored headers
  are guarded behind `__cpp_lib_execution` (libc++ ships `<execution>` without
  the PSTL policies; libstdc++ has them), with a serial fallback. Both macOS
  legs compile again.
- `linux-aarch64` builds: aqtinstall is given the `linux_arm64` host, so Qt 6.8
  resolves for the `linux_gcc_arm64` arch instead of reading the arm64-less
  `linux_x64` index.
- `albdf` starts without a display server: with neither `$DISPLAY` nor
  `$WAYLAND_DISPLAY` set it selects the offscreen platform itself.
- `--help-all` is treated as help rather than an unknown command.
- CLI exceptions are guarded and the documented exit-code contract is enforced
  and regression-tested.

### Known limitations

- The **`windows-x86_64`** tarball is new and is **not self-contained**: it
  carries `albdf.exe` and `Pdf4QtLibCore.dll` but no Qt runtime, so it needs a
  Qt 6.8 (MSVC 2022, 64-bit) installation. Treat it as experimental.
- Linux binaries need the distribution Qt listed in
  `share/doc/albdf/PREREQUISITES.md` (Qt 6.8 or newer; Ubuntu 24.04's Qt 6.4 is
  too old).
- The `0.4.0` release itself stays asset-less — see `docs/RELEASES.md` for the
  backfill options.

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

[0.5.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.5.0
[0.4.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.4.0
[0.3.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.3.0
[0.2.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.2.0
[0.1.0]: https://github.com/yolka-wiz/al-bdf-engine/releases/tag/0.1.0
