# albdf Coding Standard

Applies to ALL code in this repo, human- or agent-written. Binding — see `AGENTS.md` §5.
When in doubt, follow the surrounding PDF4QT code style; this standard codifies it.

---

## 1. Language & toolchain

- **C++20**, compiled with GCC/Clang (Linux). No C++23 features until decided.
- **Qt 6.8+ (LTS)** — QtCore/QtGui/Xml/Svg only in the core. **No QtWidgets, no QML** in v1.
- **CMake ≥ 3.21**, modern targets (`target_link_libraries`, not raw include dirs).
- Build must work with `-DCMAKE_BUILD_TYPE=Debug` and `Release`; warnings-as-errors in CI.

## 2. Naming conventions (follow PDF4QT)

| Item | Convention | Example |
|---|---|---|
| Classes | `PDF` + CamelCase | `PDFDocumentTextFlowEditor` |
| Member variables | `m_` prefix, camelCase | `m_editedTextFlow` |
| Functions | camelCase | `getItemsForPageIndex()` |
| Parameters | camelCase | `pageIndex` |
| Constants / enums | CamelCase values | `enum Flag { None, Text }` |
| Namespaces | `pdf::` (core), `pdftool::` (CLI) | — |
| Files | lowercase, one concept per file | `pdfdocumenttextflow.h` |
| Macros | `PDF4QT_`/`ALBDF_` uppercase | `PDF4QTLIBCORESHARED_EXPORT` |

## 3. Formatting (enforced)

- `clang-format` — `.clang-format` at repo root is the single source of truth.
  - Gate: `clang-format --dry-run --Werror` must pass on every touched file.
  - Basis: LLVM style, 4-space indent, 120-column limit (matches PDF4QT).
- `#include` order: own header → project headers → Qt → system (clang-format sorts).
- No `using namespace std;` / `using namespace Qt;` in headers. In .cpp, prefer qualified names.
- `{}` braces on own line for classes/functions (Allman), K&R for control statements (matches Qt).

## 4. API & design rules

- **Deterministic output**: no `QDateTime::currentDateTime()`, no `qrand()`, no unordered
  iteration order leaking into output. Document generation must be byte-stable for the same
  input (like `qpdf --deterministic-id`). Tests depend on it.
- **Headless**: nothing in the core may require a display. Use `QPdfDocument`-style pure
  functions where possible. GUI code is banned from the core library.
- **Exceptions**: allowed internally (PDF4QT uses them); never cross a public C-ABI or CLI
  boundary uncaught — the CLI catches and prints a stable error.
- **Ownership**: prefer `std::unique_ptr`/values; raw pointers only as non-owning views.
  Follow PDF4QT's existing patterns (`PDFObjectStorage`, `PDFDocument` const-ref passing).
