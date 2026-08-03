# 001 — PDF4QT deep dive (CLI extension, text-flow write-back, fonts, determinism)

- **Date:** 2026-08-04
- **Author:** research subagent (Rosetta brief 001, task 0)
- **Status:** verified against source ([V]) unless flagged [unverified]
- **Source tree:** `vendor-upstream-pdf4qt/` (PDF4QT 1.6.0.0)

---

## 1. Adding a new CLI command to PdfTool

**Base class**: `pdftool::PDFToolAbstractApplication` — `PdfTool/pdftoolabstractapplication.h:209`.
Pure virtuals [V]:
- `h:271` `virtual QString getStandardString(StandardString) const = 0;` — `StandardString` enum (h:233-238): `Command, Name, Description`.
- `h:272` `virtual int execute(const PDFToolOptions& options) = 0;`
- `h:273` `virtual Options getOptionsFlags() const = 0;`
Non-virtual: `h:275` `initializeCommandLineParser(QCommandLineParser*) const` (adds options per flag, cpp:151+); `h:276` `PDFToolOptions getOptions(QCommandLineParser*) const` (reads values, cpp:440+). `Option` flag enum at h:240-268.

**Registration** [V]: constructor registers: `pdftoolabstractapplication.cpp:146-149` → `PDFToolApplicationStorage::registerApplication(this, isDefault)`. Singleton storage (h:302-331); dispatch `getApplicationByCommand` matches `getStandardString(Command) == command` (cpp:1326-1337). Per-command static instance: `static PDFToolRedact s_redactApplication;` (`pdftoolredact.cpp:32`). Entry: `PdfTool/main.cpp:47-63` — `QGuiApplication` (main.cpp:31, note Qt6::Gui dep), command dispatch, `application->execute(...)`.

**Minimal "delete-object" skeleton** — copy `PDFToolRemoveExternalLinks` (`pdftoolremoveexternallinks.cpp:100-171`):
```cpp
class PDFToolDeleteObject : public PDFToolAbstractApplication { ... }; // 1) getStandardString → Command: "delete-object", 2) getOptionsFlags() { return ConsoleFormat | OpenDocument; }, 3) execute()
static PDFToolDeleteObject s_app;
int execute(...) { // readDocument(options, doc, &src, false); PDFDocumentModifier modifier(&doc); builder->...; modifier.finalize(); PDFDocumentWriter writer(nullptr); writer.write(path, doc, true); }
```
Then add `pdftooldeleteobject.cpp` to `PdfTool/CMakeLists.txt:23-58` (links `Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Xml`, line 58). Object removal APIs: `builder->removeAnnotation(pageRef, annotRef)` (removeexternallinks.cpp:86), `PDFDocumentBuilder` in `pdfdocumentbuilder.h` (e.g. `createSignatureDictionary` at :1305).

## 2. Text-flow write-back path — **key finding: it does NOT write back**
`PDFDocumentTextFlowEditor` (`Pdf4QtLibCore/sources/pdfdocumenttextflow.h:158`): `removeItem(size_t)` h:179 → cpp:836-839 sets `Removed` flag (enum h:184-191). `createEditedTextFlow()` h:288, cpp:999+ returns a `PDFDocumentTextFlow` of active items. **No core or plugin code writes that flow back into content streams** — the only consumer is the AudioBook plugin, which turns it into TTS ("createAudioBook", `audiobookplugin.cpp:459-460`). If you want remove-items-to-PDF, you must build that bridge yourself (mark removed → regenerate page content streams).

The actual content-stream write-back lives in the **GUI Editor plugin**, not the text-flow editor:
- Parse: `PDFPageContentEditorProcessor` (`pdfpagecontenteditorprocessor.h:234`, core) parses page content into `PDFEditedPageContent` (h:196) with typed elements Path/Text/Image (h:46-51, Text element h:146-194).
- GUI scene manip: `editorplugin.cpp:855-893` (QGraphicsScene).
- Serialize: `PDFPageContentEditorContentStreamBuilder` (core, `pdfpagecontenteditorcontentstreambuilder.cpp:300`), `writeEditedElement` (cpp:538), `writeTextWithFallback` (cpp:1147-1217).
- Write-back: `editorplugin.cpp:205-389` `EditorPlugin::updatePageContent` — FlateDecode-compress (cpp:343), build `Resources` dict (Font/XObject/ExtGState, cpp:351-378), merge into page object via `PDFObjectManipulator::merge` (cpp:386) + `builder->setObject(pageRef, ...)` (cpp:387).
**GUI-only deps in this path**: the *orchestration* is GUI-bound (`editorplugin.cpp:325` uses `QMessageBox`; QGraphicsScene). The *building blocks* (processor, builder) are core and depend only on Qt Core/Gui + core classes — usable headless. `PDFDocumentTextFlowEditorModel` is `QAbstractTableModel`-based (pdfdocumenttextfloweditormodel.h:35) — Qt model, not widget-level.

Core-only content rewrite exists in redaction: `PDFRedact::perform` (`pdfredact.cpp:48-117`) rebuilds every page via `PDFPrecompiledPage::redact` + `PDFPageContentStreamBuilder`.

