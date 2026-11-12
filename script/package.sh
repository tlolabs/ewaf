#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
PACKAGE_MODE="${1:---notarize}"
[[ $# -le 1 ]] || { echo 'Usage: package.sh [--notarize|--unsigned]' >&2; exit 2; }
case "$PACKAGE_MODE" in
  --notarize)
    : "${SIGNING_IDENTITY:?Set SIGNING_IDENTITY to a Developer ID Application identity}"
    : "${NOTARY_PROFILE:?Set NOTARY_PROFILE to a stored notarytool Keychain profile}"
    [[ "$SIGNING_IDENTITY" == "Developer ID Application:"* ]] || { echo 'A Developer ID Application identity is required.' >&2; exit 2; }
    security find-identity -v -p codesigning | grep -F -- "\"$SIGNING_IDENTITY\"" >/dev/null || { echo 'Developer ID identity is not available in Keychain.' >&2; exit 2; }
    xcrun notarytool history --keychain-profile "$NOTARY_PROFILE" --output-format json >/dev/null
    ;;
  --unsigned)
    [[ "${EWAF_PRODUCTION:-0}" != 1 ]] || { echo 'Production packaging requires notarization.' >&2; exit 2; }
    SIGNING_IDENTITY=; NOTARY_PROFILE= ;;
  *) echo 'Usage: package.sh [--notarize|--unsigned]' >&2; exit 2 ;;
esac
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
    if DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer xcodebuild -license status >/dev/null 2>&1; then
        export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
    fi
fi
./script/build_core.sh
CONFIGURATION="${CONFIGURATION:-release}"
if [[ "${EWAF_PRODUCTION:-0}" == 1 && "$CONFIGURATION" != release ]]; then
    echo "Production packaging requires release configuration" >&2; exit 1
fi
CORE_VERSION="$(python3 script/version.py)"
APP_VERSION="${APP_VERSION:-$CORE_VERSION}"
[[ "$APP_VERSION" == "$CORE_VERSION" ]] || { echo "Application version must match Cargo.toml" >&2; exit 1; }
PACKAGE_ARCH="$(uname -m)"
if [[ "${UNIVERSAL:-0}" == 1 ]]; then PACKAGE_ARCH=universal; fi
PACKAGE_SUFFIX=
if [[ "$PACKAGE_MODE" == --unsigned ]]; then PACKAGE_SUFFIX=-development-unnotarized; fi
ARCHIVE="$ROOT_DIR/dist/EWAF-$APP_VERSION-macos-$PACKAGE_ARCH$PACKAGE_SUFFIX.zip"
APP_BUILD="$APP_VERSION" # Stable updater comparison uses the authoritative SemVer, never a CI counter.
APP_BUNDLE="$ROOT_DIR/dist/notarized/EWAF.app"
# The development launcher retains its historical local app path.
if [[ "$PACKAGE_MODE" == --unsigned ]]; then APP_BUNDLE="$ROOT_DIR/dist/EWAF.app"; fi
BUILD_FLAGS=(-c "$CONFIGURATION")
if [[ "${UNIVERSAL:-0}" == 1 ]]; then BUILD_FLAGS+=(--arch arm64 --arch x86_64); fi
swift build "${BUILD_FLAGS[@]}" --product EWAF
BUILD_DIR="$(swift build "${BUILD_FLAGS[@]}" --show-bin-path)"
mkdir -p "$APP_BUNDLE/Contents/MacOS" "$APP_BUNDLE/Contents/Resources"
ICON_BUILD="$ROOT_DIR/build/macos-icon"
mkdir -p "$ICON_BUILD"
ACTOOL_CMD=(xcrun --sdk macosx actool)
if ! xcrun --find actool >/dev/null 2>&1 && [[ -x /Applications/Xcode.app/Contents/Developer/usr/bin/actool ]]; then
    ACTOOL_CMD=(/Applications/Xcode.app/Contents/Developer/usr/bin/actool)
fi
"${ACTOOL_CMD[@]}" "$ROOT_DIR/assets/icon/EWAF.icon" --compile "$ICON_BUILD" \
    --output-format human-readable-text --notices --warnings --errors \
    --output-partial-info-plist "$ICON_BUILD/icon-info.plist" --app-icon EWAF \
    --enable-on-demand-resources NO --development-region en --target-device mac \
    --minimum-deployment-target 14.0 --platform macosx
