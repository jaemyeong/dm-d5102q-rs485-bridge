"""Reviewed 0.1.2 -> B1 USB bootstrap. Default is host-only; never auto-retry."""
from __future__ import annotations
import argparse
import hashlib
import json
import ota_controller as ota
import upgrade_usb_bootstrap as legacy
import upload_usb_bootstrap as base
from validate_usb_bootstrap import validate_usb_bootstrap

BUNDLE = base.ROOT / ".arduino/build/usb-bootstrap-0.2.0-195ad1aa6f44"
APP_SHA256 = "195ad1aa6f44222786ae749afe1ef7eabf76b461037e2d142fa1582724bfca63"
MANIFEST_SHA256 = "ff4613c69ba3c405bf73d2388cfba1a94d50c4025e3edc07e7cc8eed2c99555f"
KEY_ID = "1950c9592a94a88c2cb7184e8b2a841ae75d25003633a0c940d51b7100c73d51"
SEGMENTS = tuple((offset, BUNDLE / path.name, size, digest)
                 for offset, path, size, digest in legacy.SEGMENTS[:-1]) + (
    (0x10000, BUNDLE / "firmware.bin", 832080, APP_SHA256),)


def preflight():
    base.check_segments()
    validate_usb_bootstrap()
    content = (BUNDLE / "manifest.json").read_bytes()
    if hashlib.sha256(content).hexdigest() != MANIFEST_SHA256:
        raise RuntimeError("B1 review manifest changed")
    manifest = json.loads(content)
    metadata, _, _ = ota.load_release(BUNDLE)
    if metadata["keyId"] != KEY_ID or metadata["buildId"] != "usb-bootstrap-0.2.0" or metadata["version"] != 200:
        raise RuntimeError("B1 signed release identity changed")
    for offset, path, size, digest in SEGMENTS:
        if path.is_symlink() or path.stat().st_size != size or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError("B1 image segment changed")
        if offset + ((size + 4095) & ~4095) > 0x190000:
            raise RuntimeError("B1 segment exceeds app0 boundary")
    for name, digest in manifest["sourceSha256"].items():
        if hashlib.sha256((base.ROOT / "firmware/usb_bootstrap" / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError("Source drifted from B1 candidate")
    for path, expected in ((base.ROOT / ".arduino/tmp/ota_identity.h", manifest["identityHeaderSha256"]),
                           (base.ROOT / "platformio.ini", manifest["platformioIniSha256"]),
                           (BUNDLE / "firmware.elf", manifest["elfSha256"])):
        if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise RuntimeError("B1 build input/ELF changed")


def validate_current(current):
    if len(current) != base.FLASH_SIZE:
        raise RuntimeError("Backup is not a full flash image")
    for offset, _, size, digest in (legacy.SEGMENTS[0], legacy.SEGMENTS[1], legacy.SEGMENTS[-1]):
        if hashlib.sha256(current[offset:offset + size]).hexdigest() != digest:
            raise RuntimeError("Exact known 0.1.2 baseline is required before B1 write")


def boot_metadata(line, state):
    text, _ = base.classify_boot_line(line)
    if text == "BOOT_BUILD=usb-bootstrap-0.2.0":
        state["buildMarkers"] += 1; return text
    if text == "RS485_DISABLED SIGNED_OTA_BASELINE":
        state["otaBaseline"] = True; return text
    if text == "OTA_KEY_CONFIGURED":
        state["otaKeyConfigured"] = True; return text
    if text.startswith("OTA_BOOT_STATE="):
        state["otaBootState"] = text.split("=", 1)[1]
        if state["otaBootState"] != "USB_BASELINE": state["errors"] += 1
        return "OTA_BOOT_STATE=" + ("USB_BASELINE" if state["otaBootState"] == "USB_BASELINE" else "UNEXPECTED")
    if text == "OTA_KEY_MISSING":
        state["errors"] += 1; return text
    return legacy.boot_metadata(line, state)


def boot_accepted(state):
    return bool(legacy.boot_accepted(state) and state.get("otaBaseline") and state.get("otaKeyConfigured") and
                state.get("otaBootState") == "USB_BASELINE" and state["staIp"] and state["mdnsUrl"])


def capture(port, esp, directory, report, seconds=45):
    legacy.capture_upgrade(port, esp, directory, report, seconds, boot_metadata, boot_accepted)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upload", action="store_true")
    parser.add_argument("--approved-image-sha256")
    args = parser.parse_args()
    if args.upload:
        if args.approved_image_sha256 != APP_SHA256:
            parser.error("Explicit image-specific approval/hash is required; host preflight alone is not upload authority")
        base.upload(segments=SEGMENTS, preflight=preflight, boot_capture=capture,
                    validate_current=validate_current, evidence_prefix="b1-upgrade-20260909-")
    else:
        preflight()
        base.emit("b1-host-preflight-pass", applicationSha256=APP_SHA256,
                  noPortOpened=True, uploaded=False, targetRollbackVerified=False)


if __name__ == "__main__": main()
