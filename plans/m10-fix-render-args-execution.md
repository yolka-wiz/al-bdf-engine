# F#1/F#2 Render Argument Validation Fixes — Execution Plan (DB #32/#33)

> Binding context: repo root AGENTS.md (read fully first), src/AGENT.md,
> src/PdfTool/AGENT.md (exit-code contract!), src/UnitTests/AGENT.md,
> docs/PROBLEMS.md (F#1/F#2 entries).

## Goal

Fix two fuzz-found crashes in the `albdf render` CLI command, with TDD regression
tests. Reproducers saved by the fuzz harness (DB #30) at
`/tmp/fuzz-final/fail/` — all three are on **valid** PDF input with abusive CLI args.

## Verified findings (orchestrator + fuzz harness, do not re-derive)

| # | Symptom | Root cause hypothesis | Reproducer |
|---|---|---|---|
| F#1 | `render --page-first 0` → SIGABRT 134: uncaught `std::out_of_range` (`(size_t)-1`) from Qt Concurrent worker thread | page index 0 (or 1-based page > count, e.g. `--page-last 999999999`) is not validated against the document's page count before indexing; `(size_t)-1` = underflow from `page-1` | `/tmp/fuzz-final/fail/cli_render_page0.cmd`, `cli_render_huge_last.cmd` |
| F#2 | `--image-res-dpi 999999` → never completes (~94 GP image; resource exhaustion), caught as HANG 124 | DPI not bounded above (log shows "Dpi must be in range from 72 to 6000. Defaulting to 6000." is *not* in the huge_dpi repro — verify where DPI clamping lives or should live) | `/tmp/fuzz-final/fail/cli_render_huge_dpi.cmd` |

The reproducer PDF is `multipage.pdf` (5 pages, 612x792). Exact crash commands:
```
albdf render multipage.pdf --page-first 0 --page-last 1 --image-format png --image-res-dpi 72 --image-output-dir <dir>
albdf render multipage.pdf --page-first 1 --page-last 999999999 --image-format png --image-res-dpi 72 --image-output-dir <dir>
albdf render multipage.pdf --page-first 1 --page-last 1 --image-format png --image-res-dpi 999999 --image-output-dir <dir>
```

## Scope (read src/PdfTool/AGENT.md first — it documents the render command)

- `src/PdfTool/pdftoolrender.cpp` — argument parsing: page range validation (must
  be within [1, pageCount]; reject 0 and > count with the exit-code contract, not
  a crash) and DPI validation/bounding (upper bound per existing "72..6000" clamp
  if present, else add one; check `PDFImage::ImageEncodeOptions` / renderer API).
- Check whether the page count is available at parse time (document must be read
  before page range validation) — if the crash happens later (inside the Qt
  Concurrent worker), the fix may need to be in the render loop bounds instead.
- Add regression tests (see below). Do NOT touch unrelated code.

## TDD steps

1. **RED** — add `src/UnitTests/tst_rendertest.cpp` (or extend an existing CLI
   test suite following the `runTool` pattern from `tst_pageopstest.cpp`):
   - `--page-first 0` → must exit with the documented error code (not signal 134)
   - `--page-last 999999999` → same
   - `--image-res-dpi 999999` → must complete within a sane bound (either clamped
     to 6000 or rejected with error code) — no hang; run with a generous test
     timeout but assert completion + correct exit
   - a *valid* render still works (positive control: `--page-first 1 --page-last 1
     --image-res-dpi 72` produces a PNG, exit 0)
   Run the suite, confirm the two crash cases FAIL for the right reason. Commit
   `test(cli): RED — render out-of-range page / huge DPI crash (F#1/F#2)`.
2. **GREEN** — implement minimal fixes. Gate: new tests pass AND full suite green
   AND the three original reproducer commands no longer crash/hang (verify by
   running them directly). Commit `fix(cli): validate render page range + bound
   DPI (F#1/F#2)`.
3. **Full suite** — `ctest` 13+/13 green (12 existing + RenderTest; existing
   suites must not regress).
4. **Determinism** — the fixed invalid-arg paths must return stable exit codes;
   valid render output must remain byte-deterministic across two runs.

## Build & test (exact commands — vcpkg is NOT at /workspace!)

```bash
export VCPKG_ROOT=/home/agent/vcpkg-cache/vcpkg
cd /home/agent/workspace/wt-fix-render/src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DALBDF_BUILD_TESTS=ON
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
# direct reproducer verification (must no longer crash/hang):
B=build/bin/albdf
QT_QPA_PLATFORM=offscreen $B render ../src/tests/fixtures/multipage.pdf --page-first 0 --page-last 1 --image-format png --image-res-dpi 72 --image-output-dir /tmp/r0 ; echo "page0 rc=$?"
QT_QPA_PLATFORM=offscreen timeout 60 $B render ../src/tests/fixtures/multipage.pdf --page-first 1 --page-last 999999999 --image-format png --image-res-dpi 72 --image-output-dir /tmp/r1 ; echo "huge-last rc=$?"
QT_QPA_PLATFORM=offscreen timeout 60 $B render ../src/tests/fixtures/multipage.pdf --page-first 1 --page-last 1 --image-format png --image-res-dpi 999999 --image-output-dir /tmp/r2 ; echo "huge-dpi rc=$?"
```

## Gates

- RED committed first (failing for the right reason, not a compile error)
- GREEN: new tests pass, 3 original reproducers no longer crash/hang, full suite green
- Exit-code contract respected (check src/PdfTool/AGENT.md / pdftoolrender for the
  documented error codes — likely ErrorInvalidArguments=7 or similar)
- Conventional Commits, one logical change per commit, no push (branch
  `m10/fix-render-args` already has the QFlags build fix; do NOT touch
  pdftoolabstractapplication.h)

## Report-back contract

Return: root-cause analysis (where the underflow/DPI-unbounded happens), exact
test names + pass/fail lines, commit SHAs (RED + GREEN), the three reproducer
rc values from direct runs, full ctest summary. NEVER fabricate results. If
blocked after 2 attempts, STOP and report with evidence. If you hit your
tool-call cap, stop cleanly (tree committed or clean) and report what remains.
