#!/usr/bin/env python3
"""Native production verification, followed by a hash-bound CI receipt (not a trust root)."""
import argparse, hashlib, json, os, plistlib, re, subprocess, tempfile, zipfile
from pathlib import Path
from version import ROOT, VERSION
p = argparse.ArgumentParser()
p.add_argument('artifact', type=Path)
p.add_argument('--platform', choices=['macos', 'windows', 'linux'], required=True)
p.add_argument('--arch', choices=['x64', 'arm64'], required=True)
a = p.parse_args()
trust = json.loads((ROOT / 'updates/trust.json').read_text())
artifact = a.artifact.resolve()
digest = hashlib.sha256(artifact.read_bytes()).hexdigest()
signer = trust[{'macos': 'macos_team_id', 'windows': 'windows_publisher', 'linux': 'linux_gpg_fingerprint'}[a.platform]]
if not signer:
    raise SystemExit('Missing pinned native signer')

def run(*args):
    return subprocess.check_output(args, stderr=subprocess.STDOUT, text=True)
if a.platform == 'macos':
    with tempfile.TemporaryDirectory() as temp:
        run('ditto', '-x', '-k', str(artifact), temp)
        app = Path(temp) / 'EWAF.app'
        info = plistlib.loads((app / 'Contents/Info.plist').read_bytes())
        if not (info['CFBundleIdentifier'] == 'com.tlolabs.ewaf' and info['CFBundleShortVersionString'] == VERSION and (info['CFBundleVersion'] == VERSION)):
            raise SystemExit('Release security verification failed')
        if not info['SUPublicEDKey'] in trust['keys'].values():
            raise SystemExit('Release security verification failed')
        if not (info['SURequireSignedFeed'] and info['SUVerifyUpdateBeforeExtraction'] and (info['SUSignedFeedFailureExpirationInterval'] == 0)):
            raise SystemExit('Release security verification failed')
        if not info['SUFeedURL'] == f'https://github.com/tlolabs/ewaf/releases/latest/download/appcast-{a.arch}.xml':
            raise SystemExit('Release security verification failed')
        signature = run('codesign', '-d', '--verbose=4', str(app))
        if not ('Authority=Developer ID Application:' in signature and f'TeamIdentifier={signer}' in signature and ('runtime' in signature)):
            raise SystemExit('Release security verification failed')
        run('codesign', '--verify', '--deep', '--strict', str(app))
        run('xcrun', 'stapler', 'validate', str(app))
        run('spctl', '--assess', '--type', 'execute', str(app))
        arches = run('lipo', '-archs', str(app / 'Contents/MacOS/EWAF')).strip()
        if not arches == {'arm64': 'arm64', 'x64': 'x86_64'}[a.arch]:
            raise SystemExit('Release security verification failed')
elif a.platform == 'windows':
    stage_arch = 'ARM64' if a.arch == 'arm64' else 'x64'
    helper = ROOT / 'dist' / f'windows-{stage_arch}' / 'updater-installer/ewaf-installer.exe'
    run(str(helper), '--verify-only', str(artifact), digest, signer, VERSION, a.arch)
else:
    data = artifact.read_bytes()
    if not (data[:4] == b'\x7fELF' and int.from_bytes(data[18:20], 'little') == {'x64': 62, 'arm64': 183}[a.arch]):
        raise SystemExit('Release security verification failed')
    status = run('gpg', '--batch', '--status-fd', '1', '--verify', str(artifact) + '.asc', str(artifact))
    fingerprints = re.findall('\\[GNUPG:\\] VALIDSIG (.*)', status)
    if not any((signer.upper() in line.split() for line in fingerprints)):
        raise SystemExit('Unexpected Linux signing key')
    env = dict(os.environ, APPIMAGE_EXTRACT_AND_RUN='1')
    if not 'BEGIN PGP SIGNATURE' in subprocess.check_output([str(artifact), '--appimage-signature'], env=env, text=True):
        raise SystemExit('Release security verification failed')
    if not f'EWAF {VERSION} ABI' in subprocess.check_output([str(artifact), '--core-smoke'], env=env, text=True):
        raise SystemExit('Release security verification failed')
    # The detached signature and pinned fingerprint above authorize executing
    # this image's extractor. Inspect the helper that will actually ship, never
    # an unrelated build-tree executable with a matching version.
    with tempfile.TemporaryDirectory() as temp:
        subprocess.run([str(artifact), '--appimage-extract'], cwd=temp,
                       stdout=subprocess.DEVNULL, check=True, timeout=120)
        extracted = Path(temp) / 'squashfs-root'
        helper = extracted / 'usr/bin/ewaf-update'
        if not helper.resolve().is_relative_to(extracted.resolve()):
            raise SystemExit('Packaged updater escapes the AppImage')
        with helper.open("rb") as file:
            header = file.read(20)
        if not (header[:4] == b'\x7fELF' and int.from_bytes(header[18:20], 'little') == {'x64': 62, 'arm64': 183}[a.arch]):
            raise SystemExit('Packaged updater architecture mismatch')
        helper_env = dict(os.environ, LD_LIBRARY_PATH=str(extracted / 'usr/lib'))
        embedded = subprocess.check_output([str(helper), 'version'], env=helper_env,
                                           text=True, timeout=30)
        if json.loads(embedded)['version'] != VERSION:
            raise SystemExit('Packaged updater version mismatch')
source = os.environ.get('GITHUB_SHA') or run('git', 'rev-parse', 'HEAD').strip()
receipt = dict(schema=1, application_id='com.tlolabs.ewaf', version=VERSION, source_commit=source, platform=a.platform, architecture=a.arch, sha256=digest, size=artifact.stat().st_size, signer=signer, minimum_os={'macos': '14.0', 'windows': '10.0.17763', 'linux': '2.39'}[a.platform], format={'macos': 'zip', 'windows': 'msi', 'linux': 'AppImage'}[a.platform])
artifact.with_suffix(artifact.suffix + '.verified.json').write_text(json.dumps(receipt, indent=2) + '\n')
print('Native signature, identity and version verified: ' + artifact.name)
