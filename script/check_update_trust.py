#!/usr/bin/env python3
"""Production preflight: never treat missing enrollment as an unsigned-release fallback."""
import base64, json, os, re
from pathlib import Path
root = Path(__file__).resolve().parent.parent
trust = json.loads((root / 'updates/trust.json').read_text())
if not (trust['application_id'] == 'com.tlolabs.ewaf' and trust['repository'] == 'tlolabs/ewaf'):
    raise SystemExit('Release security verification failed')
if not trust['keys']:
    raise SystemExit('Owner must enroll an EWAF public update key before a stable release')
for key in trust['keys'].values():
    if not len(base64.b64decode(key, validate=True)) == 32:
        raise SystemExit('Release security verification failed')
if not re.fullmatch('[A-Z0-9]{10}', trust['macos_team_id']):
    raise SystemExit('Enroll Apple team identity')
if not trust['windows_publisher'].startswith('CN='):
    raise SystemExit('Enroll exact Azure certificate subject')
if not os.environ.get('WINDOWS_DISTRIBUTION_APPROVED') == 'true':
    raise SystemExit('Existing Windows distribution hold must be resolved by the owner')
print('Production public trust and existing distribution gate are configured')
