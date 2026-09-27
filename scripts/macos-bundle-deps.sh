#!/usr/bin/env bash
# Make a staged albdf prefix self-contained on macOS (W8 / DB #26).
#
# macOS ships no system Qt, so a tarball linked against Homebrew's frameworks
# only runs where they already exist — `brew install qt` first is not a release
# artifact. This script copies the Qt frameworks, plain dylibs (fontconfig) and
# platform plugins that the binaries actually reference into <prefix>, rewrites
# every absolute reference to @rpath/..., gives each bundled Mach-O the one
# rpath that always resolves inside the bundle (@executable_path/../Frameworks)
# and writes bin/qt.conf so Qt finds the plugins. It refuses to report success
# while any Homebrew path or unresolved @rpath reference survives: the runtime
# smoke test cannot catch an incomplete bundle, because a CI runner has Homebrew
# Qt installed and would happily load that instead of the shipped copy.
#
# Linux deliberately does NOT do this. Distro Qt is a documented prerequisite
# there, so nothing is shipped and the system's Qt is the only copy in use — no
# duplicate to conflict with anything else on the machine.
#
# Usage: bash scripts/macos-bundle-deps.sh <prefix>
# Exit:  0 = bundled and self-contained; 1 = anything wrong (message on stderr).

set -u

PREFIX="${1:-}"
usage() { echo "bundle-macos: usage: macos-bundle-deps.sh <prefix>" >&2; exit 2; }
[ -n "$PREFIX" ] || usage
[ -d "$PREFIX" ] || usage
fail() { echo "bundle-macos: ERROR: $*" >&2; exit 1; }

command -v otool >/dev/null 2>&1 || fail "otool not found (install the Xcode command line tools)"
command -v install_name_tool >/dev/null 2>&1 || fail "install_name_tool not found"

BIN_DIR="$PREFIX/bin"
FW="$PREFIX/Frameworks"
PLUGINS_OUT="$PREFIX/plugins/platforms"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/albdf-macos-bundle.XXXXXX")"
MAP="$WORK/deps.tsv"
FW_ROOTS="$WORK/fw-roots"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$FW" "$PLUGINS_OUT"
: > "$MAP"
: > "$FW_ROOTS"

[ -f "$BIN_DIR/albdf" ] || fail "$BIN_DIR/albdf not found"
# Upstream installs the core library into bin/ on macOS (not lib/).
CORE_LIB="$(ls "$BIN_DIR"/libPdf4QtLibCore*.dylib 2>/dev/null | head -1)"
[ -n "$CORE_LIB" ] || fail "libPdf4QtLibCore*.dylib not found in $BIN_DIR"

# Every reference of one Mach-O EXCEPT its own LC_ID_DYLIB. `otool -L` lists a
# dylib's own install name as its first entry, and treating that as a dependency
# made the bundler copy each plugin into Frameworks/ as well — caught by
# ci/test-macos-bundle-fixture.sh, which builds a Qt-shaped tree and runs the
# bundler against it.
macho_refs() {
    local f="$1" id
    id="$(otool -D "$f" 2>/dev/null | tail -n +2 | head -1 || true)"
    otool -L "$f" 2>/dev/null | tail -n +2 | awk '{print $1}' \
        | awk -v id="$id" 'id == "" || $0 != id' || true
}
# Absolute non-system dependencies of one Mach-O (@refs are already ours).
macos_deps() {
    macho_refs "$1" | grep -vE '^@|^/usr/lib/|^/System/Library/' || true
}
rel_for() { awk -F'\t' -v o="$1" '$1 == o { print $2; exit }' "$MAP"; }
# Every Mach-O we ship: the executable, the core library, frameworks, plugins.
shipped() {
    printf '%s\n' "$BIN_DIR/albdf" "$CORE_LIB"
    find "$FW" "$PLUGINS_OUT" -type f -perm -u+x 2>/dev/null || true
}

