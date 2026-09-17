# albdf — Master Plan (v3, 2026-09-17)

> **Canonical forward roadmap.** Granular status lives in `db/albdf.db`
> (`python3 scripts/db.py status`). Execution contract: `AGENTS.md` +
> `docs/coding-standard.md`; role contracts in `agents/roles/`.
>
> **v3 changes:** records the repository/release hygiene pass (§1, R0),
> reorders the remaining work by priority (R0–R6), adds an explicit
> parallel-execution plan (§5), and makes the **anti-slop quality bar** a
> binding gate (§3; full rules in `docs/coding-standard.md` §11).
> Historical milestones M0–M14 are condensed in Appendix A. Supersedes v2.

**Goal:** a headless PDF editing **library + CLI** for Linux — a fork of MIT
PDF4QT that adds object deletion, add-text, and correct **RTL
(Arabic/Persian/Hebrew) write + search** — engineered so it can grow for years.

**Architecture:** `Pdf4QtLibCore` (engine) + `albdf` (CLI, dir `src/PdfTool/`)
+ `UnitTests`/`tests`. Optional vendored GUI behind `ALBDF_BUILD_GUI=ON`.
Core + CLI + tests are always headless and deterministic (ADR-0002).

**Operating constraint:** max **3 parallel subagents**. The orchestrator
dispatches ≤3 workers per batch; anything beyond queues. See §5 for which
tracks may run together.

---

## 0. How to read this plan

- **Priority:** `P0` = now (correctness/hygiene, unblocks the rest);
  `P1` = next (leverage/architecture); `P2` = later (ergonomics/release);
  **Deferred** = explicitly not now; **Dropped** = non-goal.
- **`∥`** marks a task that may run concurrently with other `∥` tasks in the
  same phase. One writer per file (see §5).
- **DB ref:** every active task must exist in the tracking DB before work
  starts (`db.py task-add`). IDs shown as `#TBD` must be created.
- **Evidence gate:** a task closes only with `db.py task-done <id> --ref <sha>`
  and a passing test. No evidence, no close (`AGENTS.md` §3).

---

## 1. Snapshot (2026-09-17)

**Shipped:** 0.1.0 → 0.4.0 — RTL write + search, object deletion, add-text,
forms & signatures, page ops, redaction, deterministic builds, hosted CI,
optional GUI. Details in Appendix A and `docs/RELEASES.md`.

**Repository hygiene pass landed** (`main`, PR #9 — commits `edb44f3`,
`3cdc791`, `87ce856`, `2796128`):

- README rewritten user-facing (260 → ~110 lines); `CHANGELOG.md` added;
  `.editorconfig` / `.gitattributes`; man page version corrected.
- Root `CMakeLists.txt` shim: `cmake -S . -B build` works; `src/`-rooted build
  unchanged. `config.h` now written to `${CMAKE_BINARY_DIR}` to stay on the
  core include path under either invocation.
- CI clang-format gate unblocked (missing `pdfutils.h` exemption) — `main`
  format job green again.
- GitHub metadata fixed (description, 12 topics, wiki off); releases
  normalized to `albdf X.Y.Z` and `0.1.0` / `0.4.0` created.
- `docs/branch-protection.md` corrected to describe the real `pr check`
  ruleset (1 approval; deletion/force-push blocked; checks not enforced).

**Known gaps carried into the roadmap:**

- `0.4.0` has **no binary assets** — the multi-platform release workflow failed
  on macOS + linux-aarch64 (logs expired). → R6.
- `docs/RELEASES.md` still claims 0.4.0 shipped native macOS binaries (false).
- Dead remote branches: `handoff-migration` (contains a force-added
  `db/albdf.db`), `m14/form-field-ap`, `m14/freetext-ap` (merged).
- `REPO_MAP.md` has duplicated "unmapped" rows from the generator.
- The dev `Dockerfile` installs Ubuntu 24.04's Qt **6.4.2**, but
  `src/CMakeLists.txt` requires Qt **≥ 6.8**, so the documented dev container
  cannot configure. → R5.
- Root build path is reasoned but not compile-verified from the repo root.
- `main`'s CI status checks are *not* required by the ruleset; only the
  clang-format failure was fixed.

---

## 2. Non-goals (unchanged)

