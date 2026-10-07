"""Exercise release assembly's fail-closed boundary before secrets or signing."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[2]
class ReferenceIsolation(unittest.TestCase):
    def test_internal_artifact_rejected_before_signing(self):
        with tempfile.TemporaryDirectory() as directory:
            Path(directory, 'internal-qt-reference-1.0.5-osx-arm64.zip').write_bytes(b'internal')
            result = subprocess.run(['python3', str(ROOT/'script/build_update_release.py'), directory], capture_output=True, text=True, env={k:v for k,v in os.environ.items() if k != 'TLO_UPDATE_SIGNING_SEED'})
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Internal reference artifacts must never enter', result.stderr)
            self.assertFalse(Path(directory, 'update-manifest.json').exists())
