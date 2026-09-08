"""One approved C008 USB upload, preserving the port for a private first boot.

Default is host-only preflight. --upload requires explicit user approval outside
this script. No firmware recompilation, full erase, key display, or network setup.
Run with .platformio-venv/bin/python; vendor esptool is pinned to 4.11.0.
"""
from __future__ import annotations

import argparse
from contextlib import ExitStack, redirect_stdout, redirect_stderr
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import site
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
TARGET = (0x0403, 0x6001, "2956578B12")
EXPECTED_MAC = "14:2b:2f:a1:10:88"
FLASH_SIZE = 4 * 1024 * 1024
BUNDLE = ROOT / ".arduino/build/usb-bootstrap-0.1.0-1ced44aa4041"
PRIVATE_ROOT = ROOT / ".arduino/evidence/hardware/private"
SEGMENTS = (
    (0x1000, BUNDLE / "bootloader.bin", 17536, "3d234a7471f67b013686dabd4dee7c1fa915c9928463616a94bc9297acf1abf8"),
    (0x8000, BUNDLE / "partitions.bin", 3072, "fb9da29aa7639cfae2ffb31c94bdff9a4d99eb23bc0813d72fed72e7f4f3ae04"),
    (0xe000, ROOT / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin", 8192,
     "f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0"),
    (0x10000, BUNDLE / "firmware.bin", 763280, "1ced44aa40418d1e8f7d304d753b90586b2b2b52d7612d894181713548bee4fe"),
)
KEY_RE = re.compile(r"INSTALL_KEY_ONCE=([ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{20})")


def timestamp():
    return datetime.now(timezone.utc).isoformat()


def emit(stage, **fields):
    print(json.dumps({"time": timestamp(), "stage": stage, **fields}), flush=True)


def identify(ports):
    matches = [p for p in ports if (p.vid, p.pid, p.serial_number) == TARGET]
    if len(matches) != 1:
        raise RuntimeError("Exactly one approved FTDI VID/PID/serial must be enumerated")
    if not matches[0].device.startswith("/dev/cu.usbserial-"):
        raise RuntimeError("Unexpected target path; review before opening")
    return matches[0].device


def check_segments():
    previous_end = 0
    for offset, path, size, digest in SEGMENTS:
        if path.is_symlink():
            raise RuntimeError("Candidate segment must not be a symlink")
        data = path.read_bytes()
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise RuntimeError(f"Approved segment identity changed: {path.name}")
        erase_end = (offset + size + 4095) & ~4095
        if offset < previous_end or erase_end > FLASH_SIZE:
            raise RuntimeError("Overlapping or out-of-flash segment")
        for start, end in ((0x9000, 0xe000), (0x310000, 0x315000)):
            if offset < end and erase_end > start:
                raise RuntimeError("Upload would overwrite a preserved NVS region")
        previous_end = erase_end
    backup = ROOT / ".arduino/evidence/deployment/atom-lite-pre-smoke-4mb.bin"
    if backup.stat().st_mode & 0o077 or hashlib.sha256(backup.read_bytes()).hexdigest() != \
            "3f2a06f6ecd7d8bf358201923d00b4f10bf5ad847cb66a1473d220b787e13c8c":
        raise RuntimeError("Historical rollback backup identity/privacy check failed")


def private_file(path, binary=False):
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    return os.fdopen(descriptor, "wb" if binary else "w")


def classify_boot_line(raw):
    text = raw.decode("utf-8", errors="replace").rstrip("\r\n")
    # Redact even malformed secret handoffs; never fall back to printing them.
    if "INSTALL_KEY_ONCE" in text:
        match = KEY_RE.fullmatch(text)
        return "INSTALL_KEY_ONCE=[REDACTED]", match.group(1) if match else None
    return text, None


