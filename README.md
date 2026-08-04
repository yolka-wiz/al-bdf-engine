# pdfedit

Headless PDF editing **library + CLI** for Linux — fork of [PDF4QT](https://github.com/JakubMelka/PDF4QT) (MIT, our additions **GPL-3.0-or-later**).
The reader/editor core behind a future GUI. **RTL (Arabic/Persian/Hebrew) text write + search** is the differentiator.

**Status:** v0.1.0 released. M0–M7 complete (fork proven, recognize/delete/add-text/RTL-write/RTL-search shipped, CI green).

---

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
  - [add-text (incl. RTL)](#add-text)
  - [recognize-text](#recognize-text)
  - [delete-object](#delete-object)
  - [search-text (RTL-aware)](#search-text)
  - [Other commands](#other-commands)
- [For AI agents contributing](#for-ai-agents-contributing)
  - [Start here](#start-here-required-reading)
  - [Repo layout](#repo-layout)
  - [AGENT.md guide map](#agentmd-guide-map)
  - [Build & test](#build--test)
  - [The tracking database](#the-tracking-database)
  - [Determinism & coding rules](#determinism--coding-rules)
  - [Known limitations](#known-limitations)
- [Testing & CI](#testing--ci)
- [License](#license)

---

## Installation

### Prerequisites
- CMake ≥ 3.25, Ninja, GCC/Clang (C++20)
- Qt 6.8+ (Core/Gui/Xml/Svg/Test — **no Widgets/QML**; 6.10.2 tested)
- [vcpkg](https://github.com/microsoft/vcpkg) with deps in `src/vcpkg.json` (HarfBuzz, FriBidi, FreeType, OpenJPEG, OpenSSL, TBB, LCMS2, zlib, libjpeg-turbo, libpng, blend2d)

### Option A — your own machine
```bash
git clone <repo-url> pdfedit && cd pdfedit/src
export VCPKG_ROOT=/path/to/vcpkg
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DPDFEDIT_BUILD_TESTS=ON
cmake --build build
```

### Option B — dev container (recommended, reproducible)
```bash
docker build -t pdfedit-dev -f Dockerfile .
docker run -it --rm -v $(pwd):/workspace/pdfedit -w /workspace/pdfedit pdfedit-dev
# inside: cmake + build + test as above (toolchain already at /workspace/vcpkg)
```
The Dockerfile installs Qt 6 + vcpkg deps + all tools, sets `QT_QPA_PLATFORM=offscreen`, and provides the `ci/run-ci.sh` gate.

### Verify
```bash
build/bin/PdfTool --version        # → PdfTool 0.1.0
QT_QPA_PLATFORM=offscreen ctest --test-dir build   # → 100% passed (10 tests)
```

---

## Usage

All commands follow `PdfTool <command> <args>`. Exit codes: **0** success, **7** invalid arguments.
RTL fonts (OFL) ship in `src/tests/fonts/` (Vazirmatn, Noto Naskh Arabic, Noto Sans Hebrew).

### add-text

```bash
# LTR — standard Helvetica, no font embedding
PdfTool add-text in.pdf out.pdf --page 1 --x 72 --y 700 --text "Hello" --size 18

# RTL — FriBidi + HarfBuzz + embedded TrueType font
PdfTool add-text in.pdf out.pdf --page 1 --x 72 --y 700 \
        --text "سلام دنیا" --size 24 --rtl \
        --font src/tests/fonts/Vazirmatn-Regular.ttf --lang fa
```
Coordinates are PDF points (origin bottom-left). `--lang` ∈ `fa|ar|he|ur`.

### recognize-text

```bash
# List page content objects (text/image/path) with bounding boxes.
# The printed index is the address used by delete-object.
PdfTool recognize-text in.pdf
```

### delete-object

```bash
# Delete a whole content object by page + index (see recognize-text).
PdfTool delete-object in.pdf out.pdf --page 1 --index 3
PdfTool delete-object in.pdf --page 1 --list      # list without modifying
```

### search-text

```bash
# RTL-aware, logical-order query; normalization ON by default.
PdfTool search-text in.pdf "سلام"
PdfTool search-text in.pdf "123"                   # matches ۱۲۳ (digit unification)
PdfTool search-text in.pdf "محمد" --no-normalize   # exact match only
PdfTool search-text in.pdf "hello" --case-sensitive
```
Normalization strips tashkeel/ZWNJ/ZWJ, folds presentation forms & lam-alef, unifies Persian/Arabic letters and digit sets. Output: page / item / bounding box / matched text.

### Other commands

`render`, `fetch-text`, `info`, `info-fonts`, `info-inks`, `unite`, `separate`, `redact`, `encrypt`, `decrypt`, `optimize`, `xml`, `statistics`, `diff`, `attachments`, `cert-store`, `verify-signatures`, `remove-external-links`, `benchmark`, … — run `PdfTool help` for the full list.

---

## For AI agents contributing

### Start here (required reading)
1. **`AGENTS.md`** (repo root) — the binding contract. Read it fully first; it overrides general habits.
2. **`docs/coding-standard.md`** — C++20/Qt style, determinism, TDD, git rules.
3. **This README** — layout, build, commands.
4. **`plans/PLAN.md`** — milestone roadmap and exit criteria.
5. **The tracking DB** — every component/task/decision/question is tracked.

### Repo layout

```
AGENTS.md                  binding contract for every agent (read first)
README.md                  this file
.clang-format              enforced style (LLVM base, 4-space, 120-col)
Dockerfile                 reproducible dev container
ci/run-ci.sh               CI gate: build + ctest + ASAN/UBSAN + clang-format
db/                        tracking DB (schema.sql + seed.py committed; pdfedit.db gitignored)
scripts/db.py              tracking DB CLI
plans/PLAN.md              master roadmap (M0–M7)
docs/                      coding standard, ADRs, research, release notes, man page
agents/roles/              one markdown contract per agent role
skills/                    vendored skills (qt-cmake-project, qt-cpp-docs, qt-cpp-review)
src/                       the fork: Pdf4QtLibCore + PdfTool + UnitTests + tests
tools/                     (legacy empty scaffold — unused)
vendor-upstream-pdf4qt/    gitignored upstream clone (reference only)
```

### AGENT.md guide map

Each major directory has an `AGENT.md` onboarding guide (with its own TOC):

| Guide | What it covers |
|---|---|
| `src/AGENT.md` | source tree layout, build, RTL pipeline location, custom CLI tools, determinism, pitfalls |
| `src/Pdf4QtLibCore/AGENT.md` | the core PDF library: naming, subsystems, registering new sources, RTL quirks |
| `src/PdfTool/AGENT.md` | how to add a CLI command, output formatter + exit-code contracts |
| `src/UnitTests/AGENT.md` | how to add a unit/integration test, the QProcess helper pattern |
| `src/tests/AGENT.md` | fixtures, fonts, golden images, smoke.sh |
| `db/AGENT.md` | the tracking database: db.py commands, evidence-gated completion |
| `docs/AGENT.md` | writing docs, ADRs vs research notes, man page, context7 vendoring |

### Build & test

```bash
cd src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DPDFEDIT_BUILD_TESTS=ON
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure   # 10 tests
# Full gate (build + ctest + ASAN/UBSAN + clang-format):
bash ci/run-ci.sh            # or --skip-asan --skip-format for a fast loop
```

### The tracking database

```bash
python3 scripts/db.py status              # full status by component
python3 scripts/db.py search "rtl"        # FTS5 search across tasks/decisions/notes
python3 scripts/db.py tasks --open        # open tasks
python3 scripts/db.py task-done 9 --ref <sha>   # close a task WITH evidence (required)
```
Every task you touch must exist in the DB. **Closing a task requires `--ref <commit-sha>`** — no evidence, no close. Never commit `db/pdfedit.db`; only `schema.sql` + `seed.py`.

### Determinism & coding rules

- **Byte-deterministic output** for a given input — no timestamps/random IDs in document output.
- **Headless** — everything passes with `QT_QPA_PLATFORM=offscreen`; no window-needed tests.
- **CLI-first** — a capability exists only if reachable from a shell command.
- **TDD** — failing test first for every fix/feature; golden-image tests for anything visual.
- **Small commits** — one logical change, Conventional Commits (`feat:|fix:|test:|docs:|refactor:|chore:`), commit messages explain WHY.
- **No scope creep** — note extras in the DB as proposals, don't implement silently.
- **Never fake results** — a task isn't done until `git log` + passing test prove it.
- **clang-format** must be clean on every touched file. Vendored upstream files are exempt (keep cherry-picks clean).

### Known limitations

- ToUnicode CMap entries are one UTF-16 unit: ligatures degrade in `fetch-text` (full text in `/ActualText`; search still works via normalization).
- Decomposed marks duplicate their base letter in extraction.
- Vertical mark offsets (diacritic height) dropped in v1 rendering.
- Search matches within a single text item (no cross-item spans).

---

## Testing & CI

- **10 ctest targets**: unit, font encoding, recognize-text, delete-object, add-text, RTL add-text (incl. mirror-regression), RTL search (Hebrew/digits/ZWNJ/tashkeel/mixed-bidi corpus), golden-image render harness, CLI smoke (34 checks).
- **ASAN/UBSAN**: full suite clean.
- **clang-format gate**: authored files only (vendored upstream exempt).
- Run everything: `bash ci/run-ci.sh`.

## License

**GPL-3.0-or-later** (see `LICENSE` and ADR-0005). Our authored code is GPL-3.0-or-later;
vendored upstream PDF4QT portions keep their MIT headers (MIT is GPLv3-compatible).
Deps (all GPLv3-compatible — ADR-0005): Qt (LGPL-3, dynamic), FreeType (FTL), OpenJPEG
(MIT), OpenSSL (Apache-2.0), ZLIB, HarfBuzz (MIT), FriBidi (LGPL-2.1, dynamic), blend2d
(Zlib), TBB (Apache-2.0). RTL fonts are OFL.
