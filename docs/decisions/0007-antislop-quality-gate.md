# ADR-0007: Anti-slop code-quality gate

- Status: accepted
- Date: 2026-09-17
- Deciders: user, Yolka
- Supersedes: none

## Context

Most code in this repository is written by AI agents working from short task
briefs. That model is productive but prone to a specific failure mode — "slop":
code that passes the immediate test but is structurally unsound. Observed
examples in this repo include a 990-line stringly-typed option parser
(`pdftoolabstractapplication.cpp`), a manual 64-bit flag workaround after
`QFlags` overflowed a hand-maintained enum (`pdftoolabstractapplication.h`),
assert-as-validation that disappears in Release, a documented exit-code
contract that the CLI does not actually enforce, and several hundred lines of
copy-pasted document write-back.

Existing rules (`AGENTS.md`, `docs/coding-standard.md`) were good but partly
aspirational and spread across several documents. There was no mechanical
enforcement beyond `clang-format`, ASAN/UBSAN, and tests.

## Decision

Adopt a binding anti-slop quality bar and make as much of it mechanical as
possible.

1. **Rules live in one place.** `docs/coding-standard.md` §11 is the single
   source of truth: structure/size (S1–S7), error handling/correctness
   (E1–E7), determinism (D1–D3), comments/docs (C1–C5), dependencies/upstream
   (U1–U3), testing (T1–T6), and process/agent hygiene (P1–P7). `plans/PLAN.md`
   §3 summarizes it.

2. **Mechanical enforcement.** A `scripts/check-slop.sh` gate (roadmap R2.3)
   checks the automatable rules — frozen-file growth, function/file length,
   unowned `TODO/FIXME/HACK` markers, `Q_ASSERT` used for CLI input, edits to
   vendored upstream files outside `sync(upstream):` commits, and missing SPDX
   headers on new files — and runs in CI and the pre-commit hook. Existing
   gates (clang-format on authored files, ASAN/UBSAN, ctest) stay.

3. **Frozen files.** `src/PdfTool/pdftoolabstractapplication.{h,cpp}` may not
   grow until the CLI architecture work (R3) decomposes it; new options belong
   to the command that defines them.

4. **Review gate.** Non-mechanical rules (intent-revealing names, one thing per
   function, test behavior not implementation, no test-gaming) are enforced by
   the orchestrator's spec + quality review before a task closes.

5. **Violation protocol.** A rule may be broken only with an explicit
   declaration: which rule, why, and the trade-off (E7). Undeclared violations
   are slop.

## Consequences

- Future agent output is constrained to a documented, reviewable bar, and the
  automatable half fails fast in CI instead of in review.
- The frozen-file rule intentionally blocks some convenient short-term changes
  (adding a CLI flag by extending the base options struct) to force the R3
  decomposition first.
- `check-slop.sh` inherits the usual script risk: it must itself be tested and
  kept from becoming a rubber stamp. Heuristic checks are advisory; the
  deterministic ones are gating.
- This ADR records the decision; the actual gate lands as task R2.3.