def capture_boot(port, esp, directory, report, seconds=45):
    started = time.monotonic()
    buffered = bytearray()
    count = 0
    key = None
    ap_ssid = None
    ap_url = None
    errors = []
    with private_file(directory / "first-boot.raw", binary=True) as raw, \
            private_file(directory / "first-boot.redacted.log") as redacted:
        port.timeout = 0.2
        port.baudrate = 115200
        port.reset_input_buffer()  # Only flasher residue, before application reset.
        esp.hard_reset()
        emit("first-boot-capture-started", seconds=seconds)
        while time.monotonic() - started < seconds:
            data = port.read(min(max(port.in_waiting, 1), 4096))
            if not data:
                continue
            count += len(data)
            if count > 1024 * 1024:
                raise RuntimeError("First-boot capture exceeded its fixed byte limit")
            raw.write(data)
            raw.flush()
            buffered.extend(data)
            if len(buffered) > 8192:
                raise RuntimeError("First-boot line exceeds maximum length; raw remains private")
            while b"\n" in buffered:
                line, _, rest = buffered.partition(b"\n")
                buffered = bytearray(rest)
                safe, candidate = classify_boot_line(line)
                if candidate:
                    if key and candidate != key:
                        raise RuntimeError("Multiple different installation keys in one boot")
                    if not key:
                        key = candidate
                        with private_file(directory / "credentials.txt") as credentials:
                            credentials.write("ATOM Lite B0 installation key (private)\nUsername: installer\nInstallation key: " + key + "\n")
                            credentials.flush(); os.fsync(credentials.fileno())
                        emit("installation-key-captured", secretPrinted=False)
                redacted.write(timestamp() + " " + safe + "\n")
                redacted.flush()
                if safe.startswith("AP_SSID="):
                    ap_ssid = safe.split("=", 1)[1]
                    emit("ap-announced", ssid=ap_ssid)
                elif safe.startswith("AP_URL="):
                    ap_url = safe.split("=", 1)[1]
                    emit("ap-url-announced", url=ap_url)
                elif any(marker in safe for marker in ("Guru Meditation", "Brownout", "CORRUPT", "_FAILED", "panic", "assert failed")):
                    errors.append("boot-error-marker")
                    emit("boot-error-marker", detailInPrivateLog=True)
        os.fsync(raw.fileno())
    report["firstBoot"] = {"captureSeconds": seconds, "bytes": count,
        "installationKeyCaptured": key is not None, "apSsid": ap_ssid,
        "apUrl": ap_url, "errorMarkers": len(errors), "browserVerified": False,
        "staVerified": False}
    if not key or not ap_ssid or not ap_url or errors:
        raise RuntimeError("First-boot acceptance incomplete; preserve state, inspect private capture")


def load_vendor_tools():
    vendor = ROOT / ".platformio/packages/tool-esptoolpy"
    # Match the pinned package's esptool.py launcher; dependencies are bundled.
    site.addsitedir(str(vendor / "_contrib"))
    sys.path.insert(0, str(vendor / "_contrib"))
    sys.path.insert(0, str(vendor))
    import serial
    import esptool
    if esptool.__version__ != "4.11.0" or serial.__version__ != "3.5":
        raise RuntimeError("Upload tool versions differ from the reviewed environment")
    return esptool


