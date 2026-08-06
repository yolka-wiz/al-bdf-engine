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
Every `add_executable` in `CMakeLists.txt` follows the same template:
- `add_executable(UnitTestsX tst_x.cpp)`
- `target_link_libraries(UnitTestsX PRIVATE Pdf4QtLibCore Qt6::Core Qt6::Gui Qt6::Test)`
- `set_target_properties(UnitTestsX PROPERTIES WIN32_EXECUTABLE OFF MACOSX_BUNDLE OFF ...)`
- `add_test(NAME UnitTestsX COMMAND "${CMAKE_BINARY_DIR}/${PDF4QT_INSTALL_BIN_DIR}/UnitTestsX")`
- Integration tests that shell out to the CLI add
  `add_dependencies(UnitTestsX albdf)` so the binary is built first.
- Tests that need a headless render add
  `set_tests_properties(UnitTestsX PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")`
  (see `UnitTestsGolden`).

## 3. Add a new test (step by step)
1. **Create `src/UnitTests/tst_mything.cpp`.** MIT header; a `QObject` subclass
   with a `Q_OBJECT` macro and `private slots:` (each slot = one test function).
   Add a file-top comment explaining what the suite verifies.
2. **Wire it into `CMakeLists.txt`** with the template in §2. Give the target a
   unique name (e.g. `UnitTestsMyThing`). If it runs `albdf`, add
   `add_dependencies(UnitTestsMyThing albdf)` and set the offscreen ENV.
3. **Add compile definitions** for any fixture/font paths the test needs (§5).
4. **End the file with `QTEST_GUILESS_MAIN(MyThingClass)`** followed by
   `#include "tst_mything.moc"`.
5. **Run:** `cmake -S src -B src/build && cmake --build src/build -j$(nproc)`
   then `cd src/build && QT_QPA_PLATFORM=offscreen ctest --output-on-failure -R MyThing`.
   Full gate: `ci/run-ci.sh`.

## 4. The `runTool` QProcess helper pattern
Integration tests use a shared anonymous-namespace helper (copy from
`tst_deleteobjecttest.cpp`):
```cpp
struct ToolResult { int exitCode = -1; QByteArray stdoutData; QByteArray stderrData; };
ToolResult runTool(const QString& toolPath, const QStringList& args, const QString& workingDir)
{
    ToolResult result;
    QProcess process;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(workingDir);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(toolPath, args);
    if (!process.waitForStarted() || !process.waitForFinished(180000)) return result;
    result.exitCode = process.exitCode();
    result.stdoutData = process.readAllStandardOutput();
    result.stderrData = process.readAllStandardError();
    return result;
}
```
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
