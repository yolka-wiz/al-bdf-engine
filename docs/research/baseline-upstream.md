# Baseline — Pristine PDF4QT on this Container (M0.5)

**Date:** 2026-08-04
**Status:** DONE — recorded BEFORE any fork modifications (M0.5 gate)
**Upstream:** JakubMelka/PDF4QT 1.6.0.0 (master, depth-1 clone 2026-08-03)
**Source tree:** `vendor-upstream-pdf4qt/` (working-tree only, NOT in git)
**Build dir:** `vendor-upstream-pdf4qt/build-baseline/`

---

## 1. Environment (as-built)

| Component | Version | Source |
|---|---|---|
| OS | Ubuntu 26.04 (Resolute) arm64 | container |
| GCC | 15.2.0 | apt (mirror.iranserver.com) |
| CMake | 4.2.3 (system); 3.28.4 / 3.31.6 (blend2d workaround) | apt + cmake.org |
| Ninja | system | apt |
| Qt | 6.10.2 (Core/Gui/Xml/Svg + LinguistTools + TextToSpeech + FontConfig) | apt |
| vcpkg | bootstrapped 2026-08-04 | github |
| vcpkg deps (arm64-linux) | tbb 2023.1.0, openssl 3.6.3, lcms, zlib, openjpeg, freetype, libjpeg-turbo, libpng, **blend2d (pinned 6dbc2ce via PDF4QT overlay)**, brotli, bzip2, asmjit | vcpkg + PDF4QT overlays |

**Notable environment fights (documented for the fork phase):**
- apt cannot use the socks5 proxy → `/etc/apt/apt.conf.d/99direct` + `mirror.iranserver.com`.
- blend2d source build fails on gcc15+aarch64 with CMake recursion bugs
  (`Maximum recursion depth` in GNUInstallDirs / CheckFlagCommonConfig on cmake 4.2/3.31/3.28).
  **vcpkg with PDF4QT's own overlay ports builds it fine** — use vcpkg, never hand-build blend2d.

## 2. Build (pristine, core+CLI+tests only)

```bash
cmake -S . -B build-baseline -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DPDF4QT_BUILD_TESTS=ON -DPDF4QT_INSTALL_INCLUDE=OFF
cmake --build build-baseline --target Pdf4QtLibCore PdfTool UnitTests \
  UnitTestsImageOptimizer UnitTestsFontEncoding -j$(nproc)
```
**Result: SUCCESS.** `build-baseline/usr/bin/{PdfTool,UnitTests,UnitTestsImageOptimizer,UnitTestsFontEncoding}`.
Note: `PDF4QT_BUILD_ONLY_CORE_LIBRARY=ON` excludes PdfTool too (it sits in the GUI block) —
build explicit targets instead. GUI apps NOT built (out of scope).

## 3. Upstream unit tests (QT_QPA_PLATFORM=offscreen)

| Suite | Passed | Failed |
|---|---|---|
| UnitTests (LexicalAnalyzer etc.) | 18 | 0 |
| UnitTestsImageOptimizer | 10 | 0 |
| UnitTestsFontEncoding | 8 | 0 |
| **Total** | **36** | **0** |

## 4. CLI smoke (headless)

Fixture: `test-baseline.pdf` — handwritten 2-page PDF (text + rectangles), 948 bytes, 7 objects.

| Command | Result |
|---|---|
| `PdfTool --version` | `PdfTool 1.6.0.0` |
| `PdfTool info` | Version 1.4, 2 pages, Letter |
| `PdfTool fetch-text` | "Hello PDF4QT baseline!" + "Second page with numbers 12345" |
| `PdfTool render --page-first 1 --page-last 2 --image-format png` | Image_1.png (9735B), Image_2.png (10532B) |
| `PdfTool statistics` | object-class breakdown works |
| `PdfTool unite a.pdf b.pdf target.pdf` | 4-page united.pdf (2226B) — positional args |

## 5. Golden anchors (regression baseline)

| File | sha256 |
|---|---|
| Image_1.png (page 1 @72dpi) | `44c1e6a0ab39c9b62aeaeefa95fd21fe6b91d3b570d755b0e043b295af22122ef0` |
| Image_2.png (page 2 @72dpi) | `58850c388314d2f52e234327d8a58a93daa62e3c1ae1419d2384a06932df3030` |
| test-baseline.pdf (input fixture) | `82298ab8c610c10bdc2527b5c09fa64340f4a4a97f2ec3ec89f2c8a3d4f0627f` |
| united.pdf (4-page merge) | `96beb678729517ac58d1f25a9caa19b7869e026ed1bf3c8b07c0a923ffe10c07` |

Goldens copied to `src/tests/golden/` — they are the **pre-fork** reference: after M1 strips
GUI and forks the tree, renders must match these hashes (or the diff must be explained).

## 6. Known upstream quirks (not our bugs)

- `render` prints a harmless `QCommandLineParser: option not defined: "render-software"` warning.
- ctest's registered paths use multi-config dirs (`usr/bin/Debug/...`) that don't exist on
  single-config Ninja builds → run the test binaries directly (as above) or patch CTest in M1.
- Minimal handwritten fixtures trigger `Warning: Invalid format of reference table.` only when
  the xref is hand-rolled wrong; the corrected generator (scripts-tmp/make-test-pdf.py) is clean.

## 7. What M1 must preserve

1. All 36 unit tests still pass on the forked tree.
2. `fetch-text` / `render` / `unite` outputs identical on the same fixtures.
3. Golden PNG hashes unchanged (or documented diff).
4. Build stays headless; vcpkg remains the dependency path (recorded in DB `deps`).
