#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cargo build --release --locked -p ewaf-ffi
mkdir -p build/linux
cc -std=c17 -Wall -Wextra -Werror -Wno-deprecated-declarations platform/linux/src/main.c -Ibindings/include target/release/libewaf_ffi.a $(pkg-config --cflags --libs gtk4 libadwaita-1 json-glib-1.0) -lpthread -ldl -lm -o build/linux/ewaf
cp platform/linux/data/com.tlolabs.ewaf.gschema.xml build/linux/
glib-compile-schemas --strict build/linux
build/linux/ewaf --core-smoke
