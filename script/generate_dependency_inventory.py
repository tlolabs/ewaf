#!/usr/bin/env python3
"""Generate a lockfile-based inventory without treating metadata as legal advice."""

import argparse
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / "dependency-inventory.json"


def inventory():
    metadata = json.loads(subprocess.check_output(
        ["cargo", "metadata", "--locked", "--offline", "--format-version", "1"],
        cwd=ROOT,
    ))
    rust = [
        {
            "name": package["name"],
            "version": package["version"],
            "license_expression": package.get("license"),
            "source": package["source"],
        }
        for package in metadata["packages"] if package["source"]
    ]
    rust.sort(key=lambda item: (item["name"], item["version"]))

    lock = json.loads((ROOT / "platform/windows/EWAF/packages.lock.json").read_text())
    windows = []
    for target, packages in lock["dependencies"].items():
        for name, data in packages.items():
            windows.append({
                "target": target,
                "name": name,
                "version": data["resolved"],
                "kind": data["type"],
            })
    windows.sort(key=lambda item: (item["target"], item["name"]))

    return {
        "schema_version": 1,
        "sources": ["Cargo.lock", "platform/windows/EWAF/packages.lock.json"],
        "rust_registry_packages": rust,
        "windows_nuget_packages": windows,
        "native_inputs": [
            {"platform": "macOS", "names": ["SwiftUI", "Foundation"], "source": "Apple SDK"},
            {"platform": "Windows", "names": [".NET 8"], "source": "Microsoft runtime"},
            {"platform": "Linux", "names": ["GTK 4", "libadwaita", "GLib", "json-glib"], "source": "distribution dynamic libraries"},
        ],
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = json.dumps(inventory(), indent=2, ensure_ascii=False) + "\n"
    if args.check:
        if not OUTPUT.exists() or OUTPUT.read_text() != expected:
            raise SystemExit("dependency-inventory.json is stale; regenerate it")
        print("Dependency inventory is current")
    else:
        OUTPUT.write_text(expected)
        print("Wrote dependency-inventory.json")


if __name__ == "__main__":
    main()
