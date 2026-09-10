"""Validate the additive USB-only execution amendment, not physical readiness."""

from __future__ import annotations

import hashlib
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = "firmware/usb_bootstrap"
AMENDMENT_PATH = "docs/design/USB-BOOTSTRAP-AMENDMENT-2026-09-08.md"
AMENDMENT_SHA256 = "771ddf15b9f78c4136fceb45a8d667fe2315b74b2c3b5546b54fbdc936f025db"
WEB_AMENDMENT_PATH = "docs/design/WEB-LOGIN-AP-AMENDMENT-2026-09-09.md"
WEB_AMENDMENT_SHA256 = "85c8dec92ea87bab5015dc3045e87658f2edfaa58a1a023cb74c9289a542d0e0"
VENDOR_SHA256 = "4de27631e00b4708d207819cc9630e18313ceac8348fb36ed5f374903e545770"
BOOT_AMENDMENT_PATH = "docs/design/BOOT-RESET-MDNS-AMENDMENT-2026-09-09.md"
BOOT_AMENDMENT_SHA256 = "459358390c8e52e3350e13927746bbbfc381f1e74e508cdaed8eaf91171b964e"
BUTTON_SHA256 = "cdf2a70e897a3e67edfcad491a3c1619f2b57b667a5e7b621d7380512f7aca5d"
SOURCE_FILES = {"config.h", "core.h", "core.cpp", "main.cpp", "web_ui.h", "web_sha256.inc", "web_client.inc", "boot_button.h", "ota_core.h", "ota_core.cpp", "ota_runtime.h", "ota_context.h", "github_core.h", "github_core.cpp", "github_pull.h", "github_worker.h", "automatic_policy.h"}
OTA_VENDOR_HASHES = {
    "monocypher.c": "f1f838cdd483bdebe0df0ff5c5ed60535e496f769c6a2f933ac4c0b114207123",
    "monocypher.h": "fcaf6ed771358bb4f40fba016f6518ae86ec02b1b877d2cc35ad92d3a26fd7b3",
    "monocypher-ed25519.c": "ce0d2f8e32ca8f66398ba5b3456cc74327c3eff14e7b950ce7d57be9025cc453",
    "monocypher-ed25519.h": "3a3035181f991a158d0e1c7567258f0bae8ba0f1f23c5512b4a1db1b3c9730ce",
    "LICENCE.md": "a5781770269d2516e52ba4863f790c10a16da4089a1e81823aee19ff1e9026b0",
}


def validate_usb_bootstrap(root: Path = ROOT) -> None:
    amendment = root / AMENDMENT_PATH
    if not amendment.is_file() or hashlib.sha256(amendment.read_bytes()).hexdigest() != AMENDMENT_SHA256:
        raise ValueError("USB bootstrap requires the unchanged approved execution amendment")
    web_amendment = root / WEB_AMENDMENT_PATH
    if not web_amendment.is_file() or hashlib.sha256(web_amendment.read_bytes()).hexdigest() != WEB_AMENDMENT_SHA256:
        raise ValueError("USB bootstrap requires the unchanged approved web/AP amendment")
    directory = root / SOURCE_DIR
    boot_amendment = root / BOOT_AMENDMENT_PATH
    if not boot_amendment.is_file() or hashlib.sha256(boot_amendment.read_bytes()).hexdigest() != BOOT_AMENDMENT_SHA256:
        raise ValueError("USB bootstrap requires the unchanged approved boot reset/mDNS amendment")
    if not directory.is_dir() or {p.name for p in directory.iterdir() if p.is_file()} != SOURCE_FILES:
        raise ValueError("USB bootstrap source inventory changed; review the standalone boundary")
    if hashlib.sha256((directory / "web_sha256.inc").read_bytes()).hexdigest() != VENDOR_SHA256:
        raise ValueError("Pinned js-sha256 browser source/license changed; review the dependency")
    vendor = directory / "vendor/monocypher"
    if not vendor.is_dir() or {p.name for p in vendor.iterdir()} != set(OTA_VENDOR_HASHES):
        raise ValueError("Pinned OTA dependency inventory changed")
    for name, expected in OTA_VENDOR_HASHES.items():
        if hashlib.sha256((vendor / name).read_bytes()).hexdigest() != expected:
            raise ValueError("Pinned OTA dependency source/license changed")
    forbidden = re.compile(
        r"\b(?:pinMode|digitalWrite|digitalRead|analogWrite|analogRead|delay|"
        r"gpio_\w+|uart_\w+|ledc\w*)\s*\(|"
        r"\b(?:Serial1|Serial2|HardwareSerial|ArduinoOTA|HTTPUpdate|M5Unified)\b"
    )
    if hashlib.sha256((directory / "boot_button.h").read_bytes()).hexdigest() != BUTTON_SHA256:
        raise ValueError("USB-only no-GPIO exception changed: only the pinned GPIO39 INPUT sampler is allowed")
    for name in SOURCE_FILES - {"web_ui.h", "boot_button.h"}:
        if forbidden.search((directory / name).read_text(encoding="utf-8")):
            raise ValueError(f"USB-only no-bus/no-GPIO boundary violated: {name}")
    # A static denylist is only a tripwire, not proof of target boot passivity.


if __name__ == "__main__":
    validate_usb_bootstrap()
    print("USB bootstrap amendment/source boundary: PASS (host only)")
