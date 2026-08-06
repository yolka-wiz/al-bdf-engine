# W5 CLI Fuzzing — Execution Plan (DB #30, M10 wave-2)

> Binding context: repo root AGENTS.md (read fully first), src/AGENT.md,
> src/PdfTool/AGENT.md, src/tests/AGENT.md, docs/PROBLEMS.md (Quality backlog).

## Goal

Build a lightweight **input fuzz harness for the albdf CLI** that finds crashes,
assertions, hangs, and non-zero-exit-on-valid-input regressions. Keep it simple:
a deterministic script (not libFuzzer integration) that feeds malformed /
truncated / mutated inputs to the key CLI commands and checks exit-code + crash
behavior. Must run headless, be fast (≤ a few minutes), and slot into CI as a
**non-gating** job (mirror the `benchmark` job pattern).

## Reality check (orchestrator-verified, 2026-08-06)

- No fuzz harness exists anywhere (scripts/, ci/, .github/workflows/ all clean).
- `ci/run-ci.sh` stages are the gate; `.github/workflows/ci.yml` has a
  `benchmark` job that is `continue-on-error: true` — that is the pattern to
  copy for a `fuzz` job.
- CLI exit-code contract: see `src/AGENT.md` ("Exit-Code Contract") and
  `src/PdfTool/AGENT.md`. Read them before designing assertions.

## Scope

1. **Harness script** `scripts/fuzz.sh`:
   - Deterministic input corpus: reuse `src/tests/scripts/make-*.pdf.py`
     generators for valid PDFs, then derive mutations: truncation at N bytes,
     zero-fill regions, random byte flips (seeded, reproducible via `--seed`),
     garbage headers, empty files, oversized page counts in CLI args.
   - Run commands: `info`, `fetch-text`, `search-text <q>`, `add-text`,
     `delete-object`, `delete-page`, `rotate`, `move-page`, `redact`,
     `render` — each on mutated inputs, with a timeout per invocation
     (e.g. `timeout 20`) to catch hangs.
   - Success criteria per run: process does not crash with signal (segv/abrt),
     no assertion failure, and — for "structurally valid" mutations — exit
     code matches the contract (e.g. corrupt docs may legitimately return
     read-error codes; CRASH is a signal/abort, not a nonzero exit).
   - Output: `albdf-fuzz-v1` key=value summary (mirror `benchmark.sh` style),
     failures listed with the exact input + command + observed exit.
   - Exit code: 0 = no crashes; 1 = crash/hang found (or any harness error).
   - Flags: `--seed N`, `--iterations N`, `--binary <path>`,
     `--work-dir <dir>` (cleanup by default, `--keep` to retain failing inputs).
2. **CI job** `.github/workflows/ci.yml`: add `fuzz` job, `continue-on-error:
   true`, running `bash scripts/fuzz.sh --iterations 200` (or a time-bounded
   default), uploading the summary as an artifact (copy benchmark's upload step).
3. **Docs:** add a "Fuzzing" section to `src/tests/AGENT.md` or a short
   `docs/research/` note on how to run + extend the harness. Record any real
   crashes found as PROBLEMS.md entries with the reproducer.

## Design constraints

- **Determinism:** seeded mutations; same seed → same corpus → same results.
- **No network, no display:** QT_QPA_PLATFORM=offscreen everywhere.
- **Speed:** default run under ~3 minutes on this machine; CI can raise
  iterations via env.
- **Shell script** in the style of `scripts/benchmark.sh` (bash, set -u,
  same summary conventions) — not a new language.
- **No scope creep:** this is a harness, not a fix-everything-found sweep. If
  the harness finds real crashes, STOP and report them (with reproducer
  inputs saved under the work-dir); do not start fixing library bugs unless
  they are trivial one-liners. That is a separate dispatch.

## Build (harness needs the binary)

```bash
export VCPKG_ROOT=/home/agent/vcpkg-cache/vcpkg
cd /home/agent/workspace/wt-infra-fuzz/src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DALBDF_BUILD_TESTS=ON
cmake --build build -j$(nproc)
# binary: build/bin/albdf ; fixture generators: src/tests/scripts/make-*.pdf.py
```

## Gates

- `scripts/fuzz.sh --iterations 50` runs green on valid fixtures (no crashes)
- Same seed → identical summary (determinism)
- A deliberately broken input (e.g. truncated PDF) is caught by the harness
  (verify the harness can actually detect a crash — test with a known-bad case
  if one exists; otherwise verify the timeout path)
- CI `fuzz` job added mirroring `benchmark` (non-gating)
- Conventional Commits, no push to main

## Report-back contract

Return: script path + summary output from a real run (with seed), list of any
crashes found with reproducer paths, CI diff summary, commit SHAs, and the
determinism proof (two runs, same seed → identical summary). Never fabricate
results. If blocked > 2 attempts, stop and report with evidence.