## 3. Font embedding for add-text — **no TTF embedding path exists**
- Fallback path builds **Type 3 fonts from system-font glyph outlines, deliberately without embedding font programs**: `pdfeditorfallbackfont.h:39-44` ("Glyph outlines are extracted from a similar system font, so no font program embedding is needed"). `PDFEditorFallbackFontManager::encode(codepoints, similarToFont, PDFDictionary& fontDictionary, errorCb)` h:66-69; glyphs pulled from `QRawFont::fromFont(QFont)` matched to the descriptor family/weight/stretch (cpp:85-126); Type 3 dict built by `buildFontDictionaryObject` h:122 (per-glyph charprocs + ToUnicode CMap), inserted into the page font dict at cpp:79.
- Writer: `writeTextWithFallback` splits runs on `m_textFont->encodeCharacter()` (cpp:1171), hex-string `Tj` with `Tf` font switches (cpp:1207-1214); missing font falls back to `PDF4QT_DefFnt` Helvetica Type1 (cpp:1345-1366).
- `PDFTextLayoutGenerator` is extraction-only (pdftextlayoutgenerator.h:48-54).
- `pdffont.h:203-239` (FontFile/FontFile2/FontFile3, `isEmbedded()`) is for *reading/realizing* embedded fonts from existing PDFs, not writing. So arbitrary TTF embedding requires new code: font dict + FontDescriptor + FontFile2 stream into page Resources via `PDFDocumentBuilder`.

## 4. Dependencies / standalone build
- `Pdf4QtLibCore/CMakeLists.txt:23-210`: target `Pdf4QtLibCore` SHARED; links `Qt6::Core Gui Xml Svg` (PRIVATE, :183), `LCMS2` (:184), `OpenSSL::SSL OpenSSL::Crypto` (:185), `ZLIB` (:186), `Freetype` (:187), `openjp2` (:188), `JPEG` (:189), `TBB::tbb PUBLIC` if LINUX_GCC (:191-193), `blend2d::blend2d` PRIVATE (:202).
- `vcpkg.json`: `tbb, openssl, lcms, zlib, openjpeg, freetype, libjpeg-turbo, libpng, blend2d`.
- Top CMakeLists: `find_package(Qt6 COMPONENTS Core Gui Svg Xml)` when `PDF4QT_BUILD_ONLY_CORE_LIBRARY` (option :49; only-core mode skips PdfTool, :179-193), OpenSSL/ZLIB/Freetype/OpenJPEG/JPEG/PNG/blend2d REQUIRED (:77-83), lcms2 via vcpkg or `find_library` (:85-90), TBB REQUIRED on Linux GCC (:114-116).
- **System Qt6 + system libs: yes** — the non-vcpkg branch exists (:88-90). **blend2d is NOT optional at build time** (`find_package(... REQUIRED)` :83, hard link :202), but is functionally optional at runtime: `PDFRasterizer::render` has a QPainter software fallback (`pdfrenderer.cpp:273-279`), while Blend2D engine (default, :226-229) uses `PDFBLPaintDevice` (:261; class in `pdfblpainter.h:35`). `pdfutils.cpp:329` also hard-references `PDFBLPaintDevice::getVersion()` for `info`. To drop blend2d you must patch those 3 refs + CMake.

## 5. Determinism
- Writer (`pdfdocumentwriter.cpp:242+`): classic xref table, objects written in storage order (:275-310), deterministic serializer — reals fixed 5 digits (:88-95), strings hex-escaped when needed (:97-116), padded offsets (:335-340). Header writes constant `% PDF producer: <PDF_LIBRARY_NAME>` (:258-266). Trailer copies **existing** `Size/Root/Encrypt/Info/ID` (:346-357) — the CLI editing path (redact/remove-links etc.) preserves the source `/ID`, no timestamp/random added [V].
- **Non-deterministic paths**: encryption IVs use `QRandomGenerator::securelySeeded()` (`pdfsecurityhandler.cpp:999`); GUI signing injects `QDateTime::currentDateTime()`/`currentMSecsSinceEpoch()` (`signatureplugin.cpp:347,355,362`) — you'd pass your own time in a headless signer.
- **No deterministic-ID equivalent found**; new-blank-doc `/ID` construction in `PDFDocumentBuilder::createTrailerDictionary` (pdfdocumentbuilder.cpp:1056) not fully traced — flagged [unverified].

## 6. Redact + signature headless
- **Redact: works headless** [V] — `pdftoolredact.cpp:55-94`: readDocument → `PDFRedact::perform` → `PDFDocumentWriter::write`. Core-only path (pdfredact.cpp:48-117); note it drives `PDFRenderer` → blend2d rasterizer, so blend2d must link (works with QImage, no window).
- **Signing: NO CLI command exists** [V] — only `verify-signatures` (pdftoolverifysignatures.cpp:34+, uses `PDFSignatureHandler::verifySignatures` pdfsignaturehandler.h:341). Signing lives in the GUI SignaturePlugin (`signatureplugin.cpp:362-364`: `createSignatureDictionary` + `createFormFieldSignature` + `createAcroForm`; cert store is GUI-managed `PDFCertificateManager` static, :331). The core building blocks are headless-usable — `PDFSignatureFactory::sign` (`pdfcertificatemanager.h:64-68`) and `PDFDocumentBuilder::createSignatureDictionary` (`pdfdocumentbuilder.h:1305`) — but you must write the CLI command. Offsets/`ByteRange` handling (`offsetMark`, :357) lives in the GUI plugin code too.
