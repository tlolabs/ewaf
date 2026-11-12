#!/usr/bin/env bash
# Reopen the final deliverable, not the staging app, before claiming notarization.
set -euo pipefail
[[ $# == 2 && -f "$1" && "$2" != */* && "$2" == *.app ]] || { echo 'Usage: verify_notarized_zip.sh ZIP APP_NAME.app' >&2; exit 2; }
WORK="$(mktemp -d "${TMPDIR:-/tmp}/verify-notarized.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
ditto -x -k "$1" "$WORK"
APP="$WORK/$2"
[[ -d "$APP/Contents" ]] || { echo 'Final ZIP is missing the expected app.' >&2; exit 2; }
codesign --verify --deep --strict "$APP"
xcrun stapler validate "$APP"
spctl --assess --type execute --verbose "$APP"
printf 'Final ZIP signature, staple and Gatekeeper verified: %s\n' "$1"
