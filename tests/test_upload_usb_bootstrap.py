from __future__ import annotations
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from upload_usb_bootstrap import identify, check_segments, classify_boot_line, private_file


class UsbUploadGuardTest(unittest.TestCase):
    def test_pinned_tool_imports_without_hardware_access(self):
        result = subprocess.run(
            [str(ROOT / ".platformio-venv/bin/python"), "-c",
             "from scripts.upload_usb_bootstrap import load_vendor_tools; "
             "assert load_vendor_tools().__version__ == '4.11.0'"],
            cwd=ROOT, capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stderr)

    def target(self, **overrides):
        values = dict(device="/dev/cu.usbserial-2956578B12", vid=0x0403, pid=0x6001, serial_number="2956578B12")
        values.update(overrides)
        return SimpleNamespace(**values)

    def test_exact_unique_identity(self):
        self.assertEqual(identify([self.target()]), "/dev/cu.usbserial-2956578B12")
        for ports in ([], [self.target(), self.target()], [self.target(vid=0x303a)],
                      [self.target(serial_number="different")], [self.target(device="/dev/cu.usbmodem212201")]):
            with self.assertRaises(RuntimeError):
                identify(ports)

    def test_approved_segments_and_preserved_nvs(self):
        check_segments()

    def test_secret_line_never_appears_unredacted(self):
        key = "ABCDEFGHIJKLMNOPQRST".replace("I", "2").replace("O", "3")
        safe, parsed = classify_boot_line(("INSTALL_KEY_ONCE=" + key).encode())
        self.assertEqual(parsed, key)
        self.assertNotIn(key, safe)
        safe, parsed = classify_boot_line(b"prefix INSTALL_KEY_ONCE=malformed-secret")
        self.assertIsNone(parsed)
        self.assertNotIn("malformed-secret", safe)

    def test_private_output_is_exclusive_and_mode_0600(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "private.txt"
            with private_file(path) as output:
                output.write("synthetic fixture")
            self.assertEqual(os.stat(path).st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError):
                private_file(path)


if __name__ == "__main__":
    unittest.main()
