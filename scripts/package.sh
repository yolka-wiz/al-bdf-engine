#!/usr/bin/env bash
# albdf release packaging (W7 / DB #26).
#
# Builds a deterministic release artifact from the CMake install rules:
# albdf binary + Pdf4QtLibCore shared library + public headers + man page +
# license, staged into a temp prefix via `cmake --install`, validated
# headless (the INSTALLED binary must run --version and info on a fixture
# with QT_QPA_PLATFORM=offscreen), then tarred with reproducible metadata.
#
# Cross-platform (0.4.0+): Linux (.so + $ORIGIN) and macOS (.dylib +
# @loader_path). GNU tar (or gtar on macOS) is preferred for deterministic
# output; a Python tarfile fallback keeps bsdtar-only systems working.
#
# Determinism:
#   - all files root-owned, normalized mtime (SOURCE_DATE_EPOCH, falling back
#     to the last commit time), sorted entries, gzip -n (no filename/mtime
#     header) => two runs from the same build produce identical sha256.
#   - the installed binary must resolve libPdf4QtLibCore via $ORIGIN (Linux)
#     or @loader_path (macOS), never via a build-tree path; a readelf/otool
#     check fails the run if a build-tree path leaked in.
#
# Usage:
#   bash scripts/package.sh [--build-dir src/build] [--out-dir dist]
#                           [--version X.Y.Z] [--fixture <pdf>]
#                           [--stage-dir <dir>] [--keep-stage] [--deb]
#   SOURCE_DATE_EPOCH=<epoch> bash scripts/package.sh   # fixed tarball mtime
#
# Outputs (in OUT_DIR by default):
#   albdf-<version>-<platform>.tar.gz       the release artifact (all platforms)
#   albdf-<version>-<platform>.tar.gz.sha256
#   albdf-<version>_<debarch>.deb             only with --deb (skeleton)
#   platform = linux-x86_64 / linux-aarch64 / macos-arm64 / macos-x86_64
#             / windows-x86_64
#
# Exit code: 0 = packaged + validated; 1 = any step failed.
set -u

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$REPO_DIR/src/build"
OUT_DIR="$REPO_DIR/dist"
VERSION=""
DO_DEB=0
KEEP_STAGE=0
STAGE_DIR=""
FIXTURE="$REPO_DIR/src/tests/fixtures/multipage.pdf"

usage() {
    sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --build-dir) BUILD_DIR="${2:?}"; shift 2 ;;
        --out-dir)   OUT_DIR="${2:?}";   shift 2 ;;
        --version)   VERSION="$2";       shift 2 ;;
        --fixture)   FIXTURE="$2";       shift 2 ;;
        --stage-dir) STAGE_DIR="$2"; KEEP_STAGE=1; shift 2 ;;
        --keep-stage) KEEP_STAGE=1; shift ;;
        --deb)       DO_DEB=1; shift ;;
        -h|--help)   usage; exit 0 ;;
        *) echo "package: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

fail() { echo "package: ERROR: $*" >&2; exit 1; }

# ---- inputs ---------------------------------------------------------------
# Shared-library glob differs per platform: .so* on Linux, .dylib* on macOS.
_IS_WINDOWS=0
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) _IS_WINDOWS=1 ;;
esac
[ "${OS:-}" = "Windows_NT" ] && _IS_WINDOWS=1

case "$_IS_WINDOWS" in
    1)      LIB_GLOB='Pdf4QtLibCore*.dll' ;;
    *)
        case "$(uname -s)" in
            Darwin) LIB_GLOB='libPdf4QtLibCore*.dylib' ;;
            *)      LIB_GLOB='libPdf4QtLibCore.so*' ;;
        esac
        ;;
esac
# Library subdir in the build/install trees: upstream CMake installs the
# shared library into lib/ on Linux but bin/ on macOS/Windows (the
# PDF4QT_INSTALL_LIB_DIR else() branch uses CMAKE_INSTALL_BINDIR).
case "$_IS_WINDOWS" in
    1)      LIB_SUBDIR="bin" ;;
    *)
        case "$(uname -s)" in
            Darwin) LIB_SUBDIR="bin" ;;
            *)      LIB_SUBDIR="lib" ;;
        esac
        ;;
