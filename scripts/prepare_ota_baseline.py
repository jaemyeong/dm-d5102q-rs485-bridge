"""Seal a host-verified B1 build for review; never enumerate/open/write hardware."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import ota_controller as ota
import upload_usb_bootstrap as usb
from validate_usb_bootstrap import SOURCE_DIR, SOURCE_FILES, OTA_VENDOR_HASHES, validate_usb_bootstrap


def prepare(key: Path) -> Path:
    validate_usb_bootstrap()
    build = ota.ROOT / ".arduino/build/platformio/atom_lite"
    image = build / "firmware.bin"
    sha = hashlib.sha256(image.read_bytes()).hexdigest()
    output = ota.ROOT / ".arduino/build" / ("usb-bootstrap-0.2.0-" + sha[:12])
    nm = ota.ROOT / ".platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm"
    symbols = subprocess.run([str(nm), str(build / "firmware.elf")], capture_output=True, text=True, check=True).stdout
    if not re.search(r"^[0-9a-f]+ T verifyRollbackLater$", symbols, re.M):
        raise ota.OtaError("Strong deferred-confirmation hook not found in the exact ELF")
    sdkconfig = ota.ROOT / ".platformio/packages/framework-arduinoespressif32/tools/sdk/esp32/sdkconfig"
    if "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" not in sdkconfig.read_text():
        raise ota.OtaError("Pinned rollback SDK configuration is absent")
    # This package command verifies compiled build identity, public key and signature.
    ota.package(image, key, 200, "usb-bootstrap-0.2.0", output)
    segments = []
    for offset, old_path, size, digest in usb.SEGMENTS[:-1]:
        source = build / old_path.name if old_path.name != "boot_app0.bin" else old_path
        data = source.read_bytes()
        if len(data) != size or hashlib.sha256(data).hexdigest() != digest:
            raise ota.OtaError("Bootstrap bootloader/partition/OTA metadata differs from reviewed baseline")
        ota.exclusive(output / old_path.name, data)
        segments.append(dict(offset=offset, file=old_path.name, size=size, sha256=digest))
    segments.append(dict(offset=0x10000, file="firmware.bin", size=image.stat().st_size, sha256=sha))
    ota.exclusive(output / "firmware.elf", (build / "firmware.elf").read_bytes())
    public_header = ota.ROOT / ".arduino/tmp/ota_identity.h"
    ota.exclusive(output / "ota_identity.public.h", public_header.read_bytes())
    names = sorted(SOURCE_FILES) + ["vendor/monocypher/" + name for name in sorted(OTA_VENDOR_HASHES)]
    source_hashes = {name: hashlib.sha256((ota.ROOT / SOURCE_DIR / name).read_bytes()).hexdigest() for name in names}
    record = dict(buildId="usb-bootstrap-0.2.0", otaVersion=200, boardId=ota.BOARD,
                  proofStage="host-build-only", uploaded=False, sourceSha256=source_hashes,
                  segments=segments, identityHeaderSha256=hashlib.sha256(public_header.read_bytes()).hexdigest(),
                  platformioIniSha256=hashlib.sha256((ota.ROOT / "platformio.ini").read_bytes()).hexdigest(),
                  elfSha256=hashlib.sha256((build / "firmware.elf").read_bytes()).hexdigest(),
                  bootloaderRollbackConfig=True, deferredConfirmationSymbol="strong-T",
                  targetRollbackVerified=False, privateKeyIncluded=False)
    ota.exclusive(output / "manifest.json", (json.dumps(record, indent=2) + "\n").encode())
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--key", type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.key))
