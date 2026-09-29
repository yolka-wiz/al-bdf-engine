# albdf — release notes

> The concise, user-facing changelog lives in [`/CHANGELOG.md`](../CHANGELOG.md);
> this file holds the detailed per-release engineering notes.

## albdf 0.4.1 — release binaries that actually ship (patch release)

Date: 2026-09-29
Branch: `main` (GitHub: yolka-wiz/al-bdf-engine)

A **patch release that only fixes the release itself**: `0.4.0` published zero
assets, and this is the first tag since — so four platforms (Linux x86_64 and
aarch64, macOS arm64 and x86_64) now publish tarballs plus `.sha256` from
`.github/workflows/release.yml`. No user-facing feature work is claimed here;
the R3/R4 refactors that merged in the meantime ride along because they are
already on `main`, and they are listed at the end for completeness.

The `0.4.0` release stays asset-less: its tag (`51897f21`) predates every fix
below, so re-running it would rebuild the broken commit under the old
all-or-nothing gate.

### Release engineering (R6.1)

- **The real `0.4.0` failure mode diagnosed and fixed.** The earlier hardening
  (per-platform `timeout-minutes`, a macOS `brew` retry) aimed at the wrong
  causes: those legs died on genuine defects, not stalls. Measured from the
  failed runs:

  | Cause | Evidence | Fix |
  |---|---|---|
  | macOS legs could not compile | `error: no member named 'seq' / 'par' in namespace 'std::execution'` in `pdfcms.cpp` / `pdfdiff.cpp`, on **both** macOS arches | guard the 5 `std::execution::*` uses in the two vendored headers (`pdfexecutionpolicy.h`, `pdfvisitor.h`) behind `__cpp_lib_execution >= 201603L` and fall back to the plain serial call — libc++ ships `<execution>` without the PSTL policies, libstdc++ does not |
  | linux-aarch64 could not obtain Qt | `The packages ['qt_base'] were not found while parsing XML of package information!` on all 3 retries, then `qmake not found after Qt install` | aqtinstall needs the `linux_arm64` **host** for the `linux_gcc_arm64` arch; the action passed `linux` and therefore read the arm64-less linux_x64 index |
  | a macOS-only break went unseen until tag time | `ci.yml` had no macOS job at all | new gating `gate-macos` job (`macos-15`) builds + ctests on every PR |
  | one red leg discarded the whole release | the `release` job was gated on `needs.build.result == 'success'`, the **aggregate** matrix result | `if: always() && startsWith(github.ref, 'refs/tags/')` + a "require at least one platform artifact" guard; the body now names the platforms that did not build |

- **First tag through the fixed pipeline** (`0.5.0`, run
  `36559286727`): `linux-x86_64`, `linux-aarch64`, `macos-arm64` and
  `macos-x86_64` all built, the publisher ran with `always()` and attached the
  artifacts — the gate fix proven end-to-end on its first outing. That tag was
  re-cut as **`0.4.1`** (this release) because a release-pipeline fix does not
  warrant a minor version; nothing had been downloaded.
- **The one leg that did not build: Windows.** `windows-x86_64` failed at
  `Configure + build` with
  `Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)` — the MSVC runner
  has no `pkg-config` and `src/CMakeLists.txt:52` requires it — after ~26
  minutes of vcpkg work. The leg is **dropped from the matrix** in this release
  (R6.4 covers restoring it), so no Windows artifact is published and the
  packaging path for it is documented as inert rather than advertised.

### Packaging

- **macOS tarballs are self-contained** (`4ba142a`): the Qt frameworks,
  fontconfig and the platform plugins the binary uses are bundled, with
  `@loader_path`-relative install names, so the artifact runs on a Mac that has
  never had Homebrew Qt installed.
- **The bundled plugins are the ones the linked Qt actually has** (`9f5a057`):
  the plugin set is derived from the Qt the binary links, not from the runner's
  default Qt, so the tarball no longer depends on the runner image.
- `docs/PREREQUISITES.md` ships inside every tarball as
  `share/doc/albdf/PREREQUISITES.md` (`f2b9117`), together with
  `THIRD_PARTY_NOTICES.md` and the Qt 6.8 floor (`cbcfeb7`).
- The GUI AppImage job is gone (`d74a90e`) — this repository ships CLI
  binaries, and the job was a permanent non-gating red leg.

### R3 — shared page write-back (R3.1/R3.2)

