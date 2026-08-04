# pdfedit Coding Standard

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
| Macros | `PDF4QT_`/`PDFEDIT_` uppercase | `PDF4QTLIBCORESHARED_EXPORT` |

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