- **No GUI in the product roadmap.** The vendored GUI stays optional; core +
  CLI + tests never depend on it (ADR-0002).
- **No OCR.** Scanned-PDF text recovery is out of scope.
- **No TTS.** Compiled out by design (fork divergence).
- **No rewrite of upstream.** Extend via additive changes; never reformat
  vendored files (cherry-pick hygiene).
- **No new dependencies "just in case".** Every dep needs an ADR (ADR-0004/0005).

---

## 3. Code quality bar — anti-slop (binding)

This project is built largely by agents; slop accumulates silently. The rules
below are the defense. **Full numbered rules live in
`docs/coding-standard.md` §11**; this section is the summary and the hard
limits. Rule violations fail review; the mechanically checkable ones fail CI.

### 3.1 Principles

1. **Small and typed.** One concept per file; typed options/structs over
   stringly-typed dispatch; name constants, never magic literals.
2. **No assert-as-validation.** `Q_ASSERT`/`assert` is never user-input
   validation or control flow — return an error code/`std::optional`.
3. **Errors are a contract.** Exceptions never cross the CLI boundary; every
   failure path returns a documented exit code + stderr message; no empty
   `catch`.
4. **Determinism is a feature.** No time/random/UUID/pointer-order in document
   output; any exemption is documented (encryption is one).
5. **Comments explain WHY.** No WHAT narration, no commented-out code, no
   unowned `TODO`/`FIXME`/`HACK` (must carry a DB ref: `// TODO(#123): …`).
6. **Test-first.** A RED test precedes every fix/feature; every CLI command
   has help + arg validation + a positive and a negative (exit-code) test.
7. **One writer per file in a batch.** Subagent output is a handoff, not a
   delivery — the orchestrator verifies before closing.
8. **Leave it greener.** No net growth of known-slop files; extract on the
   rule of three; delete dead code (git remembers it).

### 3.2 Hard limits (enforced)

| Limit | Value | Applies to |
|---|---|---|
| Function length | ≤ 80 lines | new/changed authored code |
| File length | ≤ 1500 lines | new authored files |
| Duplication | extract at 3rd copy | authored code |
| Frozen files | may not grow | `pdftoolabstractapplication.{h,cpp}` (until R3) |
| New deps | ADR required | any |
| Unowned TODO/FIXME/HACK | forbidden | all |
| `Q_ASSERT` for input | forbidden | `src/PdfTool/**`, core boundaries |
| Raw `new`/`delete` | forbidden | new code |
| `using namespace` in headers | forbidden | all |
| Vendored-file edits | `sync(upstream):` commits only | `src/**` upstream-derived |
| Missing SPDX header | forbidden | new authored files |

### 3.3 Enforcement

- **Mechanical gate:** `scripts/check-slop.sh` (R2.3) checks the table above
  and runs in CI + the pre-commit hook.
- **Existing gates:** clang-format (authored files only), ASAN/UBSAN, ctest.
- **Review gate:** the orchestrator's spec + quality review (`qt-cpp-review`
  skill) checks the non-mechanical principles before a task closes.

---

## 4. Roadmap (priority-ordered)

### R0 — Repo & release hygiene (P0, finish what v3 started)

Small, independent, unblocks clean work. Most items are independent → parallel.

- [ ] **R0.1 ∥** Delete the dead remote branches `m14/form-field-ap` and
  `m14/freetext-ap` (merged). For `handoff-migration`, first export the task DB
  (`git show origin/handoff-migration:db/albdf.db > /tmp/albdf.db`), then delete
  — the DB does not belong in git. `#TBD`, S.
- [ ] **R0.2 ∥** Correct the false 0.4.0 claim in `docs/RELEASES.md` (native
  macOS binaries did not ship) and point readers at the GitHub release status.
  `#TBD`, S.
- [ ] **R0.3 ∥** Archive completed execution plans (`plans/m10-*`, `plans/m14-*`,
  `plans/P3-*`, `plans/handoff/`) into `plans/archive/` so `plans/PLAN.md` is
  the single active roadmap; update `scripts/gen-repo-map.py` + docs references.
  `#TBD`, S.
- [ ] **R0.4 ∥** Update `db/seed.py` to reflect shipped state M12–M14 and the
  new R-task set, so a fresh `db.py init && db/seed.py` matches reality.
  `#TBD`, S.
