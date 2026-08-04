# pdfedit — Known Problems, Future Outlook & Catches

> Living document for agents (and humans) working on this repo. This is the
> institutional memory of the sharp edges discovered while building v0.1.0.
> If you hit something new, ADD IT HERE rather than rediscovering it.
> Keep entries terse and actionable.

---

## Table of Contents

- [Current problems](#current-problems)
  - [RTL extraction / ToUnicode limitations](#rtl-extraction--tounicode-limitations)
  - [RTL rendering limitations](#rtl-rendering-limitations)
  - [Search limitations](#search-limitations)
- [What the future holds](#what-the-future-holds)
  - [Planned (post-M7)](#planned-post-m7)
  - [Design debts to repay](#design-debts-to-repay)
  - [Quality backlog](#quality-backlog)
- [The catches (operational traps)](#the-catches-operational-traps)
  - [Container is ephemeral](#container-is-ephemeral)
  - [Lifecycle guard crashes (Hermes)](#lifecycle-guard-crashes-hermes)
  - [vcpkg / toolchain traps](#vcpkg--toolchain-traps)
  - [Determinism traps](#determinism-traps)
  - [Fork-hygiene traps](#fork-hygiene-traps)
  - [Agent-workflow traps](#agent-workflow-traps)
- [How to add to this document](#how-to-add-to-this-document)

---

## Current problems

### RTL extraction / ToUnicode limitations

These are the known degradation points in `fetch-text`/extraction of RTL text
we embed. None are silent corruption — they are spec-compliant fallbacks — but
an agent must know them before "fixing" extraction and breaking search.

| # | Problem | Root cause | Impact | Workaround today |
|---|---|---|---|---|
| P1 | Ligatures degrade in extraction | `ToUnicode` CMap destinations are one UTF-16 unit; a lam-alef ligature maps to its first letter | `fetch-text` shows `ل` where the visual glyph is `لا` | `/ActualText` carries exact logical text; search works because the normalizer collapses lam-alef |
| P2 | Decomposed marks duplicate base letter | Arabic yeh (U+064A) decomposes into base + dot; both glyphs share the base's cluster | extraction shows `علييكم` (double ي) for `عليكم`; full-phrase search of words ending in ی failed on tagged PDFs | **Fixed for search** `3660b84`: normalizer collapses `یی`/`ی ی`/`ی <space> <letter>` (phantom-space from zero-width duplicate); `/ActualText` still exact |
| P3 | Vertical mark offsets dropped | TJ spacing can't express y-offset; zero-width marks emit inline at baseline | diacritics render at baseline, not above the letter | Documented v1 limitation; needs GPOS-to-Tm or anchor machinery to fix |
| P4 | Foreign PDFs with broken ToUnicode (glyphs → C0 control chars U+0001/U+0002) break add-text/delete-object | content editor serializes content streams through XML; QXmlStreamReader rejects control chars → "Invalid XML text" | pages 1/2/10 of Elsevier 2025-2.pdf failed add-text | Fixed `0ba70c1`: sanitize invalid XML chars (→ U+FFFD) in `createItemsAsText`; glyphs intact in PDF |
| P5 | Rebuilt content streams use scientific notation (`1.5e-05`) | `QTextStream` defaults to SmartNotation when the builder writes floats | strict PDF parsers reject `1.5e-05`; e.g. mirava.pdf output would not open in strict tools | Fixed `3920a43`: `FixedNotation` + precision 8 in `PDFPageContentEditorContentStreamBuilder` |

**Design decision:** these are intentional. The RTL engine prioritizes (1) correct
search and (2) spec-valid PDF over perfect glyph-positioning in v1.

### RTL rendering limitations

- **R#1** — RTL font is merged into page resources under a **free F<N> key** (was
  hardcoded F2 — fixed `0ba70c1`). The key scan and the font merge both resolve
  INDIRECT `/Resources` references (e.g. `/Resources 29 0 R`); real-world PDFs use
  arbitrary font keys (F1/F2/F3/...) and indirect resources. **Verified against
  318.pdf** (Persian financial doc): RTL font lands at F4, original F1/F2/F3 intact.
- **R#2** — HB≥4 emits RTL runs **leftmost-first**; the engine does NOT reverse glyphs. Re-applying the fpdf2 #1802 reversal *mirrors* the text (fixed in `23dc377`, regression-tested). Do not "fix" this.
- **R#3** — Arabic presentation-form shaping in `fribidi_log2vis` (both system and vcpkg 1.0.16) requires a **second NFKC pass after inversion** in search. Removing it breaks Arabic search.

### Search limitations

- **S#1** — matches within a single text item only (no cross-item/cross-line spans). Multi-word queries spanning items miss. **Observed in the field (compat-agent-b, 2026-08-04):** our own `add-text --rtl` output can split at a word boundary on dense pages — the Layout flow algorithm (upstream `PDFDocumentTextFlowFactory`) merges the first word of the added run into the surrounding column item (reading-order continuity) while the rest forms its own item. Content stream is identical (one `TJ`, one `/ActualText` span) in both cases; the split is purely geometric. Symptom: `search-text "تست نهایی"` = 0 matches on the modified copy of `کالا.pdf` / `پروژه نهایی.pdf`, while `"تست"` and `"نهایی"` each match. Book1 (sparse page) keeps one item → phrase matches. Verified NOT an add-text defect; fixing means reworking the upstream flow builder (out of scope).
- **S#2** — mixed LTR+RTL same-run handled, but the cluster-to-char mapping relies on `hb_buffer_add_utf16(item_offset=run.begin)` returning **absolute** clusters — the engine must NOT re-add `run.begin`. This was a real data-loss bug (`d516e4e`).

---

## What the future holds

### Planned (post-M7)

From `plans/PLAN.md` / the DB / the product brief:

1. **Forms** (fill form fields) — upstream PDF4QT has form support to expose via CLI.
2. **Signatures** (sign/verify) — `verify-signatures` exists upstream; adding sign.
3. **Page ops** — deeper expose of `unite`/`separate` (rotate, reorder, delete page).
4. **Redaction** — `redact` exists upstream; wire object-level redaction.
5. **GUI** — deliberately deferred (ADR-0002). A thin Qt shell consuming `Pdf4QtLibCore`.

### Design debts to repay

- **D#1** — `src/cli/` and `src/core/` are empty legacy scaffold dirs. Either populate or delete; they confuse newcomers.
- **D#2** — `tools/` is empty. Remove or document.
- **D#3** — The `src/CMakeLists.txt.upstream.orig` reference file is noise; consider pruning once fork is stable.
- **D#4** — PDF4QT upstream renames to PDF4QT-qt6 + new naming; our fork pinned to 1.6.0.0 API. Track upstream for security fixes via the git remote.

### Compatibility sweep results (testing-temp corpus, 2026-08-04)

Two parallel agents ran the full 7-step battery (info / fetch-text /
recognize-text / search-text / add-text LTR+RTL / delete-object / render) over
the 11-file `testing-temp` corpus (Persian forms, English papers, scanned
docs, 309-page and 100-page books).

- **All 11 files pass every step.** No crashes, no corrupt output, all
  add/delete outputs reopen cleanly.
- **Both agents independently found the same P5 fix** (scientific-notation
  floats, `3920a43`) — cross-validated.
- Scanned PDFs (PASSIVE.pdf) correctly report no text (OCR out of scope).
- `search-text` on foreign PDFs works with logical Persian queries; 0-match
  results were traced to test-query typos (ک vs گ) or empty-AcroForm docs,
  not engine bugs.
- The 318.pdf / 2025-2.pdf / Book1 edge cases (P4, R#1, P2) that surfaced in
  earlier field testing remain the canonical real-world regressions.

### Quality backlog

- Perf smoke is manual (`scripts-tmp/m7-perf.sh`); promote to a tracked benchmark.
- ASAN job is in CI but not upstreamed to a hosted runner; runs only on the dev container.
- Golden-image regeneration is manual; document the exact `--image-format png` + commit flow in `src/tests/AGENT.md`.

---

## The catches (operational traps)

### Container is ephemeral

- The yolka dev container's OS layer (apt Qt6, build tools, fonts, nodejs) is **wiped on restart**; only `/workspace`, `/workspace/vcpkg`, `/workspace/.venv` survive. `/tmp` is wiped too.
- After a restart: run `/workspace/scripts-tmp/reinstall-toolchain.sh` (re-applies apt mirror + Qt + tools + fonts). sshd auto-restores; **IP may change** (Apple DHCP) — if SSH breaks, check `container list` and update `terminal.ssh_host`.
- Everything lives in `/workspace/pdfedit` (bind mount) — no code is lost on restart, only tooling.

### Lifecycle guard crashes (Hermes)

The Hermes lifecycle guard (`_read_referenced_script`) crashes with exit -1 on inline terminal commands that reference:
- a **literal binary path** in the command (reads the ELF as a script — same bug as `/workspace/vcpkg/vcpkg`), e.g. `/usr/bin/time`, `src/build/bin/qt_ref`
- **inline `$(...)` command substitution**
- **non-ASCII bytes** (Hebrew) in referenced scripts

**Workarounds:** variable indirection (`B=/path; $B`), `\u`-escaped pure-ASCII scripts, or drive via `execute_code` + `hermes_tools`. **Rule:** put any complex command in a script file under `/workspace/scripts-tmp/` and run `bash script.sh`.

### vcpkg / toolchain traps

- **Never** write `/workspace/vcpkg/vcpkg` literally inline (guard crash). Use `V="$VCPKG_ROOT/vcpkg"; $V install`.
- blend2d has CMake recursion bugs on gcc15 + aarch64 — must come from the **vcpkg overlay** (PDF4QT pins blend2d + asmjit commits).
- The apt `socks5` proxy breaks apt; the mirror is `http://mirror.iranserver.com/ubuntu/` (via `99direct`). pip needs the proxy env **unset** + `--index-url https://mirror2.chabokan.net/pypi/simple/`.
- FriBidi ships only pkg-config (no CMake config); HarfBuzz ships CONFIG. Both in `src/vcpkg.json` (manifest mode).
- ASAN builds need `-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON` or vcpkg's install hook rejects the RPATH relink under Ninja (see `ci/run-ci.sh`).

### Determinism traps

- `ctest` **must** run with `QT_QPA_PLATFORM=offscreen` exported, or tests abort (not a real failure).
- No `QDateTime::currentDateTime()` / timestamps in document output — byte-stable output is a hard requirement and tests depend on it.
- Golden PNGs are byte-verified by hash; regenerate only with an explicit reviewed commit.

### Fork-hygiene traps

- **Never reformat vendored upstream files** — it destroys cherry-pickability. clang-format gate applies to authored files only (git diff from fork base `4f46302`).
- Keep the upstream PDF4QT as a git remote for cherry-picking.
- `vendor-upstream-pdf4qt/` is gitignored (working reference only).

### Agent-workflow traps

- **Evidence gate**: a task is not done until `db.py task-done <id> --ref <sha>` — closing without a real commit sha is the #1 anti-pattern. The DB column is `evidence_ref`; components are matched by display title, not slug.
- **Max 3 parallel subagents** — operating rule. Dispatch in batches.
- Subagents hit their tool-call budget mid-task **regularly** (M2 batch: all three did). The final summary is a handoff, not a delivery — the orchestrator verifies files exist, build is green, tests pass, and finishes the work.
- Parallel agents clobber shared files (e.g. two patching the same CMakeLists). Instruct "stage only your own paths, never `git add -A`".
- Embed **pre-verified API contracts** in dispatch briefs (exact class/method names, headers, code patterns) — subagents otherwise burn their budget re-reading headers.
- The git committer is `Yolka <yolka@pdfedit.local>` — use `git -c user.name="Yolka" -c user.email="yolka@pdfedit.local" commit`.

---

## How to add to this document

- Discovered a new trap, limitation, or future item? Add a row/line to the matching table/section.
- Keep it **terse and actionable** — one line of root cause + one line of workaround.
- Reference commit shas when a fix or decision pins the behavior (e.g. `d516e4e`, `23dc377`).
- If it changes a rule agents rely on, also update `AGENTS.md` / the relevant `AGENT.md`.
