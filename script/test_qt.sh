#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
cargo build --release --locked -p ewaf-ffi
build_dir="build/qt-test"
cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --target ewaf_tests
"$build_dir/ewaf_tests"
