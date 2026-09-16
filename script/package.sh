#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
./script/build_core.sh
CONFIGURATION="${CONFIGURATION:-release}"
CORE_VERSION="$(python3 script/version.py)"
APP_VERSION="${APP_VERSION:-$CORE_VERSION}"
[[ "$APP_VERSION" == "$CORE_VERSION" ]] || { echo "Application version must match Cargo.toml" >&2; exit 1; }
PACKAGE_ARCH="$(uname -m)"
if [[ "${UNIVERSAL:-0}" == 1 ]]; then PACKAGE_ARCH=universal; fi
ARCHIVE="$ROOT_DIR/dist/EWAF-$APP_VERSION-macos-$PACKAGE_ARCH.zip"
APP_BUILD="${APP_BUILD:-2}"
APP_BUNDLE="$ROOT_DIR/dist/EWAF.app"
BUILD_FLAGS=(-c "$CONFIGURATION")
if [[ "${UNIVERSAL:-0}" == 1 ]]; then BUILD_FLAGS+=(--arch arm64 --arch x86_64); fi
swift build "${BUILD_FLAGS[@]}" --product EWAF
BUILD_DIR="$(swift build "${BUILD_FLAGS[@]}" --show-bin-path)"
mkdir -p "$APP_BUNDLE/Contents/MacOS" "$APP_BUNDLE/Contents/Resources"
cp THIRD_PARTY_NOTICES.md "$APP_BUNDLE/Contents/Resources/"
cp "$BUILD_DIR/EWAF" "$APP_BUNDLE/Contents/MacOS/EWAF"
/usr/bin/python3 - "$APP_BUNDLE/Contents/Info.plist" "$APP_VERSION" "$APP_BUILD" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'wb') as output:
    plistlib.dump({
        'CFBundleExecutable': 'EWAF',
        'CFBundleIdentifier': 'com.tlolabs.ewaf',
        'CFBundleName': 'EWAF',
        'CFBundleDisplayName': 'EWAF',
        'CFBundlePackageType': 'APPL',
        'CFBundleShortVersionString': sys.argv[2],
        'CFBundleVersion': sys.argv[3],
        'LSMinimumSystemVersion': '14.0',
        'NSPrincipalClass': 'NSApplication',
        'NSHighResolutionCapable': True,
        'NSHumanReadableCopyright': 'EWAF — Every Week a Folder',
    }, output)
PY
if [[ -n "${SIGNING_IDENTITY:-}" ]]; then
    codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$APP_BUNDLE"
else
    codesign --force --sign - "$APP_BUNDLE"
fi
codesign --verify --strict --verbose=2 "$APP_BUNDLE"
ditto -c -k --sequesterRsrc --keepParent "$APP_BUNDLE" "$ARCHIVE"
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    : "${SIGNING_IDENTITY:?Notarization requires a Developer ID Application signing identity}"
    xcrun notarytool submit "$ARCHIVE" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$APP_BUNDLE"
    xcrun stapler validate "$APP_BUNDLE"
    ditto -c -k --sequesterRsrc --keepParent "$APP_BUNDLE" "$ARCHIVE"
fi
python3 script/validate_package.py "$ARCHIVE"
printf 'Built %s\n' "$APP_BUNDLE"