esac

[ -x "$BUILD_DIR/bin/albdf" ] || \
    fail "albdf binary not found at $BUILD_DIR/bin/albdf (build first: cmake --build $BUILD_DIR)"
ls "$BUILD_DIR"/$LIB_SUBDIR/$LIB_GLOB >/dev/null 2>&1 || \
    fail "libPdf4QtLibCore not found under $BUILD_DIR/$LIB_SUBDIR"
[ -f "$FIXTURE" ] || fail "validation fixture not found: $FIXTURE"

if [ -z "$VERSION" ]; then
    VERSION="$(sed -n 's/^set(ALBDF_VERSION \(.*\))$/\1/p' "$REPO_DIR/src/CMakeLists.txt" | head -1)"
fi
[ -n "$VERSION" ] || fail "could not determine version (pass --version or fix src/CMakeLists.txt)"

# Platform naming: linux-x86_64 / linux-aarch64 / macos-arm64 / macos-x86_64
# / windows-x86_64.
OS_NAME="$(uname -s)"
case "$OS_NAME" in
    Linux)  PLATFORM_OS="linux" ;;
    Darwin) PLATFORM_OS="macos" ;;
    MINGW*|MSYS*|CYGWIN*|Windows) PLATFORM_OS="windows" ;;
    *)      PLATFORM_OS="$(printf '%s' "$OS_NAME" | tr '[:upper:]' '[:lower:]')" ;;
esac
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)  DEB_ARCH=amd64 ; PLATFORM_ARCH="x86_64" ;;
    aarch64|arm64) DEB_ARCH=arm64 ; PLATFORM_ARCH="aarch64" ;;
    *)       DEB_ARCH="$ARCH" ; PLATFORM_ARCH="$ARCH" ;;
esac
PLATFORM="${PLATFORM_OS}-${PLATFORM_ARCH}"
# Output archive extension: .tar.gz everywhere. Git Bash on the Windows
# runner provides tar + gzip, so the same deterministic path works across
# all platforms — no separate zip/Compress-Archive branch needed.
ARCHIVE_EXT="tar.gz"

# ---- helpers --------------------------------------------------------------
# sha256sum (Linux) vs shasum -a 256 (macOS).
sha256_of() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    else
        shasum -a 256 "$1" | awk '{print $1}'
    fi
}
# stat -c %s (GNU) vs stat -f %z (BSD/macOS).
file_size() {
    if [ "$PLATFORM_OS" = "macos" ]; then
        stat -f %z "$1"
    else
        stat -c %s "$1"
    fi
}
# GNU tar (Linux) or gtar (macOS: brew install gnu-tar) for deterministic
# output. Fall back to plain tar if neither is present (bsdtar-only systems).
TAR_BIN="$(command -v gtar || command -v tar || true)"
TAR_DETERMINISTIC=0
if [ -n "$TAR_BIN" ] && "$TAR_BIN" --version 2>/dev/null | grep -q GNU; then
    TAR_DETERMINISTIC=1
fi
if [ "$TAR_DETERMINISTIC" -ne 1 ]; then
    echo "package: WARNING: GNU tar (gtar) not found — tarball may not be byte-deterministic" >&2
fi

# ---- staging --------------------------------------------------------------
if [ -n "$STAGE_DIR" ]; then
    STAGE="$STAGE_DIR"; mkdir -p "$STAGE"
else
    STAGE="$(mktemp -d "${TMPDIR:-/tmp}/albdf-stage.XXXXXX")"
fi
DEB_WORK="$(mktemp -d "${TMPDIR:-/tmp}/albdf-deb.XXXXXX")"
if [ "$KEEP_STAGE" -eq 1 ]; then
    trap 'rm -rf "$DEB_WORK"' EXIT
else
    trap 'rm -rf "$STAGE" "$DEB_WORK"' EXIT
