#!/usr/bin/env python3
"""Reject incomplete or mismatched artifacts before upload; write SHA256 sidecar."""
from pathlib import Path
import hashlib, plistlib, subprocess, sys, zipfile, tempfile
from version import VERSION
p=Path(sys.argv[1])
assert p.is_file() and p.stat().st_size > 1000
assert VERSION in p.name, 'Artifact filename must include the shared version'
if p.suffix == '.zip':
    with zipfile.ZipFile(p) as archive:
        assert archive.testzip() is None
        names=archive.namelist()
        assert all(not n.startswith('/') and '..' not in Path(n).parts for n in names)
        if 'macos' in p.name:
            info=plistlib.loads(archive.read('EWAF.app/Contents/Info.plist'))
            assert info['CFBundleShortVersionString']==VERSION
            assert info['CFBundleIdentifier']=='com.tlolabs.ewaf'
            assert info['CFBundleDisplayName']=='EWAF'
            data=archive.read('EWAF.app/Contents/MacOS/EWAF')
            if data[:4] in [b'\xca\xfe\xba\xbe',b'\xca\xfe\xba\xbf']:
                stride=20 if data[:4]==b'\xca\xfe\xba\xbe' else 32
                architectures={int.from_bytes(data[8+i*stride:12+i*stride],'big') for i in range(int.from_bytes(data[4:8],'big'))}
            else:
                assert data[:4]==b'\xcf\xfa\xed\xfe', 'Expected a 64-bit Mach-O executable'
                architectures={int.from_bytes(data[4:8],'little')}
            expected={0x01000007,0x0100000c} if 'universal' in p.name else {0x0100000c} if 'arm64' in p.name else {0x01000007}
            assert architectures==expected, 'Mach-O architecture mismatch'
            assert 'EWAF.app/Contents/Resources/THIRD_PARTY_NOTICES.md' in names
        else:
            for required in ['EWAF.exe','ewaf_ffi.dll','Install.ps1','Uninstall.ps1','THIRD_PARTY_NOTICES.md']:
                assert required in names, required
            expected=0xaa64 if 'ARM64' in p.name else 0x8664
            for name in ['EWAF.exe','ewaf_ffi.dll']:
                data=archive.read(name); assert data[:2]==b'MZ'
                offset=int.from_bytes(data[60:64],'little'); assert data[offset:offset+4]==b'PE\0\0'
                assert int.from_bytes(data[offset+4:offset+6],'little')==expected
elif p.suffix == '.deb':
    assert subprocess.check_output(['dpkg-deb','-f',str(p),'Version'],text=True).strip()==VERSION
    assert subprocess.check_output(['dpkg-deb','-f',str(p),'Architecture'],text=True).strip() in p.name
    listing=subprocess.check_output(['dpkg-deb','-c',str(p)],text=True)
    for required in ['./usr/bin/ewaf','com.tlolabs.ewaf.desktop','com.tlolabs.ewaf.gschema.xml']:
        assert required in listing
    with tempfile.TemporaryDirectory(prefix='ewaf-package-check-') as temporary:
        subprocess.run(['dpkg-deb','-x',str(p),temporary],check=True)
        data=(Path(temporary)/'usr/bin/ewaf').read_bytes()
        assert data[:5]==b'\x7fELF\x02', 'Expected a 64-bit ELF executable'
        machine=int.from_bytes(data[18:20],'little' if data[5]==1 else 'big')
        assert machine==(183 if 'arm64' in p.name else 62), 'ELF architecture mismatch'
else:
    raise ValueError('Unrecognized artifact format')
p.with_suffix(p.suffix+'.sha256').write_text(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n')
print('Validated '+str(p))
