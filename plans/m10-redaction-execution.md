# Object-Level Redaction — Execution Plan (DB #29, M10 wave-2)

> Binding context: repo root AGENTS.md (read fully first), src/AGENT.md,
> src/PdfTool/AGENT.md, src/UnitTests/AGENT.md, docs/PROBLEMS.md.

## Goal

Make `albdf redact` a **verified, tested** first-class command and close DB #29
with real evidence. (The "wire upstream redact" part is already done — see below.)

## Reality check (orchestrator-verified, 2026-08-06)

- `src/PdfTool/pdftoolredact.{h,cpp}` exists (vendored upstream), is in
  `src/PdfTool/CMakeLists.txt`, self-registers via static instance, and the
  built binary responds: `albdf redact --help` works, command list includes
  `redact`. Upstream `pdf::PDFRedact` engine is linked.
- **What is missing:** any test coverage, any headless verification on a
  fixture, any DB evidence. The task as written in PLAN.md ("wire upstream
  redact") is stale — actual remaining work is verify + test + document.

## Scope

1. **Headless smoke (your first step, before writing tests):**
   ```bash
   export VCPKG_ROOT=/home/agent/vcpkg-cache/vcpkg
   cd /home/agent/workspace/wt-core-redaction/src
   # build first if needed (see commands below)
   QT_QPA_PLATFORM=offscreen build/bin/albdf redact \
     ../src/tests/fixtures/multipage.pdf /tmp/redacted.pdf --page 1
   ```
   Verify: exit 0, output exists, `fetch-text` on the output no longer returns
   the redacted region's text, document reopens cleanly. Record exact behavior.
   Explore flags (`--redact-*`) and figure out how to target a region (rect /
   page / selection) — check upstream `pdftoolredact.cpp` and `PDFRedact` API.
2. **TDD integration test** — add `tst_redacttest.cpp` (or extend an existing
   suite if cleaner — check how `tst_pageopstest.cpp` runs the CLI via
   `runTool`, reuse that pattern):
   - RED first: assert redaction removes text from `fetch-text` output; commit
     `test(cli): RED — redact region removes text (DB #29)`.
   - GREEN: pass; full suite green; commit `feat(cli): verified redact ... ` —
     or `test(cli):` if no production code change was needed (only tests).
   - Add the new test file to `src/UnitTests/CMakeLists.txt` (explicit
     enumeration — no globbing) and make sure ctest picks it up.
3. **Determinism check:** run the same redact command twice on the same input;
   outputs must be byte-identical (no timestamps/random IDs).
4. **Round-trip:** `info` on the redacted output must succeed (reopens cleanly).

## Files

- Modify: `src/UnitTests/CMakeLists.txt` (add test target)
- Create: `src/UnitTests/tst_redacttest.cpp`
- Modify: `src/PdfTool/` only if a real bug is found (record it; do not refactor)
- Docs: note findings in `docs/PROBLEMS.md` if a quirk is discovered

## Build & test (exact commands)

```bash
export VCPKG_ROOT=/home/agent/vcpkg-cache/vcpkg
cd /home/agent/workspace/wt-core-redaction/src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DALBDF_BUILD_TESTS=ON
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Fixtures: `src/tests/fixtures/multipage.pdf` (5 pages, "MULTIPAGE PAGE <N>"
text lines) — see `src/tests/AGENT.md` for fixture docs.

## Gates

- Headless smoke passes with recorded output (redaction really removes text)
- RED test committed first, then GREEN; full suite green (11+ tests)
- Determinism: identical output on re-run
- No scope creep: no refactors, no new CLI flags unless needed for the test;
  note any proposal in the commit message, don't implement it
- Conventional Commits, no push to main

## Report-back contract

Return: exact commands you ran + real output (exit codes, fetch-text before/
after snippet), test names + pass/fail lines, commit SHAs, ctest summary.
Never fabricate results. If blocked > 2 attempts, stop and report with evidence.
