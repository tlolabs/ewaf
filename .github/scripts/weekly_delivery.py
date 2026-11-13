#!/usr/bin/env python3
"""Weekly package delivery gate: wait for Mac release and record publication outcomes."""
import argparse
import base64
import json
import os
import re
import sys
from urllib.parse import quote
import weekly_release_smoke as legacy


def mac_ready(release, tag):
    return (release.get('tag_name') == tag and release.get('draft') is False
            and any(isinstance(a, dict) and a.get('state') == 'uploaded'
                    and a.get('size', 0) > 0
                    and re.search(r'macos.*\.(zip|dmg|pkg)$', a.get('name', ''), re.I)
                    for a in release.get('assets', [])))


def ready_target(api, release, item):
    if api.repo.lower().endswith('/fillr') and item['target'].startswith('windows'):
        # Preserve the existing explicit unsigned-download channel restriction.
        if not release.get('prerelease') or release.get('name') != f"FILLR {item['tag'][1:]} unsigned downloads":
            return False
        source = api.request(f"{api.root}/contents/script/release_windows.ps1?ref={item['sha']}")
        if source.get('encoding') != 'base64':
            raise legacy.GateError('Unexpected packaging script encoding')
        content = base64.b64decode(source['content'], validate=False).decode('utf-8')
        return bool(re.search(r'\[switch\]\s*\$SmokeOnly', content))
    return True


def select(api, pattern, targets, baseline):
    gate = legacy.Gate(api)
    legacy.validate_baseline(baseline, api.repo)
    releases = {}
    for release in api.pages(f'{api.root}/releases'):
        if release.get('draft') is not False:
            continue
        tag = release.get('tag_name')
        if tag in releases:
            raise legacy.GateError('Ambiguous release tag')
        releases[tag] = release
    matrix, _ = gate.select(pattern, targets, baseline=baseline)
    selected = []
    deferred = 0
    for item in matrix['include']:
        release = releases.get(item['tag'], {})
        if not mac_ready(release, item['tag']) or not ready_target(api, release, item):
            deferred += 1
            print(f"Waiting for release prerequisites: {item['tag']} ({item['target']})", file=sys.stderr)
            continue
        selected.append(item)
    return {'include': selected}, {'new': len(selected), 'deferred': deferred}


def finalize(api, matrix, run, attempt, url):
    jobs = list(api.pages(f'{api.root}/actions/runs/{run}/attempts/{attempt}/jobs', key='jobs'))
    gate = legacy.Gate(api)
    failures = 0
    for item in legacy.validate_matrix(matrix):
        smoke = [j for j in jobs if j.get('name') == f"Smoke {item['tag']} ({item['target']})"]
        if not smoke or (len(smoke) == 1 and legacy.job_proven_unstarted(smoke[0])):
            # Keep original pending provenance so the established gate can
            # recover provably unexecuted work, not retry an attempted build.
            failures += 1
            continue
        name = f"Publish {item['tag']} ({item['target']})"
        matches = [j for j in jobs if j.get('name') == name]
        success = (len(smoke) == 1 and smoke[0].get('status') == 'completed'
                   and smoke[0].get('conclusion') == 'success' and len(matches) == 1 and matches[0].get('status') == 'completed' and matches[0].get('conclusion') == 'success')
        gate.mark(item, 'success' if success else 'failure',
                  'Verified native package published' if success else 'Delivery incomplete; inspect before any explicit retry', url)
        failures += not success
    return failures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['select', 'finalize'])
    parser.add_argument('--pattern')
    parser.add_argument('--targets')
    parser.add_argument('--baseline-file')
    parser.add_argument('--claim', action='store_true')
    args = parser.parse_args()
    api = legacy.GitHub(os.environ['GITHUB_REPOSITORY'])
    if args.command == 'select':
        matrix, summary = select(api, args.pattern, legacy.parse_targets(args.targets),
                                 legacy.load_baseline(args.baseline_file, api.repo))
        if args.claim:
            if os.environ.get('GITHUB_EVENT_NAME') != 'schedule' or os.environ.get('GITHUB_RUN_ATTEMPT') != '1':
                raise legacy.GateError('Claims require a first scheduled attempt')
            _, _, url = legacy.current_run(api)
            for item in matrix['include']:
                legacy.Gate(api).mark(item, 'pending', 'Claimed for native package delivery', url)
        legacy.emit_outputs(matrix, summary)
        return 0
    run, attempt, url = legacy.current_run(api)
    if os.environ.get('GITHUB_EVENT_NAME') != 'schedule' or attempt != 1:
        raise legacy.GateError('Finalization requires a first scheduled attempt')
    return int(bool(finalize(api, json.loads(os.environ['SMOKE_MATRIX']), run, attempt, url)))


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (legacy.GateError, ValueError, KeyError) as error:
        print(f'Delivery gate failed: {error}', file=sys.stderr)
        sys.exit(1)
