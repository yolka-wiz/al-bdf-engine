#!/usr/bin/env bash
# albdf release packaging (W7 / DB #26).
#
# Builds a deterministic release artifact from the CMake install rules:
# albdf binary + Pdf4QtLibCore shared library + public headers + man page +
# license, staged into a temp prefix via `cmake --install`, validated
# headless (the INSTALLED binary must run --version and info on a fixture
# with QT_QPA_PLATFORM=offscreen), then tarred with reproducible metadata.
#
# Determinism:
#   - all files root-owned, normalized mtime (SOURCE_DATE_EPOCH, falling back
#     to the last commit time), sorted entries, gzip -n (no filename/mtime
#     header) => two runs from the same build produce identical sha256.
#   - the installed binary must resolve libPdf4QtLibCore via $ORIGIN (set by
#     the INSTALL_RPATH rule in src/CMakeLists.txt), never via a build-tree
#     path; a readelf check fails the run if a build-tree RUNPATH leaked in.
#
# Usage:
#   bash scripts/package.sh [--build-dir src/build] [--out-dir dist]
#                           [--version X.Y.Z] [--fixture <pdf>]
#                           [--stage-dir <dir>] [--keep-stage] [--deb]
#   SOURCE_DATE_EPOCH=<epoch> bash scripts/package.sh   # fixed tarball mtime
#
# Outputs (in OUT_DIR by default):
#   albdf-<version>-linux-<arch>.tar.gz      the release artifact
#   albdf-<version>-linux-<arch>.tar.gz.sha256
#   albdf-<version>_<debarch>.deb             only with --deb (skeleton)
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
[ -x "$BUILD_DIR/bin/albdf" ] || \
    fail "albdf binary not found at $BUILD_DIR/bin/albdf (build first: cmake --build $BUILD_DIR)"
ls "$BUILD_DIR"/lib/libPdf4QtLibCore.so* >/dev/null 2>&1 || \
    fail "libPdf4QtLibCore not found under $BUILD_DIR/lib"
[ -f "$FIXTURE" ] || fail "validation fixture not found: $FIXTURE"

if [ -z "$VERSION" ]; then
    VERSION="$(sed -n 's/^set(ALBDF_VERSION \(.*\))$/\1/p' "$REPO_DIR/src/CMakeLists.txt" | head -1)"
fi
[ -n "$VERSION" ] || fail "could not determine version (pass --version or fix src/CMakeLists.txt)"

ARCH="$(uname -m)"
case "$ARCH" in
    x86_64)  DEB_ARCH=amd64 ;;
    aarch64) DEB_ARCH=arm64 ;;
    *)       DEB_ARCH="$ARCH" ;;
esac
PLATFORM="linux-$ARCH"

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
[ -x "$STAGE/bin/albdf" ] || fail "installed tree missing bin/albdf"
ls "$STAGE"/lib/libPdf4QtLibCore.so* >/dev/null 2>&1 || fail "installed tree missing libPdf4QtLibCore"
[ -f "$STAGE/include/Pdf4QtLibCore/pdfglobal.h" ] || fail "installed tree missing headers"
[ -f "$STAGE/include/Pdf4QtLibCore/pdf4qtlibcore_export.h" ] || fail "installed tree missing generated export header"
[ -f "$STAGE/share/man/man1/albdf.1" ] || fail "installed tree missing man page"
[ -f "$STAGE/share/licenses/albdf/LICENSE" ] || fail "installed tree missing license"

# ---- validate the INSTALLED binary (headless) -----------------------------
echo "== validating installed binary (offscreen) =="
export QT_QPA_PLATFORM=offscreen
VER="$("$STAGE/bin/albdf" --version 2>&1)" || fail "installed albdf --version exited non-zero"
case "$VER" in *albdf*) ;; *) fail "installed albdf --version output unexpected: $VER" ;; esac
INFO="$("$STAGE/bin/albdf" info "$FIXTURE" 2>&1)" || fail "installed albdf info exited non-zero on $(basename "$FIXTURE")"
case "$INFO" in *"Page count"*) ;; *) fail "installed albdf info output missing 'Page count': $(echo "$INFO" | head -3)" ;; esac
echo "  OK: --version -> $VER ; info $(basename "$FIXTURE") -> page count present"

# The installed binary must be relocatable: no build-tree path in RUNPATH.
if command -v readelf >/dev/null 2>&1; then
    RPATH="$(readelf -d "$STAGE/bin/albdf" 2>/dev/null | grep -E 'RUNPATH|RPATH' || true)"
    if printf '%s' "$RPATH" | grep -F "$BUILD_DIR" >/dev/null; then
        fail "installed albdf still carries the build-tree RUNPATH ($RPATH); INSTALL_RPATH=\$ORIGIN fix missing"
    fi
    echo "  RUNPATH check: $(printf '%s' "$RPATH" | tr '\n' ' ' | sed 's/  */ /g')"
fi

# ---- deterministic tarball -------------------------------------------------
MTIME="${SOURCE_DATE_EPOCH:-$(git -C "$REPO_DIR" log -1 --format=%ct 2>/dev/null || date +%s)}"
mkdir -p "$OUT_DIR"
TARBALL="$OUT_DIR/albdf-${VERSION}-${PLATFORM}.tar.gz"
echo "== creating tarball (SOURCE_DATE_EPOCH=$MTIME) =="
tar --use-compress-program='gzip -n' -cf "$TARBALL" -C "$STAGE" \
    --sort=name --numeric-owner --owner=0 --group=0 --mtime=@"$MTIME" .
sha256sum "$TARBALL" | awk '{print $1}' > "$TARBALL.sha256"

# ---- optional minimal .deb skeleton ---------------------------------------
DEB=""
if [ "$DO_DEB" -eq 1 ]; then
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
        tar --use-compress-program='gzip -n' -cf "$DEB_WORK/data.tar.gz" -C "$STAGE" \
            --sort=name --numeric-owner --owner=0 --group=0 --mtime=@"$MTIME" \
            --transform='s|^\./|./usr/|' .
        tar --use-compress-program='gzip -n' -cf "$DEB_WORK/control.tar.gz" \
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
fi

# ---- summary ---------------------------------------------------------------
FILES="$(tar -tf "$TARBALL" | grep -cv '/$')"
SIZE="$(stat -c %s "$TARBALL")"
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
