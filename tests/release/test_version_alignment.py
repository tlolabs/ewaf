"""Run the version diagnostic against isolated committed-version copies."""
from pathlib import Path
import plistlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class VersionAlignmentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / 'script').mkdir()
        for name in ('check_versions.py', 'version.py'):
            shutil.copyfile(ROOT / 'script' / name, self.root / 'script' / name)
        (self.root / 'Cargo.toml').write_text('[workspace.package]\nversion = "1.2.3"\n')
        (self.root / 'CMakeLists.txt').write_text('project(EWAF VERSION 1.2.3 LANGUAGES C CXX)\n')
        self.manifest = self.root / 'crates/ewaf-core/Cargo.toml'
        self.manifest.parent.mkdir(parents=True)
        self.manifest.write_text('[package]\nname = "ewaf-core"\nversion.workspace = true\n')
        updater = self.root / 'crates/tlo-updater/Cargo.toml'
        updater.parent.mkdir(parents=True)
        updater.write_text('[package]\nname = "tlo-updater"\nversion = "0.1.0"\n')
        self.lock = self.root / 'Cargo.lock'
        self.lock.write_text('[[package]]\nname = "ewaf-core"\nversion = "1.2.3"\n'
                             '[[package]]\nname = "tlo-updater"\nversion = "0.1.0"\n')
        self.project = self.root / 'platform/macos/EWAF.xcodeproj/project.pbxproj'
        self.project.parent.mkdir(parents=True)
        self.objects = {
            'target': {'isa': 'PBXNativeTarget', 'name': 'EWAFMac', 'buildConfigurationList': 'configs'},
            'configs': {'buildConfigurations': ['debug', 'release']},
            'debug': {'name': 'Debug', 'buildSettings': {
                'CURRENT_PROJECT_VERSION': '1.2.3', 'MARKETING_VERSION': '1.2.3'}},
            'release': {'name': 'Release', 'buildSettings': {
                'CURRENT_PROJECT_VERSION': '1.2.3', 'MARKETING_VERSION': '1.2.3'}},
        }
        self.write_project()

    def write_project(self):
        self.project.write_bytes(plistlib.dumps({'objects': self.objects}))

    def run_check(self, optimize=False):
        command = [sys.executable]
        if optimize:
            command.append('-O')
        return subprocess.run(command + [str(self.root / 'script/check_versions.py')],
                              capture_output=True, text=True)

    def test_aligned_versions_accept_independently_versioned_updater(self):
        result = self.run_check()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('Unified version: 1.2.3', result.stdout)

    def test_stale_local_lock_version_is_reported(self):
        self.lock.write_text(self.lock.read_text().replace('version = "1.2.3"', 'version = "1.2.2"'))
        result = self.run_check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Cargo.lock version for ewaf-core', result.stderr)

    def test_each_native_configuration_version_copy_is_checked(self):
        for configuration in ('debug', 'release'):
            for key in ('CURRENT_PROJECT_VERSION', 'MARKETING_VERSION'):
                with self.subTest(configuration=configuration, key=key):
                    self.objects[configuration]['buildSettings'][key] = '1.2.2'
                    self.write_project()
                    result = self.run_check()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(f'Xcode EWAFMac/{configuration.title()} {key}', result.stderr)
                    self.objects[configuration]['buildSettings'][key] = '1.2.3'

    def test_missing_native_versions_are_reported(self):
        self.objects['release']['buildSettings'] = {}
        self.write_project()
        result = self.run_check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Xcode EWAFMac/Release CURRENT_PROJECT_VERSION', result.stderr)

    def test_native_cmake_copy_is_checked_under_python_optimization(self):
        cmake = self.root / 'CMakeLists.txt'
        cmake.write_text(cmake.read_text().replace('1.2.3', '1.2.2'))
        result = self.run_check(optimize=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('CMakeLists.txt version', result.stderr)

    def test_application_crate_must_inherit_workspace_version(self):
        self.manifest.write_text('[package]\nname = "ewaf-core"\nversion = "1.2.3"\n')
        result = self.run_check()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('must inherit the workspace version', result.stderr)


if __name__ == '__main__':
    unittest.main()
