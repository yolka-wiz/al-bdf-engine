# AGENT.md — albdf Source Tree Guide

Onboarding guide for AI agents working in `/workspace/albdf/src`. This is the buildable
source tree of the **albdf** project — a fork of MIT-licensed **PDF4QT** delivering a
headless PDF editing **library + CLI** for Linux (no GUI in v1). Parallel work is governed
by the binding contract in [`/workspace/albdf/AGENTS.md`](../AGENTS.md) and the coding
standard in [`/workspace/albdf/docs/coding-standard.md`](../docs/coding-standard.md).

## Table of Contents
- [Repository & Conventions](#repository--conventions)
- [Directory Layout](#directory-layout)
- [Build System (CMake + vcpkg)](#build-system-cmake--vcpkg)
- [Build & Test Commands](#build--test-commands)
- [The CLI Binary (albdf)](#the-cli-binary-albdf)
- [Our 4 Custom CLI Tools](#our-4-custom-cli-tools)
- [The RTL Pipeline](#the-rtl-pipeline)
- [Test Structure](#test-structure)
- [Exit-Code Contract](#exit-code-contract)
- [Determinism Rules](#determinism-rules)
- [Common Pitfalls](#common-pitfalls)

## Repository & Conventions

- **Language/tooling:** C++20, Qt 6.10 (Core/Gui/Xml/Svg/Test **only** — NO Widgets/QML),
  CMake + Ninja, vcpkg manifest mode.
- **Headless only.** All tests run under `QT_QPA_PLATFORM=offscreen`.
- **Determinism is sacred.** No timestamps, no random IDs, no unstable ordering in any
  generated output — output must be **byte-stable** across runs.
- **max 3 parallel agents.** Coordinate via AGENTS.md before claiming overlapping work.

## Directory Layout

| Path | Contents |
|------|----------|
| `CMakeLists.txt` | Top-level `src` build script (lib + tools + tests). |
| `vcpkg.json` | vcpkg manifest (deps: tbb, openssl, lcms, zlib, openjpeg, freetype, libjpeg-turbo, libpng, blend2d, **harfbuzz**, **fribidi**). |
| `vcpkg/` | vcpkg **overlays** only (no manifest here). |
| `Pdf4QtLibCore/` | **The core PDF library** — the real code lives here. See `Pdf4QtLibCore/AGENT.md`. |
| `PdfTool/` | The CLI. `main.cpp` + `pdftool*.{h,cpp}` per command (~30 upstream + our 4). |
| `UnitTests/` | Our test suite (`tst_*.cpp`, golden tests, smoke.sh). |
| `tests/` | Test **data** (`fixtures/`, `fonts/`, `golden/`, `scripts/`). |
| `build/` | Release build dir (Ninja). |
| `build-asan/` | Debug + AddressSanitizer build dir. |

> **Trap:** there is **no `src/albdf/`, `src/cli/`, or `src/core/`** — the old
> `cli/`/`core/` scaffolds were removed. The real code is in **`Pdf4QtLibCore/`**
> (library) and **`PdfTool/`** (CLI, binary name `albdf`). Never create those
> directories.

## Build System (CMake + vcpkg)

- `CMakeLists.txt` explicitly enumerates every source file — there is **no globbing**.
- Dependencies are declared in `vcpkg.json`; vcpkg resolves them in **manifest mode**
  (a `vcpkg.json` present in the tree = auto-build of deps).
- `-DALBDF_BUILD_TESTS=ON` enables the test targets.
- New sources must be added to the matching `CMakeLists.txt` or they silently won't compile/link.

## Build & Test Commands

```bash
cd /workspace/albdf/src
# Configure (Release, Ninja, vcpkg toolchain, tests on)
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/workspace/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DALBDF_BUILD_TESTS=ON

# Build
cmake --build build

# Run tests (headless!)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure

# Full CI gate (4 stages: build → ctest → ASAN/UBSAN → clang-format on authored files only)
bash /workspace/albdf/ci/run-ci.sh
```

## The CLI Binary (albdf)

- Binary: **`/workspace/albdf/src/build/bin/albdf`**.
- Usage: `albdf <command> [options]`.
- Dispatch: `main.cpp` resolves `<command>` via `pdftool::PDFToolApplicationStorage::getApplicationByCommand()`;
  unknown commands fall back to the default application.
- Ships **~30 upstream commands** (render, fetch-text, redact, encrypt, optimize, unite,
  separate, diff, info-\*, ...) **plus our 4 additions** (below).

## Our 4 Custom CLI Tools

These are the project's differentiators. All live in `PdfTool/` and are registered like
the upstream tools:

| Command | Class files | Purpose |
|---------|-------------|---------|
| `add-text` | `pdftooladdtext.{h,cpp}` | Write text into a PDF, incl. **RTL** text (see RTL Pipeline). |
| `recognize-text` | `pdftoolrecognizetext.{h,cpp}` | Extract structured plain text; also defines the content-stream **object index space** used by `delete-object`. |
| `delete-object` | `pdftooldeleteobject.{h,cpp}` | Delete an object by index (0-based, from `recognize-text`). |
| `search-text` | `pdftoolsearchtext.{h,cpp}` | Logical-query text search with normalization + RTL visual inversion. |

Shared option flags (`DeleteObject`, `AddText`, `SearchText`) live in
`pdftoolabstractapplication.{h,cpp}`.

## The RTL Pipeline

Our **differentiating capability**: Arabic / Persian / Hebrew **write** and **search**.
The engine files are **our additions** in the core library:

| File | Role |
|------|------|
| `Pdf4QtLibCore/sources/pdfrtltextengine.{h,cpp}` | RTL layout: **FriBidi** runs + **HarfBuzz** shaping. |
| `Pdf4QtLibCore/sources/pdfrtltextnormalizer.{h,cpp}` | Normalization + **FriBidi visual inversion** for search. |
| `Pdf4QtLibCore/sources/pdftextsearchengine.{h,cpp}` | Logical-query search with normalization. |

Write path (`add-text --rtl`): FriBidi runs → HarfBuzz shaping → embedded TrueType via
**Type0 / Identity-H** with **per-instance codes**, `/CIDToGIDMap`, **ToUnicode**, and
`/ActualText`. See `Pdf4QtLibCore/AGENT.md` → "Documented RTL Quirks".

## Test Structure

- `UnitTests/` contains the test targets and runner (`tst_*.cpp`), driven by CTest.
- Key tests: `tst_addtexttest.cpp`, `tst_rtladdtexttest.cpp`, `tst_searchtexttest.cpp`,
  `tst_recognizetext.cpp`, `tst_deleteobjecttest.cpp`, `tst_formsignaturetest.cpp`,
  `tst_fontencodingtest.cpp`, `tst_goldentest.cpp`, `tst_imageoptimizertest.cpp`,
  `tst_lexicalanalyzertest.cpp`.
- `UnitTests/CMakeLists.txt` registers each `tst_*.cpp` as its own CTest target (10 test binaries + `SmokeCli` = 11 total).
- Test **data** (fixtures, fonts, golden expected files, scripts) lives in `tests/`.

## Exit-Code Contract

| Code | Meaning |
|------|---------|
| `0` | Success. |
| `7` | **Invalid arguments** (bad CLI usage). |

Always return `0` on success and `7` on invalid-argument errors. Do not invent new codes
without updating the contract in AGENTS.md.

## Determinism Rules

- **No timestamps** in output, metadata, or generated objects.
- **No random IDs** — use stable, deterministic IDs for objects and content.
- **No unstable ordering** — sort/serialize deterministically.
- Generated output must be **byte-stable** across runs (golden tests enforce this).

## Common Pitfalls

- **Editing the wrong directory:** there is no `src/cli/` or `src/core/` (removed) and no
  `src/albdf/`. Real code is in `Pdf4QtLibCore/` (library) + `PdfTool/` (CLI).
- **New source not in CMakeLists.txt:** files aren't globbed — forgetting to register a new
  `.cpp`/`.h` causes silent missing symbols or link failures.
- **Forgetting `QT_QPA_PLATFORM=offscreen`:** tests that touch Qt GUI/rendering fail or hang
  headlessly without it.
- **Non-deterministic output:** a timestamp or random ID slips in and breaks golden tests / CI.
- **Wrong exit code:** return `7` for invalid args, not a generic error code.
- **Qt module scope:** only Core/Gui/Xml/Svg/Test are available — any Widgets/QML include
  will not build.
