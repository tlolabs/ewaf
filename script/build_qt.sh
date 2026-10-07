#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
runtime="${1:-osx-arm64}"
case "$runtime" in
  osx-arm64) [[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]]; target=aarch64-apple-darwin; native=libewaf_ffi.dylib;;
  linux-x64) [[ "$(uname -s)" == Linux ]]; target=x86_64-unknown-linux-gnu; native=libewaf_ffi.so;;
  linux-arm64) [[ "$(uname -s)" == Linux ]]; target=aarch64-unknown-linux-gnu; native=libewaf_ffi.so;;
  *) echo 'Use package_windows.ps1 for Windows; macOS Qt is ARM64 internal only.' >&2; exit 1;;
esac

rustup target add "$target"
packages=(-p ewaf-ffi)
[[ "$runtime" == osx-arm64 ]] || packages+=(-p ewaf-update)
cargo build --release --locked --target "$target" "${packages[@]}"

build_dir="build/qt/$runtime"
stage="$build_dir/stage"
rm -rf "$stage"
mkdir -p "$stage"

cmake -B "$build_dir" -DCMAKE_BUILD_TYPE=Release
cmake --build "$build_dir" --target EWAF

if [[ "$(uname -s)" == Darwin ]]; then
  cp "$build_dir/EWAF.app/Contents/MacOS/EWAF" "$stage/EWAF"
  cp "target/$target/release/$native" "$stage/$native"
else
  cp "$build_dir/EWAF" "$stage/EWAF"
  cp "target/$target/release/$native" "$stage/$native"
  if [[ "$runtime" != osx-arm64 ]]; then
    cp "target/$target/release/ewaf-update" "$stage/ewaf-update"
  fi
fi

cp LICENSE THIRD_PARTY_NOTICES.md "$stage/"
"$stage/EWAF" --core-smoke
