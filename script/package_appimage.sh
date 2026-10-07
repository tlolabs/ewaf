#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
: "${LINUXDEPLOY:?Set a reviewed linuxdeploy executable path}"
: "${LINUXDEPLOY_SHA256:?Pin the linuxdeploy digest}"
: "${APPIMAGETOOL:?Set a reviewed appimagetool executable path}"
: "${APPIMAGETOOL_SHA256:?Pin the appimagetool digest}"
printf '%s  %s\n' "$LINUXDEPLOY_SHA256" "$LINUXDEPLOY" | sha256sum --check
printf '%s  %s\n' "$APPIMAGETOOL_SHA256" "$APPIMAGETOOL" | sha256sum --check
: "${APPIMAGE_RUNTIME:?Set the pinned AppImage runtime path}"
: "${APPIMAGE_RUNTIME_SHA256:?Pin the AppImage runtime digest}"
printf '%s  %s\n' "$APPIMAGE_RUNTIME_SHA256" "$APPIMAGE_RUNTIME" | sha256sum --check
./script/build_linux.sh
version="$(python3 script/version.py)"
case "$(uname -m)" in x86_64) arch=x64;; aarch64) arch=arm64;; *) exit 1;; esac
stage="$(mktemp -d "$PWD/build/AppDir.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/usr/bin" "$stage/usr/share/doc/ewaf"
cp -R build/linux/. "$stage/usr/bin/"
cp LICENSE THIRD_PARTY_NOTICES.md "$stage/usr/share/doc/ewaf/"
cp assets/icon/ewaf.svg "$stage/com.tlolabs.ewaf.svg"
# linuxdeploy bundles ELF dependencies; explicitly include the helper in its dependency scan.
# Include the Rust FFI and rendering libraries in the dependency closure.
extra=()
for library in "$stage"/usr/bin/*.so; do extra+=(--library "$library"); done
APPIMAGE_EXTRACT_AND_RUN=1 "$LINUXDEPLOY" --appdir "$stage" --executable "$stage/usr/bin/ewaf" --executable "$stage/usr/bin/ewaf-update" "${extra[@]}" --desktop-file platform/linux/data/com.tlolabs.ewaf.desktop --icon-file "$stage/com.tlolabs.ewaf.svg"
# Preserve the canonical icon ID expected by the desktop entry.
cp assets/icon/ewaf.svg "$stage/com.tlolabs.ewaf.svg"
# linuxdeploy creates AppRun as a symlink to usr/bin/ewaf. Replace the
# link itself before writing a wrapper, otherwise redirection corrupts the ELF.
rm -f "$stage/AppRun"
cat > "$stage/AppRun" <<'RUN'
#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export XDG_DATA_DIRS="$root/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export LD_LIBRARY_PATH="$root/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$root/usr/bin/ewaf" "$@"
RUN
chmod 755 "$stage/AppRun"
mkdir -p dist
output="$PWD/dist/ewaf-$version-linux-$arch.AppImage"
APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" --runtime-file "$APPIMAGE_RUNTIME" "$stage" "$output"
chmod 755 "$output"
# Execute only our just-built package, never an unauthenticated remote artifact.
APPIMAGE_EXTRACT_AND_RUN=1 "$output" --core-smoke
# The packaged shared window validates bindings/core with isolated preferences.
mkdir -p "$stage/empty-data" "$stage/smoke-config" "$stage/smoke-cache"
APPIMAGE_EXTRACT_AND_RUN=1 \
  XDG_DATA_DIRS="$stage/empty-data" XDG_CONFIG_HOME="$stage/smoke-config" \
  XDG_CACHE_HOME="$stage/smoke-cache" \
  xvfb-run -a dbus-run-session -- "$output" --ui-smoke
sha256sum "$output" > "$output.sha256"
