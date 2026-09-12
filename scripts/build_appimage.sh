#!/usr/bin/env bash
# Build a self-contained PunPun IDE AppImage.
#
# The result is a single executable file. A user downloads it, marks it
# executable once, and double-clicks it; no Qt installation is required on the
# host. Qt, libarchive and their transitive libraries are bundled.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build-appimage}"
APPDIR="${APPDIR:-$ROOT/$BUILD_DIR/AppDir}"
TOOLS="${TOOLS:-$ROOT/$BUILD_DIR/tools}"
VERSION="$(tr -d '\r\n' < VERSION)"
ARCH="${ARCH:-x86_64}"
OUTPUT="${OUTPUT:-$ROOT/dist/PunPun-IDE-v${VERSION}-${ARCH}.AppImage}"

mkdir -p "$TOOLS" "$ROOT/dist"

# A source archive unpacked on a machine whose clock is behind the packaging
# machine leaves every file dated in the future, which sends Ninja into an
# endless "Re-running CMake..." loop. Pull them back before configuring.
# shellcheck source=scripts/clamp_timestamps.sh
source "$ROOT/scripts/clamp_timestamps.sh"
clamp_future_timestamps "$ROOT"

# The AppImage tools ship as AppImages themselves. Where FUSE is unavailable
# (containers, most CI) they cannot mount, so extract each one and call the
# extracted entry point directly.
fetch_tool() {
    local name="$1" url="$2"
    local image="$TOOLS/$name.AppImage"
    local dir="$TOOLS/$name"
    if [[ -x "$dir/AppRun" ]]; then return; fi
    if [[ ! -f "$image" ]]; then
        echo "==> Downloading $name"
        curl -sSL --fail -o "$image" "$url"
    fi
    chmod +x "$image"
    ( cd "$TOOLS" && "$image" --appimage-extract >/dev/null && mv squashfs-root "$name" )
}

CONTINUOUS="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous"
QT_PLUGIN="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous"
APPIMAGETOOL="https://github.com/AppImage/appimagetool/releases/download/continuous"

fetch_tool linuxdeploy "$CONTINUOUS/linuxdeploy-${ARCH}.AppImage"
fetch_tool linuxdeploy-plugin-qt "$QT_PLUGIN/linuxdeploy-plugin-qt-${ARCH}.AppImage"
fetch_tool appimagetool "$APPIMAGETOOL/appimagetool-${ARCH}.AppImage"

echo "==> Building PunPun IDE $VERSION"
cmake -S . -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build "$BUILD_DIR" --parallel "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo "==> Staging AppDir"
rm -rf "$APPDIR"
install -Dm755 "$BUILD_DIR/punpun-ide"     "$APPDIR/usr/bin/punpun-ide"
install -Dm755 "$BUILD_DIR/ppide-pp-bridge" "$APPDIR/usr/bin/ppide-pp-bridge"
install -Dm644 punpun-ide.desktop           "$APPDIR/usr/share/applications/punpun-ide.desktop"
install -Dm644 resources/branding/punpun-mark.svg \
               "$APPDIR/usr/share/icons/hicolor/scalable/apps/punpun-ide.svg"

# linuxdeploy wants the plugin on PATH, and the Qt plugin needs to find qmake.
export PATH="$TOOLS/linuxdeploy-plugin-qt/usr/bin:$PATH"
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake || true)}"
export EXTRA_QT_MODULES="${EXTRA_QT_MODULES:-svg}"
# xcb drives real desktops; offscreen lets CI start the binary headlessly to
# prove the bundle is complete.
export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS:-libqoffscreen.so}"
# The IDE loads no QML at runtime; skip the QML scan so the bundle stays small.
export QML_SOURCES_PATHS=""

echo "==> Bundling Qt and shared libraries"
"$TOOLS/linuxdeploy/AppRun" \
    --appdir "$APPDIR" \
    --plugin qt \
    --desktop-file "$APPDIR/usr/share/applications/punpun-ide.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/scalable/apps/punpun-ide.svg" \
    --executable "$APPDIR/usr/bin/punpun-ide"

echo "==> Creating AppImage"
rm -f "$OUTPUT"
# Unsigned, reproducible-ish: no embedded update information.
ARCH="$ARCH" "$TOOLS/appimagetool/AppRun" --no-appstream "$APPDIR" "$OUTPUT"

chmod +x "$OUTPUT"
echo
echo "AppImage: $OUTPUT"
ls -lh "$OUTPUT"
