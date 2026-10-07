#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
./script/build_qt.sh osx-arm64
bundle="dist/EWAF Qt Internal.app"
rm -rf "$bundle"
mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources"
cp build/qt/osx-arm64/stage/EWAF "$bundle/Contents/MacOS/EWAF"
cp build/qt/osx-arm64/stage/libewaf_ffi.dylib "$bundle/Contents/MacOS/libewaf_ffi.dylib"
ffi_dependency="$(otool -L "$bundle/Contents/MacOS/EWAF" | awk '/libewaf_ffi[.]dylib/{print $1; exit}')"
[[ -n "$ffi_dependency" ]] || { echo 'Internal Qt executable does not link ewaf_ffi' >&2; exit 1; }
install_name_tool -change "$ffi_dependency" '@executable_path/libewaf_ffi.dylib' "$bundle/Contents/MacOS/EWAF"
install_name_tool -id '@executable_path/libewaf_ffi.dylib' "$bundle/Contents/MacOS/libewaf_ffi.dylib"
cp LICENSE THIRD_PARTY_NOTICES.md "$bundle/Contents/Resources/"
cp assets/icon/ewaf.icns "$bundle/Contents/Resources/Reference.icns"
python3 - <<'PY'
import plistlib,sys
from pathlib import Path
sys.path.insert(0,'script')
from version import VERSION
p=Path('dist/EWAF Qt Internal.app/Contents/Info.plist')
p.write_bytes(plistlib.dumps(dict(CFBundleIdentifier='com.tlolabs.ewaf.qt-internal',CFBundleName='EWAF Qt Internal',CFBundleDisplayName='EWAF Qt Internal',CFBundleExecutable='EWAF',CFBundlePackageType='APPL',CFBundleIconFile='Reference.icns',CFBundleShortVersionString=VERSION,CFBundleVersion=VERSION,LSMinimumSystemVersion='14.0',NSHighResolutionCapable=True,EWAFDistribution='internal-reference',NSPrincipalClass='NSApplication')))
PY
# Local ad-hoc integrity only. Never Developer ID / notarization / Sparkle enrollment.
codesign --force --deep --sign - "$bundle"
archive="dist/internal-qt-reference-$(python3 script/version.py)-osx-arm64.zip"
ditto -c -k --keepParent "$bundle" "$archive"
python3 script/validate_reference.py "$archive"
