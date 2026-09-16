#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p target/swift
if [[ "${UNIVERSAL:-0}" == 1 ]]; then
  rustup target add aarch64-apple-darwin x86_64-apple-darwin
  for target in aarch64-apple-darwin x86_64-apple-darwin; do
    MACOSX_DEPLOYMENT_TARGET=14.0 cargo build --locked --release -p ewaf-ffi --target "$target"
  done
  lipo -create target/aarch64-apple-darwin/release/libewaf_ffi.a target/x86_64-apple-darwin/release/libewaf_ffi.a -output target/swift/libewaf_ffi.a
else
  MACOSX_DEPLOYMENT_TARGET=14.0 cargo build --locked --release -p ewaf-ffi
  cp target/release/libewaf_ffi.a target/swift/libewaf_ffi.a
fi
python3 - <<'PYCODE'
from pathlib import Path
import hashlib
header = Path('bindings/include/ewaf_build_stamp.h')
value = '#define EWAF_CORE_BUILD_HASH "' + hashlib.sha256(Path('target/swift/libewaf_ffi.a').read_bytes()).hexdigest() + '"\n'
if not header.exists() or header.read_text() != value:
    header.write_text(value)
PYCODE
