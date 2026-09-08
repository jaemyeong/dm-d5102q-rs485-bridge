#!/usr/bin/env python3
"""Validate the fixed flash layout and measured-memory budget contract.

The layout and firmware-size checks are host proof. Runtime heap values become
evidence only when an explicit target-system sample is supplied; this script
never turns a missing measurement into a passing runtime claim.
"""

from __future__ import annotations

import argparse
import configparser
import csv
import hashlib
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.parse import unquote, urlparse

from validate_usb_bootstrap import SOURCE_DIR, validate_usb_bootstrap


ROOT = Path(__file__).resolve().parents[1]
BUDGET_PATH = ROOT / "requirements/memory-budget.json"
PARTITION_PATH = ROOT / "partitions/dmbridge_ota.csv"
PLATFORMIO_CONFIG_PATH = ROOT / "platformio.ini"
LOCAL_PLATFORMIO_CONFIG_PATH = ROOT / ".arduino/tmp/platformio-local.ini"

DESIGN_SHA256 = "d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114"
FLASH_BYTES = 4 * 1024 * 1024
PARTITION_TABLE_OFFSET_BYTES = 0x8000
PARTITION_TABLE_SIZE_BYTES = 0x1000
FIRMWARE_IMAGE_MAX_BYTES = 0x140000
APP_SLOT_BYTES = 0x180000
NVS_MIN_BYTES = 20 * 1024
OTA_DATA_BYTES = 8 * 1024
STEADY_HEAP_MIN_BYTES = 80 * 1024
LOADED_HEAP_MIN_BYTES = 50 * 1024
LARGEST_BLOCK_MIN_BYTES = 32 * 1024
MIN_UNALLOCATED_SPARE_BYTES = 0xEB000
FILESYSTEM_SUBTYPES = {"spiffs", "fat", "fatfs", "littlefs"}
PLATFORM_URL = (
    "https://github.com/platformio/platform-espressif32.git"
    "#76b0e9b942330979860e3bc7fd3b92b383d66cc1"
)
ESPTOOL_ARCHIVE_SHA256 = (
    "0de9971e2949c874387af768e71c4a71c13b63e0242ea931a34ba0a654b56382"
)
EXPECTED_PARTITIONS = {
    "nvs": ("data", "nvs", 0x9000, 0x5000, ""),
    "otadata": ("data", "ota", 0xE000, 0x2000, ""),
    "app0": ("app", "ota_0", 0x10000, 0x180000, ""),
    "app1": ("app", "ota_1", 0x190000, 0x180000, ""),
    "diag_nvs": ("data", "nvs", 0x310000, 0x5000, ""),
}


class BudgetValidationError(AssertionError):
    """Raised when a budget, layout, image, or runtime sample is invalid."""


@dataclass(frozen=True)
class Partition:
    name: str
    partition_type: str
    subtype: str
    offset: int
    size: int
    flags: str
    line_number: int

    @property
    def end(self) -> int:
        return self.offset + self.size


@dataclass(frozen=True)
class LayoutMetrics:
    flash_bytes: int
    app_slot_bytes: int
    firmware_image_max_bytes: int
    unallocated_spare_bytes: int


def fail(message: str) -> None:
    raise BudgetValidationError(message)


def load_json_object(path: Path, label: str) -> dict[str, Any]:
    if not path.is_file():
        fail(f"missing {label}: {path}")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid JSON in {label} {path}: {exc}")
    if not isinstance(value, dict):
        fail(f"{label} must contain a JSON object")
    return value


def require_object(parent: dict[str, Any], key: str) -> dict[str, Any]:
    value = parent.get(key)
    if not isinstance(value, dict):
        fail(f"{key} must be an object")
    return value


def require_exact_int(parent: dict[str, Any], key: str, expected: int) -> None:
    value = parent.get(key)
    if isinstance(value, bool) or not isinstance(value, int):
        fail(f"{key} must be an integer")
    if value != expected:
        fail(f"{key} must be {expected}, found {value}")


