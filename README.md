# pdfedit

Headless PDF editing library + CLI for Linux. The reader/editor core behind a future GUI.

**Status:** M0–M6 shipped — fork proven, recognize/delete/add-text/RTL-write/RTL-search all working, CI green. See `plans/PLAN.md` for the roadmap.

## What this is

A fork-and-extend of **PDF4QT** (MIT, `Pdf4QtLibCore` + `PdfTool`) into a standalone
headless library and CLI, with our own additions:

- **RTL (Arabic/Persian/Hebrew) text write + search** — the differentiator; PDF4QT has none.
  HarfBuzz shaping + FriBidi bidi + embedded Type0 font + ToUnicode/ActualText, and a
  search engine with tashkeel/presentation-form/digit/ZWNJ normalization.
- **Object-level deletion** (text runs, images, other content elements) exposed via CLI.
- **Add-text** (LTR + RTL) exposed via CLI.
- Deterministic, agent-testable core: golden-image tests, headless CLI, stable output.

**GUI is explicitly out of scope.** This repo is the library + CLI only. A future GUI
(thin Qt shell or PDF4QT's existing apps) will consume this library.

## Repo layout

```
docs/            design, decisions (ADRs), coding standard, research notes
plans/           master plan + milestone breakdowns
db/              schema + seed for the SQLite tracking database
scripts/         db.py (tracking DB CLI) + other tooling
ci/              run-ci.sh — the CI gate (build + tests + sanitizers + format)
agents/          role definitions for the agent team that writes this code
skills/          skills vendored/pinned for agents
src/             the fork: Pdf4QtLibCore + PdfTool + UnitTests
src/tests/       fixtures (PDFs, RTL fonts), golden images, smoke.sh
```

## Build

Requires CMake ≥ 3.25, Ninja, Qt 6 (Core/Gui/Widgets + dev packages), and the vcpkg
deps declared in `src/vcpkg.json` (HarfBuzz, FriBidi, FreeType, ...). On the dev
container, run `scripts-tmp/reinstall-toolchain.sh` first (the OS layer is ephemeral).

```bash
cd src
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
      -DPDFEDIT_BUILD_TESTS=ON
cmake --build build
```

Binary: `build/bin/PdfTool`. Tests: `QT_QPA_PLATFORM=offscreen ctest --test-dir build`.

## CLI quick reference (our commands)

```bash
# Add text (LTR) — standard Helvetica, no embedding
PdfTool add-text in.pdf out.pdf --page 1 --x 72 --y 700 --text "Hello" --size 18

# Add text (RTL) — bidi + HarfBuzz shaping + embedded TrueType
PdfTool add-text in.pdf out.pdf --page 1 --x 72 --y 700 \
        --text "سلام دنیا" --size 24 --rtl \
        --font /path/to/Vazirmatn-Regular.ttf --lang fa

# List page content objects (text/image/path) with bounding boxes
PdfTool recognize-text in.pdf

# Delete a whole content object by its recognize-text index
PdfTool delete-object in.pdf out.pdf --page 1 --index 3

# Search (RTL-aware): logical-order query, normalization on by default
PdfTool search-text in.pdf "سلام"
PdfTool search-text in.pdf "محمد" --no-normalize      # exact match only
PdfTool search-text in.pdf "123"                      # matches ۱۲۳ too
```

Search normalization (disable with `--no-normalize`): strips tashkeel and
ZWNJ/ZWJ, folds Arabic presentation forms and the lam-alef ligature, unifies
Persian/Arabic letter forms and digit sets. Queries are given in LOGICAL
order; the engine inverts them to the visual order stored in PDF content
streams. Match output is a table of page / item / bounding box / matched text.

Rendering and inspection (from upstream PDF4QT) also work: `render`,
`fetch-text`, `info`, `info-fonts`, `unite`, `separate`, `redact`, `encrypt`,
`decrypt`, `optimize`, `xml`, and more — see `PdfTool help`.

## Testing & CI

```bash
# Full gate (build + offscreen ctest + ASAN/UBSAN + clang-format):
bash ci/run-ci.sh
bash ci/run-ci.sh --skip-asan --skip-format   # fast loop

# Manual test run
cd src && QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
```

Suite (10 targets): unit tests, font encoding, recognize-text, delete-object,
add-text, **RTL add-text** (incl. mirror-regression), **RTL search** (Hebrew,
digits, ZWNJ, tashkeel, mixed bidi), golden-image harness, CLI smoke.

Known limitations (documented in the code): ToUnicode CMap entries are
2-byte-only so ligatures degrade in `fetch-text` (full text lives in
`/ActualText`); decomposed marks duplicate their base letter in extraction;
vertical mark offsets are dropped in v1 rendering.

## How to work here (for agents AND humans)

1. **Read `AGENTS.md`** at the repo root first — it is the binding contract for every agent.
2. **Read `docs/coding-standard.md`** — style, naming, testing, commit rules.
3. **Check the tracking DB** — every component, task, decision, and question is tracked:
   `python3 scripts/db.py status` (see `scripts/db.py --help`).
4. **Follow the plan** — `plans/PLAN.md` is the master roadmap. Work in milestones.

## License

MIT (inherited from PDF4QT) unless the ADRs decide otherwise. Third-party deps:
Qt (LGPL), FreeType (FTL), OpenJPEG (MIT), OpenSSL (Apache-2.0), ZLIB, HarfBuzz
(MIT), FriBidi (LGPL-2.1, dynamically linked — see ADR-0004 / deps register).
