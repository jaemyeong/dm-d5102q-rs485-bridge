"""USB-independent DMOTA2 preflight and one-shot transfer on a trusted LAN/VPN.

No serial enumeration, mDNS requirement, external confirm, or mutation retries.
An unresolved per-device fence blocks every subsequent deployment.
"""
import argparse
import hashlib
import http.client
import json
import os
from pathlib import Path
import re
import stat
import time

import dmota_package as package
import ota_controller as ota

SAFE = ("deviceId", "boardId", "buildId", "otaVersion", "otaKeyId",
        "configSchema", "configRevision", "otaBootState", "otaPhase",
        "otaHealthy", "txBlocked", "githubAutomatic", "connected", "mode")
READ_ERRORS = (OSError, http.client.HTTPException, ota.OtaError, ValueError, KeyError)


def require(condition, message):
    if not condition:
        raise ota.OtaError(message)


def credential(path):
    matches = re.findall(r"^Installation key: ([A-HJ-NP-Z2-9]{20})$",
                         ota.private_file(path).decode("ascii"), re.MULTILINE)
    require(len(matches) == 1, "Expected one installation key in private credentials file")
    return matches[0]


def ready(state, profile):
    for field, expected in profile.items():
        require(type(state.get(field)) is type(expected) and state[field] == expected,
                "Target/profile mismatch: " + field)
    required = dict(boardId=ota.BOARD, otaProtocol=2, connected=True, mode="STA",
                    txBlocked=True, otaHealthy=True, otaSupported=True,
                    otaBootState="VALID", otaPhase="IDLE", otaReason="NONE",
                    githubAutomatic=False, githubState="AUTOMATIC_DISABLED")
    for field, expected in required.items():
        require(type(state.get(field)) is type(expected) and state[field] == expected,
                "Target not ready: " + field)
    require(state.get("otaAttemptVersion") == state["otaVersion"], "Unresolved OTA attempt")
    return {field: state[field] for field in SAFE if field in state}


def candidate(path, public_key, profile):
    metadata = package.verify(path, public_key)
    with path.open("rb") as handle:
        data = handle.read(ota.MAX_IMAGE + package.HEADER_BYTES + 1)
    require(hashlib.sha256(data).hexdigest() == metadata["packageSha256"],
            "Package changed after signature verification")
    require(metadata["keyId"] == profile["otaKeyId"] and
            metadata["boardId"] == ota.BOARD and
            metadata["version"] > profile["otaVersion"] and
            metadata["minSchema"] <= profile["configSchema"], "Candidate/profile mismatch")
    return metadata, data


def fence_path(directory, device):
    require(re.fullmatch(r"dm-bridge-[a-f0-9]{6}", device) is not None, "Invalid device ID")
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    info = directory.lstat()
    require(stat.S_ISDIR(info.st_mode) and not info.st_mode & 0o077,
            "State directory must be owner-only and not a symlink")
    return directory / (device + ".attempt.json")


def reserve(path, record):
    # O_EXCL serializes this tool's deployments even across different candidates.
    # Keep the fence on every uncertain/failing outcome; never automatically clear.
    ota.exclusive(path, (json.dumps(record) + "\n").encode())
    fd = os.open(path.parent, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def reconcile(client, record, seconds=90):
    expected = dict(record["profile"], otaVersion=record["candidate"]["version"],
                    buildId=record["candidate"]["buildId"])
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        try:
            state = client.status()
        except READ_ERRORS:
            time.sleep(1)
            continue
        if state.get("otaVersion") == expected["otaVersion"]:
            require(state.get("otaTransaction") == record["transaction"], "Transaction mismatch")
            if state.get("otaBootState") == "VALID":
                return ready(state, expected)
        time.sleep(1)
    raise ota.OtaError("Outcome unconfirmed; use reconcile, never repeat upload")


def deploy(client, profile, metadata, data, fence):
    require(hashlib.sha256(data).hexdigest() == metadata["packageSha256"], "Package digest mismatch")
    before = client.status()
    ready(before, profile)
    record = dict(profile=profile, candidate=metadata, origin=client.origin, phase="RESERVED")
    reserve(fence, record)
    # Recheck after reserving; any failure deliberately leaves the durable fence.
    before = client.status()
    ready(before, profile)
    prepared = client.signed("POST", "/api/v1/ota/file/prepare", data[:192], csrf=before["csrfToken"])
    token = prepared.get("uploadToken", "")
    require(isinstance(token, str) and ota.HEX32.fullmatch(token), "Invalid upload token")
    record.update(transaction=token, phase="UPLOAD_MAY_HAVE_STARTED")
    ota.write_state(fence, record)
    try:
        client.signed("POST", "/api/v1/ota/upload", data[192:], csrf=before["csrfToken"], token=token)
    except READ_ERRORS:
        pass  # Lost upload response is not permission to retransmit.
    result = reconcile(client, record)
    record["phase"] = "VALID"
    ota.write_state(fence, record)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("preflight", "deploy", "reconcile"))
    parser.add_argument("--url", required=True, help="Exact HTTP origin, reachable over trusted LAN/VPN")
    parser.add_argument("--device", required=True)
    parser.add_argument("--credentials", type=Path, required=True)
    parser.add_argument("--public-key", required=True, help="Pinned Ed25519 public key hex")
    parser.add_argument("--current-version", type=int, required=True)
    parser.add_argument("--current-build", required=True)
    parser.add_argument("--config-schema", type=int, default=1)
    parser.add_argument("--config-revision", type=int, required=True)
    parser.add_argument("--package", type=Path)
    parser.add_argument("--state-dir", type=Path, default=ota.ROOT / ".arduino/evidence/hardware/private/dmota-remote")
    args = parser.parse_args()
    try:
        pub = bytes.fromhex(args.public_key)
        require(len(pub) == 32, "Expected 32-byte public key")
        profile = dict(deviceId=args.device, otaKeyId=hashlib.sha256(pub).hexdigest(),
                       otaVersion=args.current_version, buildId=args.current_build,
                       configSchema=args.config_schema, configRevision=args.config_revision)
        client = ota.Client(args.url, credential(args.credentials), args.device)
        if args.command == "preflight":
            result = ready(client.status(), profile)
            if args.package:
                result["candidate"] = candidate(args.package, pub, profile)[0]
        elif args.command == "deploy":
            require(args.package is not None, "Deploy requires a signed .dmota package")
            metadata, data = candidate(args.package, pub, profile)
            result = deploy(client, profile, metadata, data, fence_path(args.state_dir, args.device))
        else:
            fence = fence_path(args.state_dir, args.device)
            record = json.loads(ota.private_file(fence))
            require(record["profile"] == profile and record["origin"] == client.origin,
                    "Fence/profile/origin mismatch")
            require(bool(record.get("transaction")), "Prepare outcome unknown; inspect manually, no retry")
            result = reconcile(client, record)
            record["phase"] = "VALID"
            ota.write_state(fence, record)
        print(json.dumps(result, indent=2))
    except (ota.OtaError, OSError, ValueError, KeyError, TypeError, http.client.HTTPException):
        parser.exit(1, "Remote OTA stopped. No mutation was retried; inspect the private fence and use read-only preflight/reconcile. No secret/raw response printed.\n")


if __name__ == "__main__":
    main()
