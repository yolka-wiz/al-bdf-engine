#!/usr/bin/env bash
# Build an AppImage of the albdf GUI (Viewer + Editor + PageMaster).
# Prereqs: linuxdeploy + appimagetool + linuxdeploy-plugin-qt in ~/tools/
# Run from repo root after building with ALBDF_BUILD_GUI=ON.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS_DIR="${TOOLS_DIR:-$HOME/tools}"
BUILD_DIR="${BUILD_DIR:-$REPO_DIR/src/build-gui}"
OUT_DIR="${OUT_DIR:-$REPO_DIR/dist}"
VERSION="${VERSION:-0.3.0}"

export VCPKG_ROOT="${VCPKG_ROOT:-/home/agent/vcpkg-cache/vcpkg}"

# --- locate tools
LDDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-x86_64.AppImage"
QT_PLUGIN="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
for t in "$LDDEPLOY" "$APPIMAGETOOL" "$QT_PLUGIN"; do
    [ -x "$t" ] || { echo "missing tool: $t" >&2; exit 1; }
done

echo "==> verifying build artifacts"
for b in Pdf4QtViewer Pdf4QtEditor Pdf4QtPageMaster; do
    [ -x "$BUILD_DIR/bin/$b" ] || { echo "missing $BUILD_DIR/bin/$b — build with ALBDF_BUILD_GUI=ON first" >&2; exit 1; }
done

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

echo "==> staging AppDir"
mkdir -p "$WORK/AppDir/usr/bin"
cp "$BUILD_DIR/bin/Pdf4QtViewer"    "$WORK/AppDir/usr/bin/albdf-viewer"
cp "$BUILD_DIR/bin/Pdf4QtEditor"    "$WORK/AppDir/usr/bin/albdf-editor"
cp "$BUILD_DIR/bin/Pdf4QtPageMaster" "$WORK/AppDir/usr/bin/albdf-pagemaster"

# copy our libs (Pdf4QtLibCore/Gui/Widgets) so linuxdeploy bundles them.
# System libs (harfbuzz, freetype, Qt) are resolved by linuxdeploy automatically.
mkdir -p "$WORK/AppDir/usr/lib"
cp -a "$BUILD_DIR"/lib/libPdf4Qt*.so* "$WORK/AppDir/usr/lib/" 2>/dev/null || true

# desktop entry + icon
mkdir -p "$WORK/AppDir/usr/share/applications" "$WORK/AppDir/usr/share/icons/hicolor/512x512/apps"
cp "$REPO_DIR/packaging/albdf-viewer.desktop" "$WORK/AppDir/usr/share/applications/"
cp "$REPO_DIR/packaging/icon/albdf.png" "$WORK/AppDir/usr/share/icons/hicolor/512x512/apps/albdf.png"
cp "$WORK/AppDir/usr/share/applications/albdf-viewer.desktop" "$WORK/AppDir/albdf-viewer.desktop"

echo "==> linuxdeploy (bundle Qt + libs)"
export LDAI_OUTPUT="$OUT_DIR/albdf-$VERSION-x86_64.AppImage"
mkdir -p "$OUT_DIR"
export QML_SOURCES_PATHS="" # no QML
"$LDDEPLOY" --appdir "$WORK/AppDir" \
    --executable "$WORK/AppDir/usr/bin/albdf-viewer" \
    --desktop-file "$WORK/AppDir/usr/share/applications/albdf-viewer.desktop" \
    --icon-file "$WORK/AppDir/usr/share/icons/hicolor/512x512/apps/albdf.png" \
    --plugin qt \
    --output appimage

echo "==> done"
ls -la "$OUT_DIR"/albdf-*.AppImage
