#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./script/build_linux.sh
version="$(python3 script/version.py)"
arch="$(dpkg --print-architecture)"
stage="build/deb-$arch"
mkdir -p "$stage/usr/bin" "$stage/usr/share/applications" "$stage/usr/share/glib-2.0/schemas" "$stage/DEBIAN" dist
mkdir -p "$stage/usr/share/doc/ewaf"
cp THIRD_PARTY_NOTICES.md "$stage/usr/share/doc/ewaf/"
cp build/linux/ewaf "$stage/usr/bin/ewaf"
cp platform/linux/data/com.tlolabs.ewaf.desktop "$stage/usr/share/applications/"
cp platform/linux/data/com.tlolabs.ewaf.gschema.xml "$stage/usr/share/glib-2.0/schemas/"
cat > "$stage/DEBIAN/control" <<EOF
Package: ewaf
Version: $version
Architecture: $arch
Maintainer: tlolabs
Depends: libgtk-4-1 (>= 4.10), libadwaita-1-0 (>= 1.4), libjson-glib-1.0-0, libglib2.0-bin, libc6 (>= 2.39)
Section: utils
Priority: optional
Description: Every Week a Folder
 Native weekly folder generator with a shared Rust core.
EOF
cat > "$stage/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
glib-compile-schemas /usr/share/glib-2.0/schemas
EOF
cat > "$stage/DEBIAN/postrm" <<'EOF'
#!/bin/sh
set -e
if command -v glib-compile-schemas >/dev/null; then glib-compile-schemas /usr/share/glib-2.0/schemas; fi
EOF
chmod 755 "$stage/DEBIAN/postinst" "$stage/DEBIAN/postrm"
desktop-file-validate "$stage/usr/share/applications/com.tlolabs.ewaf.desktop"
glib-compile-schemas --strict --dry-run "$stage/usr/share/glib-2.0/schemas"
dpkg-deb --build --root-owner-group "$stage" "dist/EWAF-$version-linux-$arch.deb"
python3 script/validate_package.py "dist/EWAF-$version-linux-$arch.deb"
