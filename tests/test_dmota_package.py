import subprocess
import unittest

import test_ota_controller as controller
import dmota_package as package
import ota_controller as legacy


class DmotaPackageTest(unittest.TestCase):
    # Reuse setup only; inheriting TestCase would rerun the six legacy tests.
    setUpClass = classmethod(controller.OtaControllerTest.setUpClass.__func__)
    tearDownClass = classmethod(controller.OtaControllerTest.tearDownClass.__func__)

    def setUp(self):
        controller.OtaControllerTest.setUp(self)
        self.v2image = self.root / "next.bin"
        self.v2image.write_bytes(self.image.read_bytes() + package.UPDATER_TAG)
        self.single = self.root / "next.dmota"
        package.package(self.v2image, self.key, 201, "host-test-next", self.single)

    def test_single_file_independent_signature_and_layout(self):
        data = self.single.read_bytes()
        self.assertEqual(package.HEADER_BYTES, 192)
        self.assertEqual(data[:8], b"DMOTA2\r\n")
        self.assertEqual(data[116:128], b"\x02\0\0\0stable\0\0")
        self.assertEqual(data[192:], self.v2image.read_bytes())
        self.assertEqual(package.verify(self.single, self.pub)["channel"], "stable")
        self.assertEqual(subprocess.run([self.verifier], input=self.pub + data[:192]).returncode, 0)
        for offset in (0, 8, 12, 16, 20, 36, 84, 116, 120, 127, 128, 191):
            changed = bytearray(data[:192]); changed[offset] ^= 1
            self.assertNotEqual(subprocess.run([self.verifier], input=self.pub + changed).returncode, 0)

    def test_v2_tamper_truncation_trailer_and_trust_anchor(self):
        original = self.single.read_bytes()
        for value in (original[:191], original[:-1], original + b"x", original[:200] + b"x" + original[201:]):
            self.single.write_bytes(value)
            with self.assertRaises(legacy.OtaError): package.verify(self.single, self.pub)
        self.single.write_bytes(original)
        with self.assertRaises(legacy.OtaError): package.verify(self.single, bytes(32))
        with self.assertRaises(FileExistsError):
            package.package(self.v2image, self.key, 201, "host-test-next", self.single)
        with self.assertRaises(legacy.OtaError):
            package.package(self.image, self.key, 201, "host-test-next", self.root / "old.dmota")
