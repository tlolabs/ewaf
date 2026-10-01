#!/usr/bin/env python3
"""Embed reviewed public trust only. Production packaging fails closed."""
import base64, json, os, plistlib, sys
from pathlib import Path
root = Path(__file__).resolve().parent.parent
trust = json.loads((root/'updates/trust.json').read_text())
production = os.environ.get('EWAF_PRODUCTION') == '1'
keys = list(trust['keys'].values())
if production and (not keys or not trust['macos_team_id']):
    raise SystemExit('Production requires pinned update keys and Apple team ID in updates/trust.json')
path = Path(sys.argv[1])
with path.open('rb') as file:
    info = plistlib.load(file)
info.update(SUEnableAutomaticChecks=True, SUAutomaticallyUpdate=False,
            SUAllowsAutomaticUpdates=False, SUSendProfileInfo=False,
            SUEnableSystemProfiling=False, SUScheduledCheckInterval=86400,
            SUVerifyUpdateBeforeExtraction=True, SURequireSignedFeed=True,
            SUSignedFeedFailureExpirationInterval=0)
info.pop('SUPublicEDKey', None)
info.pop('SUFeedURL', None)
if keys and (production or os.environ.get("EWAF_UPDATE_CHANNEL") == "stable"):
    if len(base64.b64decode(keys[0], validate=True)) != 32:
        raise SystemExit('Invalid pinned Ed25519 key')
    info['SUPublicEDKey'] = keys[0]
    # Architecture-specific signed appcasts prevent cross-architecture replacement.
    arch = os.environ.get('EWAF_UPDATE_ARCH', '')
    if arch not in ('arm64', 'x64'):
        raise SystemExit('Update-enabled macOS packages must have one declared architecture')
    info['SUFeedURL'] = f'https://github.com/tlolabs/ewaf/releases/latest/download/appcast-{arch}.xml'
with path.open('wb') as file:
    plistlib.dump(info, file)