def parse_size(raw: str, label: str) -> int:
    value = raw.strip()
    if not value:
        fail(f"{label} is empty")
    multiplier = 1
    suffix = value[-1].upper()
    if suffix in {"K", "M"}:
        multiplier = 1024 if suffix == "K" else 1024 * 1024
        value = value[:-1]
    try:
        parsed = int(value, 0) * multiplier
    except ValueError:
        fail(f"{label} is not a valid integer size: {raw!r}")
    if parsed < 0:
        fail(f"{label} must not be negative")
    return parsed


def load_budget(path: Path = BUDGET_PATH) -> dict[str, Any]:
    return load_json_object(path, "memory budget")


def validate_budget_contract(budget: dict[str, Any]) -> None:
    require_exact_int(budget, "schemaVersion", 1)
    if budget.get("designSha256") != DESIGN_SHA256:
        fail("memory budget points to the wrong governing design hash")

    target = require_object(budget, "target")
    if target.get("boardId") != "m5stack-atom" or target.get("sku") != "C008":
        fail("memory budget target must be the M5Stack Atom Lite C008")
    require_exact_int(target, "flashBytes", FLASH_BYTES)

    flash = require_object(budget, "flash")
    if flash.get("partitionCsv") != "partitions/dmbridge_ota.csv":
        fail("flash.partitionCsv must select the repository OTA layout")
    require_exact_int(
        flash, "partitionTableOffsetBytes", PARTITION_TABLE_OFFSET_BYTES
    )
    require_exact_int(flash, "partitionTableSizeBytes", PARTITION_TABLE_SIZE_BYTES)
    require_exact_int(flash, "firmwareImageMaxBytes", FIRMWARE_IMAGE_MAX_BYTES)
    require_exact_int(
        flash, "minimumUnallocatedSpareBytes", MIN_UNALLOCATED_SPARE_BYTES
    )
    if flash.get("filesystemRequired") is not False:
        fail("normal operation must not require a filesystem")

    expected = flash.get("partitions")
    if not isinstance(expected, list) or len(expected) != 5:
        fail("flash.partitions must define exactly five partitions")
    expected_by_name = {
        item.get("name"): item for item in expected if isinstance(item, dict)
    }
    if len(expected_by_name) != len(expected):
        fail("flash.partitions contains an invalid or duplicate name")
    required_names = set(EXPECTED_PARTITIONS)
    if set(expected_by_name) != required_names:
        fail("flash.partitions does not match the fixed M0 layout")

    for name, expected_values in EXPECTED_PARTITIONS.items():
        expected_type, expected_subtype, expected_offset, expected_size, expected_flags = (
            expected_values
        )
        entry = expected_by_name[name]
        if entry.get("type") != expected_type:
            fail(f"partition contract {name} type must be {expected_type}")
        if entry.get("subtype") != expected_subtype:
            fail(f"partition contract {name} subtype must be {expected_subtype}")
        require_exact_int(entry, "offsetBytes", expected_offset)
        require_exact_int(entry, "sizeBytes", expected_size)
        if entry.get("flags") != expected_flags:
            fail(f"partition contract {name} flags must be empty")

    if expected_by_name["nvs"]["sizeBytes"] < NVS_MIN_BYTES:
        fail("primary NVS partition is below 20 KiB")
    if expected_by_name["otadata"]["sizeBytes"] != OTA_DATA_BYTES:
        fail("OTA metadata partition must be exactly 8 KiB")
    if expected_by_name["app0"]["sizeBytes"] != APP_SLOT_BYTES:
        fail("app0 must be exactly 1.5 MiB")
    if expected_by_name["app1"]["sizeBytes"] != APP_SLOT_BYTES:
        fail("app1 must be exactly 1.5 MiB")

    runtime = require_object(budget, "runtime")
    require_exact_int(
        runtime, "steadyStateFreeHeapMinBytes", STEADY_HEAP_MIN_BYTES
    )
    require_exact_int(runtime, "loadedFreeHeapMinBytes", LOADED_HEAP_MIN_BYTES)
    require_exact_int(
        runtime, "largestFreeBlockMinBytes", LARGEST_BLOCK_MIN_BYTES
    )
    if runtime.get("status") != "unverified":
        fail(
            "runtime.status must remain unverified until provenance-aware "
            "target evidence is integrated"
        )
    if runtime.get("proofStage") != "target-system":
        fail("runtime memory requires target-system proof")


