"""Exercise packaging trust boundaries with public, disposable fixtures only."""
import base64
import json
import os
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class UpdateConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'script').mkdir()
        (self.root / 'updates').mkdir()
        for name in ('configure_updates.py', 'check_update_trust.py'):
            shutil.copyfile(ROOT / 'script' / name, self.root / 'script' / name)
        self.trust = dict(application_id='com.tlolabs.ewaf', repository='tlolabs/ewaf',
                          keys={'test': base64.b64encode(bytes(32)).decode()},
                          macos_team_id='TESTTEAM00', windows_publisher='CN=Test fixture')
        self.info = self.root / 'Info.plist'
        self.info.write_bytes(plistlib.dumps(dict(CFBundleIdentifier='com.tlolabs.ewaf',
                                                  SUPublicEDKey='stale', SUFeedURL='https://stale.invalid')))

    def run_script(self, name, **settings):
        (self.root / 'updates' / 'trust.json').write_text(json.dumps(self.trust))
        env = {key: value for key, value in os.environ.items()
               if key not in ('EWAF_PRODUCTION', 'EWAF_UPDATE_CHANNEL', 'EWAF_UPDATE_ARCH',
                              'WINDOWS_DISTRIBUTION_APPROVED')}
        env.update(settings)
        args = [os.sys.executable, '-O', str(self.root / 'script' / name)]
        if name == 'configure_updates.py':
            args.append(str(self.info))
        return subprocess.run(args, env=env, capture_output=True, text=True)

    def test_development_removes_stale_production_trust(self):
        result = self.run_script('configure_updates.py')
        self.assertEqual(result.returncode, 0, result.stderr)
        info = plistlib.loads(self.info.read_bytes())
        self.assertNotIn('SUPublicEDKey', info)
        self.assertNotIn('SUFeedURL', info)

    def test_production_feed_is_architecture_bound_and_requires_signatures(self):
        for arch in ('arm64', 'x64'):
            result = self.run_script('configure_updates.py', EWAF_PRODUCTION='1', EWAF_UPDATE_ARCH=arch)
            self.assertEqual(result.returncode, 0, result.stderr)
            info = plistlib.loads(self.info.read_bytes())
            self.assertEqual(info['SUFeedURL'], f'https://github.com/tlolabs/ewaf/releases/latest/download/appcast-{arch}.xml')
            self.assertTrue(info['SURequireSignedFeed'])
            self.assertTrue(info['SUVerifyUpdateBeforeExtraction'])
            self.assertEqual(info['SUSignedFeedFailureExpirationInterval'], 0)
            self.assertFalse(info['SUAutomaticallyUpdate'])

    def test_invalid_key_and_unknown_architecture_fail_even_with_python_optimization(self):
        self.assertNotEqual(self.run_script('configure_updates.py', EWAF_PRODUCTION='1', EWAF_UPDATE_ARCH='universal').returncode, 0)
        self.trust['keys']['test'] = base64.b64encode(b'bad').decode()
        self.assertNotEqual(self.run_script('configure_updates.py', EWAF_PRODUCTION='1', EWAF_UPDATE_ARCH='arm64').returncode, 0)

    def test_production_enrollment_and_distribution_gates_fail_closed(self):
        self.assertNotEqual(self.run_script('check_update_trust.py').returncode, 0)
        self.assertEqual(self.run_script('check_update_trust.py', WINDOWS_DISTRIBUTION_APPROVED='true').returncode, 0)
        for field, invalid in (('keys', {}), ('macos_team_id', ''), ('windows_publisher', ''), ('repository', 'attacker/ewaf')):
            old = self.trust[field]
            self.trust[field] = invalid
            self.assertNotEqual(self.run_script('check_update_trust.py', WINDOWS_DISTRIBUTION_APPROVED='true').returncode, 0, field)
            self.trust[field] = old


if __name__ == '__main__':
    unittest.main()
