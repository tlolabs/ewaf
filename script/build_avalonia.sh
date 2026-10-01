#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/.."
export AVALONIA_TELEMETRY_OPTOUT=1 DOTNET_CLI_TELEMETRY_OPTOUT=1
runtime="${1:-osx-arm64}"
case "$runtime" in
 osx-arm64) [[ "$(uname -s)" == Darwin && "$(uname -m)" == arm64 ]]; target=aarch64-apple-darwin; native=libewaf_ffi.dylib;;
 linux-x64) [[ "$(uname -s)" == Linux ]]; target=x86_64-unknown-linux-gnu; native=libewaf_ffi.so;;
 linux-arm64) [[ "$(uname -s)" == Linux ]]; target=aarch64-unknown-linux-gnu; native=libewaf_ffi.so;;
 *) echo 'Use package_windows.ps1 for Windows; macOS Avalonia is ARM64 internal only.' >&2; exit 1;;
esac
if ! command -v dotnet >/dev/null; then export PATH="$PWD/.build-tools/dotnet:$PATH"; fi
rustup target add "$target"
packages=(-p ewaf-ffi)
[[ "$runtime" == osx-arm64 ]] || packages+=(-p ewaf-update)
cargo build --release --locked --target "$target" "${packages[@]}"
stage="build/avalonia/$runtime"
rm -rf "$stage"
dotnet publish platform/avalonia/EWAF/EWAF.csproj -c Release -r "$runtime" --self-contained true -p:RestoreLockedMode=true -o "$stage"
cp "target/$target/release/$native" "$stage/"
if [[ "$runtime" != osx-arm64 ]]; then cp "target/$target/release/ewaf-update" "$stage/"; fi
cp LICENSE THIRD_PARTY_NOTICES.md "$stage/"
python3 script/collect_dotnet_notices.py "$stage"
"$stage/EWAF" --core-smoke