# Remove the previous flat icon when rebuilding an existing local app bundle.
rm -f "$APP_BUNDLE/Contents/Resources/ewaf.icns"
cp "$ICON_BUILD/Assets.car" "$ICON_BUILD/EWAF.icns" "$APP_BUNDLE/Contents/Resources/"
cp THIRD_PARTY_NOTICES.md "$APP_BUNDLE/Contents/Resources/"
cp LICENSE "$APP_BUNDLE/Contents/Resources/"
cp "$BUILD_DIR/EWAF" "$APP_BUNDLE/Contents/MacOS/EWAF"
/usr/bin/python3 - "$APP_BUNDLE/Contents/Info.plist" "$APP_VERSION" "$APP_BUILD" "$ICON_BUILD/icon-info.plist" <<'PY'
import plistlib, sys
with open(sys.argv[1], 'wb') as output:
    info = {
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
        'NSHumanReadableCopyright': 'Copyright © Thomas Lothian.',
    }
    with open(sys.argv[4], 'rb') as icon_info:
        info.update(plistlib.load(icon_info))
    plistlib.dump(info, output)
PY
UPDATE_ARCH="$PACKAGE_ARCH"
[[ "$UPDATE_ARCH" != x86_64 ]] || UPDATE_ARCH=x64
EWAF_UPDATE_ARCH="$UPDATE_ARCH" python3 script/configure_updates.py "$APP_BUNDLE/Contents/Info.plist"
SPARKLE_FRAMEWORK="$(./script/prepare_sparkle.sh)"
mkdir -p "$APP_BUNDLE/Contents/Frameworks"
ditto "$SPARKLE_FRAMEWORK" "$APP_BUNDLE/Contents/Frameworks/Sparkle.framework"
if [[ "${EWAF_PRODUCTION:-0}" == 1 ]]; then
    : "${SIGNING_IDENTITY:?Production requires Developer ID signing}"
    : "${NOTARY_PROFILE:?Production requires notarization}"
fi
# Sign nested Sparkle helpers from the inside out, preserving upstream entitlements.
SIGN_OPTIONS=(--force --options runtime)
if [[ -n "$SIGNING_IDENTITY" ]]; then SIGN_OPTIONS+=(--timestamp); fi
while IFS= read -r item; do
    codesign "${SIGN_OPTIONS[@]}" --preserve-metadata=entitlements --sign "${SIGNING_IDENTITY:--}" "$item"
done < <(find "$APP_BUNDLE/Contents/Frameworks/Sparkle.framework" -type f -perm +111)
for item in "$APP_BUNDLE/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Downloader.xpc" "$APP_BUNDLE/Contents/Frameworks/Sparkle.framework/Versions/B/XPCServices/Installer.xpc" "$APP_BUNDLE/Contents/Frameworks/Sparkle.framework/Versions/B/Updater.app" "$APP_BUNDLE/Contents/Frameworks/Sparkle.framework"; do
    codesign "${SIGN_OPTIONS[@]}" --preserve-metadata=entitlements --sign "${SIGNING_IDENTITY:--}" "$item"
done
if [[ -n "${SIGNING_IDENTITY:-}" ]]; then
    codesign --force --options runtime --timestamp --sign "$SIGNING_IDENTITY" "$APP_BUNDLE"
else
    codesign --force --sign - "$APP_BUNDLE"
fi
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"
if [[ -n "${NOTARY_PROFILE:-}" ]]; then
    : "${SIGNING_IDENTITY:?Notarization requires a Developer ID Application signing identity}"
    bash script/notarize_app.sh "$APP_BUNDLE" "$ROOT_DIR/dist"
fi
PACKAGE_WORK="$(mktemp -d "$ROOT_DIR/dist/package.XXXXXX")"
trap 'rm -rf "$PACKAGE_WORK"' EXIT
CANDIDATE="$PACKAGE_WORK/$(basename "$ARCHIVE")"
ditto -c -k --sequesterRsrc --keepParent "$APP_BUNDLE" "$CANDIDATE"
if [[ "$PACKAGE_MODE" == --notarize ]]; then
    bash script/verify_notarized_zip.sh "$CANDIDATE" EWAF.app
fi
python3 script/validate_package.py "$CANDIDATE"
mv "$CANDIDATE" "$ARCHIVE"
mv "$CANDIDATE.sha256" "$ARCHIVE.sha256"
printf 'Built %s\n' "$APP_BUNDLE"
