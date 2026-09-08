"""Signed B1 release tooling. No deployment occurs without an explicit command.

HTTP is limited to the existing trusted LAN/VPN profile. No redirects, proxies,
secret command-line arguments, background service install or blind upload retry.
"""
from __future__ import annotations

import argparse
import fcntl
import getpass
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import stat
import struct
import subprocess
import tempfile
import time
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
BOARD = "m5stack-atom"
MAX_IMAGE = 1310720
MANIFEST = struct.Struct("<8sIII16s48s32s")
MAGIC = b"DMOTA1\r\n"
PUB_DER_PREFIX = bytes.fromhex("302a300506032b6570032100")
TEXT = re.compile(r"[A-Za-z0-9_.-]+\Z")
HEX32 = re.compile(r"[0-9a-f]{32}\Z")


class OtaError(RuntimeError):
    pass


def private_file(path: Path) -> bytes:
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077:
        raise OtaError("Secret/state file must be a regular owner-only file (0600)")
    return path.read_bytes()


def exclusive(path: Path, data: bytes) -> None:
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as handle:
        handle.write(data)
        handle.flush()
        os.fsync(handle.fileno())


def openssl(*args: str) -> bytes:
    executable = shutil.which("openssl")
    if not executable:
        raise OtaError("OpenSSL 3 is required")
    result = subprocess.run([executable, *args], capture_output=True, timeout=20)
    if result.returncode:
        raise OtaError("OpenSSL operation failed; no secret or raw diagnostic printed")
    return result.stdout


def public_key(key: Path) -> bytes:
    private_file(key)
    der = openssl("pkey", "-in", str(key), "-pubout", "-outform", "DER")
    if len(der) != 44 or not der.startswith(PUB_DER_PREFIX):
        raise OtaError("An Ed25519 private key is required")
    return der[12:]


def init_key(directory: Path) -> dict:
    directory.mkdir(mode=0o700, parents=True, exist_ok=False)
    key = directory / "signing-key.pem"
    exclusive(key, openssl("genpkey", "-algorithm", "ED25519"))
    pub = public_key(key)
    exclusive(directory / "public-key.der", PUB_DER_PREFIX + pub)
    return {"keyFile": str(key), "publicKeyId": hashlib.sha256(pub).hexdigest()}


def identity_header(key: Path, output: Path) -> dict:
    pub = public_key(key)
    data = "#pragma once\n// Generated public verification key; private key is never embedded.\n"
    data += "static constexpr uint8_t kOtaPublicKey[32] = {" + ",".join(f"0x{x:02x}" for x in pub) + "};\n"
    output.parent.mkdir(parents=True, exist_ok=True)
    exclusive(output, data.encode("ascii"))
    return {"header": str(output), "publicKeyId": hashlib.sha256(pub).hexdigest()}


def manifest_bytes(version: int, build: str, image: bytes, schema: int = 1) -> bytes:
    if not 0 < version <= 0xffffffff or not 0 < schema <= 0xffffffff:
        raise OtaError("Version and schema must be positive uint32 values")
    if not TEXT.fullmatch(build) or len(build.encode("ascii")) > 47:
        raise OtaError("Invalid buildId")
    if not 32 <= len(image) <= MAX_IMAGE or image[0] != 0xe9:
        raise OtaError("Invalid ESP32 image size/header")
    tag = f"DMOTA-RELEASE:{build}:{version}".encode("ascii") + b"\0"
    if tag not in image:
        raise OtaError("Image does not contain the requested compiled release identity")
    return MANIFEST.pack(MAGIC, version, len(image), schema, BOARD.encode(), build.encode(), hashlib.sha256(image).digest())


def unpack_manifest(data: bytes) -> dict:
    if len(data) != MANIFEST.size:
        raise OtaError("Manifest length mismatch")
    magic, version, size, schema, board, build, digest = MANIFEST.unpack(data)
    def field(raw):
        text, sep, padding = raw.partition(b"\0")
        if not sep or any(padding):
            raise OtaError("Manifest field is not canonically zero-padded")
        value = text.decode("ascii")
        if not TEXT.fullmatch(value):
            raise OtaError("Invalid manifest text")
        return value
    board, build = field(board), field(build)
    if magic != MAGIC or board != BOARD or not version or not schema or not 32 <= size <= MAX_IMAGE:
        raise OtaError("Manifest policy rejected")
    return dict(version=version, imageSize=size, minSchema=schema, boardId=board, buildId=build, sha256=digest.hex())


