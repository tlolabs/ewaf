import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
VERSION = subprocess.check_output(
    ["python3", "script/version.py"], cwd=ROOT, text=True
).strip()


FAKE_GH = '''#!/usr/bin/env python3
import json
import os
from pathlib import Path
import shutil
import sys

args = sys.argv[1:]
state_path = Path(os.environ['FAKE_GH_STATE'])
state = json.loads(state_path.read_text())
assets = Path(os.environ['FAKE_GH_ASSETS'])
sources = Path(os.environ['FAKE_GH_SOURCES'])

def save():
    state_path.write_text(json.dumps(state))

if args[:1] == ['api'] and '/actions/runs/' in ' '.join(args):
    print('\\n'.join(state['artifacts']))
elif args[:1] == ['api'] and '/releases/tags/' in ' '.join(args):
    print('123')
elif args[:2] == ['run', 'download']:
    name = args[args.index('--name') + 1]
    target = Path(args[args.index('--dir') + 1])
    target.mkdir(parents=True, exist_ok=True)
    for file in (sources / name).iterdir():
        shutil.copy2(file, target / file.name)
elif args[:2] == ['release', 'view']:
    if not state['release']:
        sys.exit(1)
    if '--json' in args:
        field = args[args.index('--json') + 1]
        if field == 'assets':
            print('\\n'.join(sorted(p.name for p in assets.iterdir())))
        elif field == 'isDraft':
            print(str(state['draft']).lower())
        elif field == 'body':
            print(state.get('body', ''))
elif args[:2] == ['release', 'create']:
    state['release'] = True
    state['draft'] = True
    save()
elif args[:2] == ['release', 'upload']:
    file = Path(args[3])
    shutil.copy2(file, assets / file.name)
elif args[:2] == ['release', 'download']:
    target = Path(args[args.index('--dir') + 1])
    target.mkdir(parents=True, exist_ok=True)
    for i, arg in enumerate(args):
        if arg == '--pattern':
            name = args[i + 1]
            shutil.copy2(assets / name, target / name)
elif args[:2] == ['api', '--method']:
    state['draft'] = False
    save()
else:
    raise SystemExit('Unexpected gh command: ' + ' '.join(args))
'''


class DirectReleaseTests(unittest.TestCase):
    def test_each_publisher_waits_only_for_its_platform(self):
        workflow = (ROOT / ".github/workflows/native.yml").read_text()
        for platform in ("macos", "windows", "linux"):
            self.assertIn(
                f"  release-{platform}:\n    needs: {platform}\n", workflow
            )
        self.assertIn(
            "  release-notes:\n    needs: [release-macos, release-windows, release-linux]\n",
            workflow,
        )

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)
        self.sources = self.path / "sources"
        self.assets = self.path / "assets"
        self.bin = self.path / "bin"
        for folder in (self.sources, self.assets, self.bin):
            folder.mkdir()
        gh = self.bin / "gh"
        gh.write_text(FAKE_GH)
        gh.chmod(0o755)
        self.state_path = self.path / "state.json"
        self.state = {"artifacts": [], "release": False, "draft": False}
        self._save()

    def _save(self):
        self.state_path.write_text(json.dumps(self.state))

    def _run(self, platform="linux"):
        env = dict(os.environ)
        env.update(
            PATH=f"{self.bin}:{env['PATH']}",
            GITHUB_REF_NAME=f"v{VERSION}",
            GITHUB_REPOSITORY="tlolabs/ewaf",
            GITHUB_RUN_ID="123",
            GITHUB_SHA="0" * 40,
            RUNNER_TEMP=str(self.path),
            FAKE_GH_STATE=str(self.state_path),
            FAKE_GH_SOURCES=str(self.sources),
            FAKE_GH_ASSETS=str(self.assets),
        )
        return subprocess.run(
            ["bash", "script/publish_direct_release.sh", platform],
            cwd=ROOT, env=env, capture_output=True, text=True,
        )

    def _package(self, valid=True):
        artifact = "EWAF-linux-ubuntu-24.04"
        folder = self.sources / artifact
        folder.mkdir()
        package = folder / f"EWAF-{VERSION}-linux-x64.AppImage"
        package.write_bytes(b"verified AppImage fixture")
        checksum = hashlib.sha256(package.read_bytes()).hexdigest()
        if not valid:
            checksum = "0" * 64
        (folder / f"{package.name}.sha256").write_text(
            f"{checksum}  {package.name}\n"
        )
        self.state["artifacts"] = [artifact]
        self._save()
        return package

    def test_no_platform_artifact_does_not_create_release(self):
        result = self._run()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(json.loads(self.state_path.read_text())["release"])

    def test_bad_checksum_cannot_publish(self):
        self._package(valid=False)
        result = self._run()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Checksum mismatch", result.stderr)
        self.assertFalse(json.loads(self.state_path.read_text())["release"])
        self.assertEqual(list(self.assets.iterdir()), [])

    def test_verified_package_publishes_and_retry_is_idempotent(self):
        package = self._package()
        first = self._run()
        self.assertEqual(first.returncode, 0, first.stderr)
        self.assertEqual(
            sorted(p.name for p in self.assets.iterdir()),
            [package.name, package.name + ".sha256"],
        )
        self.assertFalse(json.loads(self.state_path.read_text())["draft"])
        second = self._run()
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertIn("Keeping verified existing release asset", second.stdout)
        (self.assets / package.name).write_bytes(b"changed release asset")
        third = self._run()
        self.assertNotEqual(third.returncode, 0)
        self.assertIn("Existing release asset differs", third.stderr)


if __name__ == "__main__":
    unittest.main()
