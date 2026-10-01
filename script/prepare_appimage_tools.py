#!/usr/bin/env python3
"""Fetch exact reviewed build inputs. An upstream continuous-tag move fails closed."""
import hashlib, json, os, platform, subprocess
from pathlib import Path
root=Path(__file__).resolve().parent.parent
pins=json.loads((root/'updates/build-tools.json').read_text())[platform.machine()]
cache=root/'build/appimage-tools';cache.mkdir(parents=True,exist_ok=True)
for name,(url,digest) in pins.items():
    path=cache/(name+'-'+digest)
    if not path.exists():
        subprocess.run(['curl','--fail','--location','--proto','=https','--proto-redir','=https','--retry','3',url,'-o',str(path)],check=True)
    if hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
        path.unlink();raise SystemExit('Packaging tool digest mismatch: '+name)
    path.chmod(0o755)
    if os.environ.get('GITHUB_ENV'):
        with open(os.environ['GITHUB_ENV'],'a') as output:
            label='APPIMAGE_RUNTIME' if name=='runtime' else name.upper()
            output.write(f'{label}={path}\n{label}_SHA256={digest}\n')
    print(name+': '+str(path))
