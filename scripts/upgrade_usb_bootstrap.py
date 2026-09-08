"""Exact 0.1.0 -> approved boot-button/mDNS 0.1.2 USB upgrade, no auto retry."""
from __future__ import annotations
import argparse
import hashlib
import ipaddress
import json
import os
import time

import upload_usb_bootstrap as base
from validate_usb_bootstrap import validate_usb_bootstrap

BUNDLE = base.ROOT / ".arduino/build/usb-bootstrap-0.1.2-46e869fcb9f1"
SEGMENTS = tuple((offset, BUNDLE / path.name, size, digest)
                 for offset, path, size, digest in base.SEGMENTS[:-1]) + (
    (0x10000, BUNDLE / "firmware.bin", 808880,
     "46e869fcb9f160326ba5920189ca811ef8516dea7f1d77b2d3377d62712a353d"),)


def validate_bundle():
    """Validate preserved 0.1.2 artifacts independently of the current checkout."""
    base.check_segments()  # Retain the historical rollback privacy/identity gate.
    validate_usb_bootstrap()
    manifest = json.loads((BUNDLE / "manifest.json").read_text())
    if manifest["buildId"] != "usb-bootstrap-0.1.2":
        raise RuntimeError("Upgrade manifest build identity changed")
    for offset, path, size, digest in SEGMENTS:
        if path.is_symlink() or len(path.read_bytes()) != size or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise RuntimeError("Pinned upgrade segment changed")
        if offset + ((size + 4095) & ~4095) > 0x190000:
            raise RuntimeError("Upgrade segment exceeds approved app0 boundary")
    if SEGMENTS[-1][2] > 1310720:
        raise RuntimeError("Upgrade image exceeds product ceiling")
    return manifest


def preflight():
    manifest = validate_bundle()
    for name, digest in manifest["sourceSha256"].items():
        if hashlib.sha256((base.ROOT / "firmware/usb_bootstrap" / name).read_bytes()).hexdigest() != digest:
            raise RuntimeError("Source drifted from built candidate")


def validate_current(current):
    if len(current) != base.FLASH_SIZE:
        raise RuntimeError("Backup is not a full flash image")
    # Only upgrade the known installed non-OTA bootstrap. Do not replace unknown
    # bootloaders/layouts/applications or assume an arbitrary OTA bank is active.
    for offset, _, size, digest in (base.SEGMENTS[0], base.SEGMENTS[1], base.SEGMENTS[-1]):
        if hashlib.sha256(current[offset:offset + size]).hexdigest() != digest:
            raise RuntimeError("Device no longer contains the known 0.1.0 baseline; stop before write")


def boot_metadata(line, state):
    """Allowlisted metadata only; raw device bytes never reach public output."""
    text, _ = base.classify_boot_line(line)
    if "INSTALL_KEY_ONCE" in text:
        state["unexpectedKeyCreation"] = True
        return "INSTALL_KEY_ONCE=[REDACTED]"
    if text == "USB_BOOTSTRAP_0.1.2 RS485_DISABLED OTA_UNAVAILABLE":
        state["buildMarkers"] += 1
        return text
    if text == "INSTALL_KEY_REUSED":
        state["keyReused"] = True
        return text
    if text == "BOOT_RESET_NOT_ARMED":
        state["resetNotArmed"] = True
        return text
    if "BOOT_RESET_ARMED" in text or "WIFI_RESET_COMPLETE" in text:
        state["unexpectedReset"] = True
        return "UNEXPECTED_RESET_MARKER"
    if text == "STA_CONNECT_ATTEMPT":
        state["staAttempted"] = True
        return text
    if text == "AP_SSID=DM-BRIDGE-8810A1":
        state["apSsid"] = "DM-BRIDGE-8810A1"
        return text
    if text == "AP_URL=http://192.168.4.1":
        state["apUrl"] = "http://192.168.4.1"
        return text
    if text.startswith("STA_IP="):
        try:
            address = ipaddress.IPv4Address(text[7:])
            if address.is_unspecified or address.is_multicast or address.is_loopback:
                raise ValueError("Invalid device IPv4")
            state["staIp"] = str(address)
            return "STA_IP=" + str(address)
        except ValueError:
            state["errors"] += 1
            return "INVALID_STA_IP"
    if text == "MDNS_URL=http://dm-bridge-8810a1.local":
        state["mdnsUrl"] = "http://dm-bridge-8810a1.local"
        return text
    # The old bootloader/partition combination already emitted this diagnostic.
    # Record it explicitly; do not call such a boot warning-free or a crash.
    if "core_dump" in text or "core dump" in text:
        state["coreDumpDiagnostics"] += 1
        return "CORE_DUMP_PARTITION_DIAGNOSTIC [details private]"
    if any(marker in text for marker in ("Guru Meditation", "Brownout", "CORRUPT", "_FAILED", "panic", "assert failed")):
        state["errors"] += 1
        return "BOOT_ERROR [details private]"
    return "UNCLASSIFIED_BOOT_LINE [details private]"