def upload(*, segments=None, preflight=None, boot_capture=None, validate_current=None,
           evidence_prefix="b0-usb-20260908-"):
    # Explicit injection lets the reviewed upgrade preserve this first-install
    # entry point's pinned image and first-key acceptance defaults unchanged.
    segments = SEGMENTS if segments is None else segments
    preflight = check_segments if preflight is None else preflight
    boot_capture = capture_boot if boot_capture is None else boot_capture
    esptool = load_vendor_tools()
    from serial.tools import list_ports
    from esptool import cmds
    preflight()
    port_name = identify(list_ports.comports())
    busy = subprocess.run(["lsof", port_name], capture_output=True, text=True, timeout=10)
    if busy.returncode != 1:
        raise RuntimeError("Port is busy or ownership could not be checked")
    if PRIVATE_ROOT.is_symlink() or PRIVATE_ROOT.stat().st_mode & 0o077:
        raise RuntimeError("Private evidence root must be a private real directory")
    directory = Path(tempfile.mkdtemp(prefix=evidence_prefix, dir=PRIVATE_ROOT))
    report = {"schemaVersion": 1, "started": timestamp(), "port": port_name,
        "vid": TARGET[0], "pid": TARGET[1], "serial": TARGET[2],
        "applicationSha256": segments[-1][3], "baud": 115200,
        "status": "started", "writeStarted": False, "uploadVerified": False,
        "evidenceDirectory": directory.relative_to(ROOT).as_posix()}
    emit("usb-session-starting", port=port_name, evidenceDirectory=report["evidenceDirectory"])
    esp = None
    try:
        # Re-enumerate immediately before first hardware access.
        if identify(list_ports.comports()) != port_name:
            raise RuntimeError("Device path changed before connect")
        with private_file(directory / "uploader.log") as log, redirect_stdout(log), redirect_stderr(log):
            esp = esptool.get_default_connected_device([port_name], port_name,
                connect_attempts=3, initial_baud=115200, chip="esp32", before="default_reset")
            description = esp.get_chip_description()
            mac = ":".join(f"{byte:02x}" for byte in esp.read_mac())
            if not description.startswith("ESP32-PICO-D4") or esp.get_chip_revision() != 101 or mac != EXPECTED_MAC:
                raise RuntimeError("Connected chip/revision/MAC does not match the known C008")
            if esp.get_secure_boot_enabled() or esp.get_flash_encryption_enabled():
                raise RuntimeError("Security fuses do not permit this reviewed plaintext workflow")
            esp = esp.run_stub()
            flash_id = esp.flash_id()
            if cmds.DETECTED_FLASH_SIZES.get(flash_id >> 16) != "4MB":
                raise RuntimeError("Target does not report the expected 4 MiB flash")
            esp.flash_set_parameters(FLASH_SIZE)
            report["chip"] = {"description": description, "revision": 101,
                "mac": mac, "flashId": flash_id, "flashBytes": FLASH_SIZE}
        emit("chip-verified", **report["chip"])
        last_progress = -1

        def progress(done, total):
            nonlocal last_progress
            step = done // (256 * 1024)
            if step != last_progress:
                last_progress = step
                emit("backup-progress", completedBytes=done, totalBytes=total)

        with private_file(directory / "pre-upload-4mb.bin", binary=True) as backup:
            current = esp.read_flash(0, FLASH_SIZE, progress)
            if len(current) != FLASH_SIZE or esp.flash_md5sum(0, FLASH_SIZE) != hashlib.md5(current).hexdigest():
                raise RuntimeError("Current full-flash backup failed device digest verification")
            backup.write(current); backup.flush(); os.fsync(backup.fileno())
        report["backup"] = {"bytes": len(current), "sha256": hashlib.sha256(current).hexdigest(), "deviceDigestMatched": True}
        emit("backup-verified", **report["backup"])
        if validate_current is not None:
            validate_current(current)
            report["previousImageVerified"] = True
        # Fresh identity and all four file hashes again immediately before writes.
        if identify(list_ports.comports()) != port_name:
            raise RuntimeError("Device identity changed before write")
        preflight()
        with ExitStack() as files:
            pairs = [(offset, files.enter_context(path.open("rb"))) for offset, path, _, _ in segments]
            args = argparse.Namespace(addr_filename=pairs, encrypt=False, encrypt_files=None,
                erase_all=False, force=False, ignore_flash_encryption_efuse_setting=False,
                no_stub=False, compress=True, no_compress=False, flash_mode="keep",
                flash_freq="keep", flash_size="keep", diff="no")
            emit("flash-write-starting", segments=[{"offset": hex(offset), "bytes": size} for offset, _, size, _ in segments])
            report["writeStarted"] = True
            # Vendor command does not reset the chip; keep the same port open.
            with (directory / "uploader.log").open("a") as log, redirect_stdout(log), redirect_stderr(log):
                cmds.write_flash(esp, args)
                for _, handle in pairs:
                    handle.seek(0)
                cmds.verify_flash(esp, args)
        for start, size in ((0x9000, 0x5000), (0x310000, 0x5000)):
            if esp.flash_md5sum(start, size) != hashlib.md5(current[start:start+size]).hexdigest():
                raise RuntimeError("Preserved NVS region changed during flashing")
        report["uploadVerified"] = True
        report["nvsPreservedBeforeBoot"] = True
        emit("flash-verified", applicationSha256=segments[-1][3], nvsPreserved=True)
        boot_capture(esp._port, esp, directory, report)
        report["status"] = "upload-and-first-boot-passed"
    except Exception as error:
        report["status"] = "stopped"
        report["error"] = str(error)
        # Never auto-erase/restore/retry another image after a physical failure.
        raise
    finally:
        if esp is not None:
            esp._port.close()
        report["finished"] = timestamp()
        with private_file(directory / "report.json") as output:
            json.dump(report, output, indent=2); output.write("\n")
        emit("usb-session-finished", status=report["status"],
             uploadVerified=report["uploadVerified"], evidenceDirectory=report["evidenceDirectory"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upload", action="store_true", help="Execute only with existing image-specific user approval")
    args = parser.parse_args()
    if args.upload:
        upload()
    else:
        load_vendor_tools()  # Prove imports and pinned versions without opening USB.
        from serial.tools import list_ports
        check_segments()
        emit("host-preflight-pass", port=identify(list_ports.comports()), noPortOpened=True,
             applicationSha256=SEGMENTS[-1][3])


if __name__ == "__main__":
    main()
