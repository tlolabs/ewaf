#!/usr/bin/env python3
"""Copy exact resolved package/runtime notices, preserving vendor attribution."""
import json, os, shutil, subprocess, sys
from pathlib import Path
root = Path(__file__).resolve().parent.parent
stage = Path(sys.argv[1]) / 'licenses'
cache = Path(subprocess.check_output(['dotnet', 'nuget', 'locals', 'global-packages', '--list'], text=True).strip().split(': ', 1)[1])
lock = json.loads((root/'platform/avalonia/EWAF/packages.lock.json').read_text())
packages = {(n.lower(), d['resolved']) for deps in lock['dependencies'].values() for n,d in deps.items()}
for directory in cache.glob('microsoft.netcore.app.runtime.*/*'):
    if directory.name == '10.0.12': packages.add((directory.parent.name, directory.name))
for name, version in sorted(packages):
    directory = cache/name/version
    for path in directory.rglob('*'):
        if path.is_file() and path.name.lower().startswith(('license','notice','thirdparty','third-party','copying')):
            output = stage/(name+'-'+version)/path.relative_to(directory)
            output.parent.mkdir(parents=True, exist_ok=True); shutil.copyfile(path,output)
# Full audited source notices for packages whose nuspec declares MIT without a text file.
if (root/'licenses/avalonia').exists(): shutil.copytree(root/'licenses/avalonia',stage/'avalonia-source',dirs_exist_ok=True)
