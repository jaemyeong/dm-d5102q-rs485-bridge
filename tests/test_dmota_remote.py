import ast
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import test_ota_controller  # Establish the project's scripts import path.
import dmota_remote as remote


class RemoteTest(unittest.TestCase):
    def setUp(self):
        self.profile = dict(deviceId="dm-bridge-8810a1", otaVersion=313,
                            buildId="usb-bootstrap-0.3.13-dev", otaKeyId="key",
                            configSchema=1, configRevision=1)
        self.state = dict(self.profile, boardId="m5stack-atom", otaProtocol=2,
                          connected=True, mode="STA", txBlocked=True, otaHealthy=True,
                          otaSupported=True, otaBootState="VALID", otaPhase="IDLE",
                          otaReason="NONE", githubAutomatic=False,
                          githubState="AUTOMATIC_DISABLED", otaAttemptVersion=313,
                          csrfToken="a" * 32, otaTransaction="b" * 32)

    def test_preflight_requires_no_usb_or_mdns_and_redacts(self):
        result = remote.ready(self.state, self.profile)
        self.assertNotIn("csrfToken", result)
        self.assertNotIn("otaTransaction", result)
        tree = ast.parse(Path(remote.__file__).read_text())
        imports = [alias.name for node in ast.walk(tree) if isinstance(node, ast.Import) for alias in node.names]
        self.assertFalse(any(name.startswith("serial") for name in imports))

    def test_mismatches_and_unready_states_rejected(self):
        bad = dict(deviceId="dm-bridge-000000", boardId="other", otaVersion=312,
                   buildId="other", otaKeyId="other", configRevision=2,
                   configSchema=True, otaProtocol=1, connected=False, mode="AP",
                   txBlocked=False, otaHealthy=False, otaSupported=False,
                   otaBootState="PENDING_VERIFY", otaPhase="RECEIVING",
                   otaReason="FAILED", githubAutomatic=True, githubState="CHECKING",
                   otaAttemptVersion=314)
        for field, value in bad.items():
            with self.subTest(field=field), self.assertRaises(remote.ota.OtaError):
                remote.ready(dict(self.state, **{field: value}), self.profile)

    def test_fence_is_exclusive_private_and_per_device(self):
        with tempfile.TemporaryDirectory() as folder:
            fence = remote.fence_path(Path(folder), self.profile["deviceId"])
            remote.reserve(fence, {"phase": "RESERVED"})
            self.assertEqual(fence.stat().st_mode & 0o777, 0o600)
            with self.assertRaises(FileExistsError):
                remote.reserve(fence, {"phase": "RESERVED"})

    def test_private_credentials_only(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "credentials"
            path.write_text("Installation key: " + "A" * 20 + "\n")
            path.chmod(0o600)
            self.assertEqual(remote.credential(path), "A" * 20)
            path.chmod(0o644)
            with self.assertRaises(remote.ota.OtaError):
                remote.credential(path)

    def test_lost_upload_response_reconciles_without_retry(self):
        outer = self
        class Fake:
            origin = "http://192.168.1.55"
            def __init__(self):
                self.posts = []
            def status(self):
                if len(self.posts) == 2:
                    return dict(outer.state, otaVersion=314, otaAttemptVersion=314, buildId="next")
                return outer.state.copy()
            def signed(self, method, path, body, **kwargs):
                self.posts.append(path)
                if path.endswith("prepare"):
                    return {"uploadToken": "b" * 32}
                raise TimeoutError()
        data = b"h" * 192 + b"image"
        metadata = dict(version=314, buildId="next", packageSha256=hashlib.sha256(data).hexdigest())
        client = Fake()
        with tempfile.TemporaryDirectory() as folder:
            fence = remote.fence_path(Path(folder), self.profile["deviceId"])
            result = remote.deploy(client, self.profile, metadata, data, fence)
            self.assertEqual(result["otaVersion"], 314)
            self.assertEqual(client.posts, ["/api/v1/ota/file/prepare", "/api/v1/ota/upload"])
            with self.assertRaises((FileExistsError, remote.ota.OtaError)):
                remote.deploy(client, self.profile, metadata, data, fence)
            self.assertEqual(len(client.posts), 2)

    def test_wrong_transaction_never_confirmed(self):
        record = dict(profile=self.profile, candidate=dict(version=314, buildId="next"), transaction="c" * 32)
        class Fake:
            def status(inner):
                return dict(self.state, otaVersion=314, otaAttemptVersion=314, buildId="next")
        with self.assertRaises(remote.ota.OtaError):
            remote.reconcile(Fake(), record, seconds=1)

    def test_lost_prepare_response_leaves_fence_and_never_uploads(self):
        outer = self
        class Fake:
            origin = "http://192.168.1.55"
            posts = 0
            def status(self):
                return outer.state.copy()
            def signed(self, *args, **kwargs):
                self.posts += 1
                raise TimeoutError()
        client = Fake()
        data = b"h" * 192 + b"image"
        meta = dict(version=314, buildId="next", packageSha256=hashlib.sha256(data).hexdigest())
        with tempfile.TemporaryDirectory() as folder:
            fence = remote.fence_path(Path(folder), self.profile["deviceId"])
            with self.assertRaises(TimeoutError):
                remote.deploy(client, self.profile, meta, data, fence)
            with self.assertRaises(FileExistsError):
                remote.deploy(client, self.profile, meta, data, fence)
            self.assertEqual(client.posts, 1)

    def test_package_tamper_and_downgrade_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "candidate.dmota"
            path.write_bytes(b"image")
            meta = dict(packageSha256="bad", version=314, boardId="m5stack-atom",
                        keyId="key", minSchema=1)
            with patch.object(remote.package, "verify", return_value=meta):
                with self.assertRaises(remote.ota.OtaError):
                    remote.candidate(path, bytes(32), self.profile)
                meta.update(packageSha256=hashlib.sha256(b"image").hexdigest(), version=313)
                with self.assertRaises(remote.ota.OtaError):
                    remote.candidate(path, bytes(32), self.profile)
