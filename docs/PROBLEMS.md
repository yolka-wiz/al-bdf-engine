# albdf — Known Problems, Future Outlook & Catches

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
  - [CLI robustness (fuzz harness findings)](#cli-robustness-fuzz-harness-findings)
- [What the future holds](#what-the-future-holds)
  - [Planned (post-M7)](#planned-post-m7)
  - [Design debts to repay](#design-debts-to-repay)
  - [Quality backlog](#quality-backlog)
- [The catches (operational traps)](#the-catches-operational-traps)
  - [Container is ephemeral](#container-is-ephemeral)
  - [Lifecycle guard crashes (Hermes)](#lifecycle-guard-crashes-hermes)
  - [vcpkg / toolchain traps](#vcpkg--toolchain-traps)
  - [Platform-portability traps](#platform-portability-traps)
  - [Release-packaging traps (W7)](#release-packaging-traps-w7)
  - [GitHub Actions runner traps](#github-actions-runner-traps)
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
| P2 | Decomposed marks duplicate base letter | Arabic yeh (U+064A) decomposes into base + dot; both glyphs share the base's cluster | extraction shows `علييكم` (double ي) for `عليكم`; full-phrase search of words ending in ی failed on tagged PDFs | **Fixed for search** `615b1bc`: normalizer collapses `یی`/`ی ی`/`ی <space> <letter>` (phantom-space from zero-width duplicate); `/ActualText` still exact |
| P3 | Vertical mark offsets dropped | TJ spacing can't express y-offset; zero-width marks emit inline at baseline | diacritics render at baseline, not above the letter | **Fixed** `109a4af` (emit per-glyph `Ts` text rise; sign from mark ink position), plus `9a4587d` (zero-advance marks no longer create phantom spaces in flow) |
| P4 | Foreign PDFs with broken ToUnicode (glyphs → C0 control chars U+0001/U+0002) break add-text/delete-object | content editor serializes content streams through XML; QXmlStreamReader rejects control chars → "Invalid XML text" | pages 1/2/10 of Elsevier 2025-2.pdf failed add-text | Fixed `0edbb03`: sanitize invalid XML chars (→ U+FFFD) in `createItemsAsText`; glyphs intact in PDF |
| P5 | Rebuilt content streams use scientific notation (`1.5e-05`) | `QTextStream` defaults to SmartNotation when the builder writes floats | strict PDF parsers reject `1.5e-05`; e.g. mirava.pdf output would not open in strict tools | Fixed `591dfee`: `FixedNotation` + precision 8 in `PDFPageContentEditorContentStreamBuilder` |
| P6 | Invalid short `/CIDToGIDMap` array rendered WRONG GLYPHS in every strict renderer | emitter wrote `[0 1173 728 1266]` (one entry per glyph); PDF 32000-1 §9.7.4.3 requires 65536 entries for 2-byte-CID fonts, or a stream. Strict renderers (Ghostscript) and PDF4QT's stream-only parser (`isStream()`) both fell back to Identity → code 2 painted GID 2 = Latin 'A' | ALL albdf RTL output showed Latin glyphs instead of Arabic/Persian/Hebrew in Ghostscript and albdf's own renderer; golden tests missed it because they never pixel-verify albdf's own RTL output | **Fixed** `74a1166`: emit a 65536-entry Flate stream (code→gid, unused = 0). Unblocked P3 (pixel tests were measuring 'A', not the mark) |

**Design decision:** these are intentional. The RTL engine prioritizes (1) correct
search and (2) spec-valid PDF over perfect glyph-positioning in v1.

### RTL rendering limitations

- **R#1** — RTL font is merged into page resources under a **free F<N> key** (was
  hardcoded F2 — fixed `0edbb03`). The key scan and the font merge both resolve
  INDIRECT `/Resources` references (e.g. `/Resources 29 0 R`); real-world PDFs use
  arbitrary font keys (F1/F2/F3/...) and indirect resources. **Verified against
  318.pdf** (Persian financial doc): RTL font lands at F4, original F1/F2/F3 intact.
- **R#2** — HB≥4 emits RTL runs **leftmost-first**; the engine does NOT reverse glyphs. Re-applying the fpdf2 #1802 reversal *mirrors* the text (fixed in `9d7d710`, regression-tested). Do not "fix" this.
- **R#3** — Arabic presentation-form shaping in `fribidi_log2vis` (both system and vcpkg 1.0.16) requires a **second NFKC pass after inversion** in search. Removing it breaks Arabic search.
- **R#4** — ~~the content editor (`PDFPageContentEditorProcessor`/text-flow rewrite) does **not preserve `/ActualText` marked-content** when a second `add-text` (LTR or RTL) rewrites the same page's content stream. Extraction of RTL text added by a *previous* add-text then degrades ligatures (`لا` → `ل`) on that page. The `/ActualText` overlay (P1, `pdftextlayoutgenerator.cpp`) restores full ligatures for RTL-only documents; mixed add-text on one page still degrades. Root cause: the upstream editor model has no marked-content element type. Fixing it is a content-editor change (upstream-derived, format-gate-exempt) — tracked as a future item, do not attempt in the P1/P2 wave.~~ **Resolved (2026-08-07, M13):** `PDFPageContentEditorProcessor` now overrides `performMarkedContentBegin/End`, records `/ActualText` spans on the text element's `Item` list, and `PDFPageContentEditorContentStreamBuilder` re-emits `/Span << /ActualText <...> >> BDC ... EMC` around the text section. RED regression `1d7b8c69`; fix `80548d8c`; mixed-case expectation updated (`e7c1a8eb`). Verified: second LTR add-text on an RTL page keeps the full lam-alef (`ملاس`).
- **R#5** — ~~FreeText annotations with RTL contents render as tofu: their appearance stream is generated by the QPainter/QPdfWriter path (`PDFContentStreamBuilder` inside `updateAnnotationAppearanceStreams`), which can only emit base-14 Helvetica — a QPainter cannot embed a Type0/FontFile2 font dict into the AP `/Resources`, so Arabic glyphs are missing in any renderer.~~ **Resolved (2026-08-08, M14):** `PDFDocumentBuilder::updateAnnotationAppearanceStreams` now has an additive early-return RTL branch for `PDFFreeTextAnnotation` (`pdfdocumentbuilder.cpp`): when `/Contents` `isRightToLeft()` and the builder has TTF font data (`setRtlFreeTextFontData`), the AP `/N` form stream is generated directly via `PDFRTLTextEngine::create` + `replaceObjectsByReferences` + `mergeTo`, mirroring the highlight-annotation pattern — the form's `/Resources /Font` embeds a Type0 font whose descendant CIDFontType2 `/FontDescriptor` carries `/FontFile2`. LTR contents fall through to the unchanged QPainter path (byte-identical). RED test `071ec87a` (`UnitTestsRtlFreeText::test_freetextRtlAppearanceEmbedsFont`, walks `/AP /N` → `/Resources /Font`); impl `a6ce8e87`, test-walk + format fix `507ff443`. Verified: full suite 17/17 green.
- **R#6** — ~~headless `form-fill` emits **no appearance stream at all** for filled form fields (not even tofu): the widget AP chain ends in core's `PDFFormManager::drawFormField`, a `Q_UNUSED` no-op (real drawing exists only in the optional GUI `PDFWidgetFormManager`), so `parameters.boundingRectangle` stays invalid and no `/AP` is merged. Any filled field renders blank in strict viewers, RTL or not.~~ **Resolved (2026-08-08, M14):** text form fields whose `/V` value is right-to-left now get a real appearance stream generated **directly** by `PDFDocumentBuilder::updateAnnotationAppearanceStreams` (bypassing the dead painter path) via `PDFRTLTextEngine` — embedded Type0 TrueType subset + shaped fragment, wrapped in a Form XObject (`/BBox` = widget rect, `/Resources /Font /F2`) and attached as `/AP << /N ... >>` + `/Rect`. Font source: new `form-fill --font <ttf>` option (mirrors `add-text --font`; missing file → exit 7). `/Q` quadding (1 = centered, 2 = right-anchored) and DA font size (0 = auto → 12) are honored; LTR fields are intentionally untouched (still no AP — upstream behavior). Regression `test_formFillRtlAppearance` in `UnitTestsFormSignature` asserts `/AP` + `/FontFile2` bytes in the output; manual probe verified byte-deterministic output.

### Search limitations

- **S#1** — ~~matches within a single text item only (no cross-item/cross-line spans)~~ **Resolved (2026-08-05, `6b26d14`):** the engine now searches a per-page joined visual string built from the text flow — items clustered into lines by y-center, sorted by x ascending (visual order), joined with a geometry-aware separator (touching runs `""` for mid-word splits, word-sized gaps `" "`, lines/columns `"\n"` as a hard boundary). Matches map back to `(itemIndex, charBegin..charEnd)` spans; `Match::spans` carries them and the CLI Item column shows `first+last` (e.g. `2+3`). Far-apart items cannot false-match (guarded by the `\n` boundary + word-gap threshold; regression test `test_crossItemNoFalsePositive`). Cross-**line** spans remain out of scope (a `\n` boundary); cross-item word-splits now work. **Field context:** observed 2026-08-04 that our own `add-text --rtl` output can split at a word boundary on dense pages — the Layout flow algorithm (upstream `PDFDocumentTextFlowFactory`) merges the first word of the added run into the surrounding column item (reading-order continuity) while the rest forms its own item; symptom was `search-text "تست نهایی"` = 0 matches on modified `کالا.pdf` / `پروژه نهایی.pdf` while each word matched. Deterministic regression lives in `UnitTestsSearchText::test_crossItemPhrase` via the `searchFlow()` test seam (CLI add-text cannot force a 2-item split on synthetic pages — docstrum merges adjacent runs).
- **S#2** — mixed LTR+RTL same-run handled, but the cluster-to-char mapping relies on `hb_buffer_add_utf16(item_offset=run.begin)` returning **absolute** clusters — the engine must NOT re-add `run.begin`. This was a real data-loss bug (`aa92bee`).
- **S#3** — ~~document-level search (`PDFTextSearchEngine::search`) could not find words containing lam-alef (e.g. `سلام`) in add-text --rtl output: the query collapsed `لا` in LOGICAL order while the extracted flow text is VISUAL order, where the ligature appears as the reversed pair `ال` — 3-char query vs 4-char flow never matched. The GUI search adapter (M13) exposed this (was invisible to CLI tests, which never searched a lam-alef word at document level).~~ **Resolved (2026-08-07, M13):** `PDFRTLTextNormalizer::Options::visualOrder` collapses the visual pair too; the engine enables it on the flow side only (`4cabbf7a`). Regression `test_engineSearchSalam` (in `UnitTestsSearchText`) pins the document-level path.

### CLI robustness (fuzz harness findings)

Found by `scripts/fuzz.sh` (M10 wave-2, DB #30; seed 20260806, 2026-08-06).
Both are reproducible on **valid** input with abusive CLI args — none of the
500 corpus-mutation cases (truncation/zero-fill/flips/garbage/empty/junk)
crashed any command; the parser and library are robust to malformed bytes.
Do **not** fix these in a fuzz-harness dispatch — they are a separate fix task.
Reproducers: run the harness with `--keep` and read `fail/*.cmd` + `fail/*.log`.

| # | Finding | Repro command (on any valid PDF) | Evidence |
|---|---|---|---|
| F#1 | `render` aborts (SIGABRT) on out-of-range page numbers | `albdf render doc.pdf --page-first 0 --page-last 1 --image-format png --image-res-dpi 72 --image-output-dir <dir>` — also `--page-last 999999999` with a valid `--page-first` | exit 134; stderr: "Qt Concurrent has caught an exception thrown from a worker thread … `std::out_of_range` … `vector::_M_range_check: __n (which is 18446744073709551615) >= this->size() (which is 5)`". Page 0 becomes `(size_t)-1` via an unchecked page-index decrement; an out-of-range `--page-last` trips the same `at()`/range check inside a Qt Concurrent worker, where the exception cannot be caught → `terminate`. |
| F#2 | `render` never completes at extreme DPI (resource exhaustion) | `albdf render doc.pdf --page-first 1 --page-last 1 --image-format png --image-res-dpi 10000 --image-output-dir <dir>` (999999 DPI also hangs) | 612×792 pt page at 10000 dpi ≈ 85k×110k px; 999999 dpi ≈ 94 GP. No size sanity check before allocating/render — effectively a hang/OOM (DoS via CLI). Harness classifies as HANG (exit 124). |

`--page-first -1` and `--image-res-dpi 0`/negative are rejected or tolerated
without crashing; only the above two shapes are findings.

---

## What the future holds

### Shipped (post-M7, M8)

1. **Forms** — `form-list`/`form-fill` shipped (`3b3e563`, `b8efe4a`); AcroForm tree walk + value set + appearance regeneration.
2. **Signatures** — `sign` (PKCS#7 detached, PAdES byte-range) + `verify-signatures` shipped (`02b8719`); tamper detection tested.

### Planned (post-M8)

From `plans/PLAN.md` / the DB / the product brief:

1. **Page ops** — deeper expose of `unite`/`separate` (rotate, reorder, delete page).
2. **Redaction** — `redact` exists upstream; wire object-level redaction.
3. **GUI** — deliberately deferred (ADR-0002). A thin Qt shell consuming `Pdf4QtLibCore`.

### Design debts to repay

- **D#1** — `src/cli/` and `src/core/` were empty legacy scaffold dirs (never populated; seed references them). Resolved: dirs no longer exist; CLI lives in `src/PdfTool/`.
- **D#2** — `tools/` was an empty legacy scaffold. Resolved: directory removed.
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
  floats, `591dfee`) — cross-validated.
- Scanned PDFs (PASSIVE.pdf) correctly report no text (OCR out of scope).
- `search-text` on foreign PDFs works with logical Persian queries; 0-match
  results were traced to test-query typos (ک vs گ) or empty-AcroForm docs,
  not engine bugs.
- The 318.pdf / 2025-2.pdf / Book1 edge cases (P4, R#1, P2) that surfaced in
  earlier field testing remain the canonical real-world regressions.

### Quality backlog

- ~~Perf smoke is manual (`scripts-tmp/m7-perf.sh`); promote to a tracked benchmark.~~ **Resolved** `cec5e59`: `scripts/benchmark.sh` is the tracked benchmark — deterministic inline 1000-page fixture, M7 metrics (info/fetch/render/search/delete) with result assertions, `albdf-benchmark-v1` machine-readable summary, and a documented threshold policy (`ALBDF_PERF_THRESHOLD_MS`, default 5000 ms; breaches fail the run). Runs on every PR as a non-gating CI job (`benchmark`, continue-on-error, summary uploaded as artifact); locally: `bash scripts/benchmark.sh`.
- ~~ASAN job is in CI but not upstreamed to a hosted runner; runs only on the dev container.~~ **Resolved** `d254041`: the ASAN/UBSAN Debug build + ctest now runs as the `asan` job on ubuntu-24.04 hosted runners (`.github/workflows/ci.yml`), driven by `ci/run-ci.sh --skip-release --skip-format`.
- Golden-image regeneration is manual; document the exact `--image-format png` + commit flow in `src/tests/AGENT.md`.

---

## The catches (operational traps)

### Container is ephemeral

- The dev container's OS layer (apt Qt6, build tools, fonts, nodejs) is **wiped on restart**; only `/workspace` (the repo checkout + vcpkg + venv) survives. `/tmp` is wiped too.
- After a restart, re-run the toolchain restore script (re-applies apt mirror + Qt + tools + fonts). sshd auto-restores; the container **IP may change** — if SSH breaks, check the container manager and update the SSH host.
- Everything lives in the repo checkout (bind mount) — no code is lost on restart, only tooling.

### Lifecycle guard crashes (agent tooling)

The agent runtime's lifecycle guard crashes with exit -1 on inline terminal commands that reference:
- a **literal binary path** in the command (reads the ELF as a script), e.g. `/usr/bin/time`, `src/build/bin/qt_ref`
- **inline `$(...)` command substitution**
- **non-ASCII bytes** (Hebrew) in referenced scripts

**Workarounds:** variable indirection (`B=/path; $B`), `\u`-escaped pure-ASCII scripts, or drive via `execute_code` + `hermes_tools`. **Rule:** put any complex command in a script file under a scratch dir and run `bash script.sh`.

### vcpkg / toolchain traps

- **Qt `QFlags` is 32-bit only until Qt 6.9.** `QFlags<Enum>` static-asserts
  `sizeof(Enum) <= sizeof(int)` (qflags.h:54). Adding an `Option` enum flag
  above `0x80000000` (bit 31) breaks the build on Qt 6.8/6.10. CI pins Qt 6.8
  and the dev container ships 6.8.2 — both reject 64-bit enums. Fix used:
  `4bb644f` replaced `Q_DECLARE_FLAGS(Options, Option)` with a minimal 64-bit
  `Options` class (same `testFlag`/`operator|` surface) in
  `pdftoolabstractapplication.h`. **If you add CLI commands, keep flags within
  32 bits or extend the Options class — never reintroduce `Q_DECLARE_FLAGS`
  with wide values.** Also: the page-ops milestone (`4402b1c`) never compiled
  on this toolchain — a stale pre-merge `albdf` binary can make CI look green
  locally; always rebuild from clean before trusting a baseline.

- **GitHub-hosted CI toolchain traps (all fixed 2026-08-07 in
  `.github/actions/setup-toolchain/action.yml`):**
  - vcpkg's `vcpkg` binary is NOT in the git repo — must run
    `bootstrap-vcpkg.sh` after clone or every job dies with exit 127.
  - Qt 6.8 aqt install: `modules: qtsvg` is invalid (qtsvg ships in qtbase for
    6.8); use `--archives qtbase qtsvg` and pin the exact version — the
    `6.8.*` wildcard intermittently fails with "The packages ['qt_base'] were
    not found while parsing XML" (miurahr/aqtinstall#769) depending on which
    mirror edge the runner hits; a 3× retry loop rides through it.
  - **Qt on aarch64 needs the per-arch aqt HOST (R6.1, fixed):** aqtinstall
    treats arm64 Linux as a separate *host*, not as an arch of `linux`
    (`aqt/installer.py`: `os_name == "linux"` → `linux_gcc_64`). So
    `aqt install-qt linux desktop 6.8.3 linux_gcc_arm64` reads the linux_x64
    index — which has **no** arm64 entries at all — and fails with the very
    same "packages ['qt_base'] were not found" message on *every* mirror and
    every retry. It is deterministic, not flaky: `linux_gcc_arm64` exists only
    in the `linux_arm64` repository. Use `aqt install-qt linux_arm64 desktop …`
    for that arch (the action now derives `QT_HOST` from `qt-arch`). All three
    of the 0.4.0 retries failed this way, which is what sank the
    linux-aarch64 release leg.
  - The Qt 6.8.3 online binaries need **ICU 73**, but ubuntu-24.04 ships ICU
    74 (ABI-incompatible, `ucnv_reset_73` undefined). No distro has ICU 73 —
    build it from the ICU release tarball (~1 min) **before** the aqt install
    (aqt's post-install check runs qmake, which needs ICU on LD_LIBRARY_PATH).
  - The project includes `<fontconfig/fontconfig.h>` (pdffont.cpp) — install
    `libfontconfig1-dev libfreetype-dev` in the action.

- **Benchmark comma-thousands trap (fixed `d009c4b`):** the 1000-page fixture's
  `info` output is `Page count 1,000` (locale thousands separator) but
  `scripts/benchmark.sh` compared the raw awk field against `"1000"` — every
  op asserted FAIL at ~30-90ms (well under the 5000ms threshold; not a perf
  regression). Strip commas in the parses. Also: benchmark now defaults
  `QT_QPA_PLATFORM=offscreen` internally so it never aborts (exit 134) on a
  headless box.

- **apt's CMake is too old for vcpkg's own scripts — and only arm64 notices.**
  ubuntu-24.04 ships cmake **3.28.3**, but vcpkg-scripts 2026-07-29 calls
  `string(JSON ... STRING_ENCODE)` in `scripts/cmake/z_vcpkg_spdx.cmake`, and
  that mode does not exist even in CMake **4.2.3**
  (`string sub-command JSON got an invalid mode 'STRING_ENCODE'`). The failure
  is asymmetric and therefore easy to misread as an arm64-only bug:
  - **x86_64 leg:** invisible. vcpkg sees the system cmake is too old
    ("A suitable version of cmake was not found (required v4.4.0)"), downloads
    its own 4.4.0 to run ports, and builds fine.
  - **linux-aarch64 leg:** fatal. vcpkg has no CMake asset to fetch for
    arm64-linux, silently falls back to the system 3.28.3, and
    `asmjit:arm64-linux` dies with `BUILD_FAILED`. The tell is the generated
    issue body saying **`CMake Version: 0`** — vcpkg could not even identify
    the cmake it was forced to use.
  Fix: both Linux legs now install Kitware CMake **4.4.0** (arch-mapped from
  `RUNNER_ARCH`) and skip apt's cmake — the same version vcpkg already uses for
  its ports on x86_64. Verified on an aarch64 host using the exact asset and
  `--strip-components=1` layout: 4.4.0 accepts `STRING_ENCODE`, while apt's
  4.2.3 on the same image reproduces the error verbatim. Note the trap bites
  any future old-CMake CI base, not just arm64.

### Platform-portability traps

- **libc++ has no PSTL: the `std::execution::*` policies are undeclared on
  macOS.** AppleClang's libc++ ships `<execution>` but not the parallel-algorithm
  policies, and defines neither `__cpp_lib_execution` nor
  `__cpp_lib_parallel_algorithm`; libstdc++ on Linux defines both. Any use of a
  policy therefore compiles on Linux and fails on macOS with
  `error: no member named 'seq' / 'par' in namespace 'std::execution'` — how the
  0.4.0 macOS release legs died (both arches). Fix: guard with
  `#if defined(__cpp_lib_execution) && __cpp_lib_execution >= 201603L` and fall
  back to the plain serial call. For `seq` that is the identical operation; for
  `par` the result is identical but the loop runs single-threaded on macOS.
  - Vendored files carrying the guard (CRLF, from `a52c18c`):
    `src/Pdf4QtLibCore/sources/pdfexecutionpolicy.h` (2 sites) and
    `src/Pdf4QtLibCore/sources/pdfvisitor.h` (3 sites: two `par`, one `seq`).
    Both are listed in `VENDORED_FILES` (`scripts/check-slop.sh`, U2 advisory)
    and excluded from the format gate (`ci/run-ci.sh`) so vendored CRLF code is
    never reformatted. **Re-check these guards on any re-vendor of either file**
    — a clean upstream take would drop them.
  - `grep -rn 'std::execution::' src/` is the complete inventory (5 sites in
    those 2 files). Other files include `<execution>` without using a policy and
    are fine. Fixing only the file the failing log named first is what made the
    0.4.0-class macOS break need two rounds — enumerate, don't whack-a-mole.
- **macOS `openssl` is LibreSSL, and the PKCS#12 it writes is unreadable by
  OpenSSL 3.** `/usr/bin/openssl` on macOS is LibreSSL, whose `pkcs12 -export`
  writes the certificate bag as `pbeWithSHA1And40BitRC2-CBC`. OpenSSL 3 — which
  the app links — refuses RC2 outright (legacy provider off by default):
  `error:0308010C:digital envelope routines:inner_evp_generic_fetch:unsupported:` /
  `Algorithm (RC2-40-CBC : 0)`. `UnitTestsFormSignature` mints its certificate
  fixture with the `openssl` CLI, so on macOS the two certificate-dependent
  tests (`test_signAndVerify`, `test_tamperedSignature`) fail while the other
  five pass — `5 passed, 2 failed`, with `initTestCase` still succeeding because
  LibreSSL writes the file happily. Linux has real OpenSSL 3 as `openssl`, so its
  export is PBES2/PBKDF2/AES-256-CBC and reads back cleanly. Fix: CI puts
  `$(brew --prefix openssl@3)/bin` on `GITHUB_PATH` (macOS branch of
  `setup-toolchain`) — PATH only, so build linkage is unchanged. Verified
  locally: the LibreSSL file yields 2 `unsupported` errors through OpenSSL 3,
  the OpenSSL 3 file 0. **Local macOS dev hits the same wall** — put brew's
  OpenSSL 3 first on PATH before running ctest.
- **A Linux-only PR gate hides macOS breakage.** `ci.yml` had no macOS job, so
  the break above reached a release tag unseen. The `gate-macos` job now
  builds + ctests on `macos-15` on every PR; if the release matrix keeps
  shipping macOS, keep a macOS leg in `ci.yml`.
- **The CLI could not start at all without a display server.** Qt's default
  platform plugin on Unix is xcb, which needs `$DISPLAY`; on a container, CI
  runner or headless server `QGuiApplication` aborts during construction —
  `This application failed to start because no Qt platform plugin could be
  initialized`, SIGABRT / exit 134 — before a single command runs. `main.cpp`
  (`1039f81`) now sets `QT_QPA_PLATFORM=offscreen` when neither `$DISPLAY` nor
  `$WAYLAND_DISPLAY` is set and the caller has not chosen a platform itself; an
  explicit `QT_QPA_PLATFORM` always wins, and macOS is excluded because cocoa
  works without `$DISPLAY`. CI never caught this — every job exports the
  variable, so it took running the real binary headless to see it. `ctest`
  still needs the variable (see Determinism traps). Note `main.cpp` is in the
  gate's vendored-upstream set (U2 advisory, same class as the PSTL guards):
  **re-apply this block on any re-vendor of the file.**

### Lifecycle guard crashes (agent tooling)

- blend2d has CMake recursion bugs on gcc15 + aarch64 — must come from the **vcpkg overlay** (PDF4QT pins blend2d + asmjit commits).
- FriBidi ships only pkg-config (no CMake config); HarfBuzz ships CONFIG. Both in `src/vcpkg.json` (manifest mode).
- ASAN builds need `-DCMAKE_BUILD_WITH_INSTALL_RPATH=ON` or vcpkg's install hook rejects the RPATH relink under Ninja (see `ci/run-ci.sh`).

### Release-packaging traps (W7)

- **The macOS tarball linked Homebrew Qt, so it only ran where
  `brew install qt` had already been run** — the user's symptom is
  `dyld: Library not loaded: /opt/homebrew/opt/qtbase/lib/QtXml.framework/...`.
  macOS ships no system Qt, so the bundle has to carry it:
  `scripts/macos-bundle-deps.sh` (W8) copies the referenced Qt frameworks, plain
  dylibs (fontconfig) and platform plugins into the prefix, rewrites every
  reference to `@rpath/...`, adds `@executable_path/../Frameworks` and writes
  `bin/qt.conf` (`Plugins = ../plugins`). The runtime smoke test **cannot** prove
  this — a CI runner has Homebrew Qt and loads that instead — so the script
  refuses to succeed while any `/opt/homebrew` path, or any `@rpath` reference
  that does not resolve inside the bundle, survives. `package.sh` bails if it
  does.
- **`otool -L` prints a dylib's own install name as its first entry**, so
  `tail -n +2` does not list dependencies. Treating it as one made the bundler
  copy every plugin into `Frameworks/` as well. Exclude it via `otool -D`
  (`macho_refs`). Covered by `ci/test-macos-bundle-fixture.sh`, which builds a
  fake Qt tree with clang and runs the bundler against it — no Qt install, no
  project build, so it runs in the macOS PR gate.
- **Linux deliberately ships no Qt** (W8): distro Qt is the documented
  prerequisite (`docs/PREREQUISITES.md`, also dropped into the tarball), so the
  system's copy is the only one in use and a duplicate cannot conflict. On Linux
  `qt6-svg` is **not** needed — verified by resolving the installed binary's
  `NEEDED` entries — but the platform-plugin package is (`qt6-qpa-plugins`).
- The built `albdf` carries a **build-tree RUNPATH** (`<build>/lib`) that CMake bakes in for libraries linked from the build tree. Staged installs must override it with `INSTALL_RPATH=$ORIGIN/../lib` (set in `src/CMakeLists.txt`, W7) or the release tarball leaks a machine-specific absolute path. `scripts/package.sh` verifies this with `readelf` and fails the run otherwise.
- Reproducible tarballs need `tar --sort=name --numeric-owner --owner=0 --group=0 --mtime=@$SOURCE_DATE_EPOCH` **and** `gzip -n` (plain `tar -z` lets gzip stamp the input filename + mtime into its header). `scripts/package.sh` uses both; two runs from one build produce identical sha256 (asserted during W7 testing).
- **The release workflow fires only on a `0.*` tag push.** `workflow_dispatch`
  builds and uploads artifacts but deliberately creates **no** Release, so a
  month of merged fixes ships nothing until someone tags — that is how the
  `0.4.0` release sat at 0 assets from 2026-08-15 to 2026-09-29. Tags are
  unprefixed and annotated (`0.4.1`), and three things must agree at tag time:
  `set(ALBDF_VERSION …)` in `src/CMakeLists.txt`, the tag name, and a
  `## albdf <version>` heading in `docs/RELEASES.md` — the publisher greps
  exactly that heading for the release body and silently degrades to a bare
  `albdf <version>` string when it is missing.
- **`release` must keep `if: always()`.** Gating the publisher on
  `needs.build.result == 'success'` tests the *aggregate* matrix result, so a
  single red platform skips the job and throws away every artifact the other
  legs just built. That is precisely how `0.4.0` shipped zero assets while its
  `linux-x86_64` leg had built successfully.
- **Retagging a release is only safe while `downloadCount` is 0** — check the
  API first, then `gh release delete <tag> --cleanup-tag` (removes release +
  tag together), fix the version strings, and re-tag. Never re-tag with a lower
  version than an already-published tag.
- **The `windows-x86_64` leg cannot configure**: the GitHub Windows runner has
  no `pkg-config`, and `src/CMakeLists.txt:52` calls
  `find_package(PkgConfig REQUIRED)` → `Configure + build` dies with
  `Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)` after ~26 min of
  vcpkg work. The leg was dropped from the matrix in `0.4.1`; restoring it
  needs a `pkg-config` on the runner (R6.4).

### GitHub Actions runner traps

- **Node 20 actions were running on a borrowed runtime.** The runner force-runs
  any action declaring `runs.using: node20` on Node 24 and prints
  `##[warning]Node.js 20 is deprecated. The following actions target Node.js 20
  but are being forced to run on Node.js 24: actions/cache@v4,
  actions/checkout@v4.` ([GitHub changelog](https://github.blog/changelog/2025-09-19-deprecation-of-node-20-on-github-actions-runners/),
  removal date updated to **2026-09-23**). That date has passed, so those pins
  were on borrowed time: a green run proved nothing about the next one.
- **Fixed in R6.5 — the workflows pin exact tags on Node 24-native lines**,
  picked for durability (the oldest line that is *still receiving releases*,
  never the newest major) and verified input-by-input against each action's own
  `action.yml`:

  | Action | Pin | Why this line |
  |---|---|---|
  | `actions/checkout` | `v5.1.0` | Oldest Node 24 line and still backported (released in lockstep with `v6.1.0`/`v7.0.1`). Its `[BREAKING] allow-unsafe-pr-checkout` backport only affects `pull_request_target`/`workflow_run`; this repo uses `pull_request` + tag push |
  | `actions/cache` | `v5.1.0` | Node 24, and the most-patched Node 24 line (`v5.0.0` → `v5.1.0`, released alongside `v6.1.0`). Avoids the `v6` ESM rewrite in a component that needs no churn |
  | `actions/upload-artifact` | `v7.0.1` | **`v5.0.0` is still Node 20** — `v6` is the first Node 24-by-default line. `v7` adds only an *opt-in* `archive: false` direct upload (the default still zips) plus ESM internals, which are transparent to callers |
  | `actions/download-artifact` | `v8.0.1` | **`v5` and `v6` are still Node 20**; `v8` is the current line. Its new fail-closed digest check (`digest-mismatch: error` by default) is *wanted* here — a release publisher must fail rather than attach a corrupted tarball. Escape hatch if it ever misfires: `digest-mismatch: warn` |

- **Inputs**: the pinned tags carry every input this repo passes —
  `fetch-depth` (×9); `path`/`key`/`restore-keys`;
  `name`/`path`/`if-no-files-found`; `path`/`merge-multiple`. Node 24 also
  needs runner ≥ 2.327.1, which hosted runners already satisfy (these actions
  were already executing on the forced Node 24 shim before the pin change).
- **Bumping them later**: read the candidate tag's `action.yml`
  (`gh api repos/<owner>/<action>/contents/action.yml?ref=<tag>`) and confirm
  `runs.using: node24` plus the inputs above. Prefer the oldest line that still
  gets releases over the newest major. A workflow-file push must go over the
  **SSH** remote: the `yolka-wiz` PAT has no `workflow` scope and GitHub
  rejects such pushes outright.
- **Tooling trap found while verifying this**: `gh pr checks --json` does not
  exist before gh 2.55 (Debian ships 2.46). It fails with `unknown flag: --json`
  **on stderr**, so a watcher redirecting stderr to `/dev/null` sees empty
  stdout and concludes "no checks yet" forever. Use `gh run view <id> --json`
  or the REST API instead.

### Determinism traps

- `ctest` **must** run with `QT_QPA_PLATFORM=offscreen` exported, or tests abort (not a real failure).
- No `QDateTime::currentDateTime()` / timestamps in document output — byte-stable output is a hard requirement and tests depend on it.
- Golden PNGs are byte-verified by hash; regenerate only with an explicit reviewed commit.
- `encrypt` seeds `QRandomGenerator::securelySeeded()` (correct cryptography), so its output is intentionally **not** byte-stable. Tests must not hash `encrypt` output (`docs/coding-standard.md` §D2).

### Redaction / render quirks (M10, DB #29)

- `albdf redact <in> <out>` derives regions **only** from `/Subtype /Redact` annotations in the source PDF — there is no `--page`/rect flag. A fixture without Redact annotations redacts nothing (verified: `multipage.pdf` unchanged).
- `PDFRedact` rebuilds pages through `QPdfWriter` (glyphs → vector curves): the redacted output has **no text layer** — `fetch-text` returns empty. Assert redaction with **render + pixel sampling** (regions render as solid black bars; public content outside regions keeps its black-pixel fraction), never fetch-text-after.
- `albdf render --image-output-dir <dir>` fails with exit 7 if `<dir>` does not already exist (`Target directory '<dir>' doesn't exist.`). Tests must pre-create the directory (QTemporaryDir) — `tst_pageopstest.cpp` and `tst_redacttest.cpp` both rely on this.
- Redact exit codes (verified): missing output positional → `ErrorInvalidArguments` (7); nonexistent input → `ErrorDocumentReading` (4). Redaction bars cover the full `/Rect` (55/56 fully-black rows at 72 dpi; scanline fraction ≥ 0.95).

### Fork-hygiene traps

- **Never reformat vendored upstream files** — it destroys cherry-pickability. clang-format gate applies to authored files only (git diff from fork base `a52c18c`).
- Keep the upstream PDF4QT as a git remote for cherry-picking.
- `vendor-upstream-pdf4qt/` is gitignored (working reference only).

### Agent-workflow traps

- **Evidence gate**: a task is not done until `db.py task-done <id> --ref <sha>` — closing without a real commit sha is the #1 anti-pattern. The DB column is `evidence_ref`; components are matched by display title, not slug.
- **Max 3 parallel subagents** — operating rule. Dispatch in batches.
- Subagents hit their tool-call budget mid-task **regularly** (M2 batch: all three did). The final summary is a handoff, not a delivery — the orchestrator verifies files exist, build is green, tests pass, and finishes the work.
- Parallel agents clobber shared files (e.g. two patching the same CMakeLists). Instruct "stage only your own paths, never `git add -A`".
- Embed **pre-verified API contracts** in dispatch briefs (exact class/method names, headers, code patterns) — subagents otherwise burn their budget re-reading headers.
- The git committer is `Yolka <yolka@albdf.local>` — use `git -c user.name="Yolka" -c user.email="yolka@albdf.local" commit`.

---

## How to add to this document

- Discovered a new trap, limitation, or future item? Add a row/line to the matching table/section.
- Keep it **terse and actionable** — one line of root cause + one line of workaround.
- Reference commit shas when a fix or decision pins the behavior (e.g. `aa92bee`, `9d7d710`).
- If it changes a rule agents rely on, also update `AGENTS.md` / the relevant `AGENT.md`.
