#!/usr/bin/env python3
"""Collect the license texts distributed with the exact locked Rust dependencies."""
import json, subprocess
from pathlib import Path
root = Path(__file__).resolve().parent.parent
metadata = json.loads(subprocess.check_output(['cargo','metadata','--locked','--format-version','1'],cwd=root))
sections = ['# Rust third-party notices\n\nGenerated from Cargo.lock by script/generate_notices.py. Native platform runtime libraries also retain the notices included by their vendors/distribution packages.\n']
for package in sorted(metadata['packages'],key=lambda p:p['name']):
    if package['source'] is None: continue
    directory=Path(package['manifest_path']).parent
    files=sorted(p for p in directory.iterdir() if p.is_file() and p.name.upper().startswith(('LICENSE','COPYING','NOTICE')))
    sections.append('\n## '+package['name']+' '+package['version']+'\n\nLicense expression: '+(package['license'] or 'See upstream license')+'\n')
    for path in files:
        sections.append('\n### '+path.name+'\n\n```text\n'+path.read_text(errors='replace').rstrip()+'\n```\n')
(root/'THIRD_PARTY_NOTICES.md').write_text(''.join(sections))