def signature(key: Path, payload: bytes) -> bytes:
    private_file(key)
    with tempfile.TemporaryDirectory(prefix="dm-ota-sign-") as folder:
        source = Path(folder) / "manifest.bin"
        exclusive(source, payload)
        value = openssl("pkeyutl", "-sign", "-rawin", "-inkey", str(key), "-in", str(source))
    if len(value) != 64:
        raise OtaError("Unexpected signature length")
    return value


def verify_signature(pub: bytes, envelope: bytes) -> None:
    if len(pub) != 32 or len(envelope) != MANIFEST.size + 64:
        raise OtaError("Signature envelope length mismatch")
    with tempfile.TemporaryDirectory(prefix="dm-ota-verify-") as folder:
        directory = Path(folder)
        for name, data in (("public.der", PUB_DER_PREFIX + pub), ("manifest", envelope[:MANIFEST.size]), ("signature", envelope[MANIFEST.size:])):
            exclusive(directory / name, data)
        openssl("pkeyutl", "-verify", "-rawin", "-pubin", "-keyform", "DER", "-inkey", str(directory / "public.der"),
                "-in", str(directory / "manifest"), "-sigfile", str(directory / "signature"))


def package(image_path: Path, key: Path, version: int, build: str, output: Path) -> dict:
    image = image_path.read_bytes()
    payload = manifest_bytes(version, build, image)
    pub = public_key(key)
    envelope = payload + signature(key, payload)
    if pub not in image:
        raise OtaError("Image does not contain this OTA verification public key")
    verify_signature(pub, envelope)
    metadata = unpack_manifest(payload)
    metadata.update(publicKey=pub.hex(), keyId=hashlib.sha256(pub).hexdigest(), channel="dev")
    output.mkdir(parents=True, exist_ok=False)
    exclusive(output / "firmware.bin", image)
    exclusive(output / "manifest.bin", envelope)
    exclusive(output / "release.json", (json.dumps(metadata, indent=2) + "\n").encode())
    return metadata


def load_release(directory: Path) -> tuple[dict, bytes, bytes]:
    directory = directory.resolve(strict=True)  # Pin one immutable release behind a channel symlink.
    metadata = json.loads((directory / "release.json").read_text())
    envelope = (directory / "manifest.bin").read_bytes()
    image = (directory / "firmware.bin").read_bytes()
    pub = bytes.fromhex(metadata["publicKey"])
    verify_signature(pub, envelope)
    signed = unpack_manifest(envelope[:MANIFEST.size])
    if any(metadata.get(k) != v for k, v in signed.items()) or metadata.get("channel") != "dev":
        raise OtaError("Release metadata does not match the signed manifest/dev channel")
    if metadata.get("keyId") != hashlib.sha256(pub).hexdigest():
        raise OtaError("Release key fingerprint mismatch")
    if manifest_bytes(signed["version"], signed["buildId"], image, signed["minSchema"]) != envelope[:MANIFEST.size]:
        raise OtaError("Image does not match the signed manifest")
    return metadata, envelope, image


