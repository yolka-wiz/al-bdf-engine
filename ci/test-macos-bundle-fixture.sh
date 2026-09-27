#!/usr/bin/env bash
# Regression test for scripts/macos-bundle-deps.sh (W8).
#
# macOS hosts have no system Qt, so the release tarball must carry Qt itself —
# and the only honest way to test that is to run the bundler against a Qt-shaped
# tree. clang + otool are enough; no Qt install and no build of the project are
# needed, which is why this can run on any macOS machine (and in the macOS CI
# gate, where nothing else exercises the bundler: the release workflow only
# packages on a tag).
#
# It builds, under a temp dir:
#   * a fake Qt tree in Homebrew's layout (<prefix>/lib/*.framework,
#     <prefix>/share/qt/plugins/platforms/*.dylib), with the framework chain
#     QtSvg -> QtGui -> QtCore and a plain libfontconfig.1.dylib
#   * a fake installed prefix whose executable and core library link those
#     libraries by ABSOLUTE path — exactly the shape of the real package
# and then asserts, in order:
#   1. the fixture runs while the fake Qt tree is present            (sanity)
#   2. it fails with a dyld "Library not loaded" once the tree is hidden
#      - the symptom users hit on a clean machine
#   3. scripts/macos-bundle-deps.sh completes over the prefix
#   4. with the tree STILL hidden, the bundled binary runs the whole chain,
#      including dlopen-ing the bundled platform plugin  (self-contained proof)
#
# Usage: bash ci/test-macos-bundle-fixture.sh [work-dir] [--keep]
# Exit:  0 = pass, 1 = bundler or assertions failed, 2 = bad usage.

set -u

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUNDLER="$REPO/scripts/macos-bundle-deps.sh"
KEEP=0
FX=""
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) sed -n '2,26p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) FX="$arg" ;;
    esac
done
[ -n "$FX" ] || FX="$(mktemp -d "${TMPDIR:-/tmp}/albdf-bundle-fixture.XXXXXX")"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "macos-bundle-fixture: SKIP (macOS only; this is $(uname -s))"
    exit 0
fi
for tool in clang otool install_name_tool; do
    command -v "$tool" >/dev/null 2>&1 || { echo "macos-bundle-fixture: $tool not found (Xcode command line tools)" >&2; exit 2; }
done
[ -f "$BUNDLER" ] || { echo "macos-bundle-fixture: $BUNDLER not found" >&2; exit 2; }

QTL="$FX/fakeqt/lib"
PLUGDIR="$FX/fakeqt/share/qt/plugins/platforms"
SRC="$FX/src"
PFX="$FX/prefix"
rm -rf "$FX"
mkdir -p "$SRC" "$QTL" "$PLUGDIR" "$PFX/bin"
cleanup() { [ "$KEEP" -eq 1 ] || rm -rf "$FX"; }
trap cleanup EXIT
hr() { printf '\n=== %s ===\n' "$1"; }
FAILED=0
note_fail() { echo "  !! $1"; FAILED=1; }

# ---- fake Qt in Homebrew's layout ------------------------------------------
cat > "$SRC/qtcore.c" <<'EOF'
int qt_core(void) { return 10; }
EOF
cat > "$SRC/qtgui.c" <<'EOF'
extern int qt_core(void);
int qt_gui(void) { return qt_core() + 20; }
EOF
cat > "$SRC/qtsvg.c" <<'EOF'
extern int qt_gui(void);
int qt_svg(void) { return qt_gui() + 30; }
EOF
cat > "$SRC/fontconfig.c" <<'EOF'
int fc(void) { return 7; }
EOF
cat > "$SRC/platformplugin.c" <<'EOF'
extern int qt_gui(void);
int qoff(void) { return qt_gui() + 100; }
EOF
cat > "$SRC/core.c" <<'EOF'
extern int qt_gui(void);
int core(void) { return qt_gui() + 1000; }
EOF
cat > "$SRC/albdf.c" <<'EOF'
#include <dlfcn.h>
#include <stdio.h>
extern int core(void);
extern int qt_gui(void);
extern int fc(void);
int main(int argc, char **argv) {
    printf("  core=%d qt_gui=%d fc=%d\n", core(), qt_gui(), fc());
    if (argc < 2) { printf("  (no plugin path given)\n"); return 0; }
    void *h = dlopen(argv[1], RTLD_NOW);
    if (!h) { printf("  PLUGIN LOAD FAILED: %s\n", dlerror()); return 1; }
    int (*qoff)(void) = (int (*)(void))dlsym(h, "qoff");
    if (!qoff) { printf("  SYMBOL MISSING\n"); return 1; }
    printf("  plugin qoff=%d\n", qoff());
    printf("  CHAIN OK\n");
    return 0;
}
EOF

