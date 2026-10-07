#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
case "$(uname -m)" in x86_64) arch=x64;; aarch64) arch=arm64;; *) exit 1;; esac
./script/build_qt.sh "linux-$arch"
rm -rf build/linux
mkdir -p build/linux
cp -R "build/qt/linux-$arch/stage/." build/linux/
mv build/linux/EWAF build/linux/ewaf
