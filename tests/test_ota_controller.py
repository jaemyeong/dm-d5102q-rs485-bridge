from __future__ import annotations
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_controller as ota


class OtaControllerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.native = tempfile.TemporaryDirectory(prefix="dm-ota-native-")
        objects = []
        source = ROOT / "firmware/usb_bootstrap"
        for name in ("monocypher.c", "monocypher-ed25519.c"):
            target = Path(cls.native.name) / (name + ".o")
            subprocess.run([shutil.which("clang") or "cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-c",
                            str(source / "vendor/monocypher" / name), "-o", str(target)], check=True, capture_output=True)
            objects.append(str(target))
        cls.verifier = str(Path(cls.native.name) / "verify")
        subprocess.run([shutil.which("clang++") or "c++", "-std=c++11", "-Wall", "-Wextra", "-Werror", "-I", str(source),
                        str(source / "core.cpp"), str(source / "ota_core.cpp"), str(ROOT / "tests/host/ota_verify.cpp"),
                        *objects, "-o", cls.verifier], check=True, capture_output=True)

    @classmethod
    def tearDownClass(cls):
        cls.native.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dm-ota-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        result = ota.init_key(self.root / "keys")
        self.key = Path(result["keyFile"])
        self.pub = ota.public_key(self.key)
        self.image = self.root / "input.bin"
        self.image.write_bytes(b"\xe9" + bytes(63) + self.pub + b"DMOTA-RELEASE:host-test-next:201\0" + bytes(50))
        self.release = self.root / "release"
        self.meta = ota.package(self.image, self.key, 201, "host-test-next", self.release)

    def test_private_key_and_public_only_header(self):
        self.assertEqual(self.key.stat().st_mode & 0o777, 0o600)
        header = self.root / "identity.h"
        result = ota.identity_header(self.key, header)
        self.assertEqual(result["publicKeyId"], hashlib.sha256(self.pub).hexdigest())
        self.assertNotIn("PRIVATE", header.read_text())
        with self.assertRaises(FileExistsError): ota.identity_header(self.key, header)
        self.key.chmod(0o644)
        with self.assertRaises(ota.OtaError): ota.public_key(self.key)

    def test_package_and_cross_implementation_signature(self):
        metadata, envelope, image = ota.load_release(self.release)
        self.assertEqual(metadata["sha256"], hashlib.sha256(image).hexdigest())
        self.assertEqual(len(envelope), 180)
        self.assertEqual(subprocess.run([self.verifier], input=self.pub + envelope).returncode, 0)
        for offset in (8, 20, 36, 84, 116, 179):
            changed = bytearray(envelope); changed[offset] ^= 1
            self.assertNotEqual(subprocess.run([self.verifier], input=self.pub + changed).returncode, 0)
        with self.assertRaises(FileExistsError): ota.package(self.image, self.key, 201, "host-test-next", self.release)

    def test_manifest_image_metadata_and_key_rejection(self):
        with self.assertRaises(ota.OtaError): ota.manifest_bytes(200, "host-test-next", self.image.read_bytes())
        original = (self.release / "firmware.bin").read_bytes()
        (self.release / "firmware.bin").write_bytes(original[:-1] + b"x")
        with self.assertRaises(ota.OtaError): ota.load_release(self.release)
        (self.release / "firmware.bin").write_bytes(original)
        metadata = dict(self.meta, version=202)
        (self.release / "release.json").write_text(json.dumps(metadata))
        with self.assertRaises(ota.OtaError): ota.load_release(self.release)
        self.image.write_bytes(b"\xe9" + bytes(63) + b"DMOTA-RELEASE:host-test-next:201\0")
        with self.assertRaises(ota.OtaError): ota.package(self.image, self.key, 201, "host-test-next", self.root / "wrong-key")

    def test_digest_nonce_count_and_no_redirect(self):
        client = ota.Client("http://dm-bridge-8810a1.local", "A" * 20, "dm-bridge-8810a1")
        calls = []
        def raw(method, path, body=b"", headers=None):
            if path.endswith("/auth"):
                return dict(realm="DM-BRIDGE-USB", algorithm="SHA-256", qop="auth", nonce="a" * 32)
            calls.append(headers); return {}
        client.raw = raw
        client.signed("GET", "/api/v1/ota/status")
        client.signed("GET", "/api/v1/ota/status")
        self.assertIn("nc=00000001", calls[0]["Authorization"])
        self.assertIn("nc=00000002", calls[1]["Authorization"])
        self.assertNotIn("A" * 20, calls[0]["Authorization"])
        for url in ("https://host", "http://user:pass@host", "http://host/path", "http://host:8080"):
            with self.assertRaises(ota.OtaError): ota.Client(url, "A" * 20, "dm-bridge-8810a1")

    def fake_client(self, lost_upload=False):
        metadata = self.meta
        class Fake:
            device = "dm-bridge-8810a1"
            writes = []
            def status(self):
                return dict(deviceId=self.device, boardId=ota.BOARD, txBlocked=True, otaSupported=True, otaHealthy=True,
                            otaKeyId=metadata["keyId"], buildId="current", otaVersion=200, configSchema=1,
                            otaPhase="IDLE", otaBootState="VALID", csrfToken="a" * 32)
            def signed(self, method, path, body=b"", csrf="", token=""):
                self.writes.append(path)
                if path.endswith("/prepare"): return {"uploadToken": "b" * 32}
                if lost_upload: raise OSError("lost response")
                return {}
        return Fake()

    def test_lost_upload_response_reconciles_once_and_quarantines(self):
        client = self.fake_client(lost_upload=True)
        with patch.object(ota, "reconcile", return_value=False) as reconcile:
            with self.assertRaises(ota.OtaError): ota.deploy(client, self.release, self.root / "state")
            self.assertEqual(reconcile.call_count, 1)
        self.assertEqual(client.writes.count("/api/v1/ota/upload"), 1)
        with self.assertRaises(ota.OtaError): ota.deploy(client, self.release, self.root / "state")
        self.assertEqual(client.writes.count("/api/v1/ota/upload"), 1)

    def test_success_and_interrupted_process_reconciliation(self):
        client = self.fake_client()
        with patch.object(ota, "reconcile", return_value=True):
            result = ota.deploy(client, self.release, self.root / "state")
        self.assertEqual(result["status"], "SUCCESS")
        state_path = self.root / "state" / (client.device + ".json")
        state = json.loads(state_path.read_text()); state["status"] = "IN_PROGRESS"
        ota.write_state(state_path, state)
        before = len(client.writes)
        with patch.object(ota, "reconcile", return_value=True):
            ota.deploy(client, self.release, self.root / "state")
        self.assertEqual(len(client.writes), before)
        self.assertEqual(state_path.stat().st_mode & 0o777, 0o600)


if __name__ == "__main__": unittest.main()
