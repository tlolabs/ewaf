#!/usr/bin/env python3
"""Publish immutable, independently smoked weekly packages; never create releases."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def gh(*args):
    return subprocess.check_output(['gh', *args], text=True)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def package_name(tag, target):
    match = re.fullmatch(r'v(\d+\.\d+\.\d+)(?:-(?:beta|rc)\.\d+)?', tag)
    if not match:
        raise ValueError('Invalid version tag')
    suffix = {'linux-x64': 'linux-amd64.deb', 'linux-arm64': 'linux-arm64.deb',
              'windows-x64': 'windows-x64.zip', 'windows-arm64': 'windows-ARM64.zip'}[target]
    return f'EWAF-{match[1]}-{suffix}'


def validate_download(directory, name):
    expected_files = {name, name + '.sha256'}
    if {p.name for p in directory.iterdir()} != expected_files:
        raise ValueError('Unexpected package inventory')
    package, checksum = directory / name, directory / (name + '.sha256')
    if not all(p.is_file() and not p.is_symlink() for p in (package, checksum)):
        raise ValueError('Package must contain regular files')
    if checksum.read_text().strip() != f'{digest(package)}  {name}':
        raise ValueError('Checksum or basename mismatch')
    return [checksum, package]


def main():
    env = os.environ
    repo, tag, sha, target = [env[k] for k in
        ('GITHUB_REPOSITORY', 'RELEASE_TAG', 'RELEASE_SHA', 'RELEASE_TARGET')]
    run = env['GITHUB_RUN_ID']
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repo):
        raise ValueError('Invalid repository')
    if not re.fullmatch(r'[a-f0-9]{40}', sha) or not run.isdigit():
        raise ValueError('Invalid commit or run')
    if env.get('GITHUB_RUN_ATTEMPT') != '1' or env.get('GITHUB_EVENT_NAME') != 'schedule':
        raise ValueError('Only first scheduled attempts may publish')
    name = package_name(tag, target)
    artifacts = json.loads(gh('api', '--paginate', '--slurp',
                            f'repos/{repo}/actions/runs/{run}/artifacts'))
    matched = [a for page in artifacts for a in page['artifacts']
               if a['name'] == f'verified-{tag}-{target}' and not a['expired']]
    if not matched:
        raise ValueError('No verified package from this run; publication did not complete')
    if len(matched) != 1:
        raise ValueError('Ambiguous verified artifact')
    if json.loads(gh('api', f'repos/{repo}/commits/{tag}'))['sha'] != sha:
        raise ValueError('Remote tag moved; refusing publication')
    release = json.loads(gh('api', f'repos/{repo}/releases/tags/{tag}'))
    if release['draft'] or release['tag_name'] != tag:
        raise ValueError('A matching already-published release is required')
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        package_dir = root / 'package'
        package_dir.mkdir()
        gh('run', 'download', run, '--repo', repo, '--name', matched[0]['name'],
           '--dir', str(package_dir))
        files = validate_download(package_dir, name)
        evidence = package_dir / (name + '.verification.json')
        evidence.write_text(json.dumps({
            'source_tag': tag, 'source_commit': sha, 'target': target,
            'package': name, 'sha256': digest(package_dir / name),
            'run_url': f'https://github.com/{repo}/actions/runs/{run}',
            'verification': 'Native final-package integrity, dependency and core/window startup smoke passed',
            'limits': 'Basic automated smoke only; not full regression, interactive hardware acceptance, or authenticated update qualification',
            'distribution': 'Direct download; no update feed or Latest change',
        }, indent=2) + '\n')
        files.insert(1, evidence)
        existing = {asset['name'] for asset in release['assets']}
        # Verify every existing name before uploading anything; never clobber.
        for file in files:
            if file.name in existing:
                check_dir = root / file.name
                check_dir.mkdir()
                gh('release', 'download', tag, '--repo', repo, '--pattern', file.name,
                   '--dir', str(check_dir))
                if digest(file) != digest(check_dir / file.name):
                    raise ValueError('Existing release asset differs: ' + file.name)
        for file in files:  # checksum before package
            if file.name not in existing:
                gh('release', 'upload', tag, str(file), '--repo', repo)
        # Confirm server-side digests, including files that already existed.
        verified = json.loads(gh('api', f'repos/{repo}/releases/tags/{tag}'))
        remote = {a['name']: a for a in verified['assets']}
        for file in files:
            if remote[file.name].get('digest') != 'sha256:' + digest(file):
                raise ValueError('Published asset digest did not verify: ' + file.name)
        # All target publishers append the same footer, so concurrent targets
        # cannot lose distinct per-target prose. Exact evidence is in assets.
        footer = ("\n\n### Weekly platform qualification\n"
                  "The initial publication notes above describe the original downloads. "
                  "Additional platform assets may now be present. Each weekly package has "
                  "a matching `.verification.json` documenting its exact source commit, "
                  "native basic package/startup checks and limitations. These direct "
                  "downloads are not authenticated automatic updates.\n")
        current = json.loads(gh('api', f'repos/{repo}/releases/tags/{tag}'))
        body = current.get('body') or ''
        if '### Weekly platform qualification' not in body:
            notes = root / 'notes.md'
            notes.write_text(body + footer)
            # Editing body only preserves Latest, prerelease, draft and feed state.
            gh('api', '--method', 'PATCH', f"repos/{repo}/releases/{current['id']}",
               '-F', 'body=@' + str(notes))
        print('Verified published package:', name)


if __name__ == '__main__':
    main()