- [ ] **R0.5 ∥** Ratify §3: add `docs/coding-standard.md` §11 (rules +
  enforcement mapping) and ADR-0008 (quality gate). `#TBD`, S.

**Exit:** branches gone; docs truthful; one active roadmap; DB seed current;
anti-slop rules binding.

### R1 — Correctness & CLI contract (P0, one writer)

The only crash class found by fuzzing, and the exit-code contract, are still
paper-thin. Do this before architectural work.

- [ ] **R1.1 ∥** Top-level `try/catch` in `src/PdfTool/main.cpp`: map
  `pdf::PDFException` / `std::exception` to a stable error + exit code; never
  reach `std::terminate` from user input (closes F#1's root cause). Add a test
  that exercises the guard. `#TBD`, S.
- [ ] **R1.2 ∥** Exit-code normalization: use `parser.parse()` (not `process()`)
  for non-help invocations so unknown/malformed options return the documented
  `ErrorInvalidArguments` (7) and a usage message, instead of Qt's `EXIT_FAILURE`
  (1); unknown command must not silently succeed (currently returns 0). `#TBD`, S.
- [ ] **R1.3 ∥** CLI contract test: a table of `(argv → expected exit code +
  stderr shape)` covering `add-text`, `delete-object`, `search-text`, `render`,
  and the help/version paths. `#TBD`, S. Depends on R1.2.
- [ ] **R1.4** Document the encryption determinism exemption (secure RNG is
  correct crypto, but it violates the byte-stable rule) in
  `docs/coding-standard.md` + `docs/PROBLEMS.md`. `#TBD`, S.

**Exit:** no user input can abort the process; exit codes match the documented
contract and are regression-tested.

### R2 — Test-harness leverage (P1, one writer)

Make new tests cheap before doing large refactors; the same `runTool` is
copy-pasted in 8 files and the CMake test block repeats 16×.

- [ ] **R2.1 ∥** Extract `src/UnitTests/testsupport/tst_toolrunner.h` (shared
  `runTool`/binary-path helpers) and migrate all 8 call sites. `#TBD`, S.
- [ ] **R2.2 ∥** Add a CMake helper `add_albdf_test(<name> <source> [deps…])`
  encapsulating the repeated `set_target_properties`/`add_test` block; migrate
  `src/UnitTests/CMakeLists.txt`. `#TBD`, S–M.
- [ ] **R2.3 ∥** Implement `scripts/check-slop.sh` (the mechanical rules in
  §3.2) with its own test, and wire it into `ci/run-ci.sh` + `.githooks/pre-commit`.
  `#TBD`, M.
- [ ] **R2.4** Add at least one negative/exit-code test per CLI command; roll
  out per command once R1.2 + R2.1 land. `#TBD`, M.

**Exit:** new CLI test requires ≤20 lines; `UnitTests/CMakeLists.txt` shrinks;
slop gate runs in CI.

### R3 — CLI architecture & shared write-back (P1)

Highest structural leverage for expandability. The base file is **frozen**
(§3.2) until the options have moved out.

- [ ] **R3.1** Extract a core page-write-back helper
  (`PDFPageContentRewriter`: replace resources → compress content → build
  content/page dicts → merge → finalize) and use it in `add-text` (LTR+RTL) and
  `delete-object`; add tests for `/Contents` arrays and indirect resources.
  `#TBD`, M.
- [ ] **R3.2** Replace `Q_ASSERT`-as-validation in the custom commands with
  explicit bounds checks + error codes (e.g. `pdftooladdtext.cpp:173`,
  `pdftooldeleteobject.cpp:147`). `#TBD`, S.
- [ ] **R3.3** Introduce per-command `CommandSpec` (options declared + parsed
  into a typed struct by the command) and migrate commands incrementally;
  remove the 64-bit `Options` workaround once the base stops growing.
  `#TBD`, L. Framework is one writer; command migrations may then parallelize.
- [ ] **R3.4** ADR-0007 documenting the command architecture and the frozen
  base. `#TBD`, S.

**Exit:** adding a command touches only its own file; `PDFToolOptions` stops
growing; `add-text`/`delete-object` share one write-back path.

### R4 — RTL backend seam (P2)

- [ ] **R4.1** Introduce an internal `PDFBidi` / `PDFShaper` interface; move the
  three `<fribidi.h>` call sites and the `<hb.h>` usage behind it; unit-test the
  seam (bidirectional inversion, lam-alef collapse, digit folding). `#TBD`, M.
- [ ] **R4.2** Consolidate the duplicated visual↔logical inversion logic
  (`pdfrtltextnormalizer.cpp` vs `pdftextsearchengine.cpp`). `#TBD`, S.

**Exit:** only the seam headers include FriBidi/HarfBuzz; RTL logic is testable
without the full engine.

### R5 — Build ergonomics (P2)

- [ ] **R5.1** Variable-ize build output paths (`ALBDF_BIN_DIR` etc.) and
  replace the hardcoded `src/build` in `scripts/package.sh`,
  `scripts/benchmark.sh`, `scripts/fuzz.sh`, and CI; add a CI job that builds
  from the repo root to keep the shim honest. `#TBD`, M.
- [ ] **R5.2** Fix `scripts/gen-repo-map.py` duplicate "unmapped" rows; decide
  the generated-file policy (keep committing `REPO_MAP.md` or generate on
  demand). `#TBD`, S.
- [ ] **R5.3** Add a `--warnings-as-errors` build to CI. `#TBD`, S.
- [ ] **R5.4** Fix the dev `Dockerfile`: it installs Qt 6.4 but the project needs
  ≥ 6.8 (replicate the CI path — official Qt archives — or pin a base image
  with 6.8); verify `docker build` + a full `ci/run-ci.sh` run inside it. `#TBD`, M.

**Exit:** `cmake -S . -B build` verified in CI; no root-relative path is
hardcoded in scripts.

### R6 — Release engineering (P2)

- [ ] **R6.1** Diagnose and fix the macOS + linux-aarch64 release-build
  failures; re-run the workflow for `0.4.0` to attach the missing artifacts.
  `#TBD`, M.
- [ ] **R6.2** Generate release notes from `CHANGELOG.md` (single source of
  truth) and keep the workflow title scheme (`albdf X.Y.Z`). `#TBD`, S.
- [ ] **R6.3** Cut `0.5.0` once R1–R3 land (correctness + architecture are
  user-visible quality). `#TBD`, S.

**Exit:** all four platform builds green on a tag; release assets attached;
notes auto-derived.

### R7 — Deferred / not now

- **Cross-line RTL search (S#1):** documented limitation; low user impact
  versus effort. Revisit only with a concrete user need.
- **Content-editor refactor beyond R#4:** upstream-derived; avoid.
- **Per-command `CommandSpec` completion:** if R3.3 stops at the framework,
  finish opportunistically — never let the base regrow first.
- **GUI as a product:** out of the roadmap (ADR-0002).
- **OCR / TTS:** non-goals.
- **Upstream rename tracking (D#4):** monitor for security fixes via the
  remote; no work until upstream cuts a release we need.

---

## 5. Parallel execution plan

**Rules (binding for all batches):**

1. **≤3 subagents.** One **writer per file** per batch.
2. Each subagent is told its exact file allow-list and to **stage only its own
   paths** (never `git add -A`).
3. The orchestrator integrates sequentially (rebase), then runs the full gate
   before closing tasks.
4. Embed pre-verified API contracts in dispatch briefs (exact class/method
   names, headers, patterns) — subagents otherwise burn budget re-reading.
5. A subagent's summary is a **handoff**; verify files exist, build is green,
   tests pass before trusting it.

**Track map:**

| Track | Files owned | Can run with | Notes |
|---|---|---|---|
| R0 hygiene | branches, `docs/RELEASES.md`, `plans/`, `db/seed.py`, docs | R6 (workflows) | mostly independent |
| R1 correctness | `main.cpp`, `pdftoolabstractapplication.*` | R2, R4, R6 | single writer (shared base) |
| R2 harness | `UnitTests/**`, `ci/run-ci.sh`, `scripts/check-slop.sh` | R1, R4, R6 | avoid `pdftoolabstractapplication` |
| R3 architecture | `Pdf4QtLibCore` new files, `PdfTool` commands | R4 (different core files) | serial vs R2 (CMake) |
| R4 RTL seam | `pdfrtl*`, `pdfshaper*`, `pdfbidi*` | R1, R2, R6 | new headers |
| R5 build | root `CMakeLists.txt`, `scripts/*`, CI | R4, R6 | coordinate with R2 on `ci/` |
| R6 release | `.github/workflows/**` | all | isolated |

**Suggested batch order:** Batch 1 = R0.1–R0.5 (parallel, 3+ optional) + R6.1
(kick off the long release diagnosis). Batch 2 = R1 + R2.1 + R4.1. Batch 3 =
R3.1 then R3.3. Batch 4 = R5 then R6.2/R6.3.

---

## 6. Definition of Done

**Per task:** RED test first → implementation → GREEN → `clang-format` clean →
`ci/run-ci.sh` green → Conventional Commit explaining WHY → DB
`task-done --ref <sha>`.

**Per phase:** all checkboxes ticked with evidence; exit criteria met; docs
(`AGENT.md`, `docs/PROBLEMS.md`, man page, `CHANGELOG.md`) updated; ADRs written
where a decision was made; the next phase's DB tasks created.

---

## 7. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Agent slop accumulates | §3 anti-slop gate (CI + review); frozen base; rule of three |
| Refactors break RTL output | golden-image + corpus tests; ASAN; byte-determinism checks |
| Parallel agents clobber files | one writer per file; stage own paths; sequential integration |
| Root build shim untested | R5.1 adds a root-build CI job before relying on it |
| Release workflow stays red | R6.1 owns it; assets verified before 0.5.0 |
| Subagent budget exhaustion | pre-verified contracts in briefs; orchestrator finishes work |
| Upstream drift | upstream remote + cherry-picks; baseline anchors behavior |
| Scope creep | §2 non-goals; proposals go to DB, not code |

---

## 8. Proposed ADRs

| ADR | Decision | Phase |
|---|---|---|
| 0007 | CLI command architecture (`CommandSpec`, typed options, frozen base) | R3.4 |
| 0008 | Anti-slop quality gate (rules + `check-slop.sh`) | R0.5 |
| 0009 | Root build layout (`CMakeLists.txt` shim + `${CMAKE_BINARY_DIR}` policy) | R5 |
| 0010 | Generated-file policy (`REPO_MAP.md`, release notes) | R5.2 |

---

## Appendix A — Shipped history (M0–M14)

- **M0 / M0.5 / M1** — infra, agent roles, tracking DB; pristine-upstream
  baseline; fork vendored into `src/` with GUI stripped.
- **M2 / M3** — `recognize-text` + `delete-object`; `add-text` (LTR).
- **M4 / M5** — RTL write (FriBidi + HarfBuzz + Type0/Identity-H + ToUnicode +
  `/ActualText`); RTL search (normalization + bidi inversion).
- **M6 / M7** — golden tests + ASAN/UBSAN + format gate; `0.1.0`.
- **M8 / M8.1** — forms + signatures; real-world compatibility sweep.
- **M9 (Wave 1)** — extraction fidelity (P1/P2), page ops, hosted CI,
  deterministic packaging, tracked benchmark.
- **M10 (Wave 2)** — cross-item search, redaction, fuzz harness + fixes,
  render argument validation (F#1/F#2).
- **M11** — history scrub, SECURITY/CODEOWNERS, branch ruleset, `0.2.0`.
- **M12 / M13** — GUI restored behind `ALBDF_BUILD_GUI`; GUI RTL wiring;
  `0.3.0`.
- **M14** — RTL appearance streams (FreeText + form fields), upstream sync,
  multi-platform release workflow; `0.4.0`.
- **v3 hygiene pass** — README/CHANGELOG/docs, root build shim, CI green,
  metadata + releases normalized (2026-09-17).

---

## Appendix B — Open questions

- Should the tracking DB live in git (e.g. a dedicated `handoff` branch) or
  stay gitignored with a reproducible `seed.py`? (R0.1 forces the decision.)
- Should `REPO_MAP.md` remain committed or be generated on demand? (R5.2.)
- Does the project want `v`-prefixed tags (`v0.5.0`) for a conventional
  release URL scheme? Current tags are unprefixed; titles are `albdf X.Y.Z`.