- `PDFPageContentRewriter` is now the single core path for updating a page's
  content (replace resources → compress content → build content/page dicts →
  merge → finalize): `10540e8`, with `add-text` routed through it at `5105e2a`
  and `delete-object` at `e5da2f4`; covered by `803162b` (including
  `/Contents` arrays and indirect resources).
- `Q_ASSERT`-as-validation in the two custom commands is gone (R3.2): bounds
  are checked explicitly and failures return documented error codes instead of
  aborting the process in a Release build.

### R4 — RTL backend seam (R4.1/R4.2)

- New internal `PDFBidi` (FriBidi) and `PDFShaper` (HarfBuzz) interfaces: the
  only `<fribidi.h>` / `<hb.h>` uses in core now sit behind the seam
  (`3283509`), and the RTL writer routes through them (`5bcadd4`).
- The duplicated visual↔logical inversion logic (previously in
  `pdfrtltextnormalizer.cpp` and `pdftextsearchengine.cpp`) is consolidated
  behind the seam (`a351b53`) and unit-tested without the engine (`65dbc2a`,
  `UnitTestsRtlBidi`).

### CLI correctness

- `albdf` starts with no display server: with neither `$DISPLAY` nor
  `$WAYLAND_DISPLAY` set it selects the offscreen platform itself (`d88b800`).
- Exceptions are guarded and the documented exit-code contract is enforced
  (`bbd37a2`) and regression-tested (`7d1e6e3`); `--help-all` is treated as
  help (`4bd6ee6`).

### Verified

- Reproduced locally and fixed: the macOS PSTL failure reproduces with Apple
  clang 21 (`_LIBCPP_VERSION 220106`) as `no member named 'seq' in namespace
  'std::execution'`, and the guard takes the serial-fallback branch there.
- aqt host resolution confirmed against the live Qt archives:
  `aqt list-qt linux desktop --arch 6.8.3` → `linux_gcc_64` only;
  `aqt list-qt linux_arm64 desktop --arch 6.8.3` → `linux_gcc_arm64`.
- Static gates: `bash -n` on `ci/run-ci.sh` and `scripts/check-slop.sh`;
  `bash scripts/check-slop.sh --self-test`.
- Hosted CI on the release PR: Linux `gate` (Release build + ctest),
  ASAN/UBSAN, clang-format + slop, the new `gate-macos`, plus the non-gating
  GUI / benchmark / fuzz jobs.
- **Assets**: the `0.4.1` tag run builds the four platforms and attaches the
  tarballs and their `.sha256` files; the platform-by-platform result is the
  *Platform availability* table the workflow appends to this release body.

### Known limitations

- No **Windows** artifact — the leg is disabled pending a runner-side
  `pkg-config` (R6.4, `docs/PROBLEMS.md`).
- Linux binaries need the distribution Qt listed in
  `share/doc/albdf/PREREQUISITES.md` (Qt 6.8+; Ubuntu 24.04's Qt 6.4 is too
  old).
- The workflow pins Node 20-era actions (`checkout@v4`, `cache@v4`, the artifact
  actions at `@v4`). They currently run on the forced Node 24 shim with a
  deprecation warning; the Node 24-native pins are identified and tracked as
  R6.5 (`docs/PROBLEMS.md`).

---

## albdf 0.4.0 — RTL appearance streams (M14) + multi-platform release

Date: 2026-08-15
Branch: `main` (GitHub: yolka-wiz/al-bdf-engine)

New **multi-platform GitHub Actions release workflow**
(`.github/workflows/release.yml`), triggered from a version tag, targeting
`{linux-x86_64, linux-aarch64, macos-arm64, macos-x86_64}` builds. The **0.4.0
multi-platform binaries did not ship**: the macOS and linux-aarch64 builds
failed, so no binary assets were attached to the 0.4.0 release (only the
linux-x86_64 matrix leg built). See the GitHub release status for the current
asset list; fixing the workflow is tracked as R6.1.

### M14 — RTL appearance streams (tasks #41/#42, R#5 resolved)

- **FreeText RTL appearance streams** (`m14/freetext-ap`): `add-text`-style RTL
  text on FreeText annotations now embeds the shaped Type0 font in the
  annotation's `/Resources` — no more tofu in viewers that render the AP
  instead of the annotation contents.
- **Form-field RTL appearance streams** (`m14/form-field-ap`): headless
  `form-fill` previously emitted **no appearance stream at all** (worse than
  tofu — `PDFFormManager::drawFormField` is a no-op in core). Form-field APs
  are now generated via `PDFRTLTextEngine` when an RTL font is supplied
  (`--font`), with the shaped font embedded in the AP `/Resources`.