class Client:
    def __init__(self, url: str, key: str, device: str):
        parsed = urlsplit(url)
        if parsed.scheme != "http" or not parsed.hostname or parsed.username or parsed.password or parsed.path not in ("", "/") or parsed.query or parsed.fragment:
            raise OtaError("Use the exact device HTTP LAN/VPN origin without credentials/path")
        if not re.fullmatch(r"[A-Za-z0-9.-]+", parsed.hostname) or parsed.port not in (None, 80):
            raise OtaError("B1 requires the exact device IPv4/hostname on port 80")
        if not re.fullmatch(r"[A-HJ-NP-Z2-9]{20}", key):
            raise OtaError("Invalid device installation-key format")
        if not re.fullmatch(r"dm-bridge-[a-f0-9]{6}", device):
            raise OtaError("Exact device identity is required")
        self.host, self.key, self.device = parsed.hostname, key, device
        self.origin = "http://" + self.host
        self.cnonce, self.nonce, self.count = secrets.token_hex(16), "", 0

    def raw(self, method: str, path: str, body: bytes = b"", headers: dict | None = None) -> dict:
        connection = http.client.HTTPConnection(self.host, 80, timeout=20 if path.endswith("/upload") else 4)
        try:
            connection.request(method, path, body=body, headers=headers or {})
            response = connection.getresponse()
            data = response.read(16385)
            if len(data) > 16384 or response.status not in (200, 202):
                raise OtaError(f"Device HTTP status {response.status}; mutation outcome must be reconciled")
            value = json.loads(data)
            if not isinstance(value, dict):
                raise OtaError("Invalid device response")
            return value
        finally:
            connection.close()

    def signed(self, method: str, path: str, body: bytes = b"", csrf: str = "", token: str = "") -> dict:
        challenge = self.raw("GET", "/api/v1/auth")
        if challenge.get("realm") != "DM-BRIDGE-USB" or challenge.get("algorithm") != "SHA-256" or challenge.get("qop") != "auth" or not HEX32.fullmatch(challenge.get("nonce", "")):
            raise OtaError("Unsupported authentication challenge")
        if self.nonce != challenge["nonce"]:
            self.nonce, self.count = challenge["nonce"], 0
        self.count += 1
        if self.count > 0xffffffff:
            raise OtaError("Authentication nonce count exhausted")
        nc = f"{self.count:08x}"
        digest = lambda value: hashlib.sha256(value.encode("ascii")).hexdigest()
        ha1 = digest(f"installer:DM-BRIDGE-USB:{self.key}")
        ha2 = digest(f"{method}:{path}")
        answer = digest(f"{ha1}:{self.nonce}:{nc}:{self.cnonce}:auth:{ha2}")
        authorization = (f'Digest username="installer", realm="DM-BRIDGE-USB", nonce="{self.nonce}", '
                         f'uri="{path}", response="{answer}", algorithm=SHA-256, qop=auth, nc={nc}, cnonce="{self.cnonce}"')
        headers = {"Authorization": authorization, "Content-Type": "application/octet-stream", "Content-Length": str(len(body))}
        if csrf:
            if not HEX32.fullmatch(csrf):
                raise OtaError("Invalid CSRF token")
            headers.update(Origin=self.origin, **{"X-CSRF-Token": csrf})
        if token:
            if not HEX32.fullmatch(token):
                raise OtaError("Invalid OTA token")
            headers["X-OTA-Token"] = token
        return self.raw(method, path, body, headers)

    def status(self) -> dict:
        result = self.signed("GET", "/api/v1/ota/status")
        if result.get("deviceId") != self.device or result.get("boardId") != BOARD or result.get("txBlocked") is not True:
            raise OtaError("Device identity or TX safety state mismatch")
        return result


def write_state(path: Path, state: dict) -> None:
    # Private atomic metadata, including the ephemeral transaction for restart reconciliation.
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix="ota-state-", delete=False) as handle:
        temporary = Path(handle.name)
        handle.write((json.dumps(state) + "\n").encode()); handle.flush(); os.fsync(handle.fileno())
    os.replace(temporary, path)


def reconcile(client: Client, metadata: dict, token: str, seconds: float = 45) -> bool:
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            state = client.status()
            if state.get("buildId") == metadata["buildId"] and state.get("otaVersion") == metadata["version"] and state.get("otaKeyId") == metadata["keyId"] and state.get("otaTransaction") == token:
                if state.get("otaBootState") == "VALID":
                    return True
                if state.get("otaBootState") == "PENDING_VERIFY" and state.get("otaHealthy") is True:
                    client.signed("POST", "/api/v1/ota/confirm", csrf=state["csrfToken"], token=token)
                    # Observe the committed state on another authenticated request.
            elif state.get("otaBootState") == "ROLLED_BACK_OR_INTERRUPTED" and state.get("otaTransaction") == token:
                return False
        except (OSError, http.client.HTTPException, OtaError, ValueError, KeyError):
            pass  # Read/confirmation reconciliation only; NEVER re-upload here.
        time.sleep(1)
    return False


