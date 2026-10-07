#!/usr/bin/env python3
"""Assemble/sign one six-target stable release; verify every byte before draft upload."""
import argparse, base64, datetime, hashlib, json, os, shutil, subprocess, uuid, xml.etree.ElementTree as X
from pathlib import Path
from version import ROOT, VERSION
p = argparse.ArgumentParser()
p.add_argument('directory', type=Path)
a = p.parse_args()
directory = a.directory.resolve()
# Reject unexpected/internal inputs before any signing or upload.
for candidate in directory.iterdir():
    if 'internal' in candidate.name.lower() or 'avalonia' in candidate.name.lower() or 'qt-internal' in candidate.name.lower():
        raise SystemExit('Internal reference artifacts must never enter production release assets')
trust = json.loads((ROOT / 'updates/trust.json').read_text())
if not os.environ.get('GITHUB_REF_NAME') == 'v' + VERSION:
    raise SystemExit('Stable tag must match Cargo version')
if not (trust['keys'] and all((trust[k] for k in ('macos_team_id', 'windows_publisher')))):
    raise SystemExit('Enroll production public trust first')
seed = os.environ.get('TLO_UPDATE_SIGNING_SEED')
if not (seed and len(base64.b64decode(seed, validate=True)) == 32):
    raise SystemExit('Missing protected Ed25519 seed')
sign = ROOT / 'build/sparkle/2.9.6/bin/sign_update'
subprocess.run([str(ROOT / 'script/prepare_sparkle.sh')], check=True, stdout=subprocess.DEVNULL)
now = datetime.datetime.now(datetime.timezone.utc)
manifest = dict(schema=1, application_id=trust['application_id'], repository=trust['repository'], version=VERSION, tag='v' + VERSION, channel='stable', draft=False, published_at=now.isoformat(), expires_at=int(now.timestamp()) + 90 * 86400, release_notes_url=f"https://github.com/{trust['repository']}/releases/tag/v{VERSION}", restart_required=True, migration=None, artifacts=[])
for platform, extension in [('macos', 'zip'), ('windows', 'msi'), ('linux', 'AppImage')]:
    for arch in ['x64', 'arm64']:
        filename = f'ewaf-{VERSION}-{platform}-{arch}.{extension}'
        path = directory / filename
        receipt = json.loads(path.with_suffix(path.suffix + '.verified.json').read_text())
        if not (receipt['source_commit'] == os.environ['GITHUB_SHA'] and receipt['version'] == VERSION):
            raise SystemExit('Release security verification failed')
        if not (receipt['application_id'] == trust['application_id'] and receipt['platform'] == platform and (receipt['architecture'] == arch)):
            raise SystemExit('Release security verification failed')
        if not (receipt['sha256'] == hashlib.sha256(path.read_bytes()).hexdigest() and receipt['size'] == path.stat().st_size):
            raise SystemExit('Release security verification failed')
        artifact = {k: receipt[k] for k in ('platform', 'architecture', 'minimum_os', 'format', 'size', 'sha256', 'signer')}
        artifact.update(filename=filename, url=f"https://github.com/{trust['repository']}/releases/download/v{VERSION}/{filename}", sparkle_signature=None)
        if platform == 'macos':
            signature = subprocess.check_output([str(sign), '--ed-key-file', '-', '-p', str(path)], input=seed, text=True).strip()
            subprocess.run([str(sign), '--ed-key-file', '-', '--verify', str(path), signature], input=seed, text=True, check=True)
            artifact['sparkle_signature'] = signature
            ns = 'http://www.andymatuschak.org/xml-namespaces/sparkle'
            X.register_namespace('sparkle', ns)
            rss = X.Element('rss', version='2.0')
            channel = X.SubElement(rss, 'channel')
            X.SubElement(channel, 'title').text = 'EWAF stable updates'
            item = X.SubElement(channel, 'item')
            X.SubElement(item, 'title').text = 'EWAF ' + VERSION
            X.SubElement(item, '{' + ns + '}version').text = VERSION
            X.SubElement(item, '{' + ns + '}shortVersionString').text = VERSION
            X.SubElement(item, '{' + ns + '}minimumSystemVersion').text = artifact['minimum_os']
            X.SubElement(item, 'description').text = 'Release notes: ' + manifest['release_notes_url']
            X.SubElement(item, 'enclosure', {'url': artifact['url'], 'length': str(artifact['size']), 'type': 'application/octet-stream', '{' + ns + '}edSignature': signature})
            appcast = directory / f'appcast-{arch}.xml'
            X.ElementTree(rss).write(appcast, encoding='utf-8', xml_declaration=True)
            subprocess.run([str(sign), '--ed-key-file', '-', str(appcast)], input=seed, text=True, check=True)
            subprocess.run([str(sign), '--ed-key-file', '-', '--verify', str(appcast)], input=seed, text=True, check=True)
        manifest['artifacts'].append(artifact)
payload = directory / 'update-payload.json'
payload.write_text(json.dumps(manifest, separators=(',', ':')))
subprocess.run(['cargo', 'run', '--locked', '-p', 'tlo-updater', '--bin', 'tlo-release', '--', 'sign', str(ROOT / 'updates/trust.json'), str(payload), str(directory / 'update-manifest.json')], check=True)
if json.loads((directory / 'update-manifest.json').read_text())['key_id'] != next(iter(trust['keys'])):
    raise SystemExit('The release seed must match the active Sparkle key (first enrolled key)')
subprocess.run(['cargo', 'run', '--locked', '-p', 'tlo-updater', '--bin', 'tlo-release', '--', 'verify', str(ROOT / 'updates/trust.json'), str(directory / 'update-manifest.json')], check=True)
# Publish the exact reviewed notices/inventory and a source-dependency SBOM for this release family.
for name in ('dependency-inventory.json', 'THIRD_PARTY_NOTICES.md', 'LICENSE'):
    shutil.copyfile(ROOT / name, directory / name)
inventory = json.loads((ROOT / 'dependency-inventory.json').read_text())
components = []
for package in inventory['rust_registry_packages']:
    components.append({'type': 'library', 'name': package['name'], 'version': package['version'],
                       'purl': f"pkg:cargo/{package['name']}@{package['version']}"})
for package in inventory.get('qt_components', []):
    components.append({'type': 'library', 'name': package['name'], 'version': package.get('minimum_version', '6.5'),
                       'purl': f"pkg:generic/{package['name']}@{package.get('minimum_version', '6.5')}"})
components.append({'type': 'library', 'name': 'Sparkle', 'version': '2.9.6', 'purl': 'pkg:github/sparkle-project/Sparkle@2.9.6'})
bom = {'bomFormat': 'CycloneDX', 'specVersion': '1.5', 'serialNumber': 'urn:uuid:' + str(uuid.uuid4()), 'version': 1,
       'metadata': {'timestamp': now.isoformat(), 'component': {'type': 'application', 'name': 'EWAF', 'version': VERSION},
                    'properties': [{'name': 'tlolabs:scope', 'value': 'Locked source dependencies across all platforms; native system libraries and binary file inventory require separate platform audit'}]},
       'components': components}
(directory / 'source-dependencies.cdx.json').write_text(json.dumps(bom, indent=2) + '\n')
for stale in directory.glob('EWAF-*.sha256'):
    stale.unlink()
(directory / 'SHA256SUMS').write_text(''.join((f'{hashlib.sha256(f.read_bytes()).hexdigest()}  {f.name}\n' for f in sorted(directory.iterdir()) if f.is_file() and f.name != 'SHA256SUMS')))
