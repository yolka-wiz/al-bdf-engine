# ADR-0001: Fork PDF4QT (MIT) as the base library + CLI

**Status:** proposed (2026-08-04)

## Context

We need a headless PDF editing library + CLI for Linux with RTL write/search and object
deletion. Research (`pdf-editor-research/report.md`) showed greenfield permissive-stack cost
of 50–80 person-months. On inspection, PDF4QT (MIT, relicensed 2025-04-27) already provides:

- `Pdf4QtLibCore` — headless PDF library (QtCore/Gui/Xml/Svg, no widgets): rendering, text
  flow extraction with per-char boxes (`PDFDocumentTextFlow`), text-flow editing
  (`PDFDocumentTextFlowEditor::removeItem/setEditedText`), content-stream editing
  (`PDFPageContentEditorProcessor`), annotations, forms, redaction, signatures, encryption,
  image optimization, color management.
- `PdfTool` — CLI with ~30 commands (fetch-text, fetch-images, render, unite/separate, redact,
  encrypt/decrypt, optimize, diff, verify-signatures, info-*).
- `UnitTests/` — existing test infra.

Verified in source (cloned 2026-08-03, `/tmp/pdf4qt`): every file carries the MIT header;
core links Qt6::Core/Gui/Xml/Svg + LCMS2/OpenSSL/ZLIB/FreeType/openjp2/JPEG/TBB/blend2d —
all permissive-compatible.

## Decision

Fork PDF4QT into `src/` (core = `Pdf4QtLibCore`, CLI = `PdfTool`), strip the GUI apps
(Viewer/Editor/PageMaster/Diff/LaunchPad/plugins), keep `UnitTests`, and extend from there.
Add upstream as a git remote for cherry-picking. Do NOT reimplement PDF parsing/rendering.

## Consequences

- Massive cost reduction: fork path ≈ 6–10 person-months vs 50–80 greenfield.
- We inherit PDF4QT's architecture, naming, and bugs. Fix forward.
- Single-maintainer upstream: we own the fork; cherry-pick selectively.
- RTL is still 100% greenfield (PDF4QT has no HarfBuzz/FriBidi) — see ADR-0003.