def validate_platformio_config(path: Path = PLATFORMIO_CONFIG_PATH) -> None:
    if not path.is_file():
        fail(f"missing PlatformIO configuration: {path}")
    parser = configparser.ConfigParser(interpolation=None)
    try:
        with path.open(encoding="utf-8") as handle:
            parser.read_file(handle)
    except (OSError, configparser.Error) as exc:
        fail(f"invalid PlatformIO configuration {path}: {exc}")

    if set(parser.sections()) != {"platformio", "env:atom_lite"}:
        fail("PlatformIO configuration must contain exactly one atom_lite environment")
    if parser.get("platformio", "default_envs", fallback="").strip() != "atom_lite":
        fail("PlatformIO default environment must be atom_lite")
    if (
        parser.get("platformio", "src_dir", fallback="").strip()
        != SOURCE_DIR
    ):
        fail("PlatformIO source must match the approved USB bootstrap amendment")
    try:
        validate_usb_bootstrap()
    except ValueError as exc:
        fail(str(exc))
    if (
        parser.get("platformio", "build_dir", fallback="").strip()
        != ".arduino/build/platformio"
    ):
        fail("PlatformIO build_dir must remain inside the ignored evidence area")
    if (
        parser.get("platformio", "extra_configs", fallback="").strip()
        != ".arduino/tmp/platformio-local.ini"
    ):
        fail("PlatformIO may load only the designated ignored local override")

    environment = parser["env:atom_lite"]
    required_values = {
        "platform": PLATFORM_URL,
        "board": "m5stack-atom",
        "framework": "arduino",
        "build_type": "release",
        "board_build.partitions": "partitions/dmbridge_ota.csv",
        "upload_speed": "115200",
        "monitor_speed": "115200",
        "extra_scripts": "post:scripts/platformio_image_gate.py",
    }
    for key, expected in required_values.items():
        actual = environment.get(key, "").strip()
        if actual != expected:
            fail(f"PlatformIO {key} must be {expected!r}, found {actual!r}")

    build_flags = environment.get("build_flags", "").split()
    expected_flags = ["-Wall", "-Wextra", "-Werror", "-flto"]
    if build_flags != expected_flags:
        fail(f"PlatformIO build_flags must be {expected_flags!r}")
    if environment.get("build_unflags", "").split() != ["-fno-lto"]:
        fail("PlatformIO build_unflags must remove the framework -fno-lto")
    if "lib_deps" in environment:
        fail("M0 PlatformIO environment must not declare third-party libraries")

    validate_local_platformio_override()


def validate_local_platformio_override(
    path: Path = LOCAL_PLATFORMIO_CONFIG_PATH,
) -> None:
    if not path.exists():
        return
    if not path.is_file():
        fail(f"PlatformIO local override is not a file: {path}")

    parser = configparser.ConfigParser(interpolation=None)
    try:
        with path.open(encoding="utf-8") as handle:
            parser.read_file(handle)
    except (OSError, configparser.Error) as exc:
        fail(f"invalid PlatformIO local override {path}: {exc}")
    if set(parser.sections()) != {"env:atom_lite"}:
        fail("PlatformIO local override may contain only env:atom_lite")
    environment = parser["env:atom_lite"]
    if set(environment) != {"platform_packages"}:
        fail("PlatformIO local override may set only platform_packages")

    package_specs = [
        line.strip()
        for line in environment.get("platform_packages", "").splitlines()
        if line.strip()
    ]
    if len(package_specs) != 1:
        fail("PlatformIO local override must contain exactly one package source")
    package_name, separator, raw_uri = package_specs[0].partition(" @ ")
    if package_name != "tool-esptoolpy" or not separator:
        fail("PlatformIO local override may replace only tool-esptoolpy")
    parsed_uri = urlparse(raw_uri)
    if parsed_uri.scheme != "file" or parsed_uri.netloc:
        fail("PlatformIO local package source must be a local file URI")
    archive = Path(unquote(parsed_uri.path))
    if not archive.is_file():
        fail(f"local tool-esptoolpy archive is unavailable: {archive}")
    try:
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    except OSError as exc:
        fail(f"could not hash local tool-esptoolpy archive {archive}: {exc}")
    if digest != ESPTOOL_ARCHIVE_SHA256:
        fail(f"local tool-esptoolpy archive SHA-256 mismatch: {digest}")


