# albdf

**Headless PDF editing that speaks Arabic, Persian, and Hebrew.**

albdf is a headless PDF editing **library + CLI** for Linux. It is a fork of
[PDF4QT](https://github.com/JakubMelka/PDF4QT) (MIT) with first-class RTL
support — writing, searching, and extracting Arabic/Persian/Hebrew text, which
upstream lacks. Authored code is **GPL-3.0-or-later**; vendored upstream
portions stay MIT. It is the engine behind a future GUI.

## Install

### Prerequisites

- CMake ≥ 3.25, Ninja, GCC/Clang (C++20)
- Qt 6.8+ (Core/Gui/Xml/Svg/Test — **no Widgets/QML**; 6.10.2 tested)
- [vcpkg](https://github.com/microsoft/vcpkg) with the deps in `src/vcpkg.json`
  (HarfBuzz, FriBidi, FreeType, OpenJPEG, OpenSSL, TBB, LCMS2, zlib,
  libjpeg-turbo, libpng, blend2d)

### Option A — dev container (recommended, reproducible)

```bash
git clone https://github.com/yolka-wiz/al-bdf-engine.git albdf
cd albdf
docker build -t albdf-dev -f Dockerfile .
docker run -it --rm -v "$(pwd)":/workspace/albdf -w /workspace/albdf albdf-dev
```

The image installs Qt 6, the vcpkg deps, and all tools, sets
`QT_QPA_PLATFORM=offscreen`, and ships the `ci/run-ci.sh` gate.

### Option B — local build

```bash
git clone https://github.com/yolka-wiz/al-bdf-engine.git albdf
cd albdf
export VCPKG_ROOT=/path/to/vcpkg
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DALBDF_BUILD_TESTS=ON
cmake --build build
```

The root `CMakeLists.txt` is a shim over the upstream-derived project in
`src/`; configuring from inside `src/` (`cmake -S src -B src/build`) still
works. Pass `-DALBDF_BUILD_GUI=ON` to additionally build the optional GUI.

### Verify

```bash
build/bin/albdf --version                          # → albdf 0.4.0
QT_QPA_PLATFORM=offscreen ctest --test-dir build   # → 100% passed (17 tests)
```

## Quick start

RTL fonts (OFL) ship in `src/tests/fonts/`. Coordinates are PDF points with the
origin at the bottom-left. Exit codes: `0` success, `7` invalid arguments.

```bash
# Add a Persian label with the RTL pipeline (FriBidi + HarfBuzz, embedded font)
albdf add-text in.pdf out.pdf --page 1 --x 72 --y 700 \
        --text "سلام دنیا" --size 24 --rtl \
        --font src/tests/fonts/Vazirmatn-Regular.ttf --lang fa

# RTL-aware search; normalization is on by default (۱۲۳ matches "123")
albdf search-text out.pdf "سلام"

# Fill interactive form fields and write a new document
albdf form-fill in.pdf filled.pdf --field name --value "Ali" --field agree --value On

# Render a page to PNG (the output directory must already exist)
albdf render in.pdf --page-first 1 --page-last 1 --image-format png \
        --image-res-dpi 144 --image-output-dir ./out

# Full command list and per-command help
albdf help
albdf help add-text
```

## Commands

| Area | Commands |
|---|---|
| RTL text | `add-text` (LTR/RTL), `search-text` |
| Content | `recognize-text`, `delete-object`, `rotate`, `move-page`, `delete-page` |
| Forms & signing | `form-list`, `form-fill`, `sign`, `verify-signatures` |
| Render & inspect | `render`, `fetch-text`, `info`, `statistics`, `xml` |
| Documents | `unite`, `separate`, `redact`, `encrypt`, `decrypt`, `optimize`, `attachments` |

Run `albdf help` for every command — including the inherited PDF4QT commands.
The albdf-specific reference is `docs/albdf.1`.

## Documentation

- [`CONTRIBUTING.md`](CONTRIBUTING.md) — how to contribute: branches, commits, gates.
- [`CHANGELOG.md`](CHANGELOG.md) — concise release history.
- [`docs/RELEASES.md`](docs/RELEASES.md) — detailed release notes.
- [`docs/albdf.1`](docs/albdf.1) — CLI man page.
- [`docs/coding-standard.md`](docs/coding-standard.md) — C++/Qt style and rules.
- [`SECURITY.md`](SECURITY.md) — how to report a vulnerability.
- [`docs/branch-protection.md`](docs/branch-protection.md) — how `main` is guarded.
- AI agents: read [`AGENTS.md`](AGENTS.md) first.

## License

**GPL-3.0-or-later** — see [`LICENSE`](LICENSE) and ADR-0005. Authored code is
GPL-3.0-or-later; vendored upstream PDF4QT portions keep their MIT headers (MIT
is GPLv3-compatible). RTL fonts are OFL.
