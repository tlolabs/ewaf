#!/usr/bin/env python3
"""The internal build is a separate artifact family, never a production receipt."""
import plistlib, subprocess, sys, tempfile, zipfile
from pathlib import Path
from version import VERSION
p = Path(sys.argv[1])
assert p.name == f'internal-qt-reference-{VERSION}-osx-arm64.zip'
with zipfile.ZipFile(p) as z:
    prefix = 'EWAF Qt Internal.app/Contents/'
    info = plistlib.loads(z.read(prefix + 'Info.plist'))
    assert z.read(prefix + 'Resources/THIRD_PARTY_NOTICES.md') == Path('THIRD_PARTY_NOTICES.md').read_bytes()
    assert info['CFBundleIdentifier'] == 'com.tlolabs.ewaf.qt-internal'
    assert info['EWAFDistribution'] == 'internal-reference'
    assert info['CFBundleShortVersionString'] == VERSION
    assert not any(k.startswith('SU') for k in info)
    assert not any('ewaf-update' in n or 'Sparkle' in n or 'installer' in n for n in z.namelist())
    binary = z.read(prefix + 'MacOS/EWAF')
    assert binary[:4] == b'\xcf\xfa\xed\xfe' and int.from_bytes(binary[4:8], 'little') == 0x0100000c
    with tempfile.TemporaryDirectory() as staging:
        executable = Path(staging) / 'EWAF'
        executable.write_bytes(binary)
        linked = subprocess.check_output(['otool', '-L', str(executable)], text=True)
        assert '@executable_path/libewaf_ffi.dylib' in linked, 'Rust FFI must load from the internal bundle'
        assert '/target/' not in linked, 'Internal executable links a build-tree library'
print('Internal ARM64 identity, architecture and absent production updater verified')
