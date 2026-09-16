#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
./script/build_linux.sh
cc -std=c17 -Wall -Wextra -Werror -Wno-deprecated-declarations platform/linux/tests/widgets.c -Ibindings/include target/release/libewaf_ffi.a $(pkg-config --cflags --libs gtk4 libadwaita-1 json-glib-1.0) -lpthread -ldl -lm -o build/linux/widget-tests
GSETTINGS_SCHEMA_DIR="$PWD/build/linux" GSETTINGS_BACKEND=memory build/linux/widget-tests