mkfw() { # <name> <source> [linked dylibs...]
    local name="$1" src="$2"; shift 2
    mkdir -p "$QTL/$name.framework/Versions/A/Resources"
    clang -dynamiclib -o "$QTL/$name.framework/Versions/A/$name" "$src" \
        -install_name "$QTL/$name.framework/Versions/A/$name" "$@" || exit 2
    ln -sf A "$QTL/$name.framework/Versions/Current"
    ln -sf "Versions/Current/$name" "$QTL/$name.framework/$name"
    printf 'fake framework\n' > "$QTL/$name.framework/Versions/A/Resources/Info.plist"
}
mkfw QtCore "$SRC/qtcore.c"
mkfw QtGui  "$SRC/qtgui.c" "$QTL/QtCore.framework/Versions/A/QtCore"
mkfw QtSvg  "$SRC/qtsvg.c" "$QTL/QtGui.framework/Versions/A/QtGui" "$QTL/QtCore.framework/Versions/A/QtCore"

clang -dynamiclib -o "$QTL/libfontconfig.1.dylib" "$SRC/fontconfig.c" \
    -install_name "$QTL/libfontconfig.1.dylib" || exit 2
for p in libqoffscreen libqcocoa; do
    clang -dynamiclib -o "$PLUGDIR/$p.dylib" "$SRC/platformplugin.c" \
        -install_name "$PLUGDIR/$p.dylib" \
        "$QTL/QtGui.framework/Versions/A/QtGui" "$QTL/QtCore.framework/Versions/A/QtCore" || exit 2
done

# ---- fake installed prefix (upstream puts the core library in bin/ on macOS)
clang -dynamiclib -o "$PFX/bin/libPdf4QtLibCore.1.6.0.0.dylib" "$SRC/core.c" \
    -install_name "@rpath/libPdf4QtLibCore.1.6.0.0.dylib" \
    "$QTL/QtGui.framework/Versions/A/QtGui" "$QTL/QtSvg.framework/Versions/A/QtSvg" \
    "$QTL/QtCore.framework/Versions/A/QtCore" "$QTL/libfontconfig.1.dylib" || exit 2
ln -sf libPdf4QtLibCore.1.6.0.0.dylib "$PFX/bin/libPdf4QtLibCore.dylib"
clang -o "$PFX/bin/albdf" "$SRC/albdf.c" \
    -Wl,-rpath,@loader_path/../bin \
    "$PFX/bin/libPdf4QtLibCore.1.6.0.0.dylib" \
    "$QTL/QtGui.framework/Versions/A/QtGui" "$QTL/libfontconfig.1.dylib" || exit 2

hr "dependencies before bundling (absolute paths into the fake Qt tree)"
otool -L "$PFX/bin/albdf" | tail -n +2 | sed 's/^/  /'

hr "1. sanity: the fixture runs while the fake Qt tree is present"
"$PFX/bin/albdf" "$PLUGDIR/libqoffscreen.dylib" || note_fail "the fixture itself does not run"

hr "2. symptom: hide the fake Qt tree (== a Mac with no Homebrew Qt)"
mv "$FX/fakeqt" "$FX/fakeqt-hidden"
# Nested bash so the shell's own "Abort trap" job message is suppressed too.
if bash -c '"$0" "$1"' "$PFX/bin/albdf" "$PLUGDIR/libqoffscreen.dylib" 2>/dev/null; then
    note_fail "expected a dyld failure with the Qt tree absent"
else
    echo "  ok: dyld refused to load the missing framework (this is the user-visible bug)"
fi

hr "3. bundle: run scripts/macos-bundle-deps.sh over the prefix"
mv "$FX/fakeqt-hidden" "$FX/fakeqt"
bash "$BUNDLER" "$PFX" || note_fail "the bundler exited non-zero"

hr "4. proof: hide the Qt tree again and run the BUNDLED binary"
mv "$FX/fakeqt" "$FX/fakeqt-hidden"
if "$PFX/bin/albdf" "$PFX/plugins/platforms/libqoffscreen.dylib"; then
    echo "  ok: the bundled binary runs with no Qt on the machine"
else
    note_fail "the bundled binary still needs the system libraries"
fi

hr "after: references of the bundled files"
for f in "$PFX/bin/albdf" "$PFX/bin/libPdf4QtLibCore.1.6.0.0.dylib" \
         "$PFX/plugins/platforms/libqoffscreen.dylib" "$PFX/Frameworks/QtSvg.framework/Versions/A/QtSvg"; do
    echo "  --- ${f#"$PFX"/}"
    otool -L "$f" | tail -n +2 | awk '{print "      " $1}'
done

# The bundler copies dependencies into Frameworks/ and the plugins into
# plugins/platforms/: a plugin appearing under Frameworks/ means its own install
# name was mistaken for a dependency (a bug this test was written to catch).
for p in libqoffscreen libqcocoa; do
    [ -e "$PFX/Frameworks/$p.dylib" ] && note_fail "$p.dylib was copied into Frameworks/ (own install name mistaken for a dependency)"
done
[ -f "$PFX/bin/qt.conf" ] || note_fail "qt.conf is missing (Qt would not find the plugins)"
[ -d "$PFX/Frameworks/QtSvg.framework" ] || note_fail "the transitive QtSvg framework was not bundled"

echo
if [ "$FAILED" -eq 0 ]; then
    echo "RESULT: PASS - the macOS bundle is self-contained"
    [ "$KEEP" -eq 1 ] && echo "fixture kept at $FX"
    exit 0
fi
echo "RESULT: FAIL"
[ "$KEEP" -eq 1 ] && echo "fixture kept at $FX"
exit 1