fi

echo "== staging install (prefix=$STAGE) =="
cmake --install "$BUILD_DIR" --prefix "$STAGE" >/dev/null || fail "cmake --install failed"
# Binary is albdf on unix, albdf.exe on Windows.
if [ "$_IS_WINDOWS" -eq 1 ]; then
    [ -f "$STAGE/bin/albdf.exe" ] || fail "installed tree missing bin/albdf.exe"
else
    [ -x "$STAGE/bin/albdf" ] || fail "installed tree missing bin/albdf"
fi
ls "$STAGE"/$LIB_SUBDIR/$LIB_GLOB >/dev/null 2>&1 || fail "libPdf4QtLibCore not found under $STAGE/$LIB_SUBDIR"
[ -f "$STAGE/include/Pdf4QtLibCore/pdfglobal.h" ] || fail "installed tree missing headers"
[ -f "$STAGE/include/Pdf4QtLibCore/pdf4qtlibcore_export.h" ] || fail "installed tree missing generated export header"
[ -f "$STAGE/share/man/man1/albdf.1" ] || fail "installed tree missing man page"
[ -f "$STAGE/share/licenses/albdf/LICENSE" ] || fail "installed tree missing license"

# ---- validate the INSTALLED binary (headless) -----------------------------
echo "== validating installed binary (offscreen) =="
export QT_QPA_PLATFORM=offscreen
if [ "$_IS_WINDOWS" -eq 1 ]; then
    ALBDF_BIN="$STAGE/bin/albdf.exe"
else
    ALBDF_BIN="$STAGE/bin/albdf"
fi
VER="$(QT_QPA_PLATFORM=offscreen "$ALBDF_BIN" --version 2>&1)" || fail "installed albdf --version exited non-zero"
case "$VER" in *albdf*) ;; *) fail "installed albdf --version output unexpected: $VER" ;; esac
INFO="$(QT_QPA_PLATFORM=offscreen "$ALBDF_BIN" info "$FIXTURE" 2>&1)" || fail "installed albdf info exited non-zero on $(basename "$FIXTURE")"
case "$INFO" in *"Page count"*) ;; *) fail "installed albdf info output missing 'Page count': $(echo "$INFO" | head -3)" ;; esac
echo "  OK: --version -> $VER ; info $(basename "$FIXTURE") -> page count present"

# The installed binary must be relocatable: no build-tree path in RUNPATH
# (Linux) / LC_RPATH (macOS). Windows has no RPATH (DLL resolves next to the
# .exe), so the check is N/A there.
if [ "$_IS_WINDOWS" -eq 0 ]; then
    if [ "$PLATFORM_OS" = "macos" ] && command -v otool >/dev/null 2>&1; then
        RPATH="$(otool -l "$STAGE/bin/albdf" 2>/dev/null | grep -A2 LC_RPATH || true)"
        if printf '%s' "$RPATH" | grep -F "$BUILD_DIR" >/dev/null; then
            fail "installed albdf still carries the build-tree LC_RPATH ($RPATH); INSTALL_RPATH=@loader_path fix missing"
        fi
        echo "  LC_RPATH check: $(printf '%s' "$RPATH" | tr '\n' ' ' | sed 's/  */ /g')"
    elif command -v readelf >/dev/null 2>&1; then
        RPATH="$(readelf -d "$STAGE/bin/albdf" 2>/dev/null | grep -E 'RUNPATH|RPATH' || true)"
        if printf '%s' "$RPATH" | grep -F "$BUILD_DIR" >/dev/null; then
            fail "installed albdf still carries the build-tree RUNPATH ($RPATH); INSTALL_RPATH=\$ORIGIN fix missing"
        fi
        echo "  RUNPATH check: $(printf '%s' "$RPATH" | tr '\n' ' ' | sed 's/  */ /g')"
    fi
fi

