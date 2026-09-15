#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
CONFIGURATION="${CONFIGURATION:-release}"
APP_VERSION="${APP_VERSION:-1.0.0}"
APP_BUILD="${APP_BUILD:-1}"
APP_BUNDLE="$ROOT_DIR/dist/EWAF.app"
BUILD_FLAGS=(-c "$CONFIGURATION")
if [[ "${UNIVERSAL:-0}" == 1 ]]; then BUILD_FLAGS+=(--arch arm64 --arch x86_64); fi
swift build "${BUILD_FLAGS[@]}" --product EWAF
BUILD_DIR="$(swift build "${BUILD_FLAGS[@]}" --show-bin-path)"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
cp "$BUILD_DIR/EWAF" "$APP_BUNDLE/Contents/MacOS/EWAF"
/usr/bin/python3 - "$APP_BUNDLE/Contents/Info.plist" "$APP_VERSION" "$APP_BUILD" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'wb') as output:
    plistlib.dump({
        'CFBundleExecutable': 'EWAF',
        'CFBundleIdentifier': 'com.tlolabs.ewaf',
        'CFBundleName': 'E.W.A.F.',
        'CFBundleDisplayName': 'E.W.A.F.',
        'CFBundlePackageType': 'APPL',
        'CFBundleShortVersionString': sys.argv[2],
        'CFBundleVersion': sys.argv[3],
        'LSMinimumSystemVersion': '14.0',
        'NSPrincipalClass': 'NSApplication',
        'NSHighResolutionCapable': True,
        'NSHumanReadableCopyright': 'E.W.A.F. — Every Week a Folder',
    }, output)
PY
if [[ -n "${SIGNING_IDENTITY:-}" ]]; then
    codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$APP_BUNDLE"
else
    codesign --force --sign - "$APP_BUNDLE"
fi
codesign --verify --strict --verbose=2 "$APP_BUNDLE"
ditto -c -k --sequesterRsrc --keepParent "$APP_BUNDLE" "$ROOT_DIR/dist/EWAF-macOS.zip"
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    : "${SIGNING_IDENTITY:?Notarization requires a Developer ID Application signing identity}"
    xcrun notarytool submit "$ROOT_DIR/dist/EWAF-macOS.zip" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$APP_BUNDLE"
    xcrun stapler validate "$APP_BUNDLE"
    ditto -c -k --sequesterRsrc --keepParent "$APP_BUNDLE" "$ROOT_DIR/dist/EWAF-macOS.zip"
fi
shasum -a 256 "$ROOT_DIR/dist/EWAF-macOS.zip" > "$ROOT_DIR/dist/EWAF-macOS.zip.sha256"
printf 'Built %s\n' "$APP_BUNDLE"
