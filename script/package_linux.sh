#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./script/build_linux.sh
version="$(python3 script/version.py)"
arch="$(dpkg --print-architecture)"
stage="build/deb-$arch"
rm -rf "$stage"
mkdir -p "$stage/usr/bin" "$stage/usr/lib/ewaf" "$stage/usr/share/applications" "$stage/DEBIAN" dist
mkdir -p "$stage/usr/share/doc/ewaf" "$stage/usr/share/icons/hicolor/scalable/apps"
cp -R build/linux/. "$stage/usr/lib/ewaf/"
ln -s ../lib/ewaf/ewaf "$stage/usr/bin/ewaf"
cp THIRD_PARTY_NOTICES.md LICENSE "$stage/usr/share/doc/ewaf/"
cp assets/icon/ewaf.svg "$stage/usr/share/icons/hicolor/scalable/apps/com.tlolabs.ewaf.svg"
cp platform/linux/data/com.tlolabs.ewaf.desktop "$stage/usr/share/applications/"
cat > "$stage/DEBIAN/control" <<CONTROL
Package: ewaf
Version: $version
Architecture: $arch
Maintainer: Thomas Lothian <153565009+tlolabs@users.noreply.github.com>
Depends: libc6 (>= 2.39), libgcc-s1, libstdc++6, libqt6core6t64, libqt6gui6, libqt6widgets6, libqt6concurrent6, xdg-utils, dconf-cli
Section: utils
Priority: optional
Homepage: https://github.com/tlolabs/ewaf
Description: Every Week a Folder
 A TLO Labs open-source project. Shared Qt UI with a Rust core.
CONTROL
desktop-file-validate "$stage/usr/share/applications/com.tlolabs.ewaf.desktop"
dpkg-deb --build --root-owner-group "$stage" "dist/EWAF-$version-linux-$arch.deb"
python3 script/validate_package.py "dist/EWAF-$version-linux-$arch.deb"
