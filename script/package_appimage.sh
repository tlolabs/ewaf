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
mkdir -p "$stage/usr/bin" "$stage/usr/share/glib-2.0/schemas" "$stage/usr/share/doc/ewaf"
cp build/linux/ewaf build/linux/ewaf-update "$stage/usr/bin/"
cp platform/linux/data/com.tlolabs.ewaf.gschema.xml "$stage/usr/share/glib-2.0/schemas/"
# Native GTK dialogs use their own schemas and symbolic icons. Include these
# resources rather than relying on the build host's XDG data directories.
gtk_prefix="$(pkg-config --variable=prefix gtk4)"
shopt -s nullglob
gtk_schemas=("$gtk_prefix"/share/glib-2.0/schemas/org.gtk.gtk4.Settings.*.gschema.xml)
if (( ${#gtk_schemas[@]} == 0 )); then
  echo 'GTK 4 settings schemas are required for the AppImage.' >&2
  exit 1
fi
cp "${gtk_schemas[@]}" "$stage/usr/share/glib-2.0/schemas/"
glib-compile-schemas --strict "$stage/usr/share/glib-2.0/schemas"
mkdir -p "$stage/usr/share/icons"
cp -R "$gtk_prefix/share/icons/Adwaita" "$stage/usr/share/icons/"
cp "$gtk_prefix/share/doc/adwaita-icon-theme/copyright" "$stage/usr/share/doc/ewaf/Adwaita-icons-copyright"
cp LICENSE THIRD_PARTY_NOTICES.md "$stage/usr/share/doc/ewaf/"
cp assets/icon/ewaf.svg "$stage/com.tlolabs.ewaf.svg"
# linuxdeploy bundles ELF dependencies; explicitly include the helper in its dependency scan.
APPIMAGE_EXTRACT_AND_RUN=1 "$LINUXDEPLOY" --appdir "$stage" --executable "$stage/usr/bin/ewaf" --executable "$stage/usr/bin/ewaf-update" --desktop-file platform/linux/data/com.tlolabs.ewaf.desktop --icon-file "$stage/com.tlolabs.ewaf.svg"
# Preserve the canonical icon ID expected by the desktop entry.
cp assets/icon/ewaf.svg "$stage/com.tlolabs.ewaf.svg"
cat > "$stage/AppRun" <<'RUN'
#!/bin/sh
set -eu
root="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export GSETTINGS_SCHEMA_DIR="$root/usr/share/glib-2.0/schemas"
export XDG_DATA_DIRS="$root/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export LD_LIBRARY_PATH="$root/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$root/usr/bin/ewaf" "$@"
RUN
chmod 755 "$stage/AppRun"
mkdir -p dist
output="$PWD/dist/ewaf-$version-linux-$arch.AppImage"
flags=()
if [[ "${EWAF_PRODUCTION:-0}" == 1 ]]; then
  : "${LINUX_SIGNING_KEY:?Production AppImages require GPG signing}"
  flags+=(--sign --sign-key "$LINUX_SIGNING_KEY")
fi
APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" --runtime-file "$APPIMAGE_RUNTIME" "${flags[@]}" "$stage" "$output"
chmod 755 "$output"
# Execute only our just-built package, never an unauthenticated remote artifact.
APPIMAGE_EXTRACT_AND_RUN=1 "$output" --core-smoke
# A native window/settings smoke uses only packaged XDG resources and an
# isolated in-memory settings backend. It performs no automatic update checks.
mkdir -p "$stage/empty-data" "$stage/smoke-config" "$stage/smoke-cache"
APPIMAGE_EXTRACT_AND_RUN=1 GSETTINGS_BACKEND=memory \
  XDG_DATA_DIRS="$stage/empty-data" XDG_CONFIG_HOME="$stage/smoke-config" \
  XDG_CACHE_HOME="$stage/smoke-cache" \
  xvfb-run -a dbus-run-session -- "$output" --ui-smoke
if [[ "${EWAF_PRODUCTION:-0}" == 1 ]]; then
  printf '%s' "${APPIMAGETOOL_SIGN_PASSPHRASE:-}" | gpg --batch --pinentry-mode loopback --passphrase-fd 0 --local-user "$LINUX_SIGNING_KEY" --armor --detach-sign "$output"
  gpg --batch --verify "$output.asc" "$output"
fi
sha256sum "$output" > "$output.sha256"
