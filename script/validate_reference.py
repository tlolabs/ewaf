#!/usr/bin/env python3
"""The internal build is a separate artifact family, never a production receipt."""
import plistlib, sys, zipfile
from pathlib import Path
from version import VERSION
p = Path(sys.argv[1])
assert p.name == f'internal-avalonia-reference-{VERSION}-osx-arm64.zip'
with zipfile.ZipFile(p) as z:
    prefix = 'EWAF Avalonia Internal.app/Contents/'
    info = plistlib.loads(z.read(prefix + 'Info.plist'))
    assert info['CFBundleIdentifier'] == 'com.tlolabs.ewaf.avalonia-internal'
    assert info['EWAFDistribution'] == 'internal-reference'
    assert info['CFBundleShortVersionString'] == VERSION
    assert not any(k.startswith('SU') for k in info)
    assert not any('ewaf-update' in n or 'Sparkle' in n or 'installer' in n for n in z.namelist())
    binary = z.read(prefix + 'MacOS/EWAF')
    assert binary[:4] == b'\xcf\xfa\xed\xfe' and int.from_bytes(binary[4:8], 'little') == 0x0100000c
print('Internal ARM64 identity, architecture and absent production updater verified')