def parse_partitions(path: Path = PARTITION_PATH) -> list[Partition]:
    if not path.is_file():
        fail(f"missing partition CSV: {path}")
    partitions: list[Partition] = []
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            for line_number, row in enumerate(csv.reader(handle), 1):
                if not row or not any(cell.strip() for cell in row):
                    continue
                if row[0].strip().startswith("#"):
                    continue
                if len(row) != 6:
                    fail(
                        f"partition CSV line {line_number} must contain six columns"
                    )
                name, partition_type, subtype, offset, size, flags = (
                    cell.strip() for cell in row
                )
                if not name or not partition_type or not subtype:
                    fail(f"partition CSV line {line_number} has an empty identity field")
                partitions.append(
                    Partition(
                        name=name,
                        partition_type=partition_type,
                        subtype=subtype,
                        offset=parse_size(offset, f"line {line_number} offset"),
                        size=parse_size(size, f"line {line_number} size"),
                        flags=flags,
                        line_number=line_number,
                    )
                )
    except OSError as exc:
        fail(f"could not read partition CSV {path}: {exc}")
    if not partitions:
        fail("partition CSV contains no partitions")
    return partitions


def validate_partition_layout(
    budget: dict[str, Any], partitions: list[Partition]
) -> LayoutMetrics:
    target = require_object(budget, "target")
    flash = require_object(budget, "flash")
    total_flash = target["flashBytes"]
    partition_table_end = (
        flash["partitionTableOffsetBytes"] + flash["partitionTableSizeBytes"]
    )

    by_name = {partition.name: partition for partition in partitions}
    if len(by_name) != len(partitions):
        fail("partition CSV contains duplicate names")
    expected_list = flash["partitions"]
    expected_by_name = {item["name"]: item for item in expected_list}
    if set(by_name) != set(expected_by_name):
        missing = sorted(set(expected_by_name) - set(by_name))
        extra = sorted(set(by_name) - set(expected_by_name))
        fail(f"partition set mismatch; missing={missing}, extra={extra}")

    for name, expected in expected_by_name.items():
        actual = by_name[name]
        actual_values = {
            "type": actual.partition_type,
            "subtype": actual.subtype,
            "offsetBytes": actual.offset,
            "sizeBytes": actual.size,
            "flags": actual.flags,
        }
        for key, expected_value in expected.items():
            if key == "name":
                continue
            if actual_values.get(key) != expected_value:
                fail(
                    f"partition {name} {key} must be {expected_value!r}, "
                    f"found {actual_values.get(key)!r}"
                )

    ordered = sorted(partitions, key=lambda item: item.offset)
    previous_end = partition_table_end
    previous_name = "partition table"
    for partition in ordered:
        if partition.size <= 0:
            fail(f"partition {partition.name} must have a positive size")
        alignment = 0x10000 if partition.partition_type == "app" else 0x1000
        if partition.offset % alignment:
            fail(
                f"partition {partition.name} offset is not aligned to 0x{alignment:x}"
            )
        if partition.size % 0x1000:
            fail(f"partition {partition.name} size is not 4 KiB aligned")
        if partition.offset < previous_end:
            fail(f"partition {partition.name} overlaps {previous_name}")
        if partition.end > total_flash:
            fail(f"partition {partition.name} exceeds 4 MiB flash")
        if partition.subtype.lower() in FILESYSTEM_SUBTYPES:
            fail(f"filesystem partition {partition.name} is prohibited")
        previous_end = partition.end
        previous_name = partition.name

    app_partitions = [
        item for item in partitions if item.partition_type == "app"
    ]
    if {item.subtype for item in app_partitions} != {"ota_0", "ota_1"}:
        fail("layout must contain exactly the ota_0 and ota_1 application slots")
    image_limit = flash["firmwareImageMaxBytes"]
    if image_limit > min(item.size for item in app_partitions):
        fail("firmware image limit exceeds an OTA application slot")

    spare = total_flash - max(item.end for item in partitions)
    minimum_spare = flash.get("minimumUnallocatedSpareBytes")
    if isinstance(minimum_spare, bool) or not isinstance(minimum_spare, int):
        fail("minimumUnallocatedSpareBytes must be an integer")
    if spare < minimum_spare:
        fail(f"unallocated spare is {spare} bytes, below {minimum_spare}")

    return LayoutMetrics(
        flash_bytes=total_flash,
        app_slot_bytes=min(item.size for item in app_partitions),
        firmware_image_max_bytes=image_limit,
        unallocated_spare_bytes=spare,
    )


