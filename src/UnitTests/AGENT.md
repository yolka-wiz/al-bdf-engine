# AGENT.md — Unit / Integration tests (`src/UnitTests/`)

Onboarding guide for agents adding or modifying tests in the `albdf` fork.
Read [`AGENTS.md`](../../AGENTS.md) and
[`docs/coding-standard.md`](../../docs/coding-standard.md) first. This file covers
**how tests are structured and how to add a new one.**

## Table of contents
- [1. What this directory is](#1-what-this-directory-is)
- [2. Test structure & ctest wiring](#2-test-structure--ctest-wiring)
- [3. Add a new test (step by step)](#3-add-a-new-test-step-by-step)
- [4. The `runTool` QProcess helper pattern](#4-the-runtool-qprocess-helper-pattern)
- [5. Compile definitions (fixtures & fonts)](#5-compile-definitions-fixtures--fonts)
- [6. Determinism rules](#6-determinism-rules)
- [7. Pitfalls](#7-pitfalls)

## 1. What this directory is
- Hosts all Qt Test suites for the fork. Each suite is one `tst_*.cpp` file:
  `tst_addtexttest`, `tst_deleteobjecttest`, `tst_fontencodingtest`,
  `tst_formsignaturetest`, `tst_goldentest`, `tst_imageoptimizertest`,
  `tst_lexicalanalyzertest`, `tst_recognizetext`, `tst_rtladdtexttest`,
  `tst_searchtexttest`.
- Registration lives **entirely in `src/UnitTests/CMakeLists.txt`** — there is no
  central test aggregator. Each `tst_*.cpp` becomes its **own `add_executable`**
  and its **own `add_test`** ctest target.
- **11 ctest targets total**: `UnitTests`, `UnitTestsImageOptimizer`,
  `UnitTestsFontEncoding`, `UnitTestsRecognizeText`, `UnitTestsDeleteObject`,
  `UnitTestsAddText`, `UnitTestsRtlAddText`, `UnitTestsSearchText`,
  `UnitTestsGolden`, `UnitTestsFormSignature`, and `SmokeCli` (the last runs
  `src/tests/smoke.sh`).
- Many suites are **integration tests**: they launch the `albdf` binary via
  `QProcess` and assert on its stdout/exit code.

## 2. Test structure & ctest wiring
Every suite is registered with the `add_albdf_test()` helper defined at the top
of `CMakeLists.txt`. It expands to the executable/link/target-properties/
`add_test` boilerplate that used to be copy-pasted per target:

```cmake
# add_albdf_test(<target> <source> [HEADLESS] [DEFINITIONS <define>...])
add_albdf_test(UnitTestsMyThing tst_mything.cpp HEADLESS
    DEFINITIONS TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf")
add_dependencies(UnitTestsMyThing albdf)   # only when the suite launches the CLI
```

- `HEADLESS` adds
  `set_tests_properties(... ENVIRONMENT "QT_QPA_PLATFORM=offscreen")`
  (tests that render or otherwise touch QtGui, e.g. `UnitTestsGolden`).
- `DEFINITIONS <define>...` forwards compile definitions (`TEST_*`,
  `ALBDF_TESTS_DIR`; see §5).
- Integration tests that shell out to the CLI still add
  `add_dependencies(<target> albdf)` themselves so the binary is built first.

## 3. Add a new test (step by step)
1. **Create `src/UnitTests/tst_mything.cpp`.** MIT header; a `QObject` subclass
   with a `Q_OBJECT` macro and `private slots:` (each slot = one test function).
   Add a file-top comment explaining what the suite verifies.
2. **Register it** with `add_albdf_test(UnitTestsMyThing tst_mything.cpp)` (§2).
   Give the target a unique name (e.g. `UnitTestsMyThing`). If it runs `albdf`,
   add `add_dependencies(UnitTestsMyThing albdf)`; add `HEADLESS` for a
   render/QtGui suite.
3. **Add compile definitions** for any fixture/font paths the test needs by
   passing `DEFINITIONS ...` to `add_albdf_test()` (§5).
4. **End the file with `QTEST_GUILESS_MAIN(MyThingClass)`** followed by
   `#include "tst_mything.moc"`.
5. **Run:** `cmake -S src -B src/build && cmake --build src/build -j$(nproc)`
   then `cd src/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -R MyThing`.
   Full gate: `ci/run-ci.sh`.

## 4. The `runTool` QProcess helper pattern
Integration tests use the shared `runAlbdfTool()` helper in
`testsupport/tst_toolrunner.h` — never copy a local `runTool` (that duplication
is what roadmap R2.1 removed):
```cpp
#include "testsupport/tst_toolrunner.h"

using testsupport::runAlbdfTool;
using testsupport::ToolResult;

ToolResult result = runAlbdfTool(toolPath, {QStringLiteral("info"), inputPath}, tmpDir.path());
QCOMPARE(result.exitCode, 0);
QVERIFY2(result.stdoutData.contains("Page count"), "info must report a page count");
```
- `ToolResult` carries `exitCode` (`-100` = could not start, `-101` = timed out),
  `exitStatus`, `finishedInTime`, and the `QByteArray` `stdoutData`/`stderrData`.
- The helper always runs the child headless (`QT_QPA_PLATFORM=offscreen`) with
  separate channels, kills it on timeout, and uses a 60000 ms watchdog by
  default; pass a fourth argument to override the timeout.
- Resolve the binary via `QCoreApplication::applicationDirPath() + "/albdf"`.
  This is why `QTEST_GUILESS_MAIN` is used — it instantiates a `QCoreApplication`.
- Do work in a `QTemporaryDir` (never the source tree). Assert with `QCOMPARE`
  (exit code) and `QVERIFY2(cond, "message")` (stdout substrings / file existence).
- Hash rendered/output files with `QCryptographicHash::Sha256` to compare goldens.

## 5. Compile definitions (fixtures & fonts)
Paths are injected at configure time so tests don't hardcode repo locations:
- `TEST_BASELINE_PDF` → the `test-baseline.pdf` **copied into the binary dir**
  via `configure_file(.../docs/research/baseline-out/test-baseline.pdf ... COPYONLY)`
  in `CMakeLists.txt`. Used by RecognizeText/DeleteObject/AddText.
- `TEST_BLANK_PDF` → `src/tests/fixtures/blank.pdf` (source dir).
- `TEST_FONT_PERSIAN` / `TEST_FONT_ARABIC` / `TEST_FONT_HEBREW` → the bundled OFL
  fonts in `src/tests/fonts/` (`Vazirmatn`, `NotoNaskhArabic`, `NotoSansHebrew`).
- `ALBDF_TESTS_DIR` → `src/tests` root (used by `UnitTestsGolden`).

Add them like this:
```cmake
target_compile_definitions(UnitTestsMyThing PRIVATE
    TEST_BLANK_PDF="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fixtures/blank.pdf"
    TEST_FONT_PERSIAN="${CMAKE_CURRENT_SOURCE_DIR}/../tests/fonts/Vazirmatn-Regular.ttf")
```
Reference in C++ as `QString::fromUtf8(TEST_BLANK_PDF)`.

## 6. Determinism rules
- Tests run headless: **always** set `QT_QPA_PLATFORM=offscreen` (in the suite's
  `runTool` env and/or the ctest `ENVIRONMENT` property).
- Output documents must be byte-deterministic (no timestamps/random IDs), or
  hash-based assertions are meaningless.
- Prefer `QCOMPARE`/`QVERIFY2` with explicit messages over bare `QVERIFY` so
  failures are diagnosable.
- Don't rely on host fonts — RTL/LTR tests must use the bundled OFL fonts.

## 7. Pitfalls
- **Missing `QTEST_GUILESS_MAIN`** → no `QCoreApplication`, and
  `applicationDirPath()` won't resolve → integration tests break.
- **Forgetting `#include "tst_x.moc"`** at the bottom → automoc doesn't see the
  `Q_OBJECT`, link fails. It must match the `.cpp` basename.
- **Hardcoding `albdf` paths or fixture paths** → breaks in CI; use
  `applicationDirPath()` and the compile definitions.
- **No `add_dependencies(... albdf)`** on a test that launches the CLI → ctest
  may run before `albdf` is built.
- **Writing into the source tree** instead of a `QTemporaryDir` → pollutes the
  repo and can trip the format/CI gate.
- **Not adding the ctest target** → your test never runs in CI.
- **clang-format**: authored `tst_*.cpp` files must pass
  `clang-format --dry-run --Werror`.
