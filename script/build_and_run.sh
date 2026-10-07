#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
MODE="${1:-run}"
if [[ "$MODE" == --qt-reference ]]; then
    # Separate identity/path; never stop or replace the production SwiftUI app.
    ./script/package_qt_reference.sh
    /usr/bin/open -n "$ROOT_DIR/dist/EWAF Qt Internal.app"
    exit 0
fi
case "$MODE" in run|--debug|--logs|--telemetry|--verify) ;; *) echo "usage: $0 [--debug|--logs|--telemetry|--verify|--qt-reference]" >&2; exit 2;; esac
pkill -f "^$ROOT_DIR/dist/EWAF.app/Contents/MacOS/EWAF([[:space:]]|$)" >/dev/null 2>&1 || true
CONFIGURATION="${CONFIGURATION:-debug}" ./script/package.sh
APP_BUNDLE="$ROOT_DIR/dist/EWAF.app"
# Rebuilt resources do not change the outer bundle's modification date.
# Refresh this bundle's registration so the Dock does not reuse its old icon.
touch "$APP_BUNDLE"
/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -f "$APP_BUNDLE"
case "$MODE" in
    --debug) lldb -- "$APP_BUNDLE/Contents/MacOS/EWAF" ;;
    --logs|--telemetry)
        /usr/bin/open -n "$APP_BUNDLE"
        /usr/bin/log stream --info --style compact --predicate 'process == "EWAF"'
        ;;
    --verify)
        /usr/bin/open -n "$APP_BUNDLE"
        sleep 2
        pgrep -x EWAF >/dev/null
        ;;
    run) /usr/bin/open -n "$APP_BUNDLE" ;;
esac
