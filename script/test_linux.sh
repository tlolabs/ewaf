#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./script/test_avalonia.sh
./script/build_linux.sh
build/linux/ewaf --ui-smoke