- **R#5 resolved** — the form-field AP tofu gap is closed for both FreeText
  and form fields (see `docs/PROBLEMS.md`).

### Upstream sync

- Re-vendored 4 core commits from PDF4QT master (`12763887..master`) — engine
  fixes from upstream now live in the fork (TTS skipped by design; see
  `docs/PROBLEMS.md` for the fork-divergence note).
- CI format gate updated to exclude re-vendored upstream files
  (`a1c15b5f`).

### Packaging

- `scripts/package.sh` is now cross-platform: `.dylib` + `@loader_path` on
  macOS, `.so` + `$ORIGIN` on Linux, GNU-tar/bsdtar-aware, `shasum`-aware.
- New `release.yml` workflow: build matrix `{linux-x86_64, linux-aarch64,
  macos-arm64, macos-x86_64}` × `{Release}`, packaging + GitHub Release
  creation on tag push.

### Verified

- Core suite green on the CI gate (Release + ASAN/UBSAN + clang-format).
- Release artifacts: **none attached for 0.4.0** — the multi-platform workflow
  failed on macOS + linux-aarch64, so only the linux-x86_64 matrix leg built.
  Re-run tracked as R6.1.

---

## albdf 0.3.0 — GUI + RTL text wiring (M12–M13)

Date: 2026-08-07
Branch: `main` (GitHub: yolka-wiz/al-bdf-engine)

The first release with a **working GUI**: the PDF4QT apps (Viewer, Editor,
PageMaster) now build against our modified core, and the GUI's text layer is
wired to the RTL engine. Three real bugs fixed along the way.

### M12 — GUI restore

- Vendored the PDF4QT v1.6.0.0 GUI layers (`Pdf4QtLibGui`, `Pdf4QtLibWidgets`,
  `Pdf4QtEditor`, `Pdf4QtViewer`, `Pdf4QtPageMaster`) into `src/`, gated
  behind `ALBDF_BUILD_GUI=ON` (headless default preserved).
- TextToSpeech compiled out (no-op stub + guards — module not provisioned);
  fork divergence documented.
- Headless GUI smoke (`src/tests/gui-smoke.sh`) + RTL fixture; non-gating GUI
  CI job added.
- API-compat audit proved the vendored GUI compiles against our modified core
  with zero fixes — first real build: 270/270 targets.

### M13 — GUI RTL text wiring

- **GUI search → `PDFTextSearchEngine`.** Both find widgets
  (`PDFFindTextTool`, `PDFAdvancedFindWidget`) route plain-text queries through
  the RTL-aware engine (normalization + visual inversion + cross-line joining);
  regex/whole-word stay on the legacy path. New adapter
  `pdfwidgetrtlsearch.{h,cpp}` maps `Match → PDFFindResult` for the existing
  highlight pipeline.
- **RTL clipboard re-inversion.** Copying Arabic/Persian/Hebrew now puts
  logical-order text on the clipboard (was visual/reversed). New
  `PDFRTLTextNormalizer::invertToLogical` (FriBidi vis2log emulation, removed
  in FriBidi 1.0).
- **R#4 fixed** — the content editor now preserves `/ActualText` marked
  content on re-edit; mixed RTL+LTR add-text keeps the full lam-alef.
- **S#3 fixed** — document-level RTL search found no lam-alef words
  (`سلام`); visual-order collapse added. The GUI adapter exposed this
  pre-existing engine bug.

### Verified

- Core suite **16/16 ctest green** (Release + ASAN/UBSAN) + clang-format gate.
- GUI builds (115 targets), headless smoke passes, RTL fixture search now
  matches (was 0).

---

## albdf 0.2.0 — first public release (M8–M10 + CI on GitHub)

Date: 2026-08-07
Branch: `main` (GitHub: yolka-wiz/al-bdf-engine)

The first albdf-branded public release. Everything after the 0.1.0 (pdfedit)
tag: the rename, forms & signatures, the wave-1 (M9) extraction-fidelity and
page-ops work, wave-2 (M10) search/redaction/fuzzing, and a fully green
GitHub-hosted CI.

### M8 — Forms & signatures

