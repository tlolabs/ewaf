#!/usr/bin/env bash
# Submit a fully signed app, require Apple's acceptance, then staple and assess.
set -euo pipefail
[[ $# == 2 ]] || { echo 'Usage: notarize_app.sh APP OUTPUT_DIRECTORY' >&2; exit 2; }
APP="$1"
: "${NOTARY_PROFILE:?Set NOTARY_PROFILE to a stored notarytool Keychain profile}"
[[ -d "$APP/Contents" ]] || { echo 'Expected an app bundle.' >&2; exit 2; }
codesign --verify --deep --strict "$APP"
DETAILS="$(codesign -dv --verbose=4 "$APP" 2>&1)"
if ! printf '%s\n' "$DETAILS" | grep -q '^Authority=Developer ID Application:' || ! printf '%s\n' "$DETAILS" | grep -Eq '^CodeDirectory .*flags=.*\([^)]*runtime'; then echo 'Notarization requires Developer ID and hardened runtime.' >&2; exit 2; fi
mkdir -p "$2"
WORK="$(mktemp -d "$2/notary.XXXXXX")"
# Keep the JSON receipt for diagnosis; an unsuccessful submission never produces success.
ditto -c -k --keepParent "$APP" "$WORK/submission.zip"
xcrun notarytool submit "$WORK/submission.zip" --keychain-profile "$NOTARY_PROFILE" --wait --output-format json > "$WORK/result.json"
python3 - "$WORK/result.json" <<'CHECK'
import json, sys
with open(sys.argv[1]) as f:
    report = json.load(f)
if report.get("status") != "Accepted":
    raise SystemExit("Apple did not accept notarization; see " + sys.argv[1])
CHECK
xcrun stapler staple "$APP"
xcrun stapler validate "$APP"
codesign --verify --deep --strict "$APP"
spctl --assess --type execute --verbose "$APP"
rm -f "$WORK/submission.zip"
printf 'Notarization accepted and stapled: %s (receipt: %s)\n' "$APP" "$WORK/result.json"
