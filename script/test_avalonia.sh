#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v dotnet >/dev/null; then export PATH="$PWD/.build-tools/dotnet:$PATH"; fi
export AVALONIA_TELEMETRY_OPTOUT=1 DOTNET_CLI_TELEMETRY_OPTOUT=1
cargo build --release --locked -p ewaf-ffi
dotnet build tests/avalonia -c Release -p:RestoreLockedMode=true
case "$(uname -s)" in Darwin) native=libewaf_ffi.dylib;; Linux) native=libewaf_ffi.so;; *) exit 1;; esac
cp "target/release/$native" tests/avalonia/bin/Release/net10.0/
dotnet tests/avalonia/bin/Release/net10.0/EWAF.Tests.dll