# Copy one dependency in, recording absolute path -> @rpath/<relative path>.
add_dep() {
    local src="$1" fwroot name inner rel
    [ -e "$src" ] || fail "dependency referenced but missing on disk: $src"
    case "$src" in
        *.framework/*)
            fwroot="$(printf '%s' "$src" | sed -E 's|^(.*\.framework)/.*$|\1|')"
            name="$(basename "$fwroot")"
            inner="${src#"$fwroot"/}"
            rel="$name/$inner"
            [ -d "$FW/$name" ] || cp -R "$fwroot" "$FW/$name"
            printf '%s\n' "$fwroot" >> "$FW_ROOTS"
            ;;
        *)
            rel="$(basename "$src")"
            [ -e "$FW/$rel" ] || cp "$src" "$FW/$rel"
            ;;
    esac
    install_name_tool -id "@rpath/$rel" "$FW/$rel" 2>/dev/null || true
    printf '%s\t@rpath/%s\n' "$src" "$rel" >> "$MAP"
}

# Frameworks first: the plugin search needs the Qt prefixes, known only once the
# frameworks have been copied and their roots recorded.
pass=0
while [ "$pass" -lt 4 ]; do
    pass=$((pass + 1))
    added=0
    for f in "$BIN_DIR/albdf" "$CORE_LIB"; do
        while IFS= read -r dep; do
            [ -n "$dep" ] || continue
            if [ -z "$(rel_for "$dep")" ]; then add_dep "$dep"; added=$((added + 1)); fi
        done < <(macos_deps "$f")
    done
    [ "$added" -eq 0 ] && break
done
[ -s "$FW_ROOTS" ] || fail "the binary references no framework at all — nothing to bundle?"

# Locate Qt's platform plugins: ask qmake, then the Homebrew layout, then the
# upstream one derived from the Qt prefixes the frameworks came from.
qt_plugins_dir=""
if command -v qmake >/dev/null 2>&1; then
    qt_plugins_dir="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi
if [ ! -d "${qt_plugins_dir:-}/platforms" ]; then
    qt_plugins_dir=""
    for cand in "$(brew --prefix qt 2>/dev/null || echo)/share/qt/plugins" \
                "$(brew --prefix qtbase 2>/dev/null || echo)/share/qt/plugins"; do
        [ -n "$cand" ] && [ -d "$cand/platforms" ] && { qt_plugins_dir="$cand"; break; }
    done
fi
if [ ! -d "${qt_plugins_dir:-}/platforms" ]; then
    for root in $(sort -u "$FW_ROOTS"); do
        for cand in "$root/../../share/qt/plugins" "$root/../../plugins"; do
            [ -d "$cand/platforms" ] && { qt_plugins_dir="$cand"; break 2; }
        done
    done
fi
[ -n "$qt_plugins_dir" ] || fail "Qt platform plugins not found (tried qmake, Homebrew and the Qt prefixes of the linked frameworks)"

# Headless CLI: offscreen is mandatory (main.cpp selects it with no display);
# cocoa covers a normal desktop session; minimal is a cheap extra fallback.
BUNDLED_PLUGINS=0
for p in libqoffscreen libqcocoa libqminimal; do
    src="$qt_plugins_dir/platforms/$p.dylib"
    [ -f "$src" ] || continue
    cp "$src" "$PLUGINS_OUT/$p.dylib"
    # A plugin's own install name is unused at load time (Qt resolves the path
    # itself), but leaving it pointing into Homebrew would survive a naive
    # "grep the tarball for /opt/homebrew" audit.
    install_name_tool -id "@rpath/$p.dylib" "$PLUGINS_OUT/$p.dylib" 2>/dev/null || true
    BUNDLED_PLUGINS=$((BUNDLED_PLUGINS + 1))
done
[ -f "$PLUGINS_OUT/libqoffscreen.dylib" ] || \
    fail "no offscreen platform plugin under $qt_plugins_dir/platforms — the CLI could not run headless"

# Fixpoint: plugins and frameworks pull further frameworks (QtSvg -> QtGui ->
# QtCore), so keep going until nothing new appears.
pass=0
while [ "$pass" -lt 8 ]; do
    pass=$((pass + 1))
    added=0
    while IFS= read -r f; do
        [ -n "$f" ] || continue
        while IFS= read -r dep; do
            [ -n "$dep" ] || continue
            if [ -z "$(rel_for "$dep")" ]; then add_dep "$dep"; added=$((added + 1)); fi
        done < <(macos_deps "$f")
    done < <(shipped)
    [ "$added" -eq 0 ] && break
done

# Rewrite absolute references to @rpath/... and add the one rpath that resolves
# inside the bundle for every Mach-O that needs it.
while IFS= read -r f; do
    [ -n "$f" ] || continue
    while IFS= read -r dep; do
        [ -n "$dep" ] || continue
        new="$(rel_for "$dep")"
        [ -n "$new" ] || continue
        install_name_tool -change "$dep" "$new" "$f" 2>/dev/null || true
    done < <(macos_deps "$f")
    if ! otool -l "$f" 2>/dev/null | grep -A2 LC_RPATH | grep -qF '@executable_path/../Frameworks'; then
        install_name_tool -add_rpath '@executable_path/../Frameworks' "$f" 2>/dev/null || true
    fi
done < <(shipped)

# Qt finds its plugins through qt.conf, relative to the executable's directory.
cat > "$BIN_DIR/qt.conf" <<'QTCONF'
[Paths]
Plugins = ../plugins
QTCONF

echo "  bundled libraries: $(ls -1 "$FW" | wc -l | tr -d ' ')"
ls -1 "$FW" | sed 's/^/    /'
echo "  bundled platform plugins: $BUNDLED_PLUGINS"
ls -1 "$PLUGINS_OUT" | sed 's/^/    platforms\//'

# ---- self-containment check ------------------------------------------------
# This is the check that matters: a runner with Homebrew Qt would pass the
# runtime smoke test even if nothing had been bundled.
LEAK=0
while IFS= read -r f; do
    [ -n "$f" ] || continue
    while IFS= read -r ref; do
        [ -n "$ref" ] || continue
        case "$ref" in
            /opt/homebrew/*|/usr/local/*)
                echo "  LEAK: $(basename "$f") -> $ref" >&2; LEAK=1 ;;
            @rpath/*)
                r="${ref#@rpath/}"
                [ -e "$FW/$r" ] || [ -e "$BIN_DIR/$r" ] || [ -e "$PREFIX/lib/$r" ] || {
                    echo "  UNRESOLVED: $(basename "$f") -> $ref" >&2; LEAK=1; } ;;
            @executable_path/*)
                [ -e "$BIN_DIR/${ref#@executable_path/}" ] || {
                    echo "  UNRESOLVED: $(basename "$f") -> $ref" >&2; LEAK=1; } ;;
            @loader_path/*)
                [ -e "$BIN_DIR/${ref#@loader_path/}" ] || {
                    echo "  UNRESOLVED: $(basename "$f") -> $ref" >&2; LEAK=1; } ;;
        esac
    done < <(macho_refs "$f")
done < <(shipped)
[ "$LEAK" -eq 0 ] || fail "bundle is not self-contained (LEAK/UNRESOLVED above)"

echo "  OK: no Homebrew reference, every dependency resolves inside the bundle"
