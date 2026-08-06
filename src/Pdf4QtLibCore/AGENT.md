# AGENT.md — Pdf4QtLibCore (Core PDF Library)

Guide to `Pdf4QtLibCore/` — the fork of the MIT-licensed **PDF4QT** core, packaged as a
shared library. This is where the PDF document model, text flow, fonts, and our RTL
extensions live. Read [`../../AGENTS.md`](../../AGENTS.md) (binding contract)
and [`../../docs/coding-standard.md`](../../docs/coding-standard.md) first.
Top-level tree guide: [`../AGENT.md`](../AGENT.md).

## Table of Contents
- [What This Library Is](#what-this-library-is)
- [Directory Layout](#directory-layout)
- [Naming & Code Conventions](#naming--code-conventions)
- [Key Subsystems](#key-subsystems)
- [Our Additions (RTL + Search)](#our-additions-rtl--search)
- [CMakeLists.txt (registering sources)](#cmakeliststxt-registering-sources)
- [ABI / Export Macro](#abi--export-macro)
- [Determinism Rules](#determinism-rules)
- [Documented RTL Quirks](#documented-rtl-quirks)

## What This Library Is

- A shared library (`add_library(Pdf4QtLibCore SHARED ...)`) built from the PDF4QT core,
  MIT-licensed, forked and extended for **headless PDF editing**.
- No GUI/Qt Widgets/QML — Core/Gui/Xml/Svg/Test only.
- The CLI (`albdf`) and the tests both link against this library.

## Directory Layout

| Path | Contents |
|------|----------|
| `CMakeLists.txt` | Build script. **Every source is enumerated here** (no globbing). |
| `sources/` | The library code — **77 `.cpp` / 88 `.h`** files. |
| `aatl/` | `aatl` support dir. |
| `cmaps/` | Pre-built CMap resources (resource via `cmaps.qrc`). |
| `*.qrc` (`aatl.qrc`, `cmaps.qrc`, `fonts.qrc`) | Qt resource manifests. |
| `liberation-fonts-ttf/` | Bundled fonts used as fallback/embedded fonts. |

## Naming & Code Conventions

- **Classes:** `PDF*` prefix, e.g. `PDFDocument`, `PDFPage`, `PDFFont`, `PDFTextSearchEngine`.
- **Namespace:** `pdf` (e.g. `pdf::PDFDocument`). Some code refers to `pdf::` explicitly.
- **Members:** `m_` prefix (e.g. `m_objects`, `m_trailerDictionary`).
- **Accessors:** `getX()` / `setX()` style.
- Formatting: clang-format per `.clang-format`; see coding-standard.md.

## Key Subsystems

An agent will most often touch these:

| Class | Role |
|-------|------|
| `pdfdocument.{h,cpp}` | The document model: objects, trailer, security handler. |
| `pdfpage.{h,cpp}` | Page model + page geometry. |
| `pdfpagecontentprocessor.{h,cpp}` | Parses/processes content streams (the "recognize" side). |
| `pdfdocumenttextflow.{h,cpp}` | Text flow extraction/layout from a document. |
| `pdffont.{h,cpp}` | Font handling incl. **ToUnicode** mapping. |
| `pdfparser.{h,cpp}` / `pdfobject.{h,cpp}` | PDF object parsing & object model. |
| `pdfdocumentwriter.{h,cpp}` / `pdfdocumentbuilder.{h,cpp}` | Write-back and rebuild of documents. |
| `pdfdocumentmanipulator.{h,cpp}` | High-level document manipulations. |

## Our Additions (RTL + Search)

These files are **ours**, not upstream — the core differentiators:

| File | Role |
|------|------|
| `pdfrtltextengine.{h,cpp}` | RTL text **layout/write**: FriBidi runs + HarfBuzz shaping. |
| `pdfrtltextnormalizer.{h,cpp}` | Normalization + FriBidi visual inversion (for search/write). |
| `pdftextsearchengine.{h,cpp}` | Logical-query search with normalization + RTL support. |

When changing RTL/write behavior, these three files (plus the CLI `pdftooladdtext` /
`pdftoolsearchtext`) are the surface area.

## CMakeLists.txt (registering sources)

- The library lists each source explicitly, e.g.:
  ```cmake
  add_library(Pdf4QtLibCore SHARED
      sources/pdfglobal.h
      sources/pdfaction.cpp
      sources/pdfaction.h
      ...
  ```
- **A new `.cpp`/`.h` MUST be added here** or it won't be compiled/linked. This is the #1
  cause of "works locally, missing symbol in CI."

## ABI / Export Macro

- Public classes/functions must be annotated with **`PDF4QTLIBCORESHARED_EXPORT`** to be
  exported from the shared library.
- Check `pdfglobal.h` for the macro definition and where it's applied. New public API you
  expose to the CLI/tests needs this macro, or the symbol won't link outside the library.

## Determinism Rules

- **Byte-stable output** across runs — enforced by golden tests in `UnitTests/`.
- **No timestamps, no random IDs** in generated content, object IDs, or metadata.
- **Stable ordering** in any emitted object/array/stream — sort deterministically.
- Applies to the writer, RTL engine output, and any generated PDF structure.

## Documented RTL Quirks

These are documented behaviors of our RTL implementation — respect them:

- **ToUnicode is 2-byte-only.** ToUnicode CMaps support 2-byte codes only; do not emit
  1-byte or 3-byte entries there.
- **Marks share the base cluster.** Combining marks are assigned to the base character's
  cluster; do not give them their own independent clusters.
- **HarfBuzz ≥ 4 does NOT reverse RTL glyphs.** HB v4+ already emits RTL runs
  **leftmost-first (visual order)**; the RTL engine must **not** reverse them again.
  Re-applying a reversal (e.g. the fpdf2 #1802 pattern) *mirrors* the text — this was a
  real bug fixed in `23dc377` and is regression-tested. Do not "fix" the ordering.
- **Per-instance codes + `/CIDToGIDMap`.** Text is written with Type0/Identity-H embedding,
  per-instance character codes, and a `/CIDToGIDMap` mapping; changing this breaks the
  write-back contract.
- **HarfBuzz clusters are absolute**, with `item_offset = run.begin`. Cluster indices are
  relative to the start of the whole text, so an item's base offset is `run.begin`; do not
  re-encode clusters as run-local.

> When in doubt about RTL behavior, consult the three RTL files above and the RTL CLI tests
> (`UnitTests/tst_rtladdtexttest.cpp`, `UnitTests/tst_searchtexttest.cpp`) — they encode the
> expected contract.
