"""Regressions for observed bytes versus source declarations; never execute packages."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('native_inventory', ROOT / 'script/inspect_native_package.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class NativeInventoryTests(unittest.TestCase):
    def test_source_inventory_never_claims_binary_verification(self):
        inventory = json.loads((ROOT / 'dependency-inventory.json').read_text())
        self.assertEqual(inventory['evidence_kind'], 'source_declarations')
        self.assertNotIn('packaged_native_components', inventory)
        for platform in inventory['declared_native_components']:
            self.assertIn('not observed', platform['verification_status'])
            for component in platform['components']:
                self.assertIn('unverified', component['security_patch_provenance'])

    def test_inspects_real_files_plugins_not_anticipated_component_list(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            plugin = root / 'plugins/platforms'
            plugin.mkdir(parents=True)
            binary = bytearray(64)
            binary[:6] = b'\x7fELF\x02\x01'
            struct.pack_into('<H', binary, 18, 183)
            path = plugin / 'libqxcb.so'
            path.write_bytes(binary)
            (root / 'LICENSE').write_text('fixture notice')
            result = module.inspect_tree(root, 'linux', 'arm64')
            observed = {entry['path']: entry for entry in result['files']}
            self.assertEqual(observed['plugins/platforms/libqxcb.so']['sha256'], module.sha256(path))
            self.assertEqual(observed['plugins/platforms/libqxcb.so']['binary']['architecture'], 'arm64')
            self.assertIn('LICENSE', observed)
            self.assertIn('unverified', result['provenance_status'])

    def test_symlinks_never_read_files_outside_package(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'package'
            root.mkdir()
            outside = root.parent / 'outside'
            outside.write_text('must not read')
            try:
                (root / 'escape').symlink_to(outside)
            except OSError:
                self.skipTest('Host does not permit fixture symlinks')
            with self.assertRaisesRegex(ValueError, 'escapes'):
                module.inspect_tree(root, 'windows', 'x64')

    def test_runtime_prefix_and_declared_digest_are_separate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'tree'
            root.mkdir()
            runtime = root.parent / 'runtime'
            runtime.write_bytes(b'fixture-runtime')
            package = root.parent / 'package.AppImage'
            package.write_bytes(b'fixture-runtime' + b'squashfs fixture')
            result = module.inspect_tree(root, 'linux', 'x64', package, runtime)
            self.assertTrue(result['appimage_runtime']['matches_package_prefix'])
            self.assertFalse(result['appimage_runtime']['matches_declared_pin'])
            package.write_bytes(b'wrong-runtime')
            result = module.inspect_tree(root, 'linux', 'x64', package, runtime)
            self.assertFalse(result['appimage_runtime']['matches_package_prefix'])

    def test_reads_pe_version_resource_without_loading_dll(self):
        data = bytearray(2048)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 0x3c, 0x80)
        data[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<H', data, 0x86, 1)  # one section
        struct.pack_into('<H', data, 0x94, 240)
        struct.pack_into('<H', data, 0x98, 0x20b)
        struct.pack_into('<II', data, 0x98 + 112 + 16, 0x1000, 512)
        struct.pack_into('<IIII', data, 0x98 + 240 + 8, 1024, 0x1000, 1024, 512)
        for offset, resource_id, child in [(0, 16, 0x80000020), (32, 1, 0x80000040), (64, 1033, 96)]:
            struct.pack_into('<HH', data, 512 + offset + 12, 0, 1)
            struct.pack_into('<II', data, 512 + offset + 16, resource_id, child)
        struct.pack_into('<II', data, 512 + 96, 0x1100, 100)
        key = 'VS_VERSION_INFO\0'.encode('utf-16le')
        data[768 + 6:768 + 6 + len(key)] = key
        fixed = 768 + ((6 + len(key) + 3) & ~3)
        struct.pack_into('<IIII', data, fixed, 0xfeef04bd, 0x10000, (6 << 16) | 8, (3 << 16) | 0)
        self.assertEqual(module.pe_version(data), ['6.8.3.0'])
        self.assertEqual(module.pe_version(b'truncated'), [])

    def test_macho_architecture_is_observed_from_header(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'app'
            header = bytearray(64)
            header[:4] = b'\xcf\xfa\xed\xfe'
            struct.pack_into('<I', header, 4, 0x0100000c)
            path.write_bytes(header)
            self.assertEqual(module.binary_metadata(path)['architectures'], ['arm64'])
            header[:4] = b'\xca\xfe\xba\xbe'
            struct.pack_into('>I', header, 4, 2)
            struct.pack_into('>I', header, 8, 0x01000007)
            struct.pack_into('>I', header, 28, 0x0100000c)
            path.write_bytes(header)
            self.assertEqual(module.binary_metadata(path)['architectures'], ['x64', 'arm64'])

    def test_historical_action_pin_evidence_remains_well_formed(self):
        # The owner replaced the old action-pinning policy with weekly smoke.
        # Preserve the old receipt as historical evidence, not as claims about
        # newer workflow commits or permission to rewrite the owner's archive.
        receipt = json.loads((ROOT / 'docs/audits/evidence/2026-10-14-action-pins.json').read_text())
        self.assertTrue(receipt['pins'])
        for item in receipt['pins']:
            self.assertRegex(item['declared'], r'^[a-f0-9]{40}$')
            self.assertRegex(item['tag'], r'^v[\w.-]+$')

    def test_archived_workflows_match_owner_manifest_and_remain_inactive(self):
        import hashlib
        archive = ROOT / '.github/workflow-archive/2026-11-08'
        manifest = json.loads((archive / 'manifest.json').read_text())
        checked = 0
        for item in manifest['workflows']:
            if 'archive_path' not in item:
                continue
            path = ROOT / item['archive_path']
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), item['sha256'])
            self.assertFalse((ROOT / item['original_path']).exists())
            checked += 1
        self.assertGreater(checked, 0)
        self.assertEqual([p.name for p in (ROOT / '.github/workflows').glob('*.yml')],
                         ['weekly-release-smoke.yml'])


if __name__ == '__main__':
    unittest.main()