- **Public API**: every public function documented with Doxygen (`\param`, `\returns`).
- **No hidden state**: globals/statics only for immutable constants or registries (e.g.,
  the CLI's `PDFToolApplicationStorage`).
- **Additive, not rewrite**: extend PDF4QT classes; refactor only when the task says so.
  Never reformat untouched files (breaks diff/upstream cherry-picking).

## 5. Memory & safety

- No raw `new`/`delete` in new code (use RAII containers/Qt parented objects).
- Bounds-check all index math; PDFs are hostile input (malformed xref, huge arrays).
- Every parser touchpoint assumes attacker-controlled input: no asserts as validation.
- Sanitizers: ASAN+UBSAN build must be clean in CI for the test suite.

## 6. Error handling

- Return `std::optional`/`bool`+error string for expected failure paths.
- Throw `PDFException`-style for programmer errors / invariant violations.
- CLI: non-zero exit code + message on stderr; stable, parseable error text.
- Never `qFatal`/`abort` in library code (only the CLI may exit).

## 7. Testing standard

- Every feature: unit tests (Qt Test, `UnitTests/` pattern) + CLI-level test.
- Anything that renders: **golden-image test** (deterministic render → PNG → diff).
- RTL: corpus fixtures incl. ZWNJ, lam-alef, tashkeel, Persian/Arabic digit unification.
- Tests must run headless: `QT_QPA_PLATFORM=offscreen ctest`.
- Test data: committed small fixtures; generated ones reproducible via `scripts/`.
- Golden updates: explicit, reviewed, committed with a reason (never silent `--update`).

## 8. Git workflow

- Conventional Commits: `feat:`, `fix:`, `test:`, `docs:`, `refactor:`, `chore:`, `build:`.
- One logical change per commit; commit message explains WHY (body) not just what (subject).
- Branch: `m<milestone>/<slug>`, e.g. `m1/delete-object-cli`.
- Linear history (rebase merge). No `wip` commits that break the build.
- Every task-closing commit records the evidence in the DB (`db.py task-done <id> --ref <sha>`).

## 9. Dependencies & licensing

- Adding a dependency requires an ADR entry + user/orchestrator approval (see `AGENTS.md`).
- **Project license: GPL-3.0-or-later** (ADR-0005, supersedes ADR-0004). Every new file
  carries the GPL-3.0-or-later header (`SPDX-License-Identifier: GPL-3.0-or-later`).
  Vendored upstream PDF4QT files keep their MIT headers (MIT is GPLv3-compatible).
- All linked deps must be GPLv3-compatible: Qt (LGPL-3, dynamic), FreeType (FTL),
  OpenJPEG (MIT), OpenSSL (Apache-2.0 — GPLv3-compatible, **not** GPLv2), ZLIB,
  HarfBuzz (MIT), FriBidi (LGPL-2.1, dynamic linking), TBB (Apache-2.0), blend2d (Zlib).
- NO GPLv2-incompatible deps (i.e. no Apache-2.0 under a GPLv2 license — hence GPLv3).
  NO new deps "just in case" (YAGNI).

## 10. Agent-specific rules

- Agents must read `AGENTS.md` + this file before writing code.
- No self-reported completion: closing a task requires evidence (`--ref <sha>` + passing test).
- If a task is blocked, record it (`db.py task-block`) — never silently skip.
- Unfamiliar PDF/Qt APIs: consult `docs/research/` (Rosetta output) or upstream source first.

## 11. Anti-slop rules (binding, enforced)

Most code here is agent-written; slop accumulates silently. These rules stop
it. The mechanically checkable ones fail CI via `scripts/check-slop.sh`
(roadmap R2.3); the rest fail the orchestrator's review. They are the detailed
form of `plans/PLAN.md` §3.

### 11.1 Structure & size

- **S1** One concept per file; one CLI command per file pair.
- **S2** Functions ≤ **80** lines; new authored files ≤ **1500** lines.
  Grandfathered long functions may not grow (see **S6**).
- **S3** Rule of three: a block copied a third time must be extracted.
- **S4** No magic numbers/strings — name constants. CLI command names are
  constants, not literals spread through the code.
- **S5** Typed data over stringly-typed dispatch. No new
  `if (command == "text")`-style branches.
- **S6** **Frozen files** may not grow: `src/PdfTool/pdftoolabstractapplication.{h,cpp}`
  until R3 completes. New options/fields belong to the command that defines them.
- **S7** **Performance hygiene:** pick the right data structure/algorithm up
  front (pass large types by `const&`, avoid copies/allocations inside loops).
  Do not micro-optimize at the cost of clarity; if an optimization is
  non-obvious, say why in a comment (still a WHY comment, see C1).

### 11.2 Error handling & correctness

- **E1** No `assert`/`Q_ASSERT` as input validation or control flow — return an
  error code / `std::optional`. Asserts vanish in Release.
- **E2** No empty `catch`, no swallowed errors, no `Q_UNUSED`-the-failure.
- **E3** Exceptions never cross the CLI boundary (or a C ABI); `main.cpp` holds
  the single top-level guard.
- **E4** Every failure path sets a documented exit code and a stable stderr
  message.
- **E5** Exit codes are a public contract: `0` success, `7` invalid arguments,
  the documented set in `src/PdfTool/AGENT.md`. Changes need a test + CHANGELOG +
  man-page update.
- **E6** Bounds-check all index math — PDFs are hostile input.
- **E7** **Violation protocol.** If a rule must be broken, stop and state:
  (1) which rule, (2) why it is necessary, (3) the trade-off. In an autonomous
  batch, record it in the task/DB instead of stalling. Undeclared violations
  are slop.

### 11.3 Determinism

- **D1** No `QDateTime::currentDateTime()`, random IDs, UUIDs, or pointer-order
  in document output. Byte-stable output is a hard requirement.
- **D2** Any nondeterminism exemption must be documented here or in
  `docs/PROBLEMS.md` (encryption uses a secure RNG — the one known exemption;
  tests must not hash `encrypt` output).
- **D3** Anything that renders has a golden-image/hash test.

### 11.4 Comments & documentation

- **C1** Comments explain **WHY**. No WHAT narration, no restating the code.
- **C2** No commented-out code — git remembers it; delete it.
- **C3** `TODO`/`FIXME`/`HACK` must carry a DB ref: `// TODO(#123): …`.
  Unowned markers are forbidden.
- **C4** Public API gets Doxygen (`\param`, `\returns`).
- **C5** No AI-slop phrasing, banner noise, or decorative attribution.

### 11.5 Dependencies & upstream

- **U1** A new dependency requires an ADR + approval (ADR-0004/0005). YAGNI.
- **U2** Vendored upstream files are never reformatted and are edited only in
  `sync(upstream):` commits (cherry-pick hygiene).
- **U3** New authored files carry `SPDX-License-Identifier: GPL-3.0-or-later`
  and the standard notice.

### 11.6 Testing

- **T1** RED test first, then GREEN — for every bug fix and feature.
- **T2** Every CLI command has: help text, argument validation, at least one
  positive and one negative (exit-code) test.
- **T3** Tests are deterministic, headless (`QT_QPA_PLATFORM=offscreen`), with
  no network and no sleeps; fuzz seeds are fixed.
- **T4** Never weaken or delete a test to go green; golden updates are explicit,
  reviewed, committed with a reason.
- **T5** **No test-gaming.** Never hardcode a result or special-case an input
  just to satisfy a test. If a test looks wrong, flag it and fix the test (or
  the spec) explicitly — do not silently work around it. Correctness must come
  from logic.
- **T6** Tests assert **behavior**, not implementation details (no testing
  private helpers, internal call order, or data-structure internals).

### 11.7 Process & agent hygiene

- **P1** One logical change per commit; Conventional Commits; the body explains
  WHY.
- **P2** Every commit compiles and passes tests — no `wip` commits.
- **P3** Evidence gate: close a task only with `--ref <sha>` + a passing test.
- **P4** A subagent's summary is a handoff, not a delivery — the orchestrator
  verifies (files exist, build green, tests pass) before closing.
- **P5** Never `git add -A`; in parallel batches stage only your own paths.
- **P6** Never hand-edit generated artifacts (`REPO_MAP.md`, version strings) —
  regenerate or derive them.
- **P7** No secrets, no personal data, no large binaries in git.

### 11.8 Enforcement map

| Rule | Mechanism |
|---|---|
| S6 | `check-slop.sh` (frozen-file LOC baseline — shrink allowed, growth is not) |
| C3 | `check-slop.sh` (marker regex needs `(#NN)`) |
| E1 | `check-slop.sh` (`Q_ASSERT`/`assert` in changed `src/PdfTool/**` files) |
| U3 | `check-slop.sh` (SPDX on newly added authored files) |
| U2 | `check-slop.sh` (edited vendored file detection — warning only) |
| S7 | `check-slop.sh` (raw `new`/`delete` — warning only) |
| S1, S2, S3, S4, S5 | orchestrator review |
| E2–E7, D1–D3, C1–C5, T1–T6, P1–P7 | orchestrator review + existing CI |
| Formatting | `clang-format` gate (authored files only) |
| Memory/safety | ASAN/UBSAN CI job |

`scripts/check-slop.sh` implements the mechanical checks above (roadmap R2.3)
and runs in CI (stage 5) + `.githooks/pre-commit`. It is **diff-scoped by
default** — changed authored files vs `origin/main` (falling back to the fork
base `6bf5047`), like the clang-format gate; `--all` is a full-repo advisory
scan. Function/file-length limits (S2) are not yet mechanized (the frozen-file
baseline covers the S6 case), so S2 stays a review rule.
