# S#1 Cross-Line Search — Execution Plan (DB #28, M10 wave-2)

> Binding context: repo root AGENTS.md (read fully first), src/AGENT.md,
> src/Pdf4QtLibCore/AGENT.md, src/UnitTests/AGENT.md, docs/PROBLEMS.md (S#1 entry).

## Goal

Extend the joined-visual-string search in `PDFTextSearchEngine::searchFlow` so a
phrase can match **across the `\n` line boundary** (currently `\n` is a hard
boundary the query can never cross; cross-*item* word splits already work).

## Reality check (orchestrator-verified, 2026-08-06)

- Cross-item search landed (`6b26d14` / rewritten `c2b4db8`): items are grouped
  into lines by y-center, sorted by x ascending, joined with geometry-aware
  separators (`""` touching, `" "` word gaps, `"\n"` lines/columns).
- The `\n` boundary is the deliberate hard stop: `joinedText += '\n'` with
  `Origin{0,-1}` and a query-crossing guard. **Removing it must not introduce
  false matches** (column text joining would be wrong — `test_crossItemNoFalsePositive`
  guards this).
- Baseline: 11/11 ctest green on main.

## Design direction (hypothesis to validate first)

Line breaks in reading order are word separators for search purposes: a query
containing a space should match text where a `\n` separator sits where the
query has the space. Options to evaluate in this order:
1. **Soft boundary:** replace `\n` in the joined string with `' '` (or a
   sentinel normalized to space) only for line-adjacent items that belong to
   the same text block/column — keeps columns apart, joins lines within a
   paragraph. Risk: docstrum block splitting may not exist in flow; check what
   item data is available (flags, boundingRect).
2. **In-query space matching:** keep `\n` in joined string but let the query's
   space match `[\s\n]+` — needs regex or a two-level match. More invasive.
3. **Per-line join with space, guard by geometry:** join lines with `' '` only
   when the y-gap is small relative to line height; keep `\n` for large gaps.

Pick the simplest that passes: new failing test + `test_crossItemNoFalsePositive`
still green + full suite green. **Do not ship a regex rewrite of the whole
engine.**

## Files

- Modify: `src/Pdf4QtLibCore/sources/pdftextsearchengine.cpp` (joined-string
  builder ~lines 160–215, match mapping after)
- Modify: `src/UnitTests/tst_searchtexttest.cpp` (new cross-line tests)
- Docs: update `docs/PROBLEMS.md` S#1 entry when done (orchestrator closes DB)

## TDD steps

1. **RED** — add `test_crossLinePhrase()`: build a flow with two items on two
   distinct lines (different y), first line ends "...تست", second starts
   "نهایی"; query "تست نهایی" must match. Run the single test, verify it FAILS
   for the right reason. Commit `test(search): RED — cross-line phrase search
   fails at \n boundary (S#1)`.
2. **GREEN** — implement the minimal change. Gate: new test passes AND
   `test_crossItemNoFalsePositive` AND `test_noFalsePositive` still pass.
   Commit `feat(search): allow cross-line phrase matches via <approach> (S#1)`.
3. **Full suite** — `ctest` must be 11+/11 green (new test adds a case inside
   the existing SearchText test, so count may stay 11).
4. Determinism: same query twice → identical matches; RTL case: query in
   logical order must match visual-order joined text across lines.

## Build & test (exact commands — vcpkg is NOT at /workspace on this machine!)

```bash
export VCPKG_ROOT=/home/agent/vcpkg-cache/vcpkg
cd /home/agent/workspace/wt-rtl-crossline/src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DALBDF_BUILD_TESTS=ON
cmake --build build -j$(nproc)
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
# single test:
QT_QPA_PLATFORM=offscreen build/bin/UnitTestsSearchText 2>/dev/null | tail -20
```

## Gates

- RED committed with failing test for the right reason
- GREEN: new test passes, no false-positive regressions, full suite green
- No changes outside `pdftextsearchengine.cpp`, `tst_searchtexttest.cpp`, docs
- Conventional Commits, one logical change per commit, no push to main

## Report-back contract

Return: exact test names + pass/fail lines, commit SHAs (RED and GREEN),
full ctest summary line, and the approach you chose (with 2-line rationale).
Never fabricate results. If blocked > 2 attempts, stop and report the blocker
with evidence — do not cargo-cult a workaround.
