from __future__ import annotations

import copy
import tempfile
import unittest
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from validate_memory_budget import (  # noqa: E402
    BudgetValidationError,
    FIRMWARE_IMAGE_MAX_BYTES,
    load_budget,
    parse_partitions,
    validate_budget_contract,
    validate_firmware_image,
    validate_local_platformio_override,
    validate_partition_layout,
    validate_platformio_config,
    validate_runtime_sample,
)


class MemoryBudgetValidationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.budget = load_budget()
        validate_budget_contract(self.budget)

    def test_repository_layout_matches_contract(self) -> None:
        validate_platformio_config()
        metrics = validate_partition_layout(self.budget, parse_partitions())

        self.assertEqual(metrics.flash_bytes, 4 * 1024 * 1024)
        self.assertEqual(metrics.app_slot_bytes, 0x180000)
        self.assertEqual(metrics.firmware_image_max_bytes, 0x140000)
        self.assertEqual(metrics.unallocated_spare_bytes, 0xEB000)

    def test_budget_cannot_raise_firmware_limit(self) -> None:
        weakened = copy.deepcopy(self.budget)
        weakened["flash"]["firmwareImageMaxBytes"] += 1

        with self.assertRaisesRegex(BudgetValidationError, "must be 1310720"):
            validate_budget_contract(weakened)

    def test_budget_cannot_move_partition(self) -> None:
        weakened = copy.deepcopy(self.budget)
        app1 = next(
            item
            for item in weakened["flash"]["partitions"]
            if item["name"] == "app1"
        )
        app1["offsetBytes"] += 0x10000

        with self.assertRaisesRegex(BudgetValidationError, "must be 1638400"):
            validate_budget_contract(weakened)

    def test_runtime_status_cannot_self_assert_verification(self) -> None:
        self_asserted = copy.deepcopy(self.budget)
        self_asserted["runtime"]["status"] = "verified"

        with self.assertRaisesRegex(BudgetValidationError, "must remain unverified"):
            validate_budget_contract(self_asserted)

    def test_local_override_cannot_change_build_flags(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            override = Path(temporary_directory) / "platformio-local.ini"
            override.write_text(
                "[env:atom_lite]\n"
                "platform_packages = tool-esptoolpy @ file:///tmp/archive.tar.gz\n"
                "build_flags = -w\n",
                encoding="utf-8",
            )

            with self.assertRaisesRegex(BudgetValidationError, "only platform_packages"):
                validate_local_platformio_override(override)

    def test_overlapping_partition_is_rejected(self) -> None:
        overlapping_budget = copy.deepcopy(self.budget)
        app1 = next(
            item
            for item in overlapping_budget["flash"]["partitions"]
            if item["name"] == "app1"
        )
        app1["offsetBytes"] = 0x180000
        partitions = [
            copy.copy(item) if item.name != "app1" else copy.copy(item)
            for item in parse_partitions()
        ]
        partitions = [
            item
            if item.name != "app1"
            else type(item)(
                name=item.name,
                partition_type=item.partition_type,
                subtype=item.subtype,
                offset=0x180000,
                size=item.size,
                flags=item.flags,
                line_number=item.line_number,
            )
            for item in partitions
        ]

        with self.assertRaisesRegex(BudgetValidationError, "overlaps app0"):
            validate_partition_layout(overlapping_budget, partitions)

    def test_image_at_limit_passes_and_one_byte_over_fails(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            image = Path(temporary_directory) / "firmware.bin"
            with image.open("wb") as handle:
                handle.truncate(FIRMWARE_IMAGE_MAX_BYTES)
            self.assertEqual(
                validate_firmware_image(image, FIRMWARE_IMAGE_MAX_BYTES),
                (FIRMWARE_IMAGE_MAX_BYTES, 0),
            )
            with image.open("ab") as handle:
                handle.write(b"\x00")

            with self.assertRaisesRegex(BudgetValidationError, "above"):
                validate_firmware_image(image, FIRMWARE_IMAGE_MAX_BYTES)

    def test_runtime_sample_requires_every_threshold(self) -> None:
        sample = {
            "targetBoardId": "m5stack-atom",
            "targetSku": "C008",
            "buildId": "fixture",
            "capturedAt": "2026-09-01T00:00:00Z",
            "steadyStateFreeHeapBytes": 80 * 1024,
            "loadedFreeHeapBytes": 50 * 1024,
            "largestFreeBlockBytes": 32 * 1024,
        }
        self.assertEqual(
            validate_runtime_sample(self.budget, sample)["loadedFreeHeapBytes"],
            50 * 1024,
        )
        sample["largestFreeBlockBytes"] -= 1

        with self.assertRaisesRegex(BudgetValidationError, "below 32768"):
            validate_runtime_sample(self.budget, sample)


if __name__ == "__main__":
    unittest.main()