# ---- deterministic tarball (.tar.gz on all platforms) --------------------
MTIME="${SOURCE_DATE_EPOCH:-$(git -C "$REPO_DIR" log -1 --format=%ct 2>/dev/null || date +%s)}"
mkdir -p "$OUT_DIR"
TARBALL="$OUT_DIR/albdf-${VERSION}-${PLATFORM}.${ARCHIVE_EXT}"
echo "== creating archive (SOURCE_DATE_EPOCH=$MTIME, tar=$TAR_BIN) =="
if [ "$TAR_DETERMINISTIC" -eq 1 ]; then
    "$TAR_BIN" --use-compress-program='gzip -n' -cf "$TARBALL" -C "$STAGE" \
        --sort=name --numeric-owner --owner=0 --group=0 --mtime=@"$MTIME" .
else
    # bsdtar / Git-Bash-tar fallback: no GNU-only flags; mtime normalization
    # via -m is not available, so determinism is best-effort here.
    (cd "$STAGE" && "$TAR_BIN" -czf "$TARBALL" .)
fi
sha256_of "$TARBALL" > "$TARBALL.sha256"

# ---- optional minimal .deb skeleton (Linux only) --------------------------
DEB=""
if [ "$DO_DEB" -eq 1 ] && [ "$PLATFORM_OS" = "linux" ]; then
    if command -v ar >/dev/null 2>&1; then
        echo "== building minimal .deb skeleton =="
        mkdir -p "$DEB_WORK/control"
        cat > "$DEB_WORK/control/control" <<EOF
Package: albdf
Version: $VERSION
Section: utils
Priority: optional
Architecture: $DEB_ARCH
Maintainer: albdf contributors <albdf@albdf.local>
Depends: libc6, libqt6core6, libqt6gui6, libqt6xml6
Description: Headless PDF editing library + CLI (RTL-aware, fork of PDF4QT)
 albdf is a headless PDF editing library and command-line tool focused on
 Middle Eastern (Arabic/Persian/Hebrew) RTL text writing and search.
 This is a minimal skeleton produced by scripts/package.sh (--deb); it is
 NOT policy-complete (no symbols, no lintian-clean control fields).
EOF
        # data: staged files re-rooted under /usr (handle ./prefix or bare)
        "$TAR_BIN" --use-compress-program='gzip -n' -cf "$DEB_WORK/data.tar.gz" -C "$STAGE" \
            --sort=name --numeric-owner --owner=0 --group=0 --mtime=@"$MTIME" \
            --transform='s|^\./|./usr/|' .
        "$TAR_BIN" --use-compress-program='gzip -n' -cf "$DEB_WORK/control.tar.gz" \
            -C "$DEB_WORK/control" \
            --sort=name --numeric-owner --owner=0 --group=0 --mtime=@"$MTIME" control
        printf '2.0\n' > "$DEB_WORK/debian-binary"
        DEB="$OUT_DIR/albdf-${VERSION}_${DEB_ARCH}.deb"
        (cd "$DEB_WORK" && ar -D -r "$DEB" debian-binary control.tar.gz data.tar.gz >/dev/null) \
            || fail "ar failed to assemble $DEB"
        echo "  OK: $DEB"
    else
        echo "package: WARNING: 'ar' (binutils) not found - skipping --deb" >&2
    fi
elif [ "$DO_DEB" -eq 1 ] && [ "$PLATFORM_OS" != "linux" ]; then
    echo "package: WARNING: --deb is Linux-only; skipping on $PLATFORM" >&2
fi

# ---- summary ---------------------------------------------------------------
FILES="$("$TAR_BIN" -tf "$TARBALL" | grep -cv '/$')"
SIZE="$(file_size "$TARBALL")"
SHA="$(cat "$TARBALL.sha256")"
echo
echo "== package summary =="
echo "albdf-package-v1"
echo "artifact=$TARBALL"
echo "sha256=$SHA"
echo "version=$VERSION"
echo "platform=$PLATFORM"
echo "size_bytes=$SIZE"
echo "files=$FILES"
echo "validated=1"
[ -n "$DEB" ] && echo "deb=$DEB"
echo
echo "package: OK - $TARBALL ($SIZE bytes, sha256 $SHA)"
