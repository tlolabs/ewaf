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
    if state['artifacts']:
        print('\\n'.join(state['artifacts']))
elif args[:1] == ['api'] and '/git/ref/tags/' in ' '.join(args):
    if not state['release']:
        sys.exit(1)
    print(state['source_sha'])
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
    state['tag'] = args[2]
    state['source_sha'] = args[args.index('--target') + 1]
    state['prerelease'] = '--prerelease' in args
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
        for platform, arch, build in (
            ("macos", "arm64", "macos-arm64"),
            ("macos", "x86_64", "macos-intel"),
            ("windows", "x64", "windows-x64"),
            ("windows", "arm64", "windows-arm64"),
            ("linux", "x64", "linux-x64"),
            ("linux", "arm64", "linux-arm64"),
        ):
            self.assertIn(
                f"  release-{platform}-{arch}:\n    needs: {build}\n", workflow
            )
        self.assertIn(
            "  release-notes:\n    needs: [release-macos-arm64, release-macos-x86_64, release-windows-x64, release-windows-arm64, release-linux-x64, release-linux-arm64]\n",
            workflow,
        )

    def test_build_jobs_are_read_only_and_attestation_is_separate(self):
        workflow = (ROOT / ".github/workflows/native.yml").read_text()
        for platform in ("windows", "linux"):
            build = (ROOT / f".github/workflows/build-{platform}.yml").read_text()
            self.assertNotIn("attestations: write", build)
            self.assertNotIn("id-token: write", build)
            self.assertIn(f"  attest-{platform}-x64:\n", workflow)
            self.assertIn(f"  attest-{platform}-arm64:\n", workflow)
        attestation = (ROOT / ".github/workflows/attest-package.yml").read_text()
        self.assertIn("actions/attest-build-provenance@v3", attestation)

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

    def _run(self, platform="linux", arch="x64", development=False):
        env = dict(os.environ)
        env.update(
            PATH=f"{self.bin}:{env['PATH']}",
            GITHUB_REF=("refs/heads/main" if development else f"refs/tags/v{VERSION}"),
            GITHUB_REF_NAME=f"v{VERSION}",
            GITHUB_EVENT_NAME="workflow_dispatch",
            GITHUB_REPOSITORY="tlolabs/ewaf",
            GITHUB_RUN_ID="123",
            GITHUB_SHA="a" * 40,
            RUNNER_TEMP=str(self.path),
            FAKE_GH_STATE=str(self.state_path),
            FAKE_GH_SOURCES=str(self.sources),
            FAKE_GH_ASSETS=str(self.assets),
        )
        return subprocess.run(
            ["bash", "script/publish_direct_release.sh", platform, arch],
            cwd=ROOT, env=env, capture_output=True, text=True,
        )

    def _package(self, platform="linux", arch="x64", valid=True, development=False):
        artifact, filename = {
            ("macos", "arm64"): (
                "EWAF-macos-macos-26", f"EWAF-{VERSION}-macos-arm64.zip"
            ),
            ("macos", "x86_64"): (
                "EWAF-macos-macos-15-intel", f"EWAF-{VERSION}-macos-x86_64.zip"
            ),
            ("windows", "x64"): (
                "EWAF-windows-x64", f"ewaf-{VERSION}-windows-x64.msi"
            ),
            ("windows", "arm64"): (
                "EWAF-windows-ARM64", f"ewaf-{VERSION}-windows-arm64.msi"
            ),
            ("linux", "x64"): (
                "EWAF-linux-ubuntu-24.04", f"ewaf-{VERSION}-linux-x64.AppImage"
            ),
            ("linux", "arm64"): (
                "EWAF-linux-ubuntu-24.04-arm", f"ewaf-{VERSION}-linux-arm64.AppImage"
            ),
        }[(platform, arch)]
        if development and platform == "macos":
            artifact = artifact.replace("EWAF-macos-", "EWAF-development-macos-")
        folder = self.sources / artifact
        folder.mkdir(exist_ok=True)
        package = folder / filename
        package.write_bytes(f"verified {platform}/{arch} fixture".encode())
        checksum = hashlib.sha256(package.read_bytes()).hexdigest()
        if not valid:
            checksum = "0" * 64
        (folder / f"{package.name}.sha256").write_text(
            f"{checksum}  {package.name}\n"
        )
        if platform == "linux":
            (folder / "companion.deb.sha256").write_text("unused")
        if platform == "windows":
            (folder / "companion.zip.sha256").write_text("unused")
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

    def test_conflicting_existing_package_is_rejected_before_upload(self):
        package = self._package()
        (self.assets / package.name).write_bytes(b"different existing package")
        self.state.update(release=True, draft=False)
        self._save()
        result = self._run()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Existing release asset differs", result.stderr)
        self.assertEqual(
            [p.name for p in self.assets.iterdir()], [package.name]
        )

    def test_real_package_names_for_all_six_architectures(self):
        for platform, arch in (
            ("macos", "arm64"),
            ("macos", "x86_64"),
            ("windows", "x64"),
            ("windows", "arm64"),
            ("linux", "x64"),
            ("linux", "arm64"),
        ):
            with self.subTest(platform=platform, arch=arch):
                package = self._package(platform, arch)
                result = self._run(platform, arch)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertTrue((self.assets / package.name).exists())
                self.assertTrue((self.assets / f"{package.name}.sha256").exists())
        self.assertEqual(len(list(self.assets.iterdir())), 12)

    def test_development_release_targets_exact_commit_and_is_prerelease(self):
        package = self._package("macos", "arm64", development=True)
        result = self._run("macos", "arm64", development=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        state = json.loads(self.state_path.read_text())
        self.assertEqual(state["tag"], "dev-" + "a" * 40)
        self.assertEqual(state["source_sha"], "a" * 40)
        self.assertTrue(state["prerelease"])
        self.assertTrue((self.assets / package.name).exists())


if __name__ == "__main__":
    unittest.main()
