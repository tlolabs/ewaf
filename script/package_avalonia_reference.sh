#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
./script/build_avalonia.sh osx-arm64
bundle="dist/EWAF Avalonia Internal.app"
rm -rf "$bundle"
mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources"
cp -R build/avalonia/osx-arm64/. "$bundle/Contents/MacOS/"
mv "$bundle/Contents/MacOS/licenses" "$bundle/Contents/Resources/"
python3 - <<'PY'
import plistlib,sys
from pathlib import Path
sys.path.insert(0,'script')
from version import VERSION
p=Path('dist/EWAF Avalonia Internal.app/Contents/Info.plist')
p.write_bytes(plistlib.dumps(dict(CFBundleIdentifier='com.tlolabs.ewaf.avalonia-internal',CFBundleName='EWAF Avalonia Internal',CFBundleDisplayName='EWAF Avalonia Internal',CFBundleExecutable='EWAF',CFBundlePackageType='APPL',CFBundleShortVersionString=VERSION,CFBundleVersion=VERSION,LSMinimumSystemVersion='14.0',NSHighResolutionCapable=True,EWAFDistribution='internal-reference',NSPrincipalClass='NSApplication')))
PY
# Local ad-hoc integrity only. Never Developer ID / notarization / Sparkle enrollment.
codesign --force --deep --sign - "$bundle"
archive="dist/internal-avalonia-reference-$(python3 script/version.py)-osx-arm64.zip"
ditto -c -k --keepParent "$bundle" "$archive"
python3 script/validate_reference.py "$archive"
