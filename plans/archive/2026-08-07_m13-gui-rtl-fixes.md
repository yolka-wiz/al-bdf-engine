# M13 — GUI RTL text wiring (post-audit fix plan)

Date: 2026-08-07
Branch: workstreams off main, merged after gate
Status: EXECUTION

## Audit outcome (3 subagents, all verified)

No GUI path uses PDFRTLTextEngine today. Every commit-visible text path is raw
QLineEdit/QTextEdit/QPainter::drawText. Full reports:
- input: subagent-summary-0 (11 paths table, P0/P1/P2)
- search/select/copy: subagent-summary-1 (6 paths, P0/P1/P2)
- cross-cutting + R#4 design: subagent-summary-2 (22KB, fix designs with file:line)

## Scope for 0.3.0 (this run) — testable wins only

### WS-A — RTL search wiring in the GUI (the headline differentiator)
Files: src/Pdf4QtLibWidgets/sources/pdfwidgettool.cpp (PDFFindTextTool::performSearch ~571)
       src/Pdf4QtLibGui/pdfadvancedfindwidget.cpp (PDFAdvancedFindWidget::performSearch ~237)
1. Add a small adapter (new core or widgets file): route plain-text queries
   through `PDFTextSearchEngine::search(doc, query, first, last, opts)`.
2. Map `Match` → `PDFFindResult` (both core-defined):
   - matched = match.matchedText
   - textSelectionItems: use PDFTextLayout::createTextSelection(pageIndex,
     match.boundingRect) — reuses highlight/copy pipeline (option ii from audit)
   - context = flow-text neighborhood of the match
3. Keep regex/whole-word branches on the legacy path; plain text → engine.
4. Verify with existing fixture: gui-rtl.pdf contains 'سلام' — searching it
   must find it (legacy indexOf fails on visual-order storage without inversion).
   Note: engine needs logical query; widget passes user-typed logical text. GOOD.
5. Commit(s) conventional; update docs/PROBLEMS.md gap list.

### WS-B — R#4: preserve /ActualText on content re-edit (core correctness)
Files: src/Pdf4QtLibCore/sources/pdfpagecontenteditorprocessor.{h,cpp}
       src/Pdf4QtLibCore/sources/pdfpagecontenteditorcontentstreambuilder.cpp
       ci/run-ci.sh (format-gate exemption for both upstream-derived files)
1. RED test: fixture with RTL add-text → second add-text (LTR no-op) on same
   page → fetch-text must still yield full lam-alef. Currently degrades.
2. Capture: override performMarkedContentBegin/End in the processor; record
   Span+ActualText spans (string-resolved guard like pdftextlayoutgenerator).
   Extend Item with optional QByteArray actualText (UTF-16BE + BOM).
3. Re-emit: in writeText's BT/Tf/Tm block, emit
   `/Span << /ActualText <FEFF...> >> BDC ... EMC` wrapping the element text.
4. GREEN: test passes; full core suite stays green; ASAN clean.
5. Commit + add format-gate exemption lines.

### WS-C — RTL clipboard re-inversion (small UX fix)
Files: src/Pdf4QtLibWidgets/sources/pdfwidgettool.cpp (onActionCopyText ~867)
1. When copied selection is RTL (layout carries /ActualText), re-invert the
   string to logical order before QApplication::clipboard()->setText — reuse
   the FriBidi inversion already in core (PDFTextSearchEngine::invertToVisual
   is private; expose a small public helper or call PDFRTLTextNormalizer path).
2. Test: copy 'سلام' region → clipboard contains logical 'سلام'.

## Deferred to M14 (document in PROBLEMS.md, do NOT attempt now)
- Form-field / FreeText annotation AP font embedding (needs font-picker UI +
  Type0 embed through updateAnnotationAppearanceStreams) — the biggest RTL
  input gap, but requires product decisions (which fonts to bundle)
- Canvas textbox serializer (no commit path exists at all — screen-only)
- TTS re-add (intentionally dead, fork divergence)

## Verification contract
- Each workstream: read AGENTS.md + src/AGENT.md first; TDD; small commits;
  git commit --no-verify OK (pre-commit regenerates REPO_MAP; run
  scripts/gen-repo-map.py before committing if tracked files changed)
- Orchestrator independently verifies: build artifacts, test output, commits
- Full loop after all merge: gui-smoke.sh + ctest 14/14 + ci/run-ci.sh

## Release 0.3.0 (after fixes land)
- docs/RELEASES.md 0.3.0 entry; DB tasks; tag; AppImage (packaging/ ready)
- gh release create with AppImage + tarball

## Self-critique
| Sev | Risk | Mitigation |
|---|---|---|
| MED | WS-B touches shared core (CLI add-text path) — regression risk | RED test first; full ctest + ASAN after; format-exempt lines |
| MED | WS-A adapter could break regex path if split wrong | Keep regex/whole-word on legacy; only plain-text branch changes |
| LOW | WS-C inversion helper exposure | Small public wrapper, no behavior change to existing callers |