- `form-list`, `form-fill`, `sign` (PKCS#7 detached, PAdES byte-range),
  `verify-signatures`; round-trip + tamper tests. Suite 11/11 (`3b3e563`,
  `b8efe4a`, `02b8719`, `6a2a325`).

### M9 — Wave 1: extraction fidelity + page ops + infra

- **P1/P2 — RTL extraction fidelity.** Writer emits `/ActualText` as the full
  cluster text in glyph (visual) order per ligature glyph (lam-alef `لا` no
  longer degrades to `ل`); reader overlays `/ActualText` spans onto the layout
  (`pdftextlayoutgenerator` + `PDFTextLayout::replaceCharacters`) and dedups
  decomposed yeh. `800f91e`, `c94f00b`.
- **R#3 pinned** — presentation-form NFKC pass regression test (`271bfe8`).
- **Page-ops CLI** — `rotate`, `move-page`, `delete-page` + `UnitTestsPageOps`
  (12/12). `delete-page` runs a full optimizer pass (unused objects +
  shrink). `4402b1c`..`411d8f7`.
- **Hosted CI (W1)** — GitHub Actions `gate`/`asan`/`format` jobs shell out to
  `ci/run-ci.sh`; vcpkg pinned + binary-cached; Qt 6.8 from official archives.
  `88413dc`.
- **Release packaging (W7)** — deterministic `scripts/package.sh`
  (SOURCE_DATE_EPOCH, `gzip -n`, `$ORIGIN` RPATH check, sha256 sidecar,
  optional `.deb`). `78f608d`.
- **Perf benchmark (W8)** — tracked `scripts/benchmark.sh` (1000-page doc,
  `albdf-benchmark-v1` summary, 5000 ms threshold, non-gating CI job).
  `b197861`.

### M10 — Wave 2: search, redaction, fuzzing, render fixes

- **S#1 cross-line search** — phrases can match across a line break when the
  vertical gap is paragraph-like (≤ 0.5× line height); column separation keeps
  the hard `\n` boundary so no false matches. `0cf3338`.
- **Redaction verified + tested** — upstream `PDFRedact` wired and proven:
  annotation-only regions (no `--page` flag), pixel-verified solid-black bars,
  round-trip, byte-determinism. Fixture `redact-annotated.pdf` +
  `tst_redacttest.cpp`. `3a7568b`, `4dcdb32`.
- **Determinism fix (core)** — `PDFDocumentBuilder` no longer stamps
  `QDateTime::currentDateTime()` into `CreationDate`/`ModDate`; uses
  `SOURCE_DATE_EPOCH` else a fixed epoch, so every rebuild is
  byte-reproducible (was breaking redact/unite/page-ops determinism).
  `e79bc06`.
- **CLI fuzz harness** — deterministic seeded `scripts/fuzz.sh` (10 commands,
  timeouts, `albdf-fuzz-v1` summary) + non-gating CI job. Found real bugs
  (below). `8d935a1`, `4726c93`.
- **F#1/F#2 render arg validation** — `--page-first 0` / `--page-last
  999999999` no longer SIGABRT (page range now enforced in
  `PDFClosedIntervalSet::parse`); `--image-res-dpi 999999` no longer hangs
  (image size pre-check at 16384×16384). `7a70767` + `UnitTestsRender`.
- **QFlags 32-bit fix** — page-ops flags overflowed `QFlags`' int storage,
  breaking every build on Qt 6.8/6.10; replaced with a 64-bit `Options`
  class. `4bb644f`.

### CI on GitHub — all green (2026-08-07)

Run 31165712432: `gate`/`asan`/`format` required checks + `benchmark` +
`fuzz` all pass. Toolchain traps fixed and documented (PROBLEMS.md):
vcpkg bootstrap, aqt archives-vs-modules + retry, ICU 73 built before Qt
install, fontconfig/freetype headers, benchmark comma-thousands parse.

### Quality gates (0.2.0, verified)

- 14/14 ctest targets (unit, font-encoding, recognize, delete, add-text, RTL
  add-text, RTL search, golden render, forms/signatures, page-ops, redaction,
  render-args, CLI smoke) — Release and ASAN/UBSAN.
- clang-format gate green (40+ authored files; vendored upstream exempt).
- Benchmark PASS (5/5 ops, max < 100 ms on GH runners vs 5000 ms threshold).
- Fuzz harness PASS on the release tree.

### Known limitations (carried from v1, documented)

- R#4 — content editor drops `/ActualText` marked content when a second
  `add-text` rewrites a page (mixed add-text degrades ligatures on that page;
  RTL-only documents are fine). Future content-editor change.
- Vertical mark offsets (diacritic height) have rendering caveats on some
  foreign PDFs; search and spec-valid output are prioritized over perfect
  glyph positioning in v1.
- GUI deliberately deferred (ADR-0002).

---

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
