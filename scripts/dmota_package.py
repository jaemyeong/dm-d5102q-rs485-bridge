"""DMOTA2 single-file packaging; local signing only, no device/network operations."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import tempfile

import ota_controller as legacy

MAGIC = b"DMOTA2\r\n"
MANIFEST = struct.Struct("<8sIII16s48s32sI8s")
HEADER_BYTES = MANIFEST.size + 64
PROTOCOL = 2
UPDATER_TAG = b"DMOTA-UPDATER:2\0"


def manifest_bytes(version: int, build: str, image: bytes, schema: int = 1) -> bytes:
    v1 = legacy.manifest_bytes(version, build, image, schema)
    if UPDATER_TAG not in image:
        raise legacy.OtaError("Image lacks the compiled DMOTA2 updater identity")
    return MAGIC + v1[8:] + struct.pack("<I8s", PROTOCOL, b"stable")


def unpack_manifest(payload: bytes) -> dict:
    if len(payload) != MANIFEST.size or payload[:8] != MAGIC:
        raise legacy.OtaError("DMOTA2 manifest length/domain mismatch")
    metadata = legacy.unpack_manifest(legacy.MAGIC + payload[8:116])
    protocol, channel = struct.unpack("<I8s", payload[116:])
    if protocol != PROTOCOL or channel != b"stable\0\0":
        raise legacy.OtaError("Unsupported signed updater protocol/channel")
    metadata.update(minUpdater=protocol, channel="stable", format="DMOTA2", headerBytes=HEADER_BYTES)
    return metadata


def verify_header(pub: bytes, header: bytes) -> dict:
    if len(pub) != 32 or len(header) != HEADER_BYTES:
        raise legacy.OtaError("DMOTA2 signature header length mismatch")
    metadata = unpack_manifest(header[:MANIFEST.size])
    with tempfile.TemporaryDirectory(prefix="dmota2-verify-") as folder:
        directory = Path(folder)
        for name, data in (("public.der", legacy.PUB_DER_PREFIX + pub),
                           ("manifest", header[:MANIFEST.size]), ("signature", header[MANIFEST.size:])):
            legacy.exclusive(directory / name, data)
        legacy.openssl("pkeyutl", "-verify", "-rawin", "-pubin", "-keyform", "DER",
                       "-inkey", str(directory / "public.der"), "-in", str(directory / "manifest"),
                       "-sigfile", str(directory / "signature"))
    metadata.update(keyId=hashlib.sha256(pub).hexdigest())
    return metadata


def verify(path: Path, pub: bytes) -> dict:
    # A .dmota never supplies its own trust anchor. The caller pins the public key.
    with path.open("rb") as handle:
        data = handle.read(legacy.MAX_IMAGE + HEADER_BYTES + 1)
    if len(data) > legacy.MAX_IMAGE + HEADER_BYTES:
        raise legacy.OtaError("DMOTA2 file exceeds the application ceiling")
    header, image = data[:HEADER_BYTES], data[HEADER_BYTES:]
    metadata = verify_header(pub, header)
    if pub not in image or manifest_bytes(metadata["version"], metadata["buildId"], image,
                                          metadata["minSchema"]) != header[:MANIFEST.size]:
        raise legacy.OtaError("DMOTA2 application/hash/identity mismatch")
    metadata.update(packageSha256=hashlib.sha256(data).hexdigest(), packageSize=len(data))
    return metadata


def package(image_path: Path, key: Path, version: int, build: str, output: Path) -> dict:
    with image_path.open("rb") as handle:
        image = handle.read(legacy.MAX_IMAGE + 1)
    payload = manifest_bytes(version, build, image)
    pub = legacy.public_key(key)
    if pub not in image:
        raise legacy.OtaError("Image does not contain the signing key's public verification key")
    header = payload + legacy.signature(key, payload)
    verify_header(pub, header)
    output.parent.mkdir(parents=True, exist_ok=True)
    legacy.exclusive(output, header + image)  # Never replace an existing release artifact.
    return verify(output, pub)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    pack = commands.add_parser("package")
    for name in ("image", "key", "output"):
        pack.add_argument("--" + name, type=Path, required=True)
    pack.add_argument("--version", type=int, required=True)
    pack.add_argument("--build", required=True)
    check = commands.add_parser("verify")
    check.add_argument("file", type=Path)
    check.add_argument("--public-key", required=True, help="Pinned 32-byte public key in hex; not a private key")
    args = parser.parse_args()
    try:
        result = (package(args.image, args.key, args.version, args.build, args.output)
                  if args.command == "package" else verify(args.file, bytes.fromhex(args.public_key)))
    except (legacy.OtaError, OSError, ValueError):
        parser.exit(1, "DMOTA2 operation rejected; no secret/raw diagnostic printed.\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
