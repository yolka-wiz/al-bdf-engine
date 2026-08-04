# AGENT.md — PdfTool CLI (`src/PdfTool/`)

Onboarding guide for AI agents adding or modifying CLI commands in the `pdfedit`
fork. Read [`AGENTS.md`](../../AGENTS.md) (binding contract) and
[`docs/coding-standard.md`](../../docs/coding-standard.md) first. This file covers
**how the CLI works and how to add a new command.**

## Table of contents
- [1. What this directory is](#1-what-this-directory-is)
- [2. How a command is wired up](#2-how-a-command-is-wired-up)
- [3. Add a new command (step by step)](#3-add-a-new-command-step-by-step)
- [4. Output formatter contract](#4-output-formatter-contract)
- [5. Exit-code contract](#5-exit-code-contract)
- [6. Pitfalls](#6-pitfalls)

## 1. What this directory is
- Builds the `PdfTool` binary (output: `src/build/bin/PdfTool`), declared in
  `src/PdfTool/CMakeLists.txt` as `add_executable(PdfTool ...)`, linked against
  `Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Xml`.
- Each **command** (e.g. `add-text`, `search-text`) is one `pdftool*` pair of
  files. The fork's custom commands live here: `pdftooladdtext.{h,cpp}`,
  `pdftoolrecognizetext.{h,cpp}`, `pdftooldeleteobject.{h,cpp}`, and
  `pdftoolsearchtext.{h,cpp}`.
- `pdftoolabstractapplication.{h,cpp}` is the shared base class, option struct,
  exit-code enum, and static application registry.
- `pdfoutputformatter.{h,cpp}` formats console output (text/xml/html).

## 2. How a command is wired up
Every command is a `pdftool::PDFToolAbstractApplication` subclass that overrides
three virtuals:
- `getStandardString(StandardString)` → returns `Command` (the CLI name, e.g.
  `"search-text"`), `Name`, and `Description` (user-facing, translatable via
  `PDFToolTranslationContext::tr(...)`).
- `getOptionsFlags()` → OR-ed `Option` bitmask declaring which option groups the
  command accepts (e.g. `ConsoleFormat | OpenDocument | SearchText`).
- `execute(const PDFToolOptions&)` → the real work; returns an exit code.

Registration is **static, in the `.cpp`, at namespace scope**:
```cpp
static PDFToolSearchText s_searchTextApplication;
```
The static constructor calls `PDFToolApplicationStorage::registerApplication(this)`,
which makes the command discoverable by its `Command` string. Adding the `static`
line is what registers the command — nothing else wires discovery.

Options/state live in the shared `struct PDFToolOptions` in
`pdftoolabstractapplication.h` (fields like `addTextX`, `searchTextQuery`), and
each option group gets a bit in the `enum Option`
(`DeleteObject = 0x04000000`, `AddText = 0x08000000`, `SearchText = 0x10000000`).
CLI flag → `PDFToolOptions` mapping is implemented in
`pdftoolabstractapplication.cpp`.

## 3. Add a new command (step by step)
1. **Create `pdftoolnewcmd.h` + `pdftoolnewcmd.cpp`.** Copy the shape of
   `pdftoolsearchtext.{h,cpp}`: class deriving from `PDFToolAbstractApplication`,
   MIT header, `#pragma once`, Doxygen on the class.
2. **Implement the three virtuals.** `Command` returns `"new-cmd"`. Set
   `getOptionsFlags()` to the relevant option bits.
3. **Static-register it** in the `.cpp`:
   `static PDFToolNewCmd s_newCmdApplication;`
4. **Add flags/options.** If the command needs new flags, add an `Option` bit and
   `PDFToolOptions` fields, then map the `QCommandLineParser` flag → option in
   `pdftoolabstractapplication.cpp` (mirror the existing `SearchText`/`AddText`
   handling).
5. **Register the sources** in `src/PdfTool/CMakeLists.txt` — add
   `pdftoolnewcmd.cpp` and `pdftoolnewcmd.h` to the `add_executable(PdfTool ...)`
   list (keep it sorted with the other `pdftool*` entries).
6. **Add a test.** Add a ctest target in `src/UnitTests/CMakeLists.txt` (see
   [`../UnitTests/AGENT.md`](../UnitTests/AGENT.md)) and a smoke.sh entry if it
   changes core invariants.
7. **Verify:** `cmake --build src/build -j$(nproc)`, then
   `QT_QPA_PLATFORM=offscreen ./src/build/bin/PdfTool new-cmd --help` and
   `cd src/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure`.
   Run `ci/run-ci.sh` (or at minimum the clang-format gate) before finishing.

## 4. Output formatter contract
- Use `PDFOutputFormatter` (not raw `qDebug`/`printf`) for structured output so
  `--output-format text|xml|html` works. Obtain it from the options:
  `PDFOutputFormatter formatter(options.outputStyle);` then emit structure with
  `beginDocument/endDocument`, tables via `beginTable`/`beginTableHeaderRow`/
  `writeTableHeaderColumn`/`beginTableRow`/`writeTableColumn`, and plain lines
  with `writeText(name, description, reference)`.
- **There is NO `writeRect`.** Don't invent one. Output rectangles as table
  columns or text.
- Flush with `PDFConsole::writeText(formatter.getString(), options.outputCodec)`.
  Use `PDFConsole::writeError(...)` for diagnostics on stderr.
- Output must be **byte-deterministic**: no timestamps, no random IDs, no
  `QDateTime::currentDateTime()` in command output (tests hash it).

## 5. Exit-code contract
- Return `ExitSuccess` (0) on success, `ErrorInvalidArguments` (7) on bad input.
- Read documents via the inherited `readDocument(options, document, &sourceData,
  authorizeOwnerOnly)`; on failure return `ErrorDocumentReading` (4).
- Full enum in `pdftoolabstractapplication.h`: `ExitSuccess`(0), `ExitFailure`(1),
  `ErrorUnknown`(2), `ErrorNoDocumentSpecified`(3), `ErrorDocumentReading`(4),
  `ErrorDocumentWriting`(5), `ErrorCertificateReading`(6), `ErrorInvalidArguments`(7), …
- The binary itself returns whatever `execute()` returns; tests assert on it.

## 6. Pitfalls
- **Forgetting the `static` registration line** → command silently missing from
  `--help` and `getApplicationByCommand`. Always add it.
- **Not adding the new `pdftool*.cpp` to CMakeLists.txt** → link/undefined-symbol
  failures at build time.
- **Missing `getOptionsFlags()` bits** → the command's declared flags won't parse.
- **Using a non-existent formatter method** (e.g. `writeRect`) → compile error;
  the formatter only has `writeText`/`writeTableColumn`/etc.
- **Non-deterministic output** → breaks golden/smoke tests. Keep output
  deterministic.
- **New command without a ctest/smoke check** → CI gate won't exercise it.
- **clang-format**: every authored file must pass
  `clang-format --dry-run --Werror`; run the formatter on your new files.
