#!/usr/bin/env python3
"""Match a Windows ZIP's files to exact cached NuGet package bytes.

This is evidence collection, not an automatic legal conclusion. Unmatched files
remain explicitly unresolved. Run on both architecture packages after publish.
"""

import argparse
import hashlib
import json
import zipfile
from collections import defaultdict
from pathlib import Path


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def build_index(root):
    by_name_size = defaultdict(list)
    for package in root.iterdir():
        if not package.is_dir():
            continue
        for version in package.iterdir():
            if not version.is_dir():
                continue
            for path in version.rglob("*"):
                if path.is_file() and path.suffix.lower() not in (".nupkg", ".msix"):
                    by_name_size[(path.name.lower(), path.stat().st_size)].append(path)
    return by_name_size


def assessment(name, matches):
    """Give every archive file a license bucket without overstating attribution."""
    basename = Path(name).name.lower()
    if name.startswith("licenses/"):
        return {"component": "vendor notice (see path)", "license_basis": "embedded notice",
                "attribution": "archive path"}
    packages = {match["package"] for match in matches}
    if any(package.startswith(("avalonia", "skiasharp", "harfbuzzsharp", "microcom", "tmds")) for package in packages):
        return {"component": ", ".join(sorted(packages)),
                "license_basis": "See locked licenses/avalonia/packages.json and full upstream notices; Avalonia includes Ms-PL components",
                "attribution": "exact NuGet SHA-256 match"}
    if any(package.startswith("microsoft.netcore.app.") for package in packages):
        return {"component": "Microsoft .NET 10 runtime/host",
                "license_basis": "MIT and .NET ThirdPartyNotices",
                "attribution": "exact NuGet SHA-256 match"}
    if any(package.startswith("microsoft.windows.sdk.buildtools") for package in packages):
        return {"component": "Windows SDK build tool / notice",
                "license_basis": "Windows SDK terms", "attribution": "exact NuGet SHA-256 match"}
    if basename in {"ewaf.exe", "ewaf.dll", "ewaf.pdb", "ewaf_ffi.dll",
                    "install.ps1", "uninstall.ps1", "ewaf-update.exe", "ewaf-installer.exe"}:
        return {"component": "EWAF-owned output", "license_basis": "GPL-3.0-or-later original code",
                "attribution": "project output name; verify generated resources"}
    if basename in {"license", "third_party_notices.md", "dependencies.md",
                    "ewaf.deps.json", "ewaf.runtimeconfig.json", "install-manifest.json"}:
        return {"component": "EWAF package metadata or notice", "license_basis": "see file",
                "attribution": "project output name"}
    if name.lower().startswith("assets/") or basename.endswith(".ico"):
        return {"component": "EWAF artwork", "license_basis": "GPL-3.0-or-later; assets/icon/README.md",
                "attribution": "project asset path"}
    if basename.endswith((".dll", ".exe")):
        return {"component": "unresolved executable",
                "license_basis": "manual byte/provenance review required; no inferred license",
                "attribution": "filename inference only; verify against release runtime pack"}
    return {"component": "unresolved", "license_basis": "manual review required",
            "attribution": "no exact NuGet SHA-256 match"}


def analyze(archive, cache, index):
    records = []
    with zipfile.ZipFile(archive) as zipped:
        for info in zipped.infolist():
            if info.is_dir():
                continue
            data_hash = sha256(zipped.read(info))
            exact = []
            for path in index.get((Path(info.filename).name.lower(), info.file_size), []):
                if sha256(path.read_bytes()) == data_hash:
                    relative = path.relative_to(cache)
                    exact.append({"package": relative.parts[0],
                                  "version": relative.parts[1],
                                  "source_file": str(relative)})
            records.append({"file": info.filename, "sha256": data_hash,
                            "size": info.file_size, "exact_nuget_matches": exact,
                            "assessment": assessment(info.filename, exact)})
    return {"sample_archive": archive.name, "archive_sha256": sha256(archive.read_bytes()),
            "scope": "Historical published package sample; regenerate from the next release build.",
            "files": records}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archives", nargs="+", type=Path)
    parser.add_argument("--cache", type=Path, default=Path.home() / ".nuget/packages")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    index = build_index(args.cache)
    result = {"schema_version": 2,
              "method": "SHA-256 match to extracted official NuGet cache by filename and size",
              "archives": [analyze(path, args.cache, index) for path in args.archives]}
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    for archive in result["archives"]:
        files = archive["files"]
        print(archive["sample_archive"], len(files), "files,",
              sum(bool(item["exact_nuget_matches"]) for item in files), "exact NuGet matches")


if __name__ == "__main__":
    main()
