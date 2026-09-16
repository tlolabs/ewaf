#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
./script/build_core.sh
if [[ -z "${DEVELOPER_DIR:-}" && -d /Applications/Xcode.app/Contents/Developer ]]; then
    export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
fi
mkdir -p dist
if [[ -d dist/UITests.xcresult ]]; then
    mv dist/UITests.xcresult "dist/UITests-$(date +%s).xcresult"
fi
xcodebuild -project EWAF.xcodeproj -scheme EWAF -destination 'platform=macOS' -derivedDataPath DerivedData -resultBundlePath dist/UITests.xcresult test "$@"
