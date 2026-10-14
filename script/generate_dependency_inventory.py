#!/usr/bin/env python3
"""Generate a lockfile-based inventory without treating metadata as legal advice."""

import argparse
import json
import subprocess
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "dependency-inventory.json"


def inventory(check=False):
    # Cargo metadata --offline can omit packages for other targets on a clean
    # runner. The lockfile is the stable cross-platform dependency list.
    previous = json.loads(OUTPUT.read_text()) if OUTPUT.exists() else {}
    licenses = {
        (package["name"], package["version"], package["source"]): package["license_expression"]
        for package in previous.get("rust_registry_packages", [])
    }
    if not check:
        metadata = json.loads(subprocess.check_output(
            ["cargo", "metadata", "--locked", "--offline", "--format-version", "1"],
            cwd=ROOT,
        ))
        licenses.update({
            (package["name"], package["version"], package["source"]): package.get("license")
            for package in metadata["packages"] if package["source"]
        })
    cargo_lock = tomllib.loads((ROOT / "Cargo.lock").read_text())
    rust = []
    for package in cargo_lock["package"]:
        source = package.get("source")
        if not source:
            continue
        key = (package["name"], package["version"], source)
        if key not in licenses:
            raise SystemExit(f"Missing license metadata for {package['name']} {package['version']}; regenerate with its source available")
        rust.append({
            "name": package["name"],
            "version": package["version"],
            "license_expression": licenses[key],
            "source": source,
        })
    rust.sort(key=lambda item: (item["name"], item["version"]))

    qt_components = [
        {
            "name": "Qt6",
            "components": ["Core", "Gui", "Widgets", "Concurrent"],
            "minimum_version": "6.4.2",
            "license": "LGPL-3.0-only OR GPL-3.0-only",
            "source": "CMakeLists.txt",
        }
    ]

    packaged_native_components = [
        {
            "platform": "Windows (x64, ARM64)",
            "delivery": "Bundled with MSI package",
            "components": [
                {
                    "name": "Qt6 (Core, Gui, Widgets, Concurrent)",
                    "version": "6.8.0",
                    "license": "LGPL-3.0-only OR GPL-3.0-only",
                    "bundled": True,
                    "replacement_obligation": "Dynamic linking; user may replace DLLs in application directory (LGPLv3 §4)",
                },
                {
                    "name": "Qt6 Plugins (platforms/qwindows, styles/qwindowsvistastyle)",
                    "version": "6.8.0",
                    "license": "LGPL-3.0-only OR GPL-3.0-only",
                    "bundled": True,
                    "replacement_obligation": "Dynamic linking; user may replace plugin DLLs (LGPLv3 §4)",
                },
                {
                    "name": "Microsoft Visual C++ Redistributable Runtime (msvcp140, vcruntime140, vcruntime140_1)",
                    "version": "14.40+ (MSVC 2022)",
                    "license": "Microsoft Visual Studio 2022 Redistributable Terms",
                    "bundled": True,
                    "replacement_obligation": "None (standard runtime redistributable)",
                },
            ],
        },
        {
            "platform": "Linux (AppImage)",
            "delivery": "Bundled within squashfs payload of AppImage",
            "components": [
                {
                    "name": "Qt6 (Core, Gui, Widgets, Concurrent, DBus, XcbQpa)",
                    "version": "6.4.2+ (Ubuntu 24.04 LTS package build)",
                    "license": "LGPL-3.0-only OR GPL-3.0-only",
                    "bundled": True,
                    "replacement_obligation": "Extract via --appimage-extract, replace shared objects in usr/lib, and rebuild with appimagetool (LGPLv3 §4)",
                },
                {
                    "name": "AppImage type 2 runtime",
                    "version": "continuous / pinned digest in updates/build-tools.json",
                    "license": "MIT",
                    "bundled": True,
                    "replacement_obligation": "User may replace runtime binary via appimagetool",
                },
                {
                    "name": "System dependencies (X11, fontconfig, OpenGL/Mesa, dconf)",
                    "version": "Distribution-managed system packages",
                    "license": "Various system licenses",
                    "bundled": False,
                    "replacement_obligation": "Managed by operating system package manager",
                },
            ],
        },
        {
            "platform": "Linux (.deb)",
            "delivery": "Distribution system package dependencies (apt)",
            "components": [
                {
                    "name": "libqt6core6t64, libqt6gui6, libqt6widgets6, libqt6concurrent6",
                    "version": "6.4.2+ (Ubuntu 24.04 LTS)",
                    "license": "LGPL-3.0-only OR GPL-3.0-only",
                    "bundled": False,
                    "replacement_obligation": "Managed and updated directly by system package manager",
                },
            ],
        },
        {
            "platform": "macOS (Apple Silicon, Intel)",
            "delivery": "Native production application bundle",
            "components": [
                {
                    "name": "Sparkle",
                    "version": "2.9.6",
                    "license": "MIT",
                    "bundled": True,
                    "replacement_obligation": "Framework bundled in EWAF.app/Contents/Frameworks",
                },
                {
                    "name": "SwiftUI / Foundation / AppKit",
                    "version": "macOS 14+ SDK",
                    "license": "Apple SDK Terms",
                    "bundled": False,
                    "replacement_obligation": "Operating system frameworks",
                },
            ],
        },
    ]

    return {
        "schema_version": 3,
        "sources": ["Cargo.lock", "CMakeLists.txt", "updates/build-tools.json"],
        "rust_registry_packages": rust,
        "qt_components": qt_components,
        "updater_build_inputs": {"sparkle_version": "2.9.6", "sparkle_lock": "Package.resolved", "wix_version": "4.0.6", "linux_tool_pins": "updates/build-tools.json"},
        "native_inputs": [
            {"platform": "macOS", "names": ["SwiftUI", "Foundation"], "source": "Apple SDK"},
            {"platform": "Windows/Linux/internal macOS", "names": ["Qt 6 (Core, Gui, Widgets, Concurrent)"], "source": "Qt 6.4.2+ (LGPL-3.0-only OR GPL-3.0-only)"},
            {"platform": "Linux", "names": ["X11", "fontconfig", "OpenGL", "dconf (preference import)"], "source": "distribution system libraries/tools"},
        ],
        "packaged_native_components": packaged_native_components,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = json.dumps(inventory(args.check), indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != expected:
            raise SystemExit("dependency-inventory.json is stale; regenerate it")
        print("Dependency inventory is current")
    else:
        OUTPUT.write_text(expected)
        print("Wrote dependency-inventory.json")


if __name__ == "__main__":
    main()
