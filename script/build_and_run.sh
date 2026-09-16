#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
MODE="${1:-run}"
case "$MODE" in run|--debug|--logs|--telemetry|--verify) ;; *) echo "usage: $0 [--debug|--logs|--telemetry|--verify]" >&2; exit 2;; esac
pkill -x EWAF >/dev/null 2>&1 || true
CONFIGURATION="${CONFIGURATION:-debug}" ./script/package.sh
APP_BUNDLE="$ROOT_DIR/dist/EWAF.app"
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