def validate_firmware_image(path: Path, maximum_bytes: int) -> tuple[int, int]:
    if not path.is_file():
        fail(f"firmware image is unavailable: {path}")
    try:
        image_bytes = path.stat().st_size
    except OSError as exc:
        fail(f"could not stat firmware image {path}: {exc}")
    if image_bytes <= 0:
        fail("firmware image must not be empty")
    if image_bytes > maximum_bytes:
        fail(
            f"firmware image is {image_bytes} bytes, above the {maximum_bytes}-byte limit"
        )
    return image_bytes, maximum_bytes - image_bytes


def validate_runtime_sample(
    budget: dict[str, Any], sample: dict[str, Any]
) -> dict[str, int]:
    target = require_object(budget, "target")
    runtime = require_object(budget, "runtime")
    if sample.get("targetBoardId") != target["boardId"]:
        fail("runtime sample targetBoardId does not match the budget")
    if sample.get("targetSku") != target["sku"]:
        fail("runtime sample targetSku does not match the budget")
    if not isinstance(sample.get("buildId"), str) or not sample["buildId"]:
        fail("runtime sample requires a non-empty buildId")
    if not isinstance(sample.get("capturedAt"), str) or not sample["capturedAt"]:
        fail("runtime sample requires a non-empty capturedAt")

    checks = {
        "steadyStateFreeHeapBytes": "steadyStateFreeHeapMinBytes",
        "loadedFreeHeapBytes": "loadedFreeHeapMinBytes",
        "largestFreeBlockBytes": "largestFreeBlockMinBytes",
    }
    measured: dict[str, int] = {}
    for sample_key, budget_key in checks.items():
        value = sample.get(sample_key)
        if isinstance(value, bool) or not isinstance(value, int):
            fail(f"runtime sample {sample_key} must be an integer")
        threshold = runtime[budget_key]
        if value < threshold:
            fail(f"runtime sample {sample_key} is {value}, below {threshold}")
        measured[sample_key] = value
    return measured


def validate_repository_budget() -> tuple[dict[str, Any], LayoutMetrics]:
    budget = load_budget()
    validate_budget_contract(budget)
    validate_platformio_config()
    layout = validate_partition_layout(budget, parse_partitions())
    return budget, layout


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate the DM bridge flash and runtime memory budget"
    )
    parser.add_argument(
        "--image", type=Path, help="optional firmware.bin to enforce the 1.25 MiB gate"
    )
    parser.add_argument(
        "--runtime-sample",
        type=Path,
        help="optional target-system JSON sample; absence never counts as runtime proof",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        budget, layout = validate_repository_budget()
        image_result = (
            validate_firmware_image(args.image, layout.firmware_image_max_bytes)
            if args.image
            else None
        )
        runtime_result = (
            validate_runtime_sample(
                budget, load_json_object(args.runtime_sample, "runtime sample")
            )
            if args.runtime_sample
            else None
        )
    except BudgetValidationError as exc:
        print(f"Memory budget validation: FAIL: {exc}", file=sys.stderr)
        return 1

    print("Memory budget validation: PASS")
    print(
        f"Flash: {layout.flash_bytes} bytes; OTA slots: 2 x "
        f"{layout.app_slot_bytes} bytes; image limit: "
        f"{layout.firmware_image_max_bytes} bytes; unallocated spare: "
        f"{layout.unallocated_spare_bytes} bytes"
    )
    if image_result:
        image_bytes, margin_bytes = image_result
        print(f"Firmware image: {image_bytes} bytes; margin: {margin_bytes} bytes")
    else:
        print("Firmware image: NOT EVALUATED (pass --image to enforce the gate)")
    if runtime_result:
        print(
            "Runtime sample: PASS; "
            + ", ".join(f"{key}={value}" for key, value in runtime_result.items())
        )
    else:
        print("Runtime proof: NOT EVALUATED (target-system measurements required)")
    print("Proof boundary: host layout/image checks do not prove target runtime memory")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