def deploy(client: Client, directory: Path, state_dir: Path) -> dict:
    metadata, envelope, image = load_release(directory)
    state_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
    if state_dir.is_symlink() or state_dir.stat().st_mode & 0o077:
        raise OtaError("Deployment state directory must be owner-only")
    path = state_dir / (client.device + ".json")
    lock = state_dir / (client.device + ".lock")
    fd = os.open(lock, os.O_RDWR | os.O_CREAT | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, "a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise OtaError("Another deployment owns this device") from exc
        prior = json.loads(private_file(path)) if path.exists() else {}
        if prior.get("status") in ("FAILED", "UNKNOWN"):
            raise OtaError("Device deployment is quarantined; review recovery before a new attempt")
        if prior.get("status") == "IN_PROGRESS":
            if prior.get("sha256") != metadata["sha256"] or not prior.get("transaction"):
                raise OtaError("Interrupted deployment requires explicit reconciliation; no new upload")
            success = reconcile(client, metadata, prior["transaction"])
            prior["status"] = "SUCCESS" if success else "FAILED"
            write_state(path, prior)
            if not success:
                raise OtaError("Prior deployment was not confirmed; quarantined")
            return {"status": "SUCCESS", "buildId": metadata["buildId"]}
        state = client.status()  # Offline preflight makes no persistent deployment intent.
        if state.get("otaSupported") is not True or state.get("otaHealthy") is not True or state.get("otaKeyId") != metadata["keyId"]:
            raise OtaError("OTA readiness or signing-key identity mismatch")
        if state.get("buildId") == metadata["buildId"] and state.get("otaVersion") == metadata["version"] and state.get("otaBootState") in ("VALID", "USB_BASELINE"):
            return {"status": "ALREADY_CURRENT", "buildId": metadata["buildId"]}
        if not isinstance(state.get("otaVersion"), int) or state["otaVersion"] >= metadata["version"] or state.get("configSchema", 0) < metadata["minSchema"]:
            raise OtaError("Downgrade/schema policy rejected")
        if state.get("otaPhase") not in ("IDLE", "FAILED") or state.get("otaBootState") == "PENDING_VERIFY":
            raise OtaError("Updater is busy or awaiting confirmation")
        record = dict(status="IN_PROGRESS", deviceId=client.device, buildId=metadata["buildId"], sha256=metadata["sha256"])
        write_state(path, record)  # Persist intent BEFORE the first mutation.
        try:
            prepared = client.signed("POST", "/api/v1/ota/prepare", envelope, state["csrfToken"])
            token = prepared.get("uploadToken", "")
            if not HEX32.fullmatch(token):
                raise OtaError("Missing upload token")
            record["transaction"] = token; write_state(path, record)
            try:
                client.signed("POST", "/api/v1/ota/upload", image, state["csrfToken"], token)
            except (OSError, http.client.HTTPException, OtaError):
                pass  # Unknown response outcome: reconcile, never resend the image.
            if not reconcile(client, metadata, token):
                raise OtaError("Candidate was not confirmed")
        except (OSError, http.client.HTTPException, OtaError, ValueError, KeyError):
            record["status"] = "FAILED"; write_state(path, record)
            raise OtaError("Deployment unconfirmed and quarantined; image was not automatically retried") from None
        record["status"] = "SUCCESS"; write_state(path, record)
        return {"status": "SUCCESS", "buildId": metadata["buildId"], "sha256": metadata["sha256"]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    new = sub.add_parser("init-key"); new.add_argument("directory", type=Path)
    header = sub.add_parser("identity-header"); header.add_argument("--key", type=Path, required=True); header.add_argument("--output", type=Path, required=True)
    pack = sub.add_parser("package")
    pack.add_argument("--image", type=Path, required=True); pack.add_argument("--key", type=Path, required=True)
    pack.add_argument("--version", type=int, required=True); pack.add_argument("--build-id", required=True); pack.add_argument("--output", type=Path, required=True)
    check = sub.add_parser("verify"); check.add_argument("release", type=Path)
    for name in ("status", "deploy", "watch"):
        command = sub.add_parser(name); command.add_argument("--url", required=True); command.add_argument("--device-id", required=True)
        command.add_argument("--credential-file", type=Path)
        if name != "status":
            command.add_argument("--release", type=Path, required=True)
            command.add_argument("--state-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "init-key": result = init_key(args.directory)
        elif args.command == "identity-header": result = identity_header(args.key, args.output)
        elif args.command == "package": result = package(args.image, args.key, args.version, args.build_id, args.output)
        elif args.command == "verify": result = load_release(args.release)[0]
        else:
            key = private_file(args.credential_file).decode("ascii").strip() if args.credential_file else getpass.getpass("Device installation key (not echoed): ")
            client = Client(args.url, key, args.device_id)
            if args.command == "status":
                state = client.status()
                result = {name: state.get(name) for name in ("deviceId", "buildId", "connected", "otaSupported", "otaBootState", "otaPhase", "otaHealthy", "otaReason")}
            elif args.command == "deploy": result = deploy(client, args.release, args.state_dir)
            else:
                print("Watching the explicitly selected local dev release; Ctrl-C stops. No service installed.", flush=True)
                while True:
                    try:
                        result = deploy(client, args.release, args.state_dir)
                        if result["status"] != "ALREADY_CURRENT": print(json.dumps(result), flush=True)
                    except (OSError, http.client.HTTPException):
                        pass  # Only offline preflight errors escape deploy's quarantine guard.
                    time.sleep(15)
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, OtaError, ValueError, KeyError, subprocess.SubprocessError):
        print("OTA command failed. Inspect safe state metadata; do not blindly repeat a mutation.")
        return 1
    except KeyboardInterrupt:
        print("Stopped. Reconcile any in-progress deployment before uploading again.")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