def new_boot_state():
    return dict(buildMarkers=0, keyReused=False, resetNotArmed=False,
                unexpectedKeyCreation=False, unexpectedReset=False, staAttempted=False,
                apSsid=None, apUrl=None, staIp=None, mdnsUrl=None,
                coreDumpDiagnostics=0, errors=0)


def boot_accepted(state):
    ready = (state["apSsid"] and state["apUrl"]) or state["staAttempted"]
    return bool(state["buildMarkers"] == 1 and state["keyReused"] and state["resetNotArmed"] and
                not state["unexpectedKeyCreation"] and not state["unexpectedReset"] and not state["errors"] and ready)


def capture_upgrade(port, esp, directory, report, seconds=45, metadata=None, accepted=None):
    metadata = metadata or boot_metadata
    accepted = accepted or boot_accepted
    state = new_boot_state()
    report["firstBoot"] = state
    started = time.monotonic()
    buffered = bytearray()
    count = 0
    with base.private_file(directory / "first-boot.raw", binary=True) as raw, \
            base.private_file(directory / "first-boot.redacted.log") as redacted:
        port.timeout = 0.2
        port.baudrate = 115200
        port.reset_input_buffer()
        esp.hard_reset()
        base.emit("upgrade-first-boot-capture", seconds=seconds)
        while time.monotonic() - started < seconds:
            data = port.read(min(max(port.in_waiting, 1), 4096))
            if not data:
                continue
            count += len(data)
            if count > 1024 * 1024:
                raise RuntimeError("Boot capture exceeded byte bound")
            raw.write(data); raw.flush()
            buffered.extend(data)
            if len(buffered) > 8192:
                raise RuntimeError("Boot capture exceeded line bound")
            while b"\n" in buffered:
                line, _, buffered = buffered.partition(b"\n")
                safe = metadata(line, state)
                redacted.write(base.timestamp() + " " + safe + "\n"); redacted.flush()
                if safe.startswith(("STA_IP=", "MDNS_URL=", "AP_URL=", "BOOT_ERROR", "UNEXPECTED_")):
                    base.emit("boot-metadata", message=safe)
        if buffered:
            redacted.write(base.timestamp() + " " + metadata(buffered, state) + "\n")
        os.fsync(raw.fileno()); redacted.flush(); os.fsync(redacted.fileno())
    state.update(captureSeconds=seconds, bytes=count, browserVerified=False,
                 physicalButtonVerified=False, mdnsResolutionVerified=False)
    if not accepted(state):
        raise RuntimeError("Upgrade first-boot acceptance incomplete; no automatic retry/reset/erase")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upload", action="store_true")
    args = parser.parse_args()
    if args.upload:
        base.upload(segments=SEGMENTS, preflight=preflight, boot_capture=capture_upgrade,
                    validate_current=validate_current, evidence_prefix="b0-upgrade-20260909-")
    else:
        base.load_vendor_tools()
        from serial.tools import list_ports
        preflight()
        base.emit("upgrade-host-preflight-pass", port=base.identify(list_ports.comports()),
                  noPortOpened=True, applicationSha256=SEGMENTS[-1][3])


if __name__ == "__main__":
    main()
