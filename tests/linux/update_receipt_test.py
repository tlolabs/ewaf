#!/usr/bin/env python3
"""Exercise Linux receipt orchestration without native tools or production keys."""
import hashlib
import json
import os
from pathlib import Path
import runpy
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "script"))
from version import VERSION


def elf(architecture=62):
    header = bytearray(64)
    header[:4] = b"\x7fELF"
    header[18:20] = architecture.to_bytes(2, "little")
    return bytes(header)


class LinuxReceiptTests(unittest.TestCase):
    def verify(self, *, valid_header=True, embedded_version=VERSION,
               embedded_architecture=62, expected_error=None):
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path(directory).resolve() / "ewaf.AppImage"
            artifact.write_bytes(elf() if valid_header else b"invalid")
            calls = []
            helper_path = None
            original_read_text = Path.read_text

            def read_text(path, *args, **kwargs):
                if path == ROOT / "updates/trust.json":
                    return json.dumps({})
                return original_read_text(path, *args, **kwargs)

            def check_output(args, **kwargs):
                calls.append(tuple(args))
                if args == [str(artifact), "--core-smoke"]:
                    return f"EWAF {VERSION} ABI 1\n"
                if helper_path and args == [str(helper_path), "version"]:
                    self.assertEqual(kwargs["env"]["LD_LIBRARY_PATH"], str(helper_path.parents[1] / "lib"))
                    return json.dumps({"version": embedded_version})
                self.fail(f"Unexpected executable: {args}")

            def extract(args, **kwargs):
                nonlocal helper_path
                calls.append(tuple(args))
                self.assertEqual(args, [str(artifact), "--appimage-extract"])
                self.assertEqual(calls[0], (str(artifact), "--core-smoke"))
                helper_path = Path(kwargs["cwd"]) / "squashfs-root/usr/bin/ewaf-update"
                helper_path.parent.mkdir(parents=True)
                helper_path.write_bytes(elf(embedded_architecture))
                return subprocess.CompletedProcess(args, 0)

            with mock.patch.object(sys, "argv", ["update_receipt.py", str(artifact), "--platform", "linux", "--arch", "x64"]), \
                 mock.patch.dict(os.environ, {"GITHUB_SHA": "b" * 40}), \
                 mock.patch.object(Path, "read_text", read_text), \
                 mock.patch("subprocess.check_output", side_effect=check_output), \
                 mock.patch("subprocess.run", side_effect=extract):
                if expected_error:
                    with self.assertRaises(expected_error):
                        runpy.run_path(str(ROOT / "script/update_receipt.py"), run_name="__main__")
                    self.assertFalse(artifact.with_suffix(".AppImage.verified.json").exists())
                else:
                    runpy.run_path(str(ROOT / "script/update_receipt.py"), run_name="__main__")
                    receipt = json.loads(artifact.with_suffix(".AppImage.verified.json").read_text())
                    self.assertEqual(receipt["version"], VERSION)
                    self.assertEqual(receipt["sha256"], hashlib.sha256(artifact.read_bytes()).hexdigest())
                    self.assertEqual(receipt["signer"], "manifest-ed25519")
                    self.assertIsNotNone(helper_path)
                    self.assertIn((str(helper_path), "version"), calls)
                    self.assertNotIn((str(ROOT / "build/linux/ewaf-update"), "version"), calls)
            if not valid_header:
                self.assertEqual(calls, [], "Invalid architecture must fail before image execution")

    def test_checks_the_embedded_helper(self):
        self.verify()

    def test_invalid_header_never_executes_image(self):
        self.verify(valid_header=False, expected_error=SystemExit)

    def test_wrong_embedded_helper_version_rejects_receipt(self):
        self.verify(embedded_version="0.0.1", expected_error=SystemExit)

    def test_wrong_embedded_helper_architecture_rejects_receipt(self):
        self.verify(embedded_architecture=183, expected_error=SystemExit)


if __name__ == "__main__":
    unittest.main()
